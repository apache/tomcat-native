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
#include "jk_channel.h"

#define DEFAULT_WORKER              ("ajp13")

int JK_METHOD jk2_workerEnv_factory(jk_env_t *env, jk_pool_t *pool,
                                    void **result,
                                    const char *type, const char *name);

static void jk2_workerEnv_close(jk_env_t *env, jk_workerEnv_t *wEnv);
static void jk2_workerEnv_initHandlers(jk_env_t *env, jk_workerEnv_t *wEnv);
static int  jk2_workerEnv_init1(jk_env_t *env, jk_workerEnv_t *wEnv);

/* ==================== Setup ==================== */

static int jk2_workerEnv_setWorkerFile( struct jk_env *env,
                                        struct jk_workerEnv *wEnv,
                                         char *workerFile)
{
    struct stat statbuf;
    int err;
    jk_map_t *props;
    int i;
    
    /* We should make it relative to JK_HOME or absolute path.
       ap_server_root_relative(cmd->pool,opt); */
    
    /* Avoid recursivity */
    for( i=0; i<wEnv->initData->size( env, wEnv->initData); i++ ) {
        char *name=wEnv->initData->nameAt(env, wEnv->initData, i);
        char *val=wEnv->initData->valueAt(env, wEnv->initData, i);
        if( strcmp( name, "workerFile" )==0 &&
            strcmp( val, workerFile ) == 0 ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "Recursive init - already read %s", workerFile );
            return JK_FALSE;
        }
    }

    if (stat(workerFile, &statbuf) == -1) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "Can't find the workers file %s", workerFile );
        return JK_FALSE;
    }
    
    /** Read worker files
     */
    env->l->jkLog(env, env->l, JK_LOG_DEBUG, "Reading properties %s %d\n",
                  workerFile,
                  wEnv->initData->size(env, wEnv->initData) );
    
    jk2_map_default_create(env, &props, wEnv->pool);
    
    err=jk2_map_readFileProperties(env, props, workerFile );
    
    if( err==JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "mod_jk.initJk() Reading worker properties %s %d\n",
                      workerFile, props->size( env, props ) );
    } else {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "mod_jk.initJk() Error reading worker properties %s\n",
                      workerFile );
        return JK_FALSE;
    }

    for( i=0; i<props->size( env, props); i++ ) {
        char *name=props->nameAt(env, props, i);
        char *val=props->valueAt(env, props, i);

        wEnv->setProperty( env, wEnv, name, val );
    }
    
    return JK_TRUE;
}


static int jk2_workerEnv_setLogLevel( struct jk_env *env,
                                      struct jk_workerEnv *wEnv,
                                      char *level)
{
     if(0 == strcasecmp(level, JK_LOG_INFO_VERB)) {
         env->l->level=JK_LOG_INFO_LEVEL;
     }
     if(0 == strcasecmp(level, JK_LOG_DEBUG_VERB)) {
         env->l->level=JK_LOG_DEBUG_LEVEL;
     }
     return JK_TRUE;
}

/** XXX make them ssl.property ?
 */
static int jk2_workerEnv_setSSLProperty( struct jk_env *env,
                                         struct jk_workerEnv *wEnv,
                                         char *name, char *opt)
{
    if( strcmp( name, "sslEnable" )==0 ) {
        wEnv->ssl_enable = JK_TRUE;
    } else if( strcmp( name, "httpsIndicator" )==0 ) {
        wEnv->https_indicator = opt;
    } else if( strcmp( name, "certsIndicator" )==0 ) {
        wEnv->certs_indicator = opt;
    } else if( strcmp( name, "cipherIndicator" )==0 ) {
        wEnv->cipher_indicator = opt;
    } else if( strcmp( name, "sessionIndicator" )==0 ) {
        wEnv->session_indicator = opt;
    } else if( strcmp( name, "keySizeIndicator" )==0 ) {
        wEnv->key_size_indicator = opt;
    } else if( strcmp( name, "ForwardKeySize" ) == 0 ) {
        /* XXX if( opt == TRUE ) */
        wEnv->options |= JK_OPT_FWDKEYSIZE;
    } else {
        return JK_FALSE;
    }
    /* One of the options was recognized */
    return JK_TRUE;
}

