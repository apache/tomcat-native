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
 * Description: Workers controller                                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_workerEnv.h" 
#include "jk_env.h"
#include "jk_worker.h"

#define DEFAULT_WORKER              ("ajp13")

int JK_METHOD jk_workerEnv_factory( jk_env_t *env, jk_pool_t *pool, void **result,
                                    const char *type, const char *name);

static void jk_workerEnv_close(jk_env_t *env, jk_workerEnv_t *_this);
static void jk_workerEnv_initHandlers(jk_env_t *env, jk_workerEnv_t *_this);
static int jk_workerEnv_init1(jk_env_t *env, jk_workerEnv_t *_this);

static int jk_workerEnv_init(jk_env_t *env, jk_workerEnv_t *workerEnv)
{
    int err;
    char *opt;
    int options;

    opt=jk_map_getString( env, workerEnv->init_data, "workerFile", NULL );
    if( opt != NULL ) {
        struct stat statbuf;
        
        /* we need an absolut path (ap_server_root_relative does the ap_pstrdup) */
        workerEnv->worker_file = opt;
        /* We should make it relative to JK_HOME or absolute path.
           ap_server_root_relative(cmd->pool,opt); */

        if (workerEnv->worker_file == NULL) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "JkWorkersFile file_name invalid %s", workerEnv->worker_file);
            return JK_FALSE;
        }
        
        if (stat(workerEnv->worker_file, &statbuf) == -1) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "Can't find the workers file specified");
            return JK_FALSE;
        }
        /** Read worker files
         */
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "Reading map %s %d\n",
                      workerEnv->worker_file,
                      workerEnv->init_data->size(env, workerEnv->init_data) );
        
        err=jk_map_readFileProperties(env, workerEnv->init_data,
                                          workerEnv->worker_file);
        if( err==JK_TRUE ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                          "mod_jk.initJk() Reading worker properties %s %d\n", workerEnv->worker_file,
                          workerEnv->init_data->size( env, workerEnv->init_data ) );
        } else {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "mod_jk.initJk() Error reading worker properties %s %d\n",
                          workerEnv->worker_file,
                          workerEnv->init_data->size( env, workerEnv->init_data ) );
        }
    }
        
    opt=jk_map_getString( env, workerEnv->init_data, "logLevel", "Error" );

    if(0 == strcasecmp(opt, JK_LOG_INFO_VERB)) {
        env->l->level=JK_LOG_INFO_LEVEL;
    }
    if(0 == strcasecmp(opt, JK_LOG_DEBUG_VERB)) {
        env->l->level=JK_LOG_DEBUG_LEVEL;
    }
    opt=jk_map_getString( env, workerEnv->init_data, "logFile", NULL );

    env->l->jkLog(env, env->l, JK_LOG_INFO, "mod_jk.init_jk()\n" ); 
    env->l->open( env, env->l, workerEnv->init_data );

    opt=jk_map_getString( env, workerEnv->init_data, "sslEnable", NULL );
    workerEnv->ssl_enable = JK_TRUE;
    opt=jk_map_getString( env, workerEnv->init_data, "httpsIndicator", NULL );
    workerEnv->https_indicator = opt;
    opt=jk_map_getString( env, workerEnv->init_data, "certsIndicator", NULL );
    workerEnv->certs_indicator = opt;
    opt=jk_map_getString( env, workerEnv->init_data, "cipherIndicator", NULL );
    workerEnv->cipher_indicator = opt;
    opt=jk_map_getString( env, workerEnv->init_data, "sessionIndicator", NULL );
    workerEnv->session_indicator = opt;
    opt=jk_map_getString( env, workerEnv->init_data, "keySizeIndicator", NULL );
    workerEnv->key_size_indicator = opt;

    /* Small change in how we treat options: we have a default,
       and assume that any option declared by user has the intention
       of overriding the default ( "-Option == no option, leave the
       default
    */
    if ( jk_map_getBool(env, workerEnv->init_data,
                        "ForwardKeySize", NULL)) {
        workerEnv->options |= JK_OPT_FWDKEYSIZE;
    } else if(jk_map_getBool(env, workerEnv->init_data,
                             "ForwardURICompat", NULL)) {
        workerEnv->options &= ~JK_OPT_FWDURIMASK;
        workerEnv->options |=JK_OPT_FWDURICOMPAT;
    } else if(jk_map_getBool(env, workerEnv->init_data,
                             "ForwardURICompatUnparsed", NULL)) {
        workerEnv->options &= ~JK_OPT_FWDURIMASK;
        workerEnv->options |=JK_OPT_FWDURICOMPATUNPARSED;
    } else if (jk_map_getBool(env, workerEnv->init_data,
                              "ForwardURIEscaped", NULL)) {
        workerEnv->options &= ~JK_OPT_FWDURIMASK;
        workerEnv->options |= JK_OPT_FWDURIESCAPED;
    }

    /* Init() - post-config initialization ( now all options are set ) */
    jk_workerEnv_init1( env, workerEnv );

    err=workerEnv->uriMap->init(env, workerEnv->uriMap,
                                workerEnv,
                                workerEnv->init_data );
    return JK_TRUE;
}

