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
 * Description: Workers controller                                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/

#include "jk_env.h"
#include "jk_workerEnv.h" 
#include "jk_config.h"
#include "jk_worker.h"
#include "jk_shm.h"
#include "jk_channel.h"
#include "jk_registry.h"

#define DEFAULT_WORKER              ("lb")

static void jk2_workerEnv_close(jk_env_t *env, jk_workerEnv_t *wEnv);
static void jk2_workerEnv_initHandlers(jk_env_t *env, jk_workerEnv_t *wEnv);
static int  jk2_workerEnv_init1(jk_env_t *env, jk_workerEnv_t *wEnv);

/* ==================== Setup ==================== */


static int JK_METHOD jk2_workerEnv_setAttribute( struct jk_env *env, struct jk_bean *mbean,
                                       char *name, void *valueP)
{
    jk_workerEnv_t *wEnv=mbean->object;
    char *value=valueP;

    /** XXX Should it be per/uri ?
     */
    if( strcmp( name, "logger" )==0 ) {
        wEnv->logger_name = value;
    } else if( strcmp( name, "sslEnable" )==0 ) {
        wEnv->ssl_enable = JK_TRUE;
    } else if( strcmp( name, "timing" )==0 ) {
        wEnv->timing= atoi( value );
    } else if( strcmp( name, "httpsIndicator" )==0 ) {
        wEnv->https_indicator = value;
    } else if( strcmp( name, "certsIndicator" )==0 ) {
        wEnv->certs_indicator = value;
    } else if( strcmp( name, "cipherIndicator" )==0 ) {
        wEnv->cipher_indicator = value;
    } else if( strcmp( name, "sessionIndicator" )==0 ) {
        wEnv->session_indicator = value;
    } else if( strcmp( name, "keySizeIndicator" )==0 ) {
        wEnv->key_size_indicator = value;
    } else if( strcmp( name, "forwardKeySize" ) == 0 ) {
        wEnv->options |= JK_OPT_FWDKEYSIZE;
    /* Small change in how we treat options: we have a default,
       and assume that any option declared by user has the intention
       of overriding the default ( "-Option == no option, leave the
       default
    */
    } else  if( strcmp( name, "forwardURICompat" )==0 ) {
        wEnv->options &= ~JK_OPT_FWDURIMASK;
        wEnv->options |=JK_OPT_FWDURICOMPAT;
    } else if( strcmp( name, "forwardURICompatUnparsed" )==0 ) {
        wEnv->options &= ~JK_OPT_FWDURIMASK;
        wEnv->options |=JK_OPT_FWDURICOMPATUNPARSED;
    } else if( strcmp( name, "forwardURIEscaped" )==0 ) {
        wEnv->options &= ~JK_OPT_FWDURIMASK;
        wEnv->options |= JK_OPT_FWDURIESCAPED;
    } else if( strcmp( name, "noRecoveryIfRequestSent" )==0 ) {
        wEnv->options &= ~JK_OPT_RECOSTRATEGYMASK;
        wEnv->options |= JK_OPT_RECO_ABORTIFTCGETREQUEST;
    } else if( strcmp( name, "noRecoveryIfHeaderSent" )==0 ) {
        wEnv->options &= ~JK_OPT_RECOSTRATEGYMASK;
        wEnv->options |= JK_OPT_RECO_ABORTIFTCSENDHEADER;
    } else {
         return JK_ERR;
    }

    return JK_OK;
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
            if( w->mbean->destroy !=NULL ) 
                w->mbean->destroy(env,w->mbean);
        }
    }
    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "workerEnv.close() done %d\n", sz); 
    wEnv->worker_map->clear(env, wEnv->worker_map);
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

static int jk2_workerEnv_initWorker(jk_env_t *env,
                                    jk_workerEnv_t *wEnv, jk_worker_t *w )
{

    int rc;
    
    if( (w == NULL) || (w->mbean == NULL) ) return JK_ERR;
    
    if( w->mbean->disabled ) return JK_OK;
        
    w->workerEnv=wEnv;

    if( w->mbean->state >= JK_STATE_INIT ) return JK_OK;

    if( w->mbean->init == NULL )
        return JK_OK;

    rc=w->mbean->init(env, w->mbean );
        
    if( rc == JK_OK ) {
        w->mbean->state=JK_STATE_INIT;
    } else {
        w->mbean->state=JK_STATE_DISABLED;
        w->mbean->disabled=JK_TRUE;
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerEnv.initWorkers() init failed for %s\n", 
                      w->mbean->name); 
    } 
    return JK_OK;
}

