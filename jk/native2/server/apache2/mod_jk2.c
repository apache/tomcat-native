/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/***************************************************************************
 * Description: Apache 2 plugin for Jakarta/Tomcat                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 *              Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/

/*
 * mod_jk2: keeps all servlet/jakarta related ramblings together.
 */


#include "jk_apache2.h"
#include "scoreboard.h"
#include "ap_mpm.h"

#include "util_script.h"

#ifdef WIN32
static char  file_name[_MAX_PATH];
#endif

/* This is used to ensure that jk2_create_dir_config creates unique
 * dir mappings. This prevents vhost configs as configured through
 * httpd.conf from getting crossed.
 */
static int dirCounter=0;

#define JK_HANDLER          ("jakarta-servlet2")
#define JK_MAGIC_TYPE       ("application/x-jakarta-servlet2")

module AP_MODULE_DECLARE_DATA jk2_module;

/* In apache1.3 this is reset when the module is reloaded ( after
 * config. No good way to discover if it's the first time or not.
 */
static jk_workerEnv_t *workerEnv;

/* ==================== Options setters ==================== */

/*
 * The JK2 module command processors. The options can be specified
 * in a properties file or in httpd.conf, depending on user's taste.
 *
 * There is absolutely no difference from the point of view of jk,
 * but apache config tools might prefer httpd.conf and the extra
 * information included in the command descriptor. It also have
 * a 'natural' feel, and is consistent with all other apache
 * settings and modules. 
 *
 * Properties file are easier to parse/generate from java, and
 * allow identical configuration for all servers. We should have
 * code to generate the properties file or use the wire protocol,
 * and make all those properties part of server.xml or jk's
 * java-side configuration. This will give a 'natural' feel for
 * those comfortable with the java side.
 *
 * The only exception is webapp definition, where in the near
 * future you can expect a scalability difference between the
 * 2 choices. If you have a large number of apps/vhosts you
 * _should_ use the apache style, that makes use of the
 * internal apache mapper ( known to scale to very large number
 * of hosts ). The internal jk mapper uses linear search, ( will
 * eventually use hash tables, when we add support for apr_hash ),
 * and is nowhere near the apache mapper.
 */

/*
 * JkSet name value
 *
 * Set jk options. Same as using workers.properties.
 * Common properties: see workers.properties documentation
 */
static const char *jk2_set2(cmd_parms *cmd,void *per_dir,
                            const char *name,  char *value)
{
    server_rec *s = cmd->server;
    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk2_module);
    jk_env_t *env=workerEnv->globalEnv;
    int rc;
    
    rc=workerEnv->config->setPropertyString( env, workerEnv->config, (char *)name, value );
    if (rc!=JK_OK) {
        ap_log_perror(APLOG_MARK, APLOG_NOTICE, 0, cmd->temp_pool,
                      "mod_jk2: Unrecognized option %s %s", name, value);
    }

    return NULL;
}

/*
 * JkSet2 oname property value
 *
 * Set jk options. 
 */
static const char *jk2_set3(cmd_parms *cmd,void *per_dir,
                            const char *name, char *property, char *value)
{
    server_rec *s = cmd->server;
    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk2_module);
    jk_env_t *env=workerEnv->globalEnv;
    int rc;
    jk_bean_t *mbean;

    if( name == NULL || property==NULL || value==NULL ) {
        ap_log_perror(APLOG_MARK, APLOG_NOTICE, 0, cmd->temp_pool,
                      "mod_jk2: Null option in JkSet2");
        return NULL;
    }
    
    mbean=env->getBean( env, name );
    if( mbean==NULL ) {
        ap_log_perror(APLOG_MARK, APLOG_NOTICE, 0, cmd->temp_pool,
                      "mod_jk2: Creating object %s", name );
        mbean=env->createBean( env, workerEnv->config->pool, (char *)name );
    }

    if( mbean == NULL ) {
        /* Can't create it, save the value in our map */
        workerEnv->config->setProperty( env, workerEnv->config, workerEnv->config->mbean, (char *)name, value );
        return NULL;
    }

    if( mbean->settings == NULL )
        jk2_map_default_create(env, &mbean->settings, workerEnv->config->pool);
    
    rc=workerEnv->config->setProperty( env, workerEnv->config, mbean, property, value );

    if (rc!=JK_OK) {
        ap_log_perror(APLOG_MARK, APLOG_NOTICE, 0, cmd->temp_pool,
                      "mod_jk2: Unrecognized option %s %s %s", name, property, value);
    }

    return NULL;
}

