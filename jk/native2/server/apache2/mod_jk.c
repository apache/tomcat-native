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

#define JK_HANDLER          ("jakarta-servlet")
#define JK_MAGIC_TYPE       ("application/x-jakarta-servlet")

module AP_MODULE_DECLARE_DATA jk_module;

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
 * In order to define a webapp you must add "JkWebapp" directive
 * in a Location. 
 *
 * Example:
 *   <VirtualHost foo.com>
 *      <Location /examples>
 *         JkWebapp worker ajp13
 *      </Location>
 *   </VirtualHost>
 *
 * This is the best way to define a webapplication in apache. It is
 * scalable ( using apache native optimizations, you can have hundreds
 * of hosts and thousands of webapplications ), 'natural' to any
 * apache user.
 */
static const char *jk_setWebapp(cmd_parms *cmd, void *per_dir, 
                                const char *name, const char *val)
{
    jk_uriEnv_t *uriEnv=(jk_uriEnv_t *)per_dir;

    if( uriEnv->webapp == NULL ) {
        /* Do we know the url ? */
        uriEnv->webapp=workerEnv->createWebapp( workerEnv->globalEnv, workerEnv,
                                                NULL, cmd->path, NULL );

        fprintf(stderr, "New webapp %p %p\n",uriEnv, uriEnv->webapp);
    } else {
        fprintf(stderr, "Existing webapp %p\n",uriEnv->webapp);
    }

    if( strcmp( name, "worker") == 0 ) {
        uriEnv->webapp->workerName=ap_pstrdup(cmd->pool, val);
    } else {
        /* Generic properties */
        uriEnv->webapp->properties->add( workerEnv->globalEnv,
                                         uriEnv->webapp->properties,
                                         ap_pstrdup(cmd->pool, name),
                                         ap_pstrdup(cmd->pool, val));
    }
    
    fprintf(stderr, "XXX Set worker %p %s %s dir=%s args=%s\n",
            uriEnv, uriEnv->webapp->workerName, cmd->path,
            cmd->directive->directive,
            cmd->directive->args);

    return NULL;
}

/**
 * Associate a servlet to a <Location>. 
 *
 * Example:
 *   <VirtualHost foo.com>
 *      <Location /examples/servlet>
 *         JkServlet name servlet
 *      </Location>
 *   </VirtualHost>
 */
static const char *jk_setServlet(cmd_parms *cmd, void *per_dir, 
                                const char *name, const char *val)
{
    jk_uriEnv_t *uriEnv=(jk_uriEnv_t *)per_dir;

    if( strcmp( name, "name") == 0 ) {
        uriEnv->servlet=ap_pstrdup(cmd->pool, val);
    } else {
        /* Generic properties */
        uriEnv->properties->add( workerEnv->globalEnv, uriEnv->properties,
                                 ap_pstrdup(cmd->pool, name),
                                 ap_pstrdup(cmd->pool, val));
    }
    
    fprintf(stderr, "XXX SetServlet %p %p %s %s dir=%s args=%s\n",
            uriEnv, uriEnv->webapp, name, val,
            cmd->directive->directive,
            cmd->directive->args);

    return NULL;
}

/** 
 * Set jk options.
 *
 * "JkFoo value" is equivalent with a "foo=value" setting in
 * workers.properties. ( XXX rename workers.properties to jk.properties)
 *
 * We are using a small trick to avoid duplicating the code ( the 'dummy'
 * parm ). The values are validated and initalized in jk_init.
 */
static const char *jk_set1(cmd_parms *cmd, void *per_dir,
                           const char *value)
{
    server_rec *s = cmd->server;
    struct stat statbuf;
    char *oldv;
    int rc;
    jk_env_t *env;

    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk_module);
    jk_workerEnv_t *workerEnv = serverEnv->workerEnv;
    
    jk_map_t *m=workerEnv->init_data;
    
    env=workerEnv->globalEnv;

    value = jk_map_replaceProperties(env, m, m->pool, value);

    if( cmd->info != NULL ) {
        /* Multi-option config. */
        m->add(env, m, (char *)cmd->info, (char *)value );

    } else {
        /* ??? Maybe this is a single-option */
        m->add(env, m, value, "On" );
    }

    return NULL;
}


/*
 * JkSet name value
 * JkEnv envvar envvalue
 * JkMount /context worker
 *
 * Special options. First is equivalent with "name=value" in workers.properties.
 * ( you should use the specialized directive, as it provides more info ).
 * The other 2 are defining special maps.
 *
 * We use a small trick ( dummy param ) to avoid duplicated code and keep it
 * simple.
 */
