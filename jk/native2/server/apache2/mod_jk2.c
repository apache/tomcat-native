/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
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
 * Description: Apache 2 plugin for Jakarta/Tomcat                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 *                 Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

/*
 * mod_jk: keeps all servlet/jakarta related ramblings together.
 */
#include "apu_compat.h"
#include "ap_config.h"
#include "apr_lib.h"
#include "apr_date.h"
#include "apr_strings.h"

#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"

#include "util_script.h"
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

#include "jk_apache2.h"

#define JK_HANDLER          ("jakarta-servlet2")
#define JK_MAGIC_TYPE       ("application/x-jakarta-servlet2")

module AP_MODULE_DECLARE_DATA jk2_module;

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
    
    jk_workerEnv_t *workerEnv = serverEnv->workerEnv;
    char *type=(char *)cmd->info;
    jk_env_t *env=workerEnv->globalEnv;
    int rc;
    
    if( type==NULL || type[0]=='\0') {
        /* Generic Jk2Set foo bar */
        workerEnv->setProperty( env, workerEnv, name, value );
    } else if( strcmp(type, "env")==0) {
        workerEnv->envvars_in_use = JK_TRUE;
        workerEnv->envvars->put(env, workerEnv->envvars,
                                ap_pstrdup(cmd->pool,name),
                                ap_pstrdup(cmd->pool,value),
                                NULL);
    } else if( strcmp(type, "mount")==0) {
        if (name[0] !='/') return "Context must start with /";
        workerEnv->setProperty( env, workerEnv, name, value );
    } else {
        fprintf( stderr, "set2 error %s %s %s ", type, name, value );
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

    uriEnv->setProperty( workerEnv->globalEnv, uriEnv, name, val );
    
    fprintf(stderr, "JkUriSet  %s %s dir=%s args=%s\n",
            uriEnv->workerName, cmd->path,
            cmd->directive->directive,
            cmd->directive->args);

    return NULL;
}

/* XXX Move to docs.
   Equivalence table:

   JkWorkersFile == workerFile ( XXX make it a multi-value, add dir, reload )
                               ( XXX Should it be 'jkPropertiesFile' - for additional props.)
   
   JkWorker == JkSet

   JkAutoMount - was not implemented in 1.2, will be added in 2.1 in a better form

   JkMount ==  ( the property name is the /uri, the value is the worker )
 
   JkMountCopy == root_apps_are_global ( XXX looking for a better name, mountCopy is very confusing )

   JkLogFile == logFile

   JkLogLevel == logLevel

   JkLogStampFormat == logStampFormat

   JkXXXIndicator == XxxIndicator

   JkExtractSSL == extractSSL

   JkOptions == Individual properties:
                  forwardSslKeySize
                  forwardUriCompat
                  forwardUriCompatUnparsed
                  forwardUriEscaped

   JkEnvVar == env.NAME=DEFAULT_VALUE
*/
/* Command table.
 */
static const command_rec jk2_cmds[] =
    {
        /* This is the 'main' directive for tunning jk2. It takes 2 parameters,
           and it behaves _identically_ as a setting in workers.properties.
        */
    AP_INIT_TAKE2(
        "JkSet", jk2_set2, NULL, RSRC_CONF,
        "Set a jk property, same syntax and rules as in JkWorkersFile"),
    AP_INIT_TAKE2(
        "JkUriSet", jk2_uriSet, NULL, ACCESS_CONF,
        "Defines a jk property associated with a Location"),
    NULL
    };

static void *jk2_create_dir_config(apr_pool_t *p, char *path)
{
    jk_uriEnv_t *new =
        workerEnv->uriMap->createUriEnv( workerEnv->globalEnv,
                                         workerEnv->uriMap, NULL, path );
    
    return new;
}


static void *jk2_merge_dir_config(apr_pool_t *p, void *basev, void *addv)
{
    jk_uriEnv_t *base =(jk_uriEnv_t *)basev;
    jk_uriEnv_t *add = (jk_uriEnv_t *)addv; 
    jk_uriEnv_t *new = (jk_uriEnv_t *)apr_pcalloc(p,sizeof(jk_uriEnv_t));
    
    
    /* XXX */
    fprintf(stderr, "XXX Merged dir config %p %p %s %s %p %p\n",
            base, new, base->uri, add->uri, base->webapp, add->webapp);

    if( add->webapp == NULL ) {
        add->webapp=base->webapp;
    }
    
    return add;
}

/** Basic initialization for jk2.
 */