/**
 * Set a property associated with a URI, using native <Location> 
 * directives.
 *
 * This is used if you want to use the native mapping and
 * integrate better into apache.
 *
 * Same behavior can be achieved by using uri.properties and/or JkSet.
 * 
 * Example:
 *   <VirtualHost foo.com>
 *      <Location /examples>
 *         JkUriSet worker ajp13
 *      </Location>
 *   </VirtualHost>
 *
 * This is the best way to define a webapplication in apache. It is
 * scalable ( using apache native optimizations, you can have hundreds
 * of hosts and thousands of webapplications ), 'natural' to any
 * apache user.
 *
 * XXX This is a special configuration, for most users just use
 * the properties files.
 */
static const char *jk2_uriSet(cmd_parms *cmd, void *per_dir, 
                              const char *name, const char *val)
{
    jk_uriEnv_t *uriEnv=(jk_uriEnv_t *)per_dir;

    char *tmp_virtual=NULL;
    char *tmp_full_url=NULL;
    server_rec *s = cmd->server;

    uriEnv->mbean->setAttribute( workerEnv->globalEnv, uriEnv->mbean, (char *)name, (void *)val );

    /*
     * all of the objects that get passed in now are unique. create_dir adds a incrementing counter to the
     * uri that is used to create the object!
     * Here we must now 'fix' the content of the object passed in.
     * Apache doesn't care what we do here as it has the reference to the unique object that has been
     * created. What we need to do is ensure that the data given to mod_jk2 is correct. Hopefully in the long run
     * we can ignore some of the mod_jk2 details...
     */

    /* if applicable we will set the hostname etc variables. */
    if (s->is_virtual && s->server_hostname != NULL &&
        (uriEnv->virtual==NULL  || !strchr(uriEnv->virtual, ':') ||
        uriEnv->port != s->port)) {
        tmp_virtual  = (char *) apr_pcalloc(cmd->pool,
                        sizeof(char *) * (strlen(s->server_hostname) + 8 )) ;
        tmp_full_url = (char *) apr_pcalloc(cmd->pool,
                        sizeof(char *) * (strlen(s->server_hostname) +
                        strlen(uriEnv->uri)+8 )) ; 
        /* do not pass the hostname:0/ scheme */
        if (s->port) {
            sprintf(tmp_virtual,  "%s:%d", s->server_hostname, s->port);
            sprintf(tmp_full_url, "%s:%d%s", s->server_hostname, s->port, uriEnv->uri );
        }
        else {
            strcpy(tmp_virtual, s->server_hostname);
            strcpy(tmp_full_url, s->server_hostname);
            strcat(tmp_full_url, uriEnv->uri);
        }
        uriEnv->mbean->setAttribute( workerEnv->globalEnv, uriEnv->mbean, "uri", tmp_full_url);
        uriEnv->mbean->setAttribute( workerEnv->globalEnv, uriEnv->mbean, "path", cmd->path);
        uriEnv->name=tmp_virtual;
        uriEnv->virtual=tmp_virtual;

    }
    /* now lets actually add the parameter set in the <Location> block */
    uriEnv->mbean->setAttribute( workerEnv->globalEnv, uriEnv->mbean, (char *)name, (void *)val );

    return NULL;
}

/* Command table.
 */
static const command_rec jk2_cmds[] =
    {
        /* This is the 'main' directive for tunning jk2. It takes 2 parameters,
           and it behaves _identically_ as a setting in workers.properties.
        */
    AP_INIT_TAKE2(
        "JkSet", jk2_set2, NULL, RSRC_CONF,
        "Set a jk property, 2 parameters - objectName.property value"),
    AP_INIT_TAKE3(
        "JkSet2", jk2_set3, NULL, RSRC_CONF,
        "Set a jk property, 3 parameters - objectName property value"),
    AP_INIT_TAKE2(
        "JkUriSet", jk2_uriSet, NULL, ACCESS_CONF,
        "Defines a jk property associated with a Location"),
    NULL
    };