/**
 *  Init the workers, prepare the we.
 * 
 *  Replaces wc_open
 */
static int jk_workerEnv_init1(jk_env_t *env, jk_workerEnv_t *_this)
{
    jk_map_t *init_data=_this->init_data;
    char **worker_list  = NULL;
    int i;
    int err;
    char *tmp;

    /*     _this->init_data=init_data; */

    tmp = jk_map_getString(env, init_data, "worker.list",
                           DEFAULT_WORKER );
    worker_list=jk_map_split( env, init_data, init_data->pool,
                              tmp, &_this->num_of_workers );

    if(worker_list==NULL || _this->num_of_workers<= 0 ) {
        /* assert() - we pass default worker, we should get something back */
        return JK_FALSE;
    }

    for(i = 0 ; i < _this->num_of_workers ; i++) {
        jk_worker_t *w = NULL;
        jk_worker_t *oldw = NULL;
        const char *name=(const char*)worker_list[i];

        w=_this->createWorker(env, _this, name, init_data);
        if( w==NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "init failed to create worker %s\n", 
                          worker_list[i]);
            /* Ignore it, other workers may be ok.
               return JK_FALSE; */
         } else {
            if( _this->defaultWorker == NULL )
                _this->defaultWorker=w;
        }
    }

    jk_workerEnv_initHandlers( env, _this );
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "workerEnv.init() %d workers, default %s\n",
                  _this->num_of_workers, worker_list[0]); 
    return JK_TRUE;
}


static void jk_workerEnv_close(jk_env_t *env, jk_workerEnv_t *_this)
{
    int sz;
    int i;
    
    sz = _this->worker_map->size(env, _this->worker_map);

    for(i = 0 ; i < sz ; i++) {
        jk_worker_t *w = _this->worker_map->valueAt(env, _this->worker_map, i);
        if(w) {
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "destroy worker %s\n",
                          _this->worker_map->nameAt(env, _this->worker_map, i));
            w->destroy(env,w);
        }
    }
    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "workerEnv.close() done %d\n", sz); 
    _this->worker_map->clear(env, _this->worker_map);
}

static jk_worker_t *jk_workerEnv_getWorkerForName(jk_env_t *env,
                                                  jk_workerEnv_t *_this,
                                                  const char *name )
{
    jk_worker_t * rc;
    
    if(!name) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerEnv.getWorkerForName() NullPointerException\n");
        return NULL;
    }

    rc = _this->worker_map->get(env, _this->worker_map, name);

    /*     if( rc==NULL ) { */
    /*         l->jkLog(l, JK_LOG_INFO, */
    /*  "workerEnv.getWorkerForName(): no worker found for %s \n", name); */
    /*     }  */
    return rc;
}


