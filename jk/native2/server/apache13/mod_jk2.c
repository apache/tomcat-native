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
/* #include "ap_config.h" */
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#include "util_script.h"
/* #include "util_date.h" */
/* #include "http_conf_globals.h" */

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

/* #include "jk_apache2.h" */

#define JK_HANDLER          ("jakarta-servlet2")
#define JK_MAGIC_TYPE       ("application/x-jakarta-servlet2")

module MODULE_VAR_EXPORT jk2_module;


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

/**
 * In order to define a webapp you must add "Jk2Webapp" directive
 * in a Location. 
 *
 * Example:
 *   <VirtualHost foo.com>
 *      <Location /examples>
 *         Jk2Webapp worker ajp13
 *      </Location>
 *   </VirtualHost>
 *
 * This is the best way to define a webapplication in apache. It is
 * scalable ( using apache native optimizations, you can have hundreds
 * of hosts and thousands of webapplications ), 'natural' to any
 * apache user.
 */
static const char *jk2_setWebapp(cmd_parms *cmd, void *per_dir, 
                                const char *name, const char *val)
{
    jk_uriEnv_t *uriEnv=(jk_uriEnv_t *)per_dir;

    if( uriEnv->webapp == NULL ) {
        /* Do we know the url ? */
        uriEnv->webapp=workerEnv->createWebapp( workerEnv->globalEnv, workerEnv,
                                                NULL, cmd->path, NULL );

        /* fprintf(stderr, "New webapp %p %p\n",uriEnv, uriEnv->webapp); */
    } else {
        /* fprintf(stderr, "Existing webapp %p\n",uriEnv->webapp); */
    }

    if( strcmp( name, "worker") == 0 ) {
        /* XXX move to common in webapp->init */
        uriEnv->webapp->workerName=ap_pstrdup(cmd->pool, val);
    } else {
        /* Generic properties */
        uriEnv->webapp->properties->add( workerEnv->globalEnv,
                                         uriEnv->webapp->properties,
                                         ap_pstrdup(cmd->pool, name),
                                         ap_pstrdup(cmd->pool, val));
    }
    
    fprintf(stderr, "Jk2Webapp  %s %s \n",
            uriEnv->webapp->workerName, cmd->path);

    return NULL;
}

/**
 * Associate a servlet to a <Location>. 
 *
 * Example:
 *   <VirtualHost foo.com>
 *      <Location /examples/servlet>
 *         Jk2Servlet name servlet
 *      </Location>
 *   </VirtualHost>
 */
static const char *jk2_setServlet(cmd_parms *cmd, void *per_dir, 
                                 const char *name, const char *val)
{
    jk_uriEnv_t *uriEnv=(jk_uriEnv_t *)per_dir;

    if( strcmp( name, "name") == 0 ) {
        /* XXX Move to webapp->init() */
        uriEnv->servlet=ap_pstrdup(cmd->pool, val);
    } else {
        /* Generic properties */
        uriEnv->properties->add( workerEnv->globalEnv, uriEnv->properties,
                                 ap_pstrdup(cmd->pool, name),
                                 ap_pstrdup(cmd->pool, val));
    }
    
    fprintf(stderr, "JkServlet %p %p %s %s \n",
            uriEnv, uriEnv->webapp, name, val);

    return NULL;
}

/** 
 * Set jk options. Used to implement backward compatibility with jk1
 *
 * "JkFoo value" in jk1 is equivalent with a "foo=value" setting in
 * workers.properties and "JkSet foo value" in jk2
 *
 * We are using a small trick to avoid duplicating the code ( the 'dummy'
 * parm ). The values are validated and initalized in jk_init.
 */
static const char *jk2_set1(cmd_parms *cmd, void *per_dir,
                            const char *value)
{
    server_rec *s = cmd->server;
    struct stat statbuf;
    char *oldv;
    int rc;
    jk_env_t *env;

    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk2_module);
    jk_workerEnv_t *workerEnv = serverEnv->workerEnv;
    
    env=workerEnv->globalEnv;

    if( cmd->info != NULL ) {
        workerEnv->setProperty( env, workerEnv, (char *)cmd->info, (char *)value );
    } else {
        /* ??? Maybe this is a single-option */
        workerEnv->setProperty( env, workerEnv, value, "On" );
    }

    return NULL;
}