/** XXX make them forward.property ?
 */
static int jk2_workerEnv_setForwardProperty( struct jk_env *env,
                                             struct jk_workerEnv *wEnv,
                                             char *name, char *opt)
{
    /* Small change in how we treat options: we have a default,
       and assume that any option declared by user has the intention
       of overriding the default ( "-Option == no option, leave the
       default
    */
    if( strcmp( name, "ForwardURICompat" )==0 ) {
        wEnv->options &= ~JK_OPT_FWDURIMASK;
        wEnv->options |=JK_OPT_FWDURICOMPAT;
    } else if( strcmp( name, "ForwardURICompatUnparsed" )==0 ) {
        wEnv->options &= ~JK_OPT_FWDURIMASK;
        wEnv->options |=JK_OPT_FWDURICOMPATUNPARSED;
    } else if( strcmp( name, "ForwardURIEscaped" )==0 ) {
        wEnv->options &= ~JK_OPT_FWDURIMASK;
        wEnv->options |= JK_OPT_FWDURIESCAPED;
    } else {
        return JK_FALSE;
    }
    return JK_TRUE;
}

static int jk2_workerEnv_setWorkerList( struct jk_env *env,
                                        struct jk_workerEnv *wEnv,
                                        char *wlist)
{
    wEnv->worker_list=jk2_map_split( env, wEnv->initData, wEnv->pool,
                                          wlist, NULL, & wEnv->declared_workers );
}

static int jk2_workerEnv_setWorkerProperty( struct jk_env *env,
                                        struct jk_workerEnv *wEnv,
                                        char *name, char *val)

{
    jk_worker_t *w = NULL;
    char *wname=NULL;
    char *pname=NULL;
    char *dot=0;
    int i;

    if( strncmp( name, "worker.", 7 ) != 0 )
        return JK_FALSE;

    wname=wEnv->pool->pstrdup( env, wEnv->pool, name + 7);
    /* env->l->jkLog(env, env->l, JK_LOG_ERROR, "wname= %s\n", wname);     */
    dot=strchr( wname, '.' );
    if( dot== NULL )
        return JK_FALSE;
    *dot='\0';
    pname= dot+1;
    /*env->l->jkLog(env, env->l, JK_LOG_ERROR, "wname1= %s pname=%s\n", wname, pname); */
    
    w=wEnv->getWorkerForName( env, wEnv, wname );

    if( w != NULL ) {
        /* If we have an object with that name, set the prop */
        return w->setProperty( env, w, pname, val );
    }

    /** Unknown worker. It may be the first option, create it
     */
    if( strcmp( pname, "type" ) != 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "The first worker property must be type %s %s\n", wname, pname);
        return JK_FALSE;
    } else {
        
        /* We know the worker type, create it and set previous props
         */
        w=wEnv->createWorker(env, wEnv, wname, val );
        
        if( w==NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "init failed to create worker %s\n", wname);
            return JK_FALSE;
        }
        
        if( wEnv->defaultWorker == NULL )
            wEnv->defaultWorker=w;
    }
    return JK_TRUE;
}