static int jk2_workerEnv_initWorkers(jk_env_t *env,
                                     jk_workerEnv_t *wEnv)
{
    int i;

    for( i=0; i< wEnv->worker_map->size( env, wEnv->worker_map ); i++ ) {
        jk_worker_t *w= wEnv->worker_map->valueAt( env, wEnv->worker_map, i );
        jk2_workerEnv_initWorker( env, wEnv, w );
    }
    return JK_OK;
}



static int jk2_workerEnv_initChannel(jk_env_t *env,
                                     jk_workerEnv_t *wEnv, jk_channel_t *ch)
{
    int rc=JK_OK;
    
    ch->workerEnv=wEnv;

    if( ch->mbean->disabled ) return JK_OK;
    
    if( ch->mbean->init != NULL ) {
        rc=ch->mbean->init(env, ch->mbean);
        if(rc!=JK_OK) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "workerEnv.initChannel() init failed for %s\n", 
                          ch->mbean->name);
            /* Disable the associated worker */
            ch->worker->channel=NULL;
            ch->worker->channelName=NULL;
        }
        jk2_workerEnv_initWorker( env, wEnv, ch->worker );
    }
    
    return rc;
}

static int jk2_workerEnv_initChannels(jk_env_t *env,
                                      jk_workerEnv_t *wEnv)
{
    int i;

    for( i=0; i< wEnv->channel_map->size( env, wEnv->channel_map ); i++ ) {
        jk_channel_t *ch= wEnv->channel_map->valueAt( env, wEnv->channel_map, i );
        jk2_workerEnv_initChannel( env, wEnv, ch );
    }
    return JK_OK;
}


static void jk2_workerEnv_initHandlers(jk_env_t *env, jk_workerEnv_t *wEnv)
{
    /* Find the max message id */
    /* XXX accessing private data... env most provide some method to get this */
    jk_map_t *registry=env->_registry;
    int size=registry->size( env, registry );
    int i;
    
    for( i=0; i<size; i++ ) {
        jk_handler_t *handler;

        char *name= registry->nameAt( env, registry, i );
        if( strncmp( name, "handler.", 8 ) == 0 ) {
            jk_bean_t *jkb=env->createBean2(env, wEnv->pool, name, ""  );
            if( jkb==NULL || jkb->object==NULL )
                continue;
            handler=(jk_handler_t *)jkb->object;
            handler->init( env, handler, wEnv );
        }
    }
}

static int jk2_workerEnv_registerHandler(jk_env_t *env, jk_workerEnv_t *wEnv,
                                         const char *type,
                                         const char *name, int preferedId,
                                         jk_handler_callback callback,
                                         char *signature)
{
    
    jk_handler_t *h=(jk_handler_t *)wEnv->pool->calloc( env, wEnv->pool, sizeof( jk_handler_t));
    h->name=(char *)name;
    h->messageId=preferedId;
    h->callback=callback;
    h->workerEnv=wEnv;

    jk2_workerEnv_checkSpace( env, wEnv->pool,
                              (void ***)&wEnv->handlerTable,
                              &wEnv->lastMessageId,
                              h->messageId );
    wEnv->handlerTable[ h->messageId ]=h;
    /*wEnv->l->jkLog( wEnv->l, JK_LOG_INFO, "Registered %s %d\n",*/
    /*           handler->name, handler->messageId); */

    return JK_OK;
}

/** Called from the parent, in a multi-process server.
 */
static int jk2_workerEnv_parentInit(jk_env_t *env, jk_workerEnv_t *wEnv)
{
    char *configFile;
    
    env->l->init( env, env->l );

    configFile=wEnv->config->file;
    if(  configFile == NULL ) {
        wEnv->config->setPropertyString( env, wEnv->config,
                                         "config.file",
                                         "${serverRoot}/conf/workers2.properties" );
        configFile=wEnv->config->file;
    }

    if (wEnv->shm != NULL && wEnv->shm->mbean->disabled)
        wEnv->shm = NULL;

    if (wEnv->shm != NULL) {
        /* Disable the shm if the initialization fails
        */
        if (wEnv->shm->init(env, wEnv->shm) == JK_ERR)
            wEnv->shm = NULL;
    }
    
    if( wEnv->shm != NULL && wEnv->shm->head != NULL ) {
        wEnv->shm->reset( env, wEnv->shm);
        if( wEnv->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG, "workerEnv.init() Reset shm\n" );
    }
    return JK_OK;
}

/** Normal child init
 */
