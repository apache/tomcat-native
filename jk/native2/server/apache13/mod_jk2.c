/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2003 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */

/***************************************************************************
 * Description: Apache 1.3 plugin for Jakarta/Tomcat                       *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 *              Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/

/*
 * mod_jk: keeps all servlet/jakarta related ramblings together.
 */
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#include "util_script.h"
#include "http_conf_globals.h"

/*
 * Jakarta (jk_) include files
 */
#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_env.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_workerEnv.h"
#include "jk_uriMap.h"
#include "jk_requtil.h"

#ifdef WIN32
static char  file_name[_MAX_PATH];
#endif


#define JK_HANDLER          ("jakarta-servlet2")
#define JK_MAGIC_TYPE       ("application/x-jakarta-servlet2")

module MODULE_VAR_EXPORT jk2_module;

int jk2_service_apache13_init(jk_env_t *env, jk_ws_service_t *s);


/* In apache1.3 this is reset when the module is reloaded ( after
 * config. No good way to discover if it's the first time or not.
 */
static jk_workerEnv_t *workerEnv;
/* This is used to ensure that jk2_create_dir_config creates unique 
 * dir mappings. This prevents vhost configs as configured through
 * httpd.conf from getting crossed.
 */
static int dirCounter=0;

/* ==================== Options setters ==================== */

/*
 * JkSet name value
 *
 * Set jk options. Same as using workers.properties or a
 * config application.
 *
 * Known properties: see workers.properties documentation
 *
 * XXX Shouldn't abuse it, there is no way to write back
 * the properties.
 */