static jk_webapp_t *jk_workerEnv_createWebapp(jk_env_t *env,
                                              jk_workerEnv_t *_this,
                                              const char *vhost,
                                              const char *name, 
                                              jk_map_t *init_data)
{
    jk_pool_t *webappPool;
    jk_webapp_t *webapp;

    webappPool=(jk_pool_t *)_this->pool->create( env, _this->pool,
                                                 HUGE_POOL_SIZE);

    webapp=(jk_webapp_t *)webappPool->calloc(env, webappPool,
                                             sizeof( jk_webapp_t ));

    webapp->pool=webappPool;

    webapp->context=_this->pool->pstrdup( env, _this->pool, name);
    webapp->virtual=_this->pool->pstrdup( env, _this->pool, vhost);

    if( name==NULL ) {
        webapp->ctxt_len=0;
    } else {
        webapp->ctxt_len = strlen(name);
    }
    

    /* XXX Find it if it's already allocated */

    /* Add vhost:name to the map */
    
    return webapp;
    
}

static void jk_workerEnv_checkSpace( jk_env_t *env, jk_pool_t *pool,
                                     void ***tableP, int *sizeP, int id )
{
    void **newTable;
    int i;
    int newSize=id+4;
    
    if( *sizeP > id ) return;
    /* resize the table */
    newTable=(void **)pool->calloc( env, pool, newSize * sizeof( void *));
    for( i=0; i<*sizeP; i++ ) {
        newTable[i]= (*tableP)[i];
    }
    *tableP=newTable;
    *sizeP=newSize;
}

static void jk_workerEnv_initHandlers(jk_env_t *env, jk_workerEnv_t *_this)
{
    /* Find the max message id */
    /* XXX accessing private data... env most provide some method to get this */
    jk_map_t *registry=env->_registry;
    int size=registry->size( env, registry );
    int i,j;
    
    for( i=0; i<size; i++ ) {
        jk_handler_t *handler;
        jk_map_t *localHandlers;
        int rc;

        char *name= registry->nameAt( env, registry, i );
        if( strstr( name, "handler" ) == name ) {
            char *type=name+ strlen( "handler" ) +1;
            localHandlers=(jk_map_t *)env->getInstance(env, _this->pool,
                                                       "handler", type );
            if( localHandlers==NULL ) continue;
            
            for( j=0; j< localHandlers->size( env, localHandlers ); j++ ) {
                handler=(jk_handler_t *)localHandlers->valueAt( env, localHandlers, j );
                jk_workerEnv_checkSpace( env, _this->pool,
                                         (void ***)&_this->handlerTable,
                                         &_this->lastMessageId,
                                         handler->messageId );
                _this->handlerTable[ handler->messageId ]=handler;
                /*_this->l->jkLog( _this->l, JK_LOG_INFO, "Registered %s %d\n",*/
                /*           handler->name, handler->messageId); */
            }
        }
    }
}


/*
 * Process incoming messages.
 *
 * We will know only at read time if the remote host closed
 * the connection (half-closed state - FIN-WAIT2). In that case
 * we must close our side of the socket and abort emission.
 * We will need another connection to send the request
 * There is need of refactoring here since we mix 
 * reply reception (tomcat -> apache) and request send (apache -> tomcat)
 * and everything using the same buffer (repmsg)
 * ajp13/ajp14 is async but handling read/send this way prevent nice recovery
 * In fact if tomcat link is broken during upload (browser ->apache ->tomcat)
 * we'll loose data and we'll have to abort the whole request.
 */