static int jk2_workerEnv_init(jk_env_t *env, jk_workerEnv_t *wEnv)
{
    char *configFile;
    jk_bean_t *jkb;
   
    /* We need to pid here - Linux will return the pid of the thread if
       called after init(), and we'll not be able to locate the slot id
       This is part of the workarounds needed for Apache2's removal of
       child_num from connection.
    */
    wEnv->childProcessId=getpid();
    
    configFile=wEnv->config->file;
    if(  configFile == NULL ) {
        wEnv->config->setPropertyString( env, wEnv->config,
                                         "config.file",
                                         "${serverRoot}/conf/workers2.properties" );
        configFile=wEnv->config->file;
    }

    jkb=env->createBean2(env, wEnv->mbean->pool,"threadMutex", NULL);
    if( jkb != NULL ) {
        wEnv->cs=jkb->object;
        jkb->init(env, jkb );
    }

    if( wEnv->logger_name!=NULL){
        jkb=env->getBean(env,wEnv->logger_name);
        if (jkb == NULL){
            jkb=env->createBean( env, env->globalPool, wEnv->logger_name);
        }
        env->alias( env, wEnv->logger_name, "logger");
        env->l = jkb->object;
    }
    env->l->init( env, env->l );

    /* Set default worker. It'll be used for all uris that have no worker
     */
    if( wEnv->defaultWorker == NULL ) {
        jk_worker_t *w=wEnv->worker_map->get( env, wEnv->worker_map, "lb:lb" );
        
        if( w==NULL ) {
            jk_bean_t *jkb=env->createBean2(env, wEnv->pool, "lb", "lb" );
            w=jkb->object;
            if( wEnv->mbean->debug > 0 )
                env->l->jkLog(env, env->l, JK_LOG_DEBUG, "workerEnv.init() create default worker %s\n",  jkb->name );
        }
        wEnv->defaultWorker= w;
    }

    if( wEnv->vm != NULL  && ! wEnv->vm->mbean->disabled ) {
        wEnv->vm->init( env, wEnv->vm );
    }

    jk2_workerEnv_initChannels( env, wEnv );

    jk2_workerEnv_initWorkers( env, wEnv );
    jk2_workerEnv_initHandlers( env, wEnv );
    
    if (wEnv->shm != NULL && wEnv->shm->mbean->disabled)
        wEnv->shm = NULL;

    if (wEnv->shm != NULL) {
        /* Disable the shm if the initialization fails
        */
        if (wEnv->shm->init(env, wEnv->shm) == JK_ERR)
            wEnv->shm = NULL;
    }

    wEnv->epStat=NULL;
    
    wEnv->uriMap->init(env, wEnv->uriMap );

    env->l->jkLog(env, env->l, JK_LOG_INFO, "workerEnv.init() ok %s\n", configFile );
    return JK_OK;
}