static void *jk2_create_dir_config(apr_pool_t *p, char *path)
{
    /* We don't know the vhost yet - so path is not
     * unique. We'll have to generate a unique name
     */
    char *tmp=NULL;
    int a=10;
    jk_bean_t *jkb;
    jk_uriEnv_t *newUri;

    if (!path)
        return NULL;

    a = strlen(path)+10;
    /* Original patch: a * sizeof( char * ) - that's weird, we only use a chars, not char*
       Maybe I wrote too much java...
    */
    tmp = (char *) apr_pcalloc(p, a); 
    sprintf(tmp, "%s-%d", path, dirCounter++);
    /* I changed the default to /, otherwise it complains */

    jkb=workerEnv->globalEnv->createBean2( workerEnv->globalEnv,
                                                      workerEnv->pool, "uri",
                                                      tmp );
    newUri = jkb->object;

    newUri->workerEnv=workerEnv;
    newUri->mbean->setAttribute( workerEnv->globalEnv, newUri->mbean, "path", tmp );
    /* I'm hoping that setting the id won't break anything. I havn't noticed it breaking anything. */
    newUri->mbean->id=(dirCounter -1);
    /* this makes the display in the status display make more sense */
    newUri->mbean->localName=path;

    return newUri;
}


static void *jk2_merge_dir_config(apr_pool_t *p, void *childv, void *parentv)
{
    jk_uriEnv_t *child =(jk_uriEnv_t *)childv;
    jk_uriEnv_t *parent = (jk_uriEnv_t *)parentv; 
    jk_uriEnv_t *winner=NULL;
    jk_uriEnv_t *loser=NULL;

    if ( child == NULL || child->uri==NULL || child->workerName==NULL ) {
           winner=parent;
           loser=child;
    } else if ( parent == NULL || parent->uri ==NULL || parent->workerName==NULL ) {
           winner=child;
           loser=parent;
           /* interresting bit... so far they are equal ... */
    } else if ( strlen(parent->uri) > strlen(child->uri) ) {
           winner=parent;
           loser=child;
    } else {
           winner=child;
           loser=parent;
    }

    /* Do we merge loser into winner - i.e. inherit properties ? */

    /*if ( winner == child )
         fprintf(stderr, "Going with the child\n");
      else if ( winner == parent )
        fprintf(stderr, "Going with the parent\n");
      else 
        fprintf(stderr, "Going with NULL\n");
   */
    fprintf(stderr, "Merging %s %s %s\n",
            (winner==NULL || winner->uri==NULL) ? "" : winner->uri,
            (child==NULL || child->uri==NULL) ? "" : child->uri,
            (parent==NULL || parent->uri==NULL) ? "" : parent->uri);
            

    return (void *) winner;

}

/** Basic initialization for jk2.
 */
static void jk2_create_workerEnv(apr_pool_t *p, server_rec *s) {
    jk_env_t *env;
    jk_logger_t *l;
    jk_pool_t *globalPool;
    jk_bean_t *jkb;
    
    jk2_pool_apr_create( NULL, &globalPool, NULL, p );

    /** Create the global environment. This will register the default
        factories
    */
    env=jk2_env_getEnv( NULL, globalPool );

    /* Optional. Register more factories ( or replace existing ones ) */
    /* Init the environment. */
    
    /* Create the logger */
#ifdef NO_APACHE_LOGGER
    jkb=env->createBean2( env, env->globalPool, "logger.file", "");
    env->alias( env, "logger.file:", "logger");
    env->alias( env, "logger.file:", "logger:");
    l = jkb->object;
#else
    env->registerFactory( env, "logger.apache2",  jk2_logger_apache2_factory );
    jkb=env->createBean2( env, env->globalPool, "logger.apache2", "");
    env->alias( env, "logger.apache2:", "logger");
    l = jkb->object;
    l->logger_private=s;
#endif
    
    env->l=l;
    
#ifdef WIN32
    env->soName=env->globalPool->pstrdup(env, env->globalPool, file_name);
    
    if( env->soName == NULL ){
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "Error creating env->soName\n");
        return;
    }