static const char *jk2_set2(cmd_parms *cmd, void *per_dir,
                            const char *name,  char *value)
{
    server_rec *s = cmd->server;
    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk2_module);
    jk_env_t *env=workerEnv->globalEnv;
    int rc;
    
    rc=workerEnv->config->setPropertyString( env, workerEnv->config, (char *)name, value );
    if( rc!=JK_OK ) {
        fprintf( stderr, "mod_jk2: Unrecognized option %s %s\n", name, value);
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
     * we can ignore some of the mod_jk details...
     */

    /* if applicable we will set the hostname etc variables. */
    if (s->is_virtual && s->server_hostname != NULL &&
        (uriEnv->virtual==NULL  || !strchr(uriEnv->virtual, ':') ||
        uriEnv->port != s->port)) {
        tmp_virtual  = (char *) ap_pcalloc(cmd->pool,
                        sizeof(char *) * (strlen(s->server_hostname) + 8 )) ;
        tmp_full_url = (char *) ap_pcalloc(cmd->pool,
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


static void *jk2_create_dir_config(pool *p, char *path)
{
    /* We don't know the vhost yet - so path is not
     * unique. We'll have to generate a unique name
     */
    char *tmp=NULL;
    int a=0;
    jk_uriEnv_t *newUri;
    jk_bean_t *jkb;

    if (!path)
        return NULL;

    a = strlen(path) + 10;
    tmp = (char *) ap_pcalloc(p, sizeof(char *) * (a )   ) ;
    sprintf(tmp, "%s-%d", path==NULL?"":path, dirCounter++);

    /* the greatest annoyance here is that we can't create the uri correctly with the hostname as well.
       as apache doesn't give us the hostname .
       we'll fix this in JkUriSet
    */

    jkb=workerEnv->globalEnv->createBean2(workerEnv->globalEnv,
                                          workerEnv->pool, "uri", tmp);
    newUri = jkb->object;
    newUri->workerEnv=workerEnv;
    newUri->mbean->setAttribute( workerEnv->globalEnv, newUri->mbean, "path", tmp ); 
    /* I'm hoping that setting the id won't break anything. I havn't noticed it breaking anything. */
    newUri->mbean->id=(dirCounter -1);
    /* this makes the display in the status display make more sense  */
    newUri->mbean->localName=path;

    return newUri;
}

/*
 * Need to re-do this to make more sense - like properly creating a new config and returning the merged config...
 * Looks like parent needs to be dominant.
 */
static void *jk2_merge_dir_config(pool *p, void *childv, void *parentv)
{
    jk_uriEnv_t *child =(jk_uriEnv_t *)childv;
    jk_uriEnv_t *parent = (jk_uriEnv_t *)parentv; 
    jk_uriEnv_t *winner=NULL;


/*        fprintf(stderr,"Merging child & parent. (dir)\n");
   fprintf(stderr, "Merging for  vhost child(%s) vhost parent(%s) uri child(%s) uri parent(%s) child worker (%s) parentworker(%s)\n",
              (child->virtual==NULL)?"":child->virtual,
         (parent->virtual==NULL)?"":parent->virtual,
         (child->uri==NULL)?"":child->uri,
              (parent->uri==NULL)?"":parent->uri,
         (child->workerName==NULL)?"":child->workerName,
         (parent->workerName==NULL)?"":parent->workerName
         ); */


     if ( child == NULL || child->uri==NULL || child->workerName==NULL )
        winner=parent;
     else if ( parent == NULL || parent->uri ==NULL || parent->workerName==NULL )
        winner=child;
     /* interresting bit... so far they are equal ... */
     else if ( strlen(parent->uri) > strlen(child->uri) )
        winner=parent;
     else 
        winner=child;

/*     if ( winner == child )
   fprintf(stderr, "Going with the child\n");  
     else if ( winner == parent )
   fprintf(stderr, "Going with the parent\n"); 
     else
   fprintf(stderr, "Going with NULL\n");    
*/
 
     return (void *) winner;

}



apr_pool_t *jk_globalPool;

/* Create the initial set of objects. You need to cut&paste this and
   adapt to your server.
 */
static int jk2_create_workerEnv(ap_pool *p, const server_rec *s)
{
    jk_env_t *env;
    jk_pool_t *globalPool;
    jk_bean_t *jkb;

    apr_initialize();
    apr_pool_create( &jk_globalPool, NULL );

    jk2_pool_apr_create( NULL, &globalPool, NULL, jk_globalPool );
    
    /** Create the global environment. This will register the default
        factories, to be overriten later.
    */
    env=jk2_env_getEnv( NULL, globalPool );

    /* Optional. Register more factories ( or replace existing ones )
       Insert your server-specific objects here.
    */

    /* Create the logger . We use the default jk logger, will output
       to a file. Check the logger for default settings.
    */
    jkb=env->createBean2( env, env->globalPool, "logger.file", "");
    env->alias( env, "logger.file:", "logger");
    env->alias( env, "logger.file:", "logger:"); 
    if( jkb==NULL ) {
        fprintf(stderr, "Error creating logger ");
        return JK_ERR;
    }
    env->l=jkb->object;
    env->alias( env, "logger.file:", "logger");
    
    /* Create the workerEnv
     */
    jkb=env->createBean2( env, env->globalPool,"workerEnv", "");
    if( jkb==NULL ) {
        fprintf(stderr, "Error creating workerEnv ");
        return JK_ERR;
    }
    workerEnv= jkb->object;
    env->alias( env, "workerEnv:", "workerEnv");

    if( workerEnv==NULL || env->l == NULL  ) {
        fprintf( stderr, "Error initializing jk, NULL objects \n");
        return JK_ERR;
    }

    /* serverRoot via ap_server_root
     */
    workerEnv->initData->add( env, workerEnv->initData, "serverRoot",
                             workerEnv->pool->pstrdup( env, workerEnv->pool, ap_server_root));

                             /* Local initialization.
     */
    env->l->jkLog(env, env->l, JK_LOG_INFO, "Set serverRoot %s\n", ap_server_root); 

    workerEnv->_private = (void *)s;

    return JK_OK;
}


/* -------------------- Apache specific initialization -------------------- */

/* Command table.
 */
static const command_rec jk2_cmds[] =
    {
        /* This is the 'main' directive for tunning jk2. It takes 2 parameters,
           and it behaves _identically_ as a setting in workers.properties.
        */
        { "JkSet", jk2_set2, NULL, RSRC_CONF, TAKE2,
          "Set a jk property, same syntax and rules as in JkWorkersFile" },
   { "JkUriSet", jk2_uriSet, NULL, ACCESS_CONF, TAKE2,
          "Defines a jk property associated with a Location"},
     NULL
    };

/** This makes the config for the specified server_rec s
    This will include vhost info.
 */
static void *jk2_create_config(ap_pool *p, server_rec *s)
{
    jk_uriEnv_t *newUri;
    jk_bean_t *jkb;
    char *tmp;

    if(  workerEnv==NULL ) {
        jk2_create_workerEnv(p, s );
    }
    if( s->is_virtual == 1 ) {
        /* Virtual host */

    tmp = (char *) ap_pcalloc(p, sizeof(char *) * (strlen(s->server_hostname) + 8 )) ;
    sprintf(tmp, "%s:%d/", s->server_hostname, s->port );

    /* for the sake of consistency we must have the port in the uri.
       Really it isn't necessary to have one - but I would like in the future for 
       the server config to hold the workers for that server... */
    jkb=workerEnv->globalEnv->createBean2( workerEnv->globalEnv,
                                           workerEnv->pool,
                                           "uri",  tmp );
    } else {
        /* Default host */
    jkb=workerEnv->globalEnv->createBean2( workerEnv->globalEnv,
                                           workerEnv->pool,
                                           "uri", "" );
    }

    newUri = jkb->object;
    newUri->workerEnv=workerEnv;

    return (void *) newUri;
}



/** Standard apache callback, merge jk options specified in 
    <Host> context. Used to set per virtual host configs
 */
static void *jk2_merge_config(ap_pool *p, 
                              void *basev, 
                              void *overridesv)
{
    jk_uriEnv_t *base = (jk_uriEnv_t *) basev;
    jk_uriEnv_t *overrides = (jk_uriEnv_t *)overridesv;
    
    /* The 'mountcopy' option should be implemented in common.
     */
    return overrides;
}

/** Standard apache callback, initialize jk. This is called after all
    the settings took place.
 */
static int jk2_init(server_rec *s, ap_pool *pconf)
{
    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *) ap_get_module_config(s->module_config, &jk2_module);
    
    jk_env_t *env=workerEnv->globalEnv;

    ap_pool *gPool=NULL;
    void *data=NULL;
    int rc=JK_OK;

    env->l->jkLog(env, env->l, JK_LOG_INFO, "mod_jk child init\n" );
    
    if(s->is_virtual) 
        return OK;
    
    if(!workerEnv->was_initialized) {
        workerEnv->was_initialized = JK_TRUE;        

        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "mod_jk.post_config() init worker env\n");

        workerEnv->parentInit(env, workerEnv );

        workerEnv->init(env, workerEnv );
        
        workerEnv->server_name   = (char *)ap_get_server_version();


#if MODULE_MAGIC_NUMBER >= 19980527
        /* Tell apache we're here */
        ap_add_version_component(JK_EXPOSED_VERSION);
#endif

    }
    return OK;
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

    /* If this is a proxy request, we'll notify an error */
    if(r->proxyreq) {
    return HTTP_INTERNAL_SERVER_ERROR; /* We don't proxy requests. */
    }

    /* changed from r->request_config to r->per_dir_config. This should give us the one that was set in 
       either the translate phase (if it was a config added through workers.properties) 
       or in the create_dir config. */
    uriEnv=ap_get_module_config( r->request_config, &jk2_module );  /* get one for the dir */
    if ( uriEnv == NULL ) {
    uriEnv=ap_get_module_config( r->per_dir_config, &jk2_module );  /* get one specific to this request if there isn't a dir one. */
    }
    
    /* not for me, try next handler */
    if(uriEnv==NULL || strcmp(r->handler,JK_HANDLER)!= 0 )
        return DECLINED;
    
    /* Get an env instance */
    env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );

    /* Set up r->read_chunked flags for chunked encoding, if present */
    if(rc = ap_setup_client_block(r, REQUEST_CHUNKED_DECHUNK)) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "mod_jk.handler() Can't setup client block %d\n", rc);
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
        return rc;
    }

    if( uriEnv == NULL ) {
        /* SetHandler case - per_dir config should have the worker*/
        worker =  workerEnv->defaultWorker;
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "mod_jk.handler() Default worker for %s %s\n",
                      r->uri, worker->mbean->name); 
    } else {
        worker=uriEnv->worker;
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "mod_jk.handler() per dir worker for %#lx %#lx\n",
                      worker, uriEnv );
        
        if( worker==NULL && uriEnv->workerName != NULL ) {
            worker=env->getByName( env,uriEnv->workerName);
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                          "mod_jk.handler() finding worker for %s %#lx %#lx\n",
                          uriEnv->workerName, worker, uriEnv );
            uriEnv->worker=worker;

        }
    }

    if(worker==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "mod_jk.handle() No worker for %s\n", r->uri); 
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
   return HTTP_INTERNAL_SERVER_ERROR; /* No worker defined for this uri */
    }

    {
        jk_ws_service_t sOnStack;
        jk_ws_service_t *s=&sOnStack;
        jk_pool_t *rPool=NULL;
        int rc1;

        /* Get a pool for the request XXX move it in workerEnv to
           be shared with other server adapters */
        rPool= worker->rPoolCache->get( env, worker->rPoolCache );

        if( rPool == NULL ) {
            rPool=worker->mbean->pool->create( env, worker->mbean->pool, HUGE_POOL_SIZE );
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "mod_jk.handler(): new rpool\n");
        }

        jk2_service_apache13_init(env, s);
        s->pool = rPool;
        s->init( env, s, worker, r );
        
	    /* reset the reco_status, will be set to INITED in LB mode */
	    s->reco_status = RECO_NONE;
	    
        s->is_recoverable_error = JK_FALSE;
        s->uriEnv = uriEnv;

        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "modjk.handler() Calling %s %#lx\n", worker->mbean->name, uriEnv); 

        rc = worker->service(env, worker, s);
        
        s->afterRequest(env, s);

        rPool->reset(env, rPool);
        
        rc1=worker->rPoolCache->put( env, worker->rPoolCache, rPool );
        if( rc1 == JK_OK ) {
            rPool=NULL;
        } else {
            rPool->close(env, rPool);
        }
    }

    if(rc==JK_OK) {
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
        return OK;    /* NOT r->status, even if it has changed. */
    }

    env->l->jkLog(env, env->l, JK_LOG_ERROR,
             "mod_jk.handler() Error connecting to tomcat %d %s\n", rc, worker==NULL?"":worker->channelName==NULL?"":worker->channelName);
    workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
    return HTTP_INTERNAL_SERVER_ERROR;  /* Is your tomcat server down ? */
}