static int jk_workerEnv_processCallbacks(jk_env_t *env, jk_workerEnv_t *_this,
                                         jk_endpoint_t *e, jk_ws_service_t *r )
{
    int code;
    jk_handler_t *handler;
    int rc;
    jk_handler_t **handlerTable=e->worker->workerEnv->handlerTable;
    int maxHandler=e->worker->workerEnv->lastMessageId;
    
    /* Process reply - this is the main loop */
    /* Start read all reply message */
    while(1) {
        rc=-1;
        handler=NULL;

        env->l->jkLog(env, env->l, JK_LOG_INFO,
                        "ajp14.processCallbacks() Waiting reply\n");
        e->reply->reset(env, e->reply);
        
        rc= e->reply->receive( env, e->reply, e );
        if( rc!=JK_TRUE ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp14.service() Error reading reply\n");
            /* we just can't recover, unset recover flag */
            return JK_FALSE;
        }

        /* e->reply->dump(env, e->reply, "Received ");  */
        
        code = (int)e->reply->getByte(env, e->reply);
        if( code < maxHandler ) {
            handler=handlerTable[ code ];
        }

        if( handler==NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp14.processCallback() Invalid code: %d\n", code);
            e->reply->dump(env, e->reply, "Message: ");
            return JK_FALSE;
        }
        
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp14.dispath() Calling %d %s\n", handler->messageId,
                      handler->name);
        
        /* Call the message handler */
        rc=handler->callback( env, e->reply, r, e );
        
        /* Process the status code returned by handler */
        switch( rc ) {
        case JK_HANDLER_LAST:
            /* no more data to be sent, fine we have finish here */
            return JK_TRUE;
        case JK_HANDLER_OK:
            /* One-time request, continue to listen */
            break;
        case JK_HANDLER_RESPONSE:
            /* 
             * in upload-mode there is no second chance since
             * we may have allready send part of uploaded data 
             * to Tomcat.
             * In this case if Tomcat connection is broken we must 
             * abort request and indicate error.
             * A possible work-around could be to store the uploaded
             * data to file and replay for it
             */
            e->recoverable = JK_FALSE; 
            rc = e->post->send(env, e->post, e );
            if (rc < 0) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.processCallbacks() error sending response data\n");
                return JK_FALSE;
            }
            break;
        case JK_HANDLER_ERROR:
            /*
             * we won't be able to gracefully recover from this so
             * set recoverable to false and get out.
             */
            e->recoverable = JK_FALSE;
            return JK_FALSE;
        case JK_HANDLER_FATAL:
            /*
             * Client has stop talking to us, so get out.
             * We assume this isn't our fault, so just a normal exit.
             * In most (all?)  cases, the ajp13_endpoint::reuse will still be
             * false here, so this will be functionally the same as an
             * un-recoverable error.  We just won't log it as such.
             */
            return JK_FALSE;
        default:
            /* Unknown status */
            /* return JK_FALSE; */
        }
    }
    return JK_FALSE;
}




static jk_worker_t *jk_workerEnv_createWorker(jk_env_t *env,
                                              jk_workerEnv_t *_this,
                                              const char *name, 
                                              jk_map_t *init_data)
{
    int err;
    char *type;
    jk_env_objectFactory_t fac;
    jk_worker_t *w = NULL;
    jk_worker_t *oldW = NULL;
    jk_pool_t *workerPool;

    /* First find if it already exists */
    w=_this->getWorkerForName( env, _this, name );
    if( w != NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "workerEnv.createWorker(): Using existing worker %s\n",
                      name);
        return w;
    }

    workerPool=_this->pool->create(env, _this->pool, HUGE_POOL_SIZE);

    type=jk_map_getStrProp( env, init_data,"worker",name,"type",NULL );

    /* Each worker has it's own pool */
    if( type == NULL ) type="ajp13";
    

    w=(jk_worker_t *)env->getInstance(env, workerPool, "worker",
                                      type );
    
    if( w == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                "workerEnv.createWorker(): factory can't create worker %s:%s\n",
                      type, name); 
        return NULL;
    }

    w->name=(char *)name;
    w->pool=workerPool;
    w->workerEnv=_this;
    
    err=w->validate(env, w, init_data, _this);
    
    if( err!=JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerEnv.createWorker(): validate failed for %s:%s\n", 
                      type, name); 
        w->destroy(env, w);
        return NULL;
    }
    
    err=w->init(env, w, init_data, _this);
    
    if(err!=JK_TRUE) {
        w->destroy(env, w);
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerEnv.createWorker() init failed for %s\n", 
                      name); 
        return NULL;
    }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "workerEnv.createWorker(): validate and init %s:%s\n",
                  type, name);

    _this->worker_map->put(env, _this->worker_map, name, w, (void *)&oldW);
            
    if(oldW!=NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerEnv.createWorker() duplicated %s worker \n",
                      name);
        oldW->destroy(env, oldW);
    }
    
    return w;
}