#else 
    env->soName=NULL;
#endif
    /* We should make it relative to JK_HOME or absolute path.
       ap_server_root_relative(cmd->pool,opt); */
    
    /* Create the workerEnv */
    jkb=env->createBean2( env, env->globalPool,"workerEnv", "");
    workerEnv= jkb->object;
/*     workerEnv->logger_name= "logger.apache2"; */
    env->alias( env, "workerEnv:" , "workerEnv");
    
    if( workerEnv==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "Error creating workerEnv\n");
        return;
    }
    workerEnv->initData->add( env, workerEnv->initData, "serverRoot",
                              workerEnv->pool->pstrdup( env, workerEnv->pool, ap_server_root));
    env->l->jkLog(env, env->l, JK_LOG_INFO, "Set serverRoot %s\n", ap_server_root);
    
    /* Local initialization */
    workerEnv->_private = s;
}

/** Create default jk_config. XXX This is mostly server-independent,
    all servers are using something similar - should go to common.

    This is the first thing called ( or should be )
 */
static void *jk2_create_config(apr_pool_t *p, server_rec *s)
{
    jk_uriEnv_t *newUri;
    jk_bean_t *jkb;
    
    if(  workerEnv==NULL ) {
        jk2_create_workerEnv(p, s );
    }
    if( s->is_virtual ) {
        /* Virtual host */
        ap_log_perror(APLOG_MARK, APLOG_DEBUG, 0, p,
                      "mod_jk2 Create config for virtual host %s",
                      s->server_hostname );
    } else {
        ap_log_perror(APLOG_MARK, APLOG_DEBUG, 0, p,
                      "mod_jk2 Create config for default server %s",
                      s->server_hostname );
    }

    jkb = workerEnv->globalEnv->createBean2( workerEnv->globalEnv,
                                             workerEnv->pool,
                                             "uri", "" );
   newUri=jkb->object;
   
   newUri->workerEnv=workerEnv;
    
   return newUri;
}



/** Standard apache callback, merge jk options specified in 
    <Host> context. Used to set per virtual host configs
 */
static void *jk2_merge_config(apr_pool_t *p, 
                              void *basev, 
                              void *overridesv)
{
    jk_uriEnv_t *base = (jk_uriEnv_t *) basev;
    jk_uriEnv_t *overrides = (jk_uriEnv_t *)overridesv;
    
    ap_log_perror(APLOG_MARK, APLOG_DEBUG, 0, p,
                  "mod_jk2 Merging workerEnv");

    
    /* The 'mountcopy' option should be implemented in common.
     */
    return overrides;
}

static apr_status_t jk2_shutdown(void *data)
{
    jk_env_t *env;
    if (workerEnv) {
        env=workerEnv->globalEnv;

/*         env->l->jkLog(env, env->l, JK_LOG_INFO, */
/*                       "mod_jk2 Shutting down\n"); */
        workerEnv->close(env, workerEnv);
        workerEnv = NULL;
    }
    return APR_SUCCESS;
}


/** Initialize jk, using worker.properties. 
    We also use apache commands ( JkWorker, etc), but this use is 
    deprecated, as we'll try to concentrate all config in
    workers.properties, urimap.properties, and ajp14 autoconf.
    
    Apache config will only be used for manual override, using 
    SetHandler and normal apache directives ( but minimal jk-specific
    stuff )
*/
static char * jk2_init(jk_env_t *env, apr_pool_t *pconf,
                       jk_workerEnv_t *workerEnv, server_rec *s )
{

    ap_mpm_query(AP_MPMQ_MAX_DAEMONS, &workerEnv->maxDaemons);

    workerEnv->init(env, workerEnv );
    workerEnv->server_name   = (char *)ap_get_server_version();

    apr_pool_cleanup_register(pconf, NULL, jk2_shutdown, apr_pool_cleanup_null);
    return NULL;
}