static void jk2_create_workerEnv(apr_pool_t *p, server_rec *s) {
    jk_env_t *env;
    jk_logger_t *l;
    jk_pool_t *globalPool;

    
    /** First create a pool. Compile time option
     */
#ifdef NO_APACHE_POOL
    jk2_pool_create( NULL, &globalPool, NULL, 2048 );
#else
    jk2_pool_apr_create( NULL, &globalPool, NULL, p );
#endif

    /** Create the global environment. This will register the default
        factories
    */
    env=jk2_env_getEnv( NULL, globalPool );

    /* Optional. Register more factories ( or replace existing ones ) */

    /* Init the environment. */
    
    /* Create the logger */
#ifdef NO_APACHE_LOGGER
    l = env->getInstance( env, env->globalPool, "logger", "file");
#else
    jk2_logger_apache2_factory( env, env->globalPool, (void *)&l, "logger", "file");
    l->logger_private=s;
#endif
    
    env->l=l;
    
    /* Create the workerEnv */
    workerEnv= env->getInstance( env, env->globalPool,"workerEnv", "default");

    if( workerEnv==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "Error creating workerEnv\n");
        return;
    }

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

    if(  workerEnv==NULL ) {
        jk2_create_workerEnv(p, s );
    }
    if( s->is_virtual == 1 ) {
        /* Virtual host */
        
        
    } else {
        /* Default host */
        
    }

    newUri=(jk_uriEnv_t *)apr_pcalloc(p, sizeof(jk_uriEnv_t));

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
    
    fprintf(stderr,  "Merging workerEnv \n" );
    
    /* The 'mountcopy' option should be implemented in common.
     */
    return overrides;
}


/** Standard apache callback, initialize jk.
 */
static void jk2_child_init(apr_pool_t *pconf, 
                           server_rec *s)
{
    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk2_module);
    jk_workerEnv_t *workerEnv = serverEnv->workerEnv;

    jk_env_t *env=workerEnv->globalEnv;

    env->l->jkLog(env, env->l, JK_LOG_INFO, "mod_jk child init\n" );
    
    /* jk2_init( pconf, conf, s );
       do we need jk2_child_init? For ajp14? */
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
    workerEnv->init(env, workerEnv );
    workerEnv->server_name   = (char *)ap_get_server_version();
    ap_add_version_component(pconf, JK_EXPOSED_VERSION);
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
        tmpPool=apr_pool_get_parent( gPool );
        if( tmpPool == NULL ) {
            break;
        }
        gPool=tmpPool;
    }

    if( tmpPool == NULL ) {
        /* We can't detect the root pool */
        return JK_FALSE;
    }
    if(mainPool != NULL )
        *mainPool=gPool;
    
    /* We have a global pool ! */
    apr_pool_userdata_get( &data, "mod_jk_init", gPool );
    if( data==NULL ) {
        return JK_TRUE;
    } else {
        return JK_FALSE;
    }
}

static int jk2_post_config(apr_pool_t *pconf, 
                           apr_pool_t *plog, 
                           apr_pool_t *ptemp, 
                           server_rec *s)
{
    apr_pool_t *gPool=NULL;
    void *data=NULL;
    int rc;
    jk_env_t *env;
    
    if(s->is_virtual) 
        return OK;

    env=workerEnv->globalEnv;
    
    rc=jk2_apache2_isValidating( plog, &gPool );

    if( rc == JK_TRUE ) {
        /* This is the first step */
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "mod_jk.post_config() first invocation\n");
        apr_pool_userdata_set( "INITOK", "mod_jk_init", NULL, gPool );
        return OK;
    }
        
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "mod_jk.post_config() second invocation\n" ); 
    
    if(!workerEnv->was_initialized) {
        workerEnv->was_initialized = JK_TRUE;        
        jk2_init( env, pconf, workerEnv, s );
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
    const char       *worker_name;
    jk_logger_t      *l=NULL;
    int              rc;
    jk_worker_t *worker=NULL;
    jk_endpoint_t *end = NULL;
    jk_uriEnv_t *uriEnv;
    jk_uriEnv_t *dirEnv;
    jk_env_t *env;
    jk_workerEnv_t *workerEnv;

    uriEnv=ap_get_module_config( r->request_config, &jk2_module );

    /* not for me, try next handler */
    if(uriEnv==NULL || strcmp(r->handler,JK_HANDLER)!= 0 )
      return DECLINED;
    
    /* If this is a proxy request, we'll notify an error */
    if(r->proxyreq) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    workerEnv = uriEnv->workerEnv;

    /* XXX Get an env instance */
    env = workerEnv->globalEnv;

    /* Set up r->read_chunked flags for chunked encoding, if present */
    if(rc = ap_setup_client_block(r, REQUEST_CHUNKED_DECHUNK)) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "mod_jk.handler() Can't setup client block %d\n", rc);
        return rc;
    }

    if( uriEnv == NULL ) {
        /* SetHandler case - per_dir config should have the worker*/
        worker =  workerEnv->defaultWorker;
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "mod_jk.handler() Default worker for %s %s\n",
                      r->uri, worker->name); 
    } else {
        worker=uriEnv->worker;
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "mod_jk.handler() per dir worker for %p %p\n",
                      worker, uriEnv->webapp );
        
        if( worker==NULL && uriEnv->workerName != NULL ) {
            worker=workerEnv->getWorkerForName( env, workerEnv,
                                                uriEnv->workerName);
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                          "mod_jk.handler() finding worker for %p %p\n",
                          worker, uriEnv );
            uriEnv->worker=worker;
        }
    }

    if(worker==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "mod_jk.handle() No worker for %s\n", r->uri); 
        return 500;
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
            rPool=worker->pool->create( env, worker->pool, HUGE_POOL_SIZE );
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "mod_jk.handler(): new rpool\n");
        }

        /* XXX we should reuse the request itself !!! */
        jk2_service_apache2_factory( env, rPool, (void *)&s,
                                    "service", "apache2");

        s->pool = rPool;
        
        s->is_recoverable_error = JK_FALSE;
        s->init( env, s, worker, r );
        
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "modjk.handler() Calling %s\n", worker->name); 
        rc = worker->service(env, worker, s);

        s->afterRequest(env, s);

        rPool->reset(env, rPool);
        
        rc1=worker->rPoolCache->put( env, worker->rPoolCache, rPool );
        if( rc1 == JK_TRUE ) {
            rPool=NULL;
        }
        if( rPool!=NULL ) {
            rPool->close(env, rPool);
        }
    }

    if(rc==JK_TRUE) {
        return OK;    /* NOT r->status, even if it has changed. */
    }

    env->l->jkLog(env, env->l, JK_LOG_ERROR,
             "mod_jk.handler() Error connecting to tomcat %d\n", rc);
    return 500;
}