static int jk2_workerEnv_setProperty( struct jk_env *env,
                                      struct jk_workerEnv *wEnv,
                                      const char *name, char *value)
{
    jk_map_t *initData=wEnv->initData;
    int status;

    value = jk2_map_replaceProperties(env, initData, initData->pool, value);

    /** XXX Do we need this ? How about removing/runtime changes/single value ?
     */
    initData->add( env, initData, name, value );

    if( strcmp( name, "workerFile" ) == 0 ) {
        return jk2_workerEnv_setWorkerFile(env, wEnv, value);
    } else if( strcmp( name, "logLevel") == 0 ) {
        return jk2_workerEnv_setLogLevel( env, wEnv, value );
    } else if( strcmp( name, "worker.list") == 0 ) {
        /* Note that worker list is what we init at startup
           in jk2 all declared workers will be configured and
           enabled on demand
        */        
        return jk2_workerEnv_setWorkerList( env, wEnv, value );
    } else if( strncmp( name, "worker.", 7) == 0 ) {
        return jk2_workerEnv_setWorkerProperty( env, wEnv, name, value );
    } else if( strncmp( name, "uriMap.", 7) == 0 ||
               strncmp( name, "uri.", 4) == 0 ||
               name[0]=='/'  ) {
        /** XXX pre-process it ? */
        wEnv->uriMap->setProperty( env, wEnv->uriMap, name, value );
    }

    /* Is it a ssl-related property ? */
    if( jk2_workerEnv_setSSLProperty( env, wEnv, name, value )==JK_TRUE ) {
        return JK_TRUE;
    }
    if( jk2_workerEnv_setForwardProperty( env, wEnv, name, value )==JK_TRUE ) {
        return JK_TRUE;
    }
    
    return JK_TRUE;
}


     
static char *jk2_workerEnv_getProperty( struct jk_env *env,
                                       struct jk_workerEnv *wEnv,
                                       const char *name)
{

    return NULL;
}


static void jk2_workerEnv_close(jk_env_t *env, jk_workerEnv_t *wEnv)
{
    int sz;
    int i;
    
    sz = wEnv->worker_map->size(env, wEnv->worker_map);

    for(i = 0 ; i < sz ; i++) {
        jk_worker_t *w = wEnv->worker_map->valueAt(env, wEnv->worker_map, i);
        if(w) {
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "destroy worker %s\n",
                          wEnv->worker_map->nameAt(env, wEnv->worker_map, i));
            if( w->destroy !=NULL ) 
                w->destroy(env,w);
        }
    }
    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "workerEnv.close() done %d\n", sz); 
    wEnv->worker_map->clear(env, wEnv->worker_map);
}

static jk_worker_t *jk2_workerEnv_getWorkerForName(jk_env_t *env,
                                                   jk_workerEnv_t *wEnv,
                                                   const char *name )
{
    jk_worker_t * rc;
    
    if(!name) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerEnv.getWorkerForName() NullPointerException\n");
        return NULL;
    }

    rc = wEnv->worker_map->get(env, wEnv->worker_map, name);

    /*     if( rc==NULL ) { */
    /*         l->jkLog(l, JK_LOG_INFO, */
    /*  "workerEnv.getWorkerForName(): no worker found for %s \n", name); */
    /*     }  */
    return rc;
}

static void jk2_workerEnv_checkSpace(jk_env_t *env, jk_pool_t *pool,
                                     void ***tableP, int *sizeP, int id)
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

static int jk2_workerEnv_initWorkers(jk_env_t *env,
                                     jk_workerEnv_t *wEnv)
{
    int i;
    char **wlist=wEnv->worker_list;

    for( i=0; i< wEnv->worker_map->size( env, wEnv->worker_map ); i++ ) {
        char *name= wEnv->worker_map->nameAt( env, wEnv->worker_map, i );
        jk_worker_t *w= wEnv->worker_map->valueAt( env, wEnv->worker_map, i );
        int err;

        if( w->init != NULL )
            err=w->init(env, w);
    
        if(err!=JK_TRUE) {
            if( w->destroy != NULL ) 
                w->destroy(env, w);
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "workerEnv.createWorker() init failed for %s\n", 
                          name); 
        }
    
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "workerEnv.createWorker(): init ok %s\n", name);
    }
    return JK_TRUE;
}


static void jk2_workerEnv_initHandlers(jk_env_t *env, jk_workerEnv_t *wEnv)
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
            localHandlers=(jk_map_t *)env->getInstance(env, wEnv->pool,
                                                       "handler", type );
            if( localHandlers==NULL ) continue;
            
            for( j=0; j< localHandlers->size( env, localHandlers ); j++ ) {
                handler=(jk_handler_t *)localHandlers->valueAt( env, localHandlers, j );
                jk2_workerEnv_checkSpace( env, wEnv->pool,
                                         (void ***)&wEnv->handlerTable,
                                         &wEnv->lastMessageId,
                                         handler->messageId );
                wEnv->handlerTable[ handler->messageId ]=handler;
                /*wEnv->l->jkLog( wEnv->l, JK_LOG_INFO, "Registered %s %d\n",*/
                /*           handler->name, handler->messageId); */
            }
        }
    }
}