/*
 * JkSet name value
 *
 * Also used for backward compatiblity for: "JkEnv envvar envvalue" and
 * "JkMount /context worker" ( using cmd->info trick ).
 */
static const char *jk2_set2(cmd_parms *cmd,void *per_dir,
                           const char *name, const char *value)
{
    server_rec *s = cmd->server;
    struct stat statbuf;
    char *oldv;
    int rc;
    jk_env_t *env;
    char *type=(char *)cmd->info;

    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk2_module);
    jk_workerEnv_t *workerEnv = serverEnv->workerEnv;
    
    env=workerEnv->globalEnv;

    if( type==NULL || type[0]=='\0') {
        /* Generic Jk2Set foo bar */
        workerEnv->setProperty(env, workerEnv, name, value);
    } else if( strcmp(type, "env")==0) {
        workerEnv->envvars_in_use = JK_TRUE;
        workerEnv->envvars->put(env, workerEnv->envvars,
                                ap_pstrdup(cmd->pool,name),
                                ap_pstrdup(cmd->pool,value),
                                NULL);
        fprintf( stderr, "set2.env %s %s\n", name, value );
    } else if( strcmp(type, "mount")==0) {
        if (name[0] !='/') return "Context must start with /";
        workerEnv->setProperty(  env, workerEnv, name, value );
    } else {
        fprintf( stderr, "set2 error %s %s %s ", type, name, value );
    }

    return NULL;
}

/*
 * JkWorker workerName workerProperty value
 *
 * Equivalent with "worker.workerName.workerProperty=value" in
 * workers.properties.
 * Not used - do we want to add it ( just syntactic sugar ) ?
 */
static const char *jk2_setWorker(cmd_parms *cmd,void *per_dir,
                                const char *wname, const char *wparam, const char *value)
{
    char * name;
    name = ap_pstrcat(cmd->pool, "worker.", wname, ".", wparam, NULL);
    return (jk2_set2(cmd, per_dir, name, value));
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
        { "JkSet", jk2_set2, NULL, RSRC_CONF, TAKE2,
          "Set a jk property, same syntax and rules as in JkWorkersFile" },
        {"JkWebapp", jk2_setWebapp, NULL, ACCESS_CONF, TAKE2,
         "Defines a webapp in a Location directive and it's properties"},
        {"JkServlet", jk2_setServlet, NULL, ACCESS_CONF, TAKE2,
         "Defines a servlet in a Location directive"},
        NULL
    };

static void *jk2_create_dir_config(ap_pool *p, char *path)
{
    jk_uriEnv_t *new =
        (jk_uriEnv_t *)ap_pcalloc(p, sizeof(jk_uriEnv_t));

    fprintf(stderr, "XXX Create dir config %s %p\n", path, new);
    new->uri = path;
    new->workerEnv=workerEnv;
    
    return new;
}


static void *jk2_merge_dir_config(ap_pool *p, void *basev, void *addv)
{
    jk_uriEnv_t *base =(jk_uriEnv_t *)basev;
    jk_uriEnv_t *add = (jk_uriEnv_t *)addv; 
    jk_uriEnv_t *new = (jk_uriEnv_t *)ap_pcalloc(p,sizeof(jk_uriEnv_t));
    
    if( add->webapp == NULL ) {
        add->webapp=base->webapp;
    }
    
    return add;
}