/* Apache will first validate the config then restart.
   That will unload all .so modules - including ourself.
   Keeping 'was_initialized' in workerEnv is pointless, since both
   will disapear.
*/
static int jk2_apache2_isValidating(apr_pool_t *gPool, apr_pool_t **mainPool) {
    apr_pool_t *tmpPool=NULL;
    void *data=NULL;
    int i;
    
    for( i=0; i<10; i++ ) {
        tmpPool=apr_pool_parent_get( gPool );
        if( tmpPool == NULL ) {
            /* fprintf(stderr, "XXX Found Root pool %#lx\n", gPool ); */
            break;
        }
        gPool=tmpPool;
    }

    if( tmpPool != NULL ) {
        /* We can't detect the root pool */
        /* fprintf(stderr, "XXX Can't find root pool\n" ); */
        return JK_ERR;
    }
    if(mainPool != NULL )
        *mainPool=gPool;
    
    /* We have a global pool ! */
    apr_pool_userdata_get( &data, "mod_jk2_init", gPool );
    if( data==NULL ) {
        return JK_OK;
    } else {
        return JK_ERR;
    }
}

static int jk2_post_config(apr_pool_t *pconf, 
                           apr_pool_t *plog, 
                           apr_pool_t *ptemp, 
                           server_rec *s)
{
    apr_pool_t *gPool=NULL;
    int rc;
    jk_env_t *env;
    
    if(s->is_virtual) 
        return OK;

    /* Other apache 2.0 modules add version info at post_config */
    ap_add_version_component(pconf, JK_EXPOSED_VERSION);

    env=workerEnv->globalEnv;
    
    rc=jk2_apache2_isValidating( plog, &gPool );

    env->setAprPool(env, gPool);

    if( rc == JK_OK && gPool != NULL ) {
        /* This is the first step */
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "mod_jk2.post_config() first invocation\n");
        
        apr_pool_userdata_set( "INITOK", "mod_jk2_init", NULL, gPool );
        return OK;
    }
        
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "mod_jk2.post_config() second invocation\n" ); 
    workerEnv->parentInit( env, workerEnv);

    return OK;
}

/** Standard apache callback, initialize jk.
 */
static void jk2_child_init(apr_pool_t *pconf, 
                           server_rec *s)
{
    apr_proc_t proc;    
    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk2_module);
    jk_env_t *env;
        
    if( workerEnv==NULL )
        workerEnv = serverEnv->workerEnv;

    env=workerEnv->globalEnv;
    
    if (!workerEnv->childProcessId)
        workerEnv->childProcessId = getpid();

    proc.pid = workerEnv->childProcessId;

    /* detect if scoreboard exists
    */
    if (!ap_exists_scoreboard_image()) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
            "jk2_init() Scoreboard image does not exists %d\n", proc.pid);
        workerEnv->childId=-2;
    }
    else
        workerEnv->childId = find_child_by_pid(&proc);
    /* Avoid looking again
    *  and fix the mpm_winnt reporting 0 daemons.
    */
    if (workerEnv->childId == -1) {
        /* If the server max daemons are less then 2
         * this is the single child mpm.
         * the WINNT mpm has a bug returning 0 instead 1
         */
        if (workerEnv->maxDaemons < 2) {
            workerEnv->childId = proc.pid;    
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                "jk2_init() Setting scoreboard slot 0 for child %d\n",
                proc.pid);
        }
        else {
            env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                "jk2_init() Can't find child %d in none of the %d scoreboard slots\n",
                proc.pid, workerEnv->maxDaemons);
            workerEnv->childId = -2;
        }
    } else {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
            "jk2_init() Found child %d in scoreboard slot %d\n",
            proc.pid, workerEnv->childId);
    }

    if(!workerEnv->was_initialized) {
        workerEnv->was_initialized = JK_TRUE;        
        
        jk2_init( env, pconf, workerEnv, s );

        if (workerEnv->childId > 0) 
            env->l->jkLog(env, env->l, JK_LOG_INFO, "mod_jk2 child %d initialized\n",
                          workerEnv->childId);
    }    
}