static int jk2_workerEnv_dispatch(jk_env_t *env, jk_workerEnv_t *wEnv,
                                  void *target, jk_endpoint_t *e, int code, jk_msg_t *msg)
{
    jk_handler_t *handler;
    int rc;
    jk_handler_t **handlerTable=wEnv->handlerTable;
    int maxHandler=wEnv->lastMessageId;
    
    rc=-1;
    handler=NULL;
        
    if( code < maxHandler ) {
        handler=handlerTable[ code ];
    }
    
    if( handler==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerEnv.dispatch() Invalid code: %d\n", code);
        e->reply->dump(env, e->reply, "Message: ");
        return JK_ERR;
    }

    if( wEnv->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "workerEnv.dispatch() Calling %d %s\n", handler->messageId,
                      handler->name);
    
    /* Call the message handler */
    rc=handler->callback( env, target, e, msg );
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
                                          jk_endpoint_t *ep, jk_ws_service_t *req )
{
    int code;
    jk_handler_t *handler;
    int rc;
    ep->currentRequest=req;
    
    /* Process reply - this is the main loop */
    /* Start read all reply message */
    while(1) {
        jk_msg_t *msg=ep->reply;
        rc=-1;
        handler=NULL;

        if( ep->worker->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "workerEnv.callbacks() %s\n",
                          ep->worker->channel->mbean->name);
        
        msg->reset(env, msg);

		/* Check for reply in timeout */
		if ( ep->worker->reply_timeout != 0) {
		    if (ep->worker->channel->hasinput(env, ep->worker->channel, ep, ep->worker->reply_timeout) != JK_TRUE) {
	        	env->l->jkLog(env, env->l, JK_LOG_ERROR,
	                      "workerEnv.processCallbacks() no reply after %d ms waiting\n",  ep->worker->reply_timeout);
	
	            /* Error is unrecoverable if tomcat failed (Tomcat allready got request) */
	            if (wEnv->options & JK_OPT_RECO_ABORTIFTCGETREQUEST)
	                req->is_recoverable_error = JK_FALSE;
	                
	        	return JK_ERR;
	    	}
	    }

        rc= ep->worker->channel->recv( env, ep->worker->channel,  ep,
                                       msg);
        if( rc!=JK_OK ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "workerEnv.processCallbacks() Error reading reply\n");

            /* Error is unrecoverable if tomcat failed (Tomcat allready got request) */
            if (wEnv->options & JK_OPT_RECO_ABORTIFTCGETREQUEST) {
                req->is_recoverable_error = JK_FALSE;
	            env->l->jkLog(env, env->l, JK_LOG_ERROR,
	                          "workerEnv.processCallbacks() by configuration, avoid recovery when tomcat has received request\n");
            }
                
            return rc;
        }

        /** After I received the first message ( head most likely ), it
            is impossible to recover */
        ep->recoverable=JK_FALSE;
        
        if( ep->worker->mbean->debug > 10 )
            ep->request->dump( env, msg, "Received" );

        /* XXX Use this _only_ for backward compatibility.
           invoke() and jk_bean should be used, probably with a first parameter
           indicating the target component and a second param indicating the
           code ( local to each component ).
        */
        code = (int)msg->getByte(env, msg);
        rc=jk2_workerEnv_dispatch( env, wEnv, req, ep, code, msg );

        /* Process the status code returned by handler */
        switch( rc ) {
        case JK_HANDLER_LAST:
            /* no more data to be sent, fine we have finish here */
            return JK_OK;
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
            req->is_recoverable_error = JK_FALSE;
            if( ep->worker->mbean->debug > 10 )
                msg->dump( env, msg, "Apache->tomcat" );

            rc = ep->worker->channel->send(env, ep->worker->channel, ep,
                                           msg );
            if (rc < 0) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "workerEnv.processCallbacks() error sending response data\n");
                ep->recoverable = JK_FALSE;
                return rc;
            }
            break;
        case JK_HANDLER_ERROR:
            /* Normal error ( for example writing to the client failed ).
             * The ajp connection is still in a stable state but if we ask in configuration
             * to abort when header has been send to client, mark as unrecoverable.
             */
            if (wEnv->options & JK_OPT_RECO_ABORTIFTCSENDHEADER) {
                req->is_recoverable_error = JK_FALSE;
	            env->l->jkLog(env, env->l, JK_LOG_ERROR,
	                          "workerEnv.processCallbacks() by configuration, avoid recovery when tomcat has started to send headers to client\n");
			}
            else
            	ep->recoverable = JK_TRUE; /* Should we do this ? not sure */
            
            return rc;
        case JK_HANDLER_FATAL:
            /*
             * Protocol error. We'll disconnect the ajp connection and reconnect.
             */
            ep->recoverable = JK_FALSE; 
            return rc;
        default:
            /* Unknown status */
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "workerEnv.processCallbacks() unknown status %d\n", rc);
            ep->recoverable = JK_FALSE; 
            return rc;
        }
    }
    return JK_ERR;
}

static int jk2_workerEnv_addChannel(jk_env_t *env, jk_workerEnv_t *wEnv,
                                    jk_channel_t *ch) 
{
    jk_bean_t *jkb;
    
    ch->mbean->id=wEnv->channel_map->size( env, wEnv->channel_map );
    wEnv->channel_map->add(env, wEnv->channel_map, ch->mbean->name, ch);

    /* Automatically create the ajp13 worker to be used with this channel.
     */
    jkb=env->createBean2(env, ch->mbean->pool, "ajp13", ch->mbean->localName );
    if( jkb == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerEnv.addChannel(): Can't find ajp13 worker\n" );
        return JK_ERR;
    }
    ch->worker=jkb->object;
    ch->worker->channelName=ch->mbean->name;
    ch->worker->channel=ch;
            
    return JK_OK;
}

static int jk2_workerEnv_addEndpoint(jk_env_t *env, jk_workerEnv_t *wEnv, jk_endpoint_t *ep)
{
    int pos=wEnv->endpointMap->size( env, wEnv->endpointMap );
    
    wEnv->endpointMap->add( env, wEnv->endpointMap, ep->mbean->localName, ep );
    ep->mbean->id=pos;
    
    if (ep->mbean->init( env, ep->mbean )==JK_ERR)
        return JK_ERR;

    return JK_OK;
}