/** Use the internal mod_jk mappings to find if this is a request for
 *    tomcat and what worker to use. 
 */
static int jk2_translate(request_rec *r)
{
    jk_workerEnv_t *workerEnv;
    jk_uriEnv_t *uriEnv;
    jk_env_t *env;
            
    if(r->proxyreq) {
        return DECLINED;
    }
    
    uriEnv=ap_get_module_config( r->per_dir_config, &jk2_module );
    workerEnv=uriEnv->workerEnv;
    
    /* XXX get_env() */
    env=workerEnv->globalEnv;
        
    /* This has been mapped to a location by apache
     * In a previous ( experimental ) version we had a sub-map,
     * but that's too complex for now.
     */
    if( uriEnv!= NULL && uriEnv->workerName != NULL) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "PerDir mapping  %s=%s\n",
                      r->uri, uriEnv->workerName);
        
        ap_set_module_config( r->request_config, &jk2_module, uriEnv );        
        r->handler=JK_HANDLER;
        return OK;
    }

    /* One idea was to use "SetHandler jakarta-servlet". This doesn't
       allow the setting of the worker. Having a specific SetWorker directive
       at location level is more powerfull. If anyone can figure any reson
       to support SetHandler, we can add it back easily */

    /* Check JkMount directives, if any */
    if( workerEnv->uriMap->size == 0 )
        return DECLINED;
    
    /* XXX TODO: Split mapping, similar with tomcat. First step will
       be a quick test ( the context mapper ), with no allocations.
       If positive, we'll fill a ws_service_t and do the rewrite and
       the real mapping. 
    */
    uriEnv = workerEnv->uriMap->mapUri(env, workerEnv->uriMap,NULL,r->uri );
    
    if(uriEnv==NULL ) {
        return DECLINED;
    }

    ap_set_module_config( r->request_config, &jk2_module, uriEnv );
    r->handler=JK_HANDLER;

    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "mod_jk.translate(): uriMap %s %s\n",
                  r->uri, uriEnv->workerName);

    return OK;
}

/* XXX Can we use type checker step to set our stuff ? */

/* bypass the directory_walk and file_walk for non-file requests */
static int jk2_map_to_storage(request_rec *r)
{
    jk_uriEnv_t *uriEnv=ap_get_module_config( r->request_config, &jk2_module );
    
    if( uriEnv != NULL ) {
        r->filename = (char *)apr_filename_of_pathname(r->uri);
        if( uriEnv->debug > 0 ) {
            /*   env->l->jkLog(env, env->l, JK_LOG_INFO,  */
            /*     "mod_jk.map_to_storage(): map %s %s\n", */
            /*                  r->uri, r->filename); */
        }
        return OK;
    }
    return DECLINED;
}

static void jk2_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(jk2_handler, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_post_config(jk2_post_config,NULL,NULL,APR_HOOK_MIDDLE);
    ap_hook_child_init(jk2_child_init,NULL,NULL,APR_HOOK_MIDDLE);
    ap_hook_translate_name(jk2_translate,NULL,NULL,APR_HOOK_FIRST);
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