/** Use the internal mod_jk mappings to find if this is a request for
 *    tomcat and what worker to use. 
 */
static int jk2_translate(request_rec *r)
{
    jk_uriEnv_t *uriEnv;
    jk_env_t *env;
            
    jk_uriMap_t *uriMap;
    char *name=NULL;
    int n;
    if(r->proxyreq) {
        return DECLINED;
    }
    
    uriEnv=ap_get_module_config( r->per_dir_config, &jk2_module );
    if( uriEnv != NULL  &&  uriEnv->workerName!=NULL) {
        /* jk2_handler tries to get the request_config and then falls back to the per_dir one. 
    so no point setting the request_config  */
   r->handler=JK_HANDLER;
   return OK;
    }

    
    uriMap= workerEnv->uriMap;
    /* Get an env instance */
    env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );
    n = uriMap->vhosts->size(env, uriMap->vhosts);

    /* Check JkMount directives, if any */
/*     if( workerEnv->uriMap->size == 0 ) */
/*         return DECLINED; */
    
    /* get_env() */
    env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );
    uriEnv = workerEnv->uriMap->mapUri(env, workerEnv->uriMap,
                                       ap_get_server_name(r),
                                       ap_get_server_port(r),
                                       r->uri);
    
    if(uriEnv==NULL || uriEnv->workerName==NULL) {
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
        return DECLINED;
    }

    ap_set_module_config( r->request_config, &jk2_module, uriEnv );
    r->handler=JK_HANDLER;

    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "mod_jk.translate(): uriMap %s %s\n",
                  r->uri, uriEnv->workerName);

    workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
    return OK;
}