static int jk2_workerEnv_init(jk_env_t *env, jk_workerEnv_t *wEnv)
{
    int err;
    char *opt;
    int options;

    env->l->jkLog(env, env->l, JK_LOG_INFO, "mod_jk.init_jk()\n" );

    /** This will eventuall read logFile.
        XXX Config per 'component', log.level, log.file, etc
    */
    env->l->open( env, env->l, wEnv->initData );

    jk2_workerEnv_initWorkers( env, wEnv );
    jk2_workerEnv_initHandlers( env, wEnv );
    
    err=wEnv->uriMap->init(env, wEnv->uriMap );
    return JK_TRUE;
}

static int jk2_workerEnv_dispatch(jk_env_t *env, jk_workerEnv_t *wEnv,
                                  jk_endpoint_t *e, jk_ws_service_t *r)
{
    int code;
    jk_handler_t *handler;
    int rc;
    jk_handler_t **handlerTable=e->worker->workerEnv->handlerTable;
    int maxHandler=e->worker->workerEnv->lastMessageId;
    
    rc=-1;
    handler=NULL;
        
    code = (int)e->reply->getByte(env, e->reply);
    if( code < maxHandler ) {
        handler=handlerTable[ code ];
    }
    
    if( handler==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.dispath() Invalid code: %d\n", code);
        e->reply->dump(env, e->reply, "Message: ");
        return JK_FALSE;
    }
        
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "ajp14.dispath() Calling %d %s\n", handler->messageId,
                  handler->name);
    
    /* Call the message handler */
    rc=handler->callback( env, e->reply, r, e );
    return rc;
}

/* XXX This should go to channel ( or a base class for socket channels ) */
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
static int jk2_workerEnv_processCallbacks(jk_env_t *env, jk_workerEnv_t *wEnv,
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
        
        rc= e->worker->channel->recv( env, e->worker->channel,  e,
                                         e->reply);
        if( rc!=JK_TRUE ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp14.service() Error reading reply\n");
            /* we just can't recover, unset recover flag */
            return JK_FALSE;
        }

        /* e->reply->dump(env, e->reply, "Received ");  */

        rc=jk2_workerEnv_dispatch( env, wEnv, e, r );

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
            rc = e->worker->channel->send(env, e->worker->channel, e, e->post );
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

static jk_worker_t *jk2_workerEnv_releasePool(jk_env_t *env,
                                              jk_workerEnv_t *wEnv,
                                              const char *name, 
                                              jk_map_t *initData)
{
    
}


static jk_worker_t *jk2_workerEnv_createWorker(jk_env_t *env,
                                               jk_workerEnv_t *wEnv,
                                               const char *name, 
                                               char *type)
{
    int err=JK_TRUE;
    jk_env_objectFactory_t fac;
    jk_worker_t *w = NULL;
    jk_worker_t *oldW = NULL;
    jk_pool_t *workerPool;

    /* First find if it already exists */
    w=wEnv->getWorkerForName( env, wEnv, name );
    if( w != NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "workerEnv.createWorker(): Using existing worker %s\n",
                      name);
        return w;
    }

    workerPool=wEnv->pool->create(env, wEnv->pool, HUGE_POOL_SIZE);

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

    w->type=type;
    w->name=(char *)name;
    w->pool=workerPool;
    w->workerEnv=wEnv;

    w->rPoolCache= jk2_objCache_create( env, workerPool  );
    err=w->rPoolCache->init( env, w->rPoolCache,
                                    1024 ); /* XXX make it unbound */
    wEnv->worker_map->put(env, wEnv->worker_map, name, w, (void *)&oldW);
            
    if(oldW!=NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerEnv.createWorker() duplicated %s worker \n",
                      name);
        if( w->destroy != NULL )
            oldW->destroy(env, oldW);
    }
    
    return w;
}