static const char *jk_set2(cmd_parms *cmd,void *per_dir,
                           const char *name, const char *value)
{
    server_rec *s = cmd->server;
    struct stat statbuf;
    char *oldv;
    int rc;
    jk_env_t *env;
    char *type=(char *)cmd->info;

    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk_module);
    jk_workerEnv_t *workerEnv = serverEnv->workerEnv;
    
    jk_map_t *m=workerEnv->init_data;
    
    env=workerEnv->globalEnv;
    
    value = jk_map_replaceProperties(env, m, m->pool, value);

    if(value==NULL)
        return NULL;

    if( type==NULL || type[0]=='\0') {
        /* Generic JkSet foo bar */
        m->add(env, m, ap_pstrdup( cmd->pool, name),
               ap_pstrdup( cmd->pool, value));
        fprintf( stderr, "set2.init_data: %s %s\n", name, value );
    } else if( strcmp(type, "env")==0) {
        workerEnv->envvars_in_use = JK_TRUE;
        workerEnv->envvars->put(env, workerEnv->envvars,
                                ap_pstrdup(cmd->pool,name),
                                ap_pstrdup(cmd->pool,value),
                                NULL);
        fprintf( stderr, "set2.env %s %s\n", name, value );
    } else if( strcmp(type, "mount")==0) {
        if (name[0] !='/') return "Context must start with /";
        workerEnv->init_data->put(  env, workerEnv->init_data,
                                    ap_pstrdup(cmd->pool,name),
                                    ap_pstrdup(cmd->pool,value),
                                    NULL );
        fprintf( stderr, "set2.mount %s %s\n", name, value );
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
 */
static const char *jk_setWorker(cmd_parms *cmd,void *per_dir,
                                const char *wname, const char *wparam, const char *value)
{
    server_rec *s = cmd->server;
    struct stat statbuf;
    char *oldv;
    int rc;
    jk_env_t *env;
    char *type=(char *)cmd->info;

    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk_module);
    jk_workerEnv_t *workerEnv = serverEnv->workerEnv;
    
    jk_map_t *m=workerEnv->init_data;
    
    env=workerEnv->globalEnv;
    
    value = jk_map_replaceProperties(env, m, m->pool, value);

    if(value==NULL)
        return NULL;

/*     workerEnv->init_data->add(  env, workerEnv->init_data, */
/*                                 ap_pstrdup(cmd->pool,name), */
/*                                 ap_pstrdup(cmd->pool,value)); */
    return NULL;
}

/* Command table.
 */