static int jk2_workerEnv_addWorker(jk_env_t *env, jk_workerEnv_t *wEnv,
                                   jk_worker_t *w) 
{
    int err=JK_OK;
    jk_worker_t *oldW = NULL;
    
    w->workerEnv=wEnv;

    w->mbean->id=wEnv->worker_map->size( env, wEnv->worker_map );

    w->rPoolCache= jk2_objCache_create( env, w->mbean->pool  );

    err=w->rPoolCache->init( env, w->rPoolCache,
                             1024 ); /* XXX make it unbound */

    wEnv->worker_map->put(env, wEnv->worker_map, w->mbean->name, w, (void *)&oldW);
            
    if(oldW!=NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerEnv.addWorker() duplicated %s worker \n",
                      w->mbean->name);
        if( w->mbean->destroy != NULL )
            oldW->mbean->destroy(env, oldW->mbean);
    }
    
    return JK_OK;
}

static jk_worker_t *jk2_workerEnv_createWorker(jk_env_t *env,
                                               jk_workerEnv_t *wEnv,
                                               char *type, char *name) 
{
    jk_worker_t *w = NULL;
    jk_bean_t *jkb;

    /* First find if it already exists */
    w=env->getByName( env, name );
    if( w != NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "workerEnv.createWorker(): Using existing worker %s\n",
                      name);
        return w;
    }
    
    jkb=env->createBean(env, wEnv->pool, name );

    if( jkb == NULL || jkb->object==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerEnv.createWorker(): factory can't create worker %s:%s\n",
                      type, name); 
        return NULL;
    }
    w=(jk_worker_t *)jkb->object;

    return w;
}


int JK_METHOD jk2_workerEnv_factory(jk_env_t *env, jk_pool_t *pool,
                                    jk_bean_t *result,
                                    const char *type, const char *name)
{
    jk_workerEnv_t *wEnv;
    jk_bean_t *jkb;

    wEnv=(jk_workerEnv_t *)pool->calloc( env, pool, sizeof( jk_workerEnv_t ));

    /* env->l->jkLog(env, env->l, JK_LOG_DEBUG, "Creating workerEnv %#lx\n", wEnv); */

    result->object=wEnv;
    wEnv->mbean=result;
    result->setAttribute=jk2_workerEnv_setAttribute;
    
    wEnv->pool=pool;

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


    wEnv->logger_name     = NULL;
    wEnv->was_initialized = JK_FALSE;
    wEnv->options         = JK_OPT_FWDURIDEFAULT | JK_OPT_RECOSTRATEGYDEFAULT;

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

    wEnv->envvars_in_use = JK_FALSE;
    jk2_map_default_create(env, &wEnv->envvars, pool);

    jk2_map_default_create(env, &wEnv->worker_map, wEnv->pool);
    jk2_map_default_create(env, &wEnv->channel_map, wEnv->pool);
    jk2_map_default_create(env, &wEnv->endpointMap, wEnv->pool);

    wEnv->perThreadWorker=0;
    
    /* methods */
    wEnv->init=&jk2_workerEnv_init;
    wEnv->parentInit=&jk2_workerEnv_parentInit;
    wEnv->close=&jk2_workerEnv_close;
    wEnv->addWorker=&jk2_workerEnv_addWorker;
    wEnv->addChannel=&jk2_workerEnv_addChannel;
    wEnv->addEndpoint=&jk2_workerEnv_addEndpoint;
    wEnv->initChannel=&jk2_workerEnv_initChannel;
    wEnv->registerHandler=&jk2_workerEnv_registerHandler;
    wEnv->processCallbacks=&jk2_workerEnv_processCallbacks;
    wEnv->dispatch=&jk2_workerEnv_dispatch;
    wEnv->globalEnv=env;

    jkb=env->createBean2(env, wEnv->pool,"uriMap", "");
    if( jkb==NULL || jkb->object==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "Error getting uriMap implementation\n");
        return JK_ERR;
    }
    wEnv->uriMap=jkb->object;
    wEnv->uriMap->workerEnv = wEnv;
    
    jkb=env->createBean2(env, wEnv->pool,"config", "");
    if( jkb==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "Error creating config\n");
        return JK_ERR;
    }
    env->alias(env, "config:", "config");
    wEnv->config=jkb->object;
    wEnv->config->file = NULL;
    wEnv->config->workerEnv = wEnv;
    wEnv->config->map = wEnv->initData;

    wEnv->childId=-1;

    wEnv->epStat=NULL;
    jkb=env->createBean2(env, wEnv->pool,"shm", "");
    if( jkb==NULL ) {
        wEnv->shm=NULL;
    } else {
        env->alias(env, "shm:", "shm");
        wEnv->shm=(jk_shm_t *)jkb->object;
    }

    return JK_OK;
}