/* ========================================================================= */
/* The JK module handlers                                                    */
/* ========================================================================= */

/** Main service method, called to forward a request to tomcat
 */
static int jk2_handler(request_rec *r)
{   
    jk_logger_t      *l=NULL;
    int              rc;
    jk_worker_t *worker=NULL;
    jk_endpoint_t *end = NULL;
    jk_uriEnv_t *uriEnv;
    jk_env_t *env;

    jk_ws_service_t *s=NULL;
    jk_pool_t *rPool=NULL;
    int rc1;

    uriEnv=ap_get_module_config( r->request_config, &jk2_module );
    if (uriEnv==NULL)
      uriEnv=ap_get_module_config( r->per_dir_config, &jk2_module );

    /* not for me, try next handler */
    if(uriEnv==NULL || strcmp(r->handler,JK_HANDLER)!=0)
      return DECLINED;
    
    /* If this is a proxy request, we'll notify an error */
    if(r->proxyreq) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    /* Get an env instance */
    env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );

    /* Set up r->read_chunked flags for chunked encoding, if present */
    if(rc = ap_setup_client_block(r, REQUEST_CHUNKED_DECHUNK)) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "mod_jk2.handler() Can't setup client block %d\n", rc);
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
        return rc;
    }

    worker=uriEnv->worker;
        
    if( worker==NULL && uriEnv->workerName != NULL ) {
        worker=env->getByName( env, uriEnv->workerName);
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "mod_jk2.handler() finding worker for %#lx %#lx %s\n",
                      worker, uriEnv, uriEnv->workerName);
        uriEnv->worker=worker;
    }

    if(worker==NULL || worker->mbean == NULL || worker->mbean->localName==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "mod_jk2.handle() No worker for %s\n", r->uri); 
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    if( uriEnv->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                      "mod_jk2.handler() serving %s with %#lx %#lx %s\n",
                      uriEnv->mbean->localName, worker, worker->mbean, worker->mbean->localName );

    /* Get a pool for the request XXX move it in workerEnv to
       be shared with other server adapters */
    rPool= worker->rPoolCache->get( env, worker->rPoolCache );
    if( rPool == NULL ) {
        rPool=worker->mbean->pool->create( env, worker->mbean->pool, HUGE_POOL_SIZE );
        if( uriEnv->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "mod_jk2.handler(): new rpool %#lx\n", rPool );
    }
    
    s=(jk_ws_service_t *)rPool->calloc( env, rPool, sizeof( jk_ws_service_t ));
    
    /* XXX we should reuse the request itself !!! */
    jk2_service_apache2_init( env, s );

    s->pool = rPool;
    s->init( env, s, worker, r );
    
    /* reset the reco_status, will be set to INITED in LB mode */
    s->reco_status = RECO_NONE;
    
    s->is_recoverable_error = JK_FALSE;
    s->uriEnv = uriEnv; 

    /* env->l->jkLog(env, env->l, JK_LOG_INFO,  */
    /*              "mod_jk2.handler() Calling %s\n", worker->mbean->name); */
    
    rc = worker->service(env, worker, s);
    
    s->afterRequest(env, s);
    
    rPool->reset(env, rPool);
    
    rc1=worker->rPoolCache->put( env, worker->rPoolCache, rPool );
    if( rc1 == JK_OK ) {
        rPool=NULL;
    }
    if( rPool!=NULL ) {
        rPool->close(env, rPool);
    }

    if(rc==JK_OK) {
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
        return OK;    /* NOT r->status, even if it has changed. */
    }

    env->l->jkLog(env, env->l, JK_LOG_ERROR,
                  "mod_jk2.handler() Error connecting to tomcat %d, status %d\n", rc, s->status);
                  
    workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );

    /* In case of error, if service() set a status code, send it back */
    /* Else fallback to HTTP_INTERNAL_SERVER_ERROR (500). */
    if (s->status != 0)
        return s->status;
    else
        return HTTP_INTERNAL_SERVER_ERROR;  
}