static const command_rec jk_cmds[] =
    {
    AP_INIT_TAKE1(
        "JkWorkersFile", jk_set1, "workerFile", RSRC_CONF,
        "the name of a worker file for the Jakarta servlet containers"),
    AP_INIT_TAKE1(
        "JkProperties", jk_set1,  "workerFile", RSRC_CONF,
        "Properties file containing additional settings ( replaces JkWoprkerFile )"),
    AP_INIT_TAKE2(
        "JkSet", jk_set2, NULL, RSRC_CONF,
        "Set a jk property, same syntax and rules as in JkWorkersFile"),
    AP_INIT_TAKE2(
        "JkMount", jk_set2, "mount", RSRC_CONF,
        "A mount point from a context to a Tomcat worker"),
    AP_INIT_TAKE2(
        "JkWorker", jk_setWorker, NULL, RSRC_CONF,
        "Defines workers and worker properties "),
    AP_INIT_TAKE2(
        "JkWebapp", jk_setWebapp, NULL, ACCESS_CONF,
        "Defines a webapp in a Location directive and it's properties"),
    AP_INIT_TAKE2(
        "JkServlet", jk_setServlet, NULL, ACCESS_CONF,
        "Defines a servlet in a Location directive"),
    AP_INIT_TAKE1(
        "JkMountCopy", jk_set1, "root_apps_are_global", RSRC_CONF,
        "Should the base server mounts be copied from main server to the virtual server"),
    AP_INIT_TAKE1(
        "JkLogFile", jk_set1, "logFile", RSRC_CONF,
        "Full path to the Jakarta Tomcat module log file"),
    AP_INIT_TAKE1(
        "JkLogLevel", jk_set1, "logLevel", RSRC_CONF,
        "The Jakarta Tomcat module log level, can be debug, "
        "info, error or emerg"),
    AP_INIT_TAKE1(
        "JkLogStampFormat", jk_set1, "logStampFormat", RSRC_CONF,
        "The Jakarta Tomcat module log format, follow strftime synthax"),
    AP_INIT_TAKE1(
        "JkHTTPSIndicator", jk_set1, "HttpsIndicator", RSRC_CONF,
        "Name of the Apache environment that contains SSL indication"),
    AP_INIT_TAKE1(
        "JkCERTSIndicator", jk_set1, "CertsIndicator", RSRC_CONF,
        "Name of the Apache environment that contains SSL client certificates"),
    AP_INIT_TAKE1(
         "JkCIPHERIndicator", jk_set1, "CipherIndicator", RSRC_CONF,
        "Name of the Apache environment that contains SSL client cipher"),
    AP_INIT_TAKE1(
        "JkSESSIONIndicator", jk_set1, "SessionIndicator", RSRC_CONF,
        "Name of the Apache environment that contains SSL session"),
    AP_INIT_TAKE1(
        "JkKEYSIZEIndicator", jk_set1, "KeySizeIndicator", RSRC_CONF,
        "Name of the Apache environment that contains SSL key size in use"),
    AP_INIT_TAKE1(
        "JkExtractSSL", jk_set1, "extractSsl", RSRC_CONF,
        "Turns on SSL processing and information gathering by mod_jk"),
    AP_INIT_TAKE1(
        "JkForwardSSLKeySize", jk_set1, "forwardSslKeySize", RSRC_CONF,
        "Forward SSL Key Size, to follow 2.3 specs but may broke old TC 3.2,"
        "off is backward compatible"),
    AP_INIT_TAKE1(
        "ForwardURICompat",  jk_set1, "forwardUriCompat", RSRC_CONF,
        "Forward URI normally, less spec compliant but mod_rewrite compatible (old TC)"),
    AP_INIT_TAKE1(
        "JkForwardURICompatUnparsed", jk_set1, "forwardUriCompatUnparsed", RSRC_CONF,
        "Forward URI as unparsed, spec compliant but broke mod_rewrite (old TC)"),
    AP_INIT_TAKE1(
        "JkForwardURIEscaped", jk_set1, "forwardUriEscaped", RSRC_CONF,
        "Forward URI escaped and Tomcat (3.3 rc2) stuff will do the decoding part"),
    AP_INIT_TAKE2(
        "JkEnvVar", jk_set2, "env", RSRC_CONF,
        "Adds a name of environment variable that should be sent from web server "
        "to servlet-engine"),
    NULL
};

static void *create_jk_dir_config(apr_pool_t *p, char *path)
{
    jk_uriEnv_t *new =
        (jk_uriEnv_t *)apr_pcalloc(p, sizeof(jk_uriEnv_t));

    fprintf(stderr, "XXX Create dir config %s %p\n", path, new);
    new->uri = path;
    new->workerEnv=workerEnv;
    
    return new;
}


static void *merge_jk_dir_config(apr_pool_t *p, void *basev, void *addv)
{
    jk_uriEnv_t *base =(jk_uriEnv_t *)basev;
    jk_uriEnv_t *add = (jk_uriEnv_t *)addv;
    jk_uriEnv_t *new = (jk_uriEnv_t *)apr_pcalloc(p,sizeof(jk_uriEnv_t));
    
    
    /* XXX */
    fprintf(stderr, "XXX Merged dir config %p %p %s %s %p %p\n",
            base, new,
            base->uri, add->uri,
            base->webapp, add->webapp);

    if( add->webapp == NULL ) {
        add->webapp=base->webapp;
    }
    
    return add;
}

static void create_workerEnv(apr_pool_t *p, server_rec *s) {
    jk_env_t *env;
    jk_logger_t *l;
    jk_pool_t *globalPool;
    
    /** First create a pool
     */
#ifdef NO_APACHE_POOL
    jk_pool_create( NULL, &globalPool, NULL, 2048 );
#else
    jk_pool_apr_create( NULL, &globalPool, NULL, p );
#endif

    /** Create the global environment. This will register the default
        factories
    */
    env=jk_env_getEnv( NULL, globalPool );

    /* Optional. Register more factories ( or replace existing ones ) */

    /* Init the environment. */
    
    /* Create the logger */
#ifdef NO_APACHE_LOGGER
    l = env->getInstance( env, env->globalPool, "logger", "file");
#else
    jk_logger_apache2_factory( env, env->globalPool, (void *)&l, "logger", "file");
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
    fprintf( stderr, "Create worker env %p\n", workerEnv );
}