static void jk2_create_workerEnv(ap_pool *p, const server_rec *s)
{
    jk_env_t *env;
    jk_logger_t *l;
    jk_pool_t *globalPool;
    
    /** First create a pool
     */
    jk2_pool_create( NULL, &globalPool, NULL, 2048 );

    /** Create the global environment. This will register the default
        factories
    */
    env=jk2_env_getEnv( NULL, globalPool );

    /* Optional. Register more factories ( or replace existing ones ) */

    /* Init the environment. */
    
    /* Create the logger */
    l = env->getInstance( env, env->globalPool, "logger", "file");
    
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
static void *jk2_create_config(ap_pool *p, server_rec *s)
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

    newUri=(jk_uriEnv_t *)ap_pcalloc(p, sizeof(jk_uriEnv_t));

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

/** Standard apache callback, initialize jk.
 */
static void jk2_child_init(ap_pool *pconf, 
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

static void jk_init(server_rec *s, ap_pool *p)
{
    /* XXX ??? */
    jk2_child_init( p, s );
}

/** Initialize jk, using worker.properties. 
    We also use apache commands ( JkWorker, etc), but this use is 
    deprecated, as we'll try to concentrate all config in
    workers.properties, urimap.properties, and ajp14 autoconf.
    
    Apache config will only be used for manual override, using 
    SetHandler and normal apache directives ( but minimal jk-specific
    stuff )
*/
static char * jk2_init(jk_env_t *env, ap_pool *pconf,
                       jk_workerEnv_t *workerEnv, server_rec *s )
{
    workerEnv->init(env, workerEnv );
    workerEnv->server_name   = (char *)ap_get_server_version();
/*     ap_add_version_component(pconf, JK_EXPOSED_VERSION); */
    return NULL;
}

static int jk2_post_config(ap_pool *pconf, 
                           ap_pool *plog, 
                           ap_pool *ptemp, 
                           server_rec *s)
{
    ap_pool *gPool=NULL;
    void *data=NULL;
    int rc;
    jk_env_t *env;
    

    if(s->is_virtual) 
        return OK;

    env=workerEnv->globalEnv;
    
    rc=JK_TRUE;

    if( rc == JK_TRUE ) {
        /* This is the first step */
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "mod_jk.post_config() first invocation\n");
        /*         ap_pool_userdata_set( "INITOK", "mod_jk_init", NULL, gPool ); */
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
        worker=uriEnv->webapp->worker;
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "mod_jk.handler() per dir worker for %p %p\n",
                      worker, uriEnv->webapp );
        
        if( worker==NULL && uriEnv->webapp->workerName != NULL ) {
            worker=workerEnv->getWorkerForName( env, workerEnv,
                                                uriEnv->webapp->workerName);
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                          "mod_jk.handler() finding worker for %p %p\n",
                          worker, uriEnv->webapp );
            uriEnv->webapp->worker=worker;
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
        jk2_service_apache13_factory( env, rPool, (void *)&s,
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
        
    /* This has been mapped to a location that has a 'webapp' property,
       i.e. belongs to a tomcat webapp.
       We'll use the webapp uriMap to find if it's a static page and
       to parse the request.
       XXX for now just forward to tomcat
    */
    if( uriEnv!= NULL && uriEnv->webapp!=NULL ) {
        jk_uriMap_t *uriMap=uriEnv->webapp->uriMap;

        if( uriMap!=NULL ) {
            /* Again, we have 2 choices. Either use our map, or again
               let apache. The second is probably faster, but requires
               using some APIs I'm not familiar with ( to modify apache's
               config on the fly ). After we learn the new APIs we can
               switch to the second method.
            */
            /* XXX Cut the context path ? */
            jk_uriEnv_t *target=uriMap->mapUri( env, uriMap, NULL, r->uri );
            if( target == NULL ) 
                return DECLINED;
            uriEnv=target;
        }

        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "PerDir mapping  %s=%s\n",
                      r->uri, uriEnv->webapp->workerName);
        
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
                  r->uri, uriEnv->webapp->workerName);

    return OK;
}


static const handler_rec jk2_handlers[] =
{
    { JK_MAGIC_TYPE, jk2_handler },
    { JK_HANDLER, jk2_handler },    
    { NULL }
};

module MODULE_VAR_EXPORT jk2_module = {
    STANDARD_MODULE_STUFF,
    jk_init,                    /* module initializer */
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
    NULL,                       /* apache child process initializer */
    NULL,  /* exit_handler, */               /* apache child process exit/cleanup */
    NULL                        /* [1] post read_request handling */
#ifdef EAPI
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