static const handler_rec jk2_handlers[] =
{
    { JK_MAGIC_TYPE, jk2_handler },
    { JK_HANDLER, jk2_handler },
    NULL
};

module MODULE_VAR_EXPORT jk2_module = {
    STANDARD_MODULE_STUFF,
    jk2_init,             /* module initializer */
    jk2_create_dir_config,                       /* per-directory config creator */
    jk2_merge_dir_config,                       /* dir config merger */
    jk2_create_config,           /* server config creator */
/*    jk2_merge_config,            / * server config merger */
    NULL,
    jk2_cmds,                    /* command table */
    jk2_handlers,                /* [7] list of handlers */
    jk2_translate,               /* [2] filename-to-URI translation */
    NULL,                       /* [5] check/validate user_id */
    NULL,                       /* [6] check user_id is valid *here* */
    NULL,                       /* [4] check access by host address */
    NULL,                       /* XXX [7] MIME type checker/setter */
    NULL,                       /* [8] fixups */
    NULL,                       /* [10] logger */
    NULL,                       /* [3] header parser */
    NULL,                    /* apache child process initializer */
    NULL,  /* exit_handler, */               /* apache child process exit/cleanup */
    NULL                        /* [1] post read_request handling */
#ifdef X_EAPI
    /*
     * Extended module APIs, needed when using SSL.
     * STDC say that we do not have to have them as NULL but
     * why take a chance
     */
    ,NULL,                      /* add_module */
    NULL,                       /* remove_module */
    NULL,                       /* rewrite_command */
    NULL,                       /* new_connection */
    NULL                       /* close_connection */
#endif /* EAPI */

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