/** Create default jk_config. XXX This is mostly server-independent,
    all servers are using something similar - should go to common.

    This is the first thing called ( or should be )
 */
static void *create_jk_config(apr_pool_t *p, server_rec *s)
{
    jk_uriEnv_t *newUri;

    if(  workerEnv==NULL ) {
        create_workerEnv(p, s );
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
static void *merge_jk_config(apr_pool_t *p, 
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
static void jk_child_init(apr_pool_t *pconf, 
                          server_rec *s)
{
    jk_uriEnv_t *serverEnv=(jk_uriEnv_t *)
        ap_get_module_config(s->module_config, &jk_module);
    jk_workerEnv_t *workerEnv = serverEnv->workerEnv;

    jk_env_t *env=workerEnv->globalEnv;

    env->l->jkLog(env, env->l, JK_LOG_INFO, "mod_jk child init\n" );
    
    /* init_jk( pconf, conf, s );
       do we need jk_child_init? For ajp14? */
}

/** Initialize jk, using worker.properties. 
    We also use apache commands ( JkWorker, etc), but this use is 
    deprecated, as we'll try to concentrate all config in
    workers.properties, urimap.properties, and ajp14 autoconf.
    
    Apache config will only be used for manual override, using 
    SetHandler and normal apache directives ( but minimal jk-specific
    stuff )
*/
static char * init_jk( jk_env_t *env, apr_pool_t *pconf,
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
static int jk_apache2_isValidating(apr_pool_t *gPool, apr_pool_t **mainPool) {
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

static int jk_post_config(apr_pool_t *pconf, 
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
    
    rc=jk_apache2_isValidating( plog, &gPool );

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
        init_jk( env, pconf, workerEnv, s );
    }
    return OK;
}

/* ========================================================================= */
/* The JK module handlers                                                    */
/* ========================================================================= */

/** Main service method, called to forward a request to tomcat
 */
static int jk_handler(request_rec *r)
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

    uriEnv=ap_get_module_config( r->request_config, &jk_module );

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
        if( worker==NULL && uriEnv->webapp->workerName != NULL ) {
            worker=workerEnv->getWorkerForName( env, workerEnv,
                                                uriEnv->webapp->workerName);
            uriEnv->webapp->worker=worker;
        }
    }

    if(worker==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "No worker for %s\n", r->uri); 
        return 500;
    }

    worker->get_endpoint(env, worker, &end);

    {
        jk_ws_service_t sOnStack;
        jk_ws_service_t *s=&sOnStack;
        int is_recoverable_error = JK_FALSE;

        jk_service_apache2_factory( env, end->cPool, (void *)&s,
                                    "service", "apache2");
        
        s->init( env, s, end, r );
        
        rc = end->service(env, end, s,  &is_recoverable_error);

        s->afterRequest(env, s);
    }

    end->done(env, end); 

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
static int jk_translate(request_rec *r)
{
    jk_workerEnv_t *workerEnv;
    jk_uriEnv_t *uriEnv;
    jk_env_t *env;
            
    if(r->proxyreq) {
        return DECLINED;
    }
    
    uriEnv=ap_get_module_config( r->per_dir_config, &jk_module );
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
        
        ap_set_module_config( r->request_config, &jk_module, uriEnv );        
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

    ap_set_module_config( r->request_config, &jk_module, uriEnv );
    r->handler=JK_HANDLER;

    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "mod_jk.translate(): uriMap %s %s\n",
                  r->uri, uriEnv->webapp->workerName);

    return OK;
}

/* XXX Can we use type checker step to set our stuff ? */

/* bypass the directory_walk and file_walk for non-file requests */
static int jk_map_to_storage(request_rec *r)
{
    jk_uriEnv_t *uriEnv=ap_get_module_config( r->request_config, &jk_module );
    
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

static void jk_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(jk_handler, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_post_config(jk_post_config,NULL,NULL,APR_HOOK_MIDDLE);
    ap_hook_child_init(jk_child_init,NULL,NULL,APR_HOOK_MIDDLE);
    ap_hook_translate_name(jk_translate,NULL,NULL,APR_HOOK_FIRST);
    ap_hook_map_to_storage(jk_map_to_storage, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA jk_module =
{
    STANDARD20_MODULE_STUFF,
    create_jk_dir_config,/*  dir config creater */
    merge_jk_dir_config, /* dir merger --- default is to override */
    create_jk_config,    /* server config */
    merge_jk_config,     /* merge server config */
    jk_cmds,             /* command ap_table_t */
    jk_register_hooks    /* register hooks */
};