int JK_METHOD jk_workerEnv_factory( jk_env_t *env, jk_pool_t *pool, void **result,
                                    const char *type, const char *name)
{
    jk_workerEnv_t *_this;
    int err;
    jk_pool_t *uriMapPool;

    env->l->jkLog(env, env->l, JK_LOG_DEBUG, "Creating workerEnv \n");

    _this=(jk_workerEnv_t *)pool->calloc( env, pool, sizeof( jk_workerEnv_t ));
    _this->pool=pool;
    *result=_this;

    _this->init_data = NULL;
    jk_map_default_create(env, & _this->init_data, pool);
    

    _this->worker_file     = NULL;
    _this->log_file        = NULL;
    _this->log_level       = -1;
    _this->mountcopy       = JK_FALSE;
    _this->was_initialized = JK_FALSE;
    _this->options         = JK_OPT_FWDURIDEFAULT;

    /*
     * By default we will try to gather SSL info.
     * Disable this functionality through JkExtractSSL
     */
    _this->ssl_enable  = JK_TRUE;
    /*
     * The defaults ssl indicators match those in mod_ssl (seems
     * to be in more use).
     */
    _this->https_indicator  = "HTTPS";
    _this->certs_indicator  = "SSL_CLIENT_CERT";

    /*
     * The following (comented out) environment variables match apache_ssl!
     * If you are using apache_sslapache_ssl uncomment them (or use the
     * configuration directives to set them.
     *
    _this->cipher_indicator = "HTTPS_CIPHER";
    _this->session_indicator = NULL;
     */

    /*
     * The following environment variables match mod_ssl! If you
     * are using another module (say apache_ssl) comment them out.
     */
    _this->cipher_indicator = "SSL_CIPHER";
    _this->session_indicator = "SSL_SESSION_ID";
    _this->key_size_indicator = "SSL_CIPHER_USEKEYSIZE";

    /*     if(!map_alloc(&(_this->automount))) { */
    /*         jk_error_exit(APLOG_MARK, APLOG_EMERG, s, p, "Memory error"); */
    /*     } */

    _this->uriMap = NULL;
    _this->secret_key = NULL; 

    _this->envvars_in_use = JK_FALSE;
    jk_map_default_create(env, &_this->envvars, pool);

    jk_map_default_create(env,&_this->worker_map, _this->pool);

    uriMapPool = _this->pool->create(env, _this->pool, HUGE_POOL_SIZE);
    
    _this->uriMap=env->getInstance(env, uriMapPool,"uriMap", "default");

    if( _this->uriMap==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "Error getting uriMap implementation\n");
        return JK_FALSE;
    }

    _this->uriMap->workerEnv = _this;
    _this->perThreadWorker=0;
    
    /* methods */
    _this->init=&jk_workerEnv_init;
    _this->getWorkerForName=&jk_workerEnv_getWorkerForName;
    _this->close=&jk_workerEnv_close;
    _this->createWorker=&jk_workerEnv_createWorker;
    _this->createWebapp=&jk_workerEnv_createWebapp;
    _this->processCallbacks=&jk_workerEnv_processCallbacks;

    _this->rootWebapp=_this->createWebapp( env, _this, NULL, "/", NULL );

    _this->globalEnv=env;
    
    return JK_TRUE;
}