/** Use the internal mod_jk2 mappings to find if this is a request for
 *    tomcat and what worker to use. 
 */
static int jk2_translate(request_rec *r)
{
    jk_uriEnv_t *uriEnv;
    jk_env_t *env;

    if(r->proxyreq || workerEnv==NULL) {
        return DECLINED;
    }

    /* For the JkUriSet */
    uriEnv=ap_get_module_config( r->per_dir_config, &jk2_module );

    /* This has been mapped to a location by apache
     * In a previous ( experimental ) version we had a sub-map,
     * but that's too complex for now.
     */
    if( uriEnv!= NULL && uriEnv->workerName != NULL) {
        /* get_env() */
        env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );

        if( uriEnv->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                          "PerDir mapping  %s=%s\n",r->uri, uriEnv->workerName);
        
        ap_set_module_config( r->request_config, &jk2_module, uriEnv );        
        r->handler=JK_HANDLER;
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );

        /* This could be a sub-request, possibly from mod_dir */
        if(r->main){
            ap_set_module_config( r->main->request_config, &jk2_module, uriEnv );
            r->main->handler=JK_HANDLER;
        }

        return OK;
    }

    return DECLINED;
}

/* XXX Can we use type checker step to set our stuff ? */

/* bypass the directory_walk and file_walk for non-file requests */
static int jk2_map_to_storage(request_rec *r)
{
    jk_uriEnv_t *uriEnv;
    jk_env_t *env;
    const char *ptr;

    if (r->proxyreq || workerEnv==NULL) {
        return DECLINED;
    }

    /* If already mapped by translate just returns OK */
    uriEnv = ap_get_module_config( r->request_config, &jk2_module );
    if (uriEnv!=NULL && uriEnv->workerName != NULL)
        return OK;

    /* From something like [uri:/examples/STAR] in workers2.properties */
    env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );
    ptr = ap_get_server_name(r);
    if ( strlen(ptr) > 1024 - 12 ) {
        /* That is probably an invalid request, DECLINED could display jsp source code. */
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                          "jk2_map_to_storage Host too big %s\n", ptr);
        return HTTP_BAD_REQUEST;
    }
    uriEnv=workerEnv->uriMap->mapUri(env, workerEnv->uriMap,
        ptr,
        ap_get_server_port(r),
        r->uri);

    if (uriEnv!=NULL && uriEnv->workerName != NULL) {
        ap_set_module_config( r->request_config, &jk2_module, uriEnv );
        r->handler=JK_HANDLER;
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
 
        /* This could be a sub-request, possibly from mod_dir */
        if(r->main){
            ap_set_module_config( r->main->request_config, &jk2_module, uriEnv );
            r->main->handler=JK_HANDLER;
        }

        return OK;
    }

    workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
    return DECLINED;
}

static void jk2_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(jk2_handler, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_post_config(jk2_post_config, NULL, NULL, APR_HOOK_MIDDLE);

    /* Force the mpm to run before us and set the scoreboard image */
    ap_hook_child_init(jk2_child_init, NULL, NULL, APR_HOOK_LAST);

    ap_hook_translate_name(jk2_translate, NULL, NULL, APR_HOOK_MIDDLE); 
    ap_hook_map_to_storage(jk2_map_to_storage, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA jk2_module =
{
    STANDARD20_MODULE_STUFF,
    jk2_create_dir_config, /*  dir config creater */
    jk2_merge_dir_config,  /* dir merger --- default is to override */
    jk2_create_config,     /* server config */
    jk2_merge_config,      /* merge server config */
    jk2_cmds,              /* command ap_table_t */
    jk2_register_hooks     /* register hooks */
};

#ifdef WIN32

BOOL WINAPI DllMain(HINSTANCE hInst,        // Instance Handle of the DLL
                    ULONG ulReason,         // Reason why NT called this DLL
                    LPVOID lpReserved)      // Reserved parameter for future use
{
    GetModuleFileName( hInst, file_name, sizeof(file_name));
    return TRUE;
}


#endif