int JK_METHOD jk2_workerEnv_factory(jk_env_t *env, jk_pool_t *pool,
                                    void **result,
                                    const char *type, const char *name)
{
    jk_workerEnv_t *wEnv;
    int err;
    jk_pool_t *uriMapPool;

    env->l->jkLog(env, env->l, JK_LOG_DEBUG, "Creating workerEnv \n");

    wEnv=(jk_workerEnv_t *)pool->calloc( env, pool, sizeof( jk_workerEnv_t ));
    wEnv->pool=pool;
    *result=wEnv;

    wEnv->initData = NULL;
    jk2_map_default_create(env, & wEnv->initData, pool);
    
    /* Add 'compile time' settings. Those are defined in jk_global,
       with the other platform-specific settings. No need to ask
       the user what we can find ourself
    */
    wEnv->initData->put( env, wEnv->initData, "fs",
                           FILE_SEPARATOR_STR, NULL );
    wEnv->initData->put( env, wEnv->initData, "ps",
                           PATH_SEPARATOR_STR, NULL );
    wEnv->initData->put( env, wEnv->initData, "so",
                           SO_EXTENSION, NULL );
    wEnv->initData->put( env, wEnv->initData, "arch",
                           ARCH, NULL );


    wEnv->log_file        = NULL;
    wEnv->log_level       = -1;
    wEnv->mountcopy       = JK_FALSE;
    wEnv->was_initialized = JK_FALSE;
    wEnv->options         = JK_OPT_FWDURIDEFAULT;

    /*
     * By default we will try to gather SSL info.
     * Disable this functionality through JkExtractSSL
     */
    wEnv->ssl_enable  = JK_TRUE;
    /*
     * The defaults ssl indicators match those in mod_ssl (seems
     * to be in more use).
     */
    wEnv->https_indicator  = "HTTPS";
    wEnv->certs_indicator  = "SSL_CLIENT_CERT";

    /*
     * The following (comented out) environment variables match apache_ssl!
     * If you are using apache_sslapache_ssl uncomment them (or use the
     * configuration directives to set them.
     *
    wEnv->cipher_indicator = "HTTPS_CIPHER";
    wEnv->session_indicator = NULL;
     */

    /*
     * The following environment variables match mod_ssl! If you
     * are using another module (say apache_ssl) comment them out.
     */
    wEnv->cipher_indicator = "SSL_CIPHER";
    wEnv->session_indicator = "SSL_SESSION_ID";
    wEnv->key_size_indicator = "SSL_CIPHER_USEKEYSIZE";

    /*     if(!map_alloc(&(wEnv->automount))) { */
    /*         jk_error_exit(APLOG_MARK, APLOG_EMERG, s, p, "Memory error"); */
    /*     } */

    wEnv->uriMap = NULL;
    wEnv->secret_key = NULL; 

    wEnv->envvars_in_use = JK_FALSE;
    jk2_map_default_create(env, &wEnv->envvars, pool);

    jk2_map_default_create(env,&wEnv->worker_map, wEnv->pool);

    uriMapPool = wEnv->pool->create(env, wEnv->pool, HUGE_POOL_SIZE);
    
    wEnv->uriMap=env->getInstance(env, uriMapPool,"uriMap", "default");

    if( wEnv->uriMap==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "Error getting uriMap implementation\n");
        return JK_FALSE;
    }

    wEnv->uriMap->workerEnv = wEnv;
    wEnv->perThreadWorker=0;
    
    /* methods */
    wEnv->init=&jk2_workerEnv_init;
    wEnv->getWorkerForName=&jk2_workerEnv_getWorkerForName;
    wEnv->close=&jk2_workerEnv_close;
    wEnv->createWorker=&jk2_workerEnv_createWorker;
    wEnv->processCallbacks=&jk2_workerEnv_processCallbacks;
    wEnv->dispatch=&jk2_workerEnv_dispatch;
    wEnv->setProperty=&jk2_workerEnv_setProperty;
    wEnv->getProperty=&jk2_workerEnv_getProperty;

    wEnv->rootWebapp=wEnv->uriMap->createUriEnv( env, wEnv->uriMap, NULL, "/" );

    wEnv->globalEnv=env;
    
    return JK_TRUE;
}
