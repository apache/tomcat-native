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
 * Description: Apache 1.3 plugin for Jakarta/Tomcat                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 *                 Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
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

#define JK_HANDLER          ("jakarta-servlet2")
#define JK_MAGIC_TYPE       ("application/x-jakarta-servlet2")

module MODULE_VAR_EXPORT jk2_module;

int jk2_service_apache13_init(jk_env_t *env, jk_ws_service_t *s);


/* In apache1.3 this is reset when the module is reloaded ( after
 * config. No good way to discover if it's the first time or not.
 */
static jk_workerEnv_t *workerEnv;

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
    
    rc=workerEnv->config->setPropertyString( env, workerEnv->config, name, value );
    if( rc!=JK_TRUE ) {
        fprintf( stderr, "mod_jk2: Unrecognized option %s %s\n", name, value);
    }

    return NULL;
}

/* Create the initial set of objects. You need to cut&paste this and
   adapt to your server.
 */
static int jk2_create_workerEnv(ap_pool *p, const server_rec *s)
{
    jk_env_t *env;
    jk_logger_t *l;
    jk_pool_t *globalPool;
    
    /** First create a pool. We use the default ( jk ) pool impl,
     *  other choices are apr or native.
     */
    jk2_pool_create( NULL, &globalPool, NULL, 2048 );

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
    l = env->createInstance( env, env->globalPool, "logger.file", "logger");
    env->l=l;
    
    /* Create the workerEnv
     */
    workerEnv= env->createInstance( env, env->globalPool,"workerEnv", "workerEnv");

    if( workerEnv==NULL || l== NULL  ) {
        fprintf( stderr, "Error initializing jk, NULL objects \n");
        return JK_FALSE;
    }

    /* Local initialization.
     */
    workerEnv->_private = s;
    return JK_TRUE;
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
        NULL
    };

/** Create default jk_config.
    This is the first thing called by apache ( or should be )
 */
static void *jk2_create_config(ap_pool *p, server_rec *s)
{
    jk_uriEnv_t *newUri;

    if(  workerEnv==NULL ) {
        jk2_create_workerEnv(p, s );
    }
    if( s->is_virtual == 1 ) {
        /* Virtual host */
        fprintf( stderr, "Create config for virtual host\n");
    } else {
        /* Default host */
        fprintf( stderr, "Create config for main host\n");
    }

    newUri = workerEnv->globalEnv->createInstance( workerEnv->globalEnv,
                                                   workerEnv->pool,
                                                   "uri", NULL );
    newUri->workerEnv=workerEnv;
    return newUri;
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
    
    fprintf(stderr,  "Merging workerEnv \n" );
    
    /* The 'mountcopy' option should be implemented in common.
     */
    return overrides;
}

/** Standard apache callback, initialize jk. This is called after all
    the settings took place.
 */
static void jk2_init(server_rec *s, ap_pool *pconf)
{
    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk2_module);
    
    jk_env_t *env=workerEnv->globalEnv;

    ap_pool *gPool=NULL;
    void *data=NULL;
    int rc=JK_TRUE;

    env->l->jkLog(env, env->l, JK_LOG_INFO, "mod_jk child init\n" );
    
    if(s->is_virtual) 
        return OK;
    
    if(!workerEnv->was_initialized) {
        workerEnv->was_initialized = JK_TRUE;        

        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "mod_jk.post_config() init worker env\n");
        workerEnv->init(env, workerEnv );
        
        workerEnv->server_name   = (char *)ap_get_server_version();
        /* ap_add_version_component(pconf, JK_EXPOSED_VERSION); */
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
    jk_env_t *env;

    uriEnv=ap_get_module_config( r->request_config, &jk2_module );

    /* If this is a proxy request, we'll notify an error */
    if(r->proxyreq) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    /* not for me, try next handler */
    if(uriEnv==NULL || strcmp(r->handler,JK_HANDLER)!= 0 )
        return DECLINED;
    
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
                      r->uri, worker->mbean->name); 
    } else {
        worker=uriEnv->worker;
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "mod_jk.handler() per dir worker for %p %p\n",
                      worker, uriEnv );
        
        if( worker==NULL && uriEnv->workerName != NULL ) {
            worker=env->getByName( env,uriEnv->workerName);
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                          "mod_jk.handler() finding worker for %s %p %p\n",
                          uriEnv->workerName, worker, uriEnv );
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

        jk2_service_apache13_init(env, s);
        s->pool = rPool;
        
        s->is_recoverable_error = JK_FALSE;
        s->init( env, s, worker, r );
        
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "modjk.handler() Calling %s\n", worker->mbean->name); 

        rc = worker->service(env, worker, s);
        
        s->afterRequest(env, s);

        rPool->reset(env, rPool);
        
        rc1=worker->rPoolCache->put( env, worker->rPoolCache, rPool );
        if( rc1 == JK_TRUE ) {
            rPool=NULL;
        } else {
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
    jk_uriEnv_t *uriEnv;
    jk_env_t *env;
            
    if(r->proxyreq) {
        return DECLINED;
    }
    
    /* Check JkMount directives, if any */
    if( workerEnv->uriMap->size == 0 )
        return DECLINED;
    
    /* XXX get_env() */
    env=workerEnv->globalEnv;
        
    uriEnv = workerEnv->uriMap->mapUri(env, workerEnv->uriMap,NULL,r->uri );
    
    if(uriEnv==NULL || uriEnv->workerName==NULL) {
        return DECLINED;
    }

    ap_set_module_config( r->request_config, &jk2_module, uriEnv );
    r->handler=JK_HANDLER;

    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "mod_jk.translate(): uriMap %s %s\n",
                  r->uri, uriEnv->workerName);
    
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
    NULL,                       /* per-directory config creator */
    NULL,                       /* dir config merger */
    jk2_create_config,           /* server config creator */
    jk2_merge_config,            /* server config merger */
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

