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

/**
 * Description: AJP14 next generation Bi-directional protocol.
 *              Backward compatible with Ajp13
 * Author:      Henri Gomez <hgomez@slib.fr>
 * Author:      Costin <costin@costin.dnt.ro>                              
 * Author:      Gal Shachor <shachor@il.ibm.com>                           
 */

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_channel.h"
#include "jk_msg.h"
#include "jk_logger.h"
#include "jk_handler.h"
#include "jk_service.h"
#include "jk_env.h"
#include "jk_objCache.h"
#include "jk_registry.h"


#define AJP_DEF_RETRY_ATTEMPTS    (2)
#define AJP14_PROTO 14
#define AJP13_PROTO 13

#define AJP13_DEF_HOST  ("localhost")
#define AJP13_DEF_PORT  (8009)
#define AJP14_DEF_HOST  ("localhost")
#define AJP14_DEF_PORT  (8011)

/* -------------------- Impl -------------------- */


/*
 * Initialize the worker.
 */
static int 
jk2_worker_ajp14_setAttribute(jk_env_t *env, jk_bean_t *mbean, 
                              char *name, void *valueP )
{
    jk_worker_t *ajp14=(jk_worker_t *)mbean->object;
    char *value=(char *)valueP;
    int    port;
    char * host;
    int err;
    char * secret_key;
    char *channelType;
           
    if( strcmp( name, "secretkey" )==0 ) {
        ajp14->secret = value;
    } else if( strcmp( name, "cachesize" )==0 ) {
        ajp14->cache_sz=atoi( value );
    } else if( strcmp( name, "lb_factor" )==0 ) {
        ajp14->lb_factor=atof( value );
    } else if( strcmp( name, "channel" )==0 ) {
        if( strncmp( value, "channel.", 8 ) != 0 ) {
            char *newValue=(char *)ajp14->pool->calloc( env, ajp14->pool, strlen(value) + 10 );
            strcpy( newValue, "channel." );
            strcat( newValue, value );
            env->l->jkLog(env, env->l, JK_LOG_INFO, "ajp14.setProperty() auto replace %s %s\n",
                          value, newValue);
            value=newValue;
        }
        ajp14->channel=env->createInstance(env, ajp14->pool, value, NULL );
        if( ajp14->channel == NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "Error creating %s channel\n", channelType );
            return JK_FALSE;
        }
        env->l->jkLog(env, env->l, JK_LOG_INFO, "ajp14.setProperty() channel: %s %s\n",
                      value,ajp14->channel->mbean->name);

     } else {
        /* It's probably a channel property
         */
        if( ajp14->channel==NULL ) {
            
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "No channel for %s, set channel before other properties %s=%s\n",
                          mbean->name, name, value );
            return JK_FALSE;
        }

        env->l->jkLog(env, env->l, JK_LOG_INFO, "endpoint.setProperty() channel %s=%s\n",
                      name, value);
        ajp14->channel->mbean->setAttribute( env, ajp14->channel->mbean, name, value );
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "ajp14.setProperty() %s %s %s\n", mbean->name, name, value );
    
    return JK_TRUE;
}

/*
 * Close the endpoint (clean buf and close socket)
 */
static void jk2_close_endpoint(jk_env_t *env, jk_endpoint_t *ae)
{
    env->l->jkLog(env, env->l, JK_LOG_INFO, "endpoint.close() %s\n",
                  ae->worker->mbean->name);

    ae->reuse = JK_FALSE;
    ae->worker->channel->close( env, ae->worker->channel, ae );
    ae->cPool->reset( env, ae->cPool );
    ae->cPool->close( env, ae->cPool );
    ae->pool->reset( env, ae->pool );
    ae->pool->close( env, ae->pool );
}

/** Connect a channel, implementing the logging protocol if ajp14
 */
static int jk2_worker_ajp14_connect(jk_env_t *env, jk_endpoint_t *ae) {
    jk_channel_t *channel=ae->worker->channel;
    jk_msg_t *msg;
    
    int err=channel->open( env, channel, ae );

    if( err != JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.connect() failed %s\n", ae->worker->mbean->name );
        return JK_FALSE;
    }

    /* Check if we must execute a logon after the physical connect */
    if (ae->worker->secret == NULL)
        return JK_TRUE;

    /* Do the logon process */
    env->l->jkLog(env, env->l, JK_LOG_INFO, "ajp14.connect() logging in\n" );

    /* use the reply buffer - it's a new channel, it is cetainly not
     in use. The request and post buffers are probably in use if this
    is a reconnect */
    msg=ae->reply;

    jk2_serialize_ping( env, msg, ae );
    
    err = ae->worker->channel->send( env, ae->worker->channel, ae, msg );

    /* Move to 'slave' mode, listening to messages */
    err=ae->worker->workerEnv->processCallbacks( env, ae->worker->workerEnv,
                                                 ae, NULL);

    /* Anything but OK - the login failed
     */
    if( err != JK_TRUE ) {
        jk2_close_endpoint( env, ae );
    }
    return err;
}

/** There is no point of trying multiple times - each channel may
    have built-in recovery mechanisms
*/
#define JK_RETRIES 2

/** First message in a stream-based connection. If the first send
    fails, try to reconnect.
*/
static int JK_METHOD
jk2_worker_ajp14_sendAndReconnect(jk_env_t *env, jk_worker_t *worker,
                              jk_ws_service_t *s,
                              jk_endpoint_t   *e )
{
    int attempt;
    int err;
    
    /*
     * Try to send the request on a valid endpoint. If one endpoint
     * fails, close the channel and try again ( maybe tomcat was restarted )
     * 
     * XXX JK_RETRIES could be replaced by the number of workers in
     * a load-balancing configuration 
     */
    for(attempt = 0 ; attempt < JK_RETRIES ;attempt++) {
        jk_channel_t *channel= worker->channel;

        /* e->request->dump(env, e->request, "Before sending "); */
        err=e->worker->channel->send( env, e->worker->channel, e,
                                      e->request );

	if (err==JK_TRUE ) {
            /* We sent the request, have valid endpoint */
            break;
        }

        if( e->recoverable != JK_TRUE ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                     "ajp14.service() error sending request %s, giving up\n",
                     worker->mbean->name);
            return JK_FALSE;
        }
        
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                "ajp14.service() error sending, retry on a new endpoint %s\n",
                      e->worker->mbean->name);

        channel->close( env, channel, e );

        err=jk2_worker_ajp14_connect(env, e); 

        if( err != JK_TRUE ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                     "ajp14.service() failed to reconnect endpoint errno=%d\n",
                     errno);
            return JK_FALSE;
        }

        /*
         * After we are connected, each error that we are going to
         * have is probably unrecoverable
         */
        e->recoverable = JK_FALSE;
    }
    return JK_TRUE;
}


static int JK_METHOD
jk2_worker_ajp14_forwardStream(jk_env_t *env, jk_worker_t *worker,
                              jk_ws_service_t *s,
                              jk_endpoint_t   *e )
{
    int err;

    err=jk2_worker_ajp14_sendAndReconnect( env, worker, s, e );
    if( err!=JK_TRUE )
        return err;
    
    /* We should have a channel now, send the post data */
    s->is_recoverable_error = JK_TRUE;
    e->recoverable = JK_TRUE;

    /* Prepare to send some post data ( ajp13 proto ). We do that after the
     request was sent ( we're receiving data from client, can be slow, no
     need to delay - we can do that in paralel. ( not very sure this is
     very usefull, and it brakes the protocol ) ! */
    if (s->is_chunked || s->left_bytes_to_send > 0) {
        /* We never sent any POST data and we check it we have to send at
	 * least of block of data (max 8k). These data will be kept in reply
	 * for resend if the remote Tomcat is down, a fact we will learn only
	 * doing a read (not yet) 
	 */
        err=jk2_serialize_postHead( env, e->post, s, e );

        if (err != JK_TRUE ) {
            /* the browser stop sending data, no need to recover */
            e->recoverable = JK_FALSE;
            s->is_recoverable_error = JK_FALSE;
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp14.service() Error receiving initial post \n");
            return JK_FALSE;
        }
        err= e->worker->channel->send( env, e->worker->channel, e,
                                       e->post );
    }

    err = e->worker->workerEnv->processCallbacks(env, e->worker->workerEnv,
                                                 e, s);
    
    /* if we can't get reply, check if no recover flag was set 
     * if is_recoverable_error is cleared, we have started received 
     * upload data and we must consider that operation is no more recoverable
     */
    if (! e->recoverable) {
        s->is_recoverable_error = JK_FALSE;
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.service() ajpGetReply unrecoverable error %d\n",
                      err);
        return JK_FALSE;
    }

    if( err != JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.service() ajpGetReply recoverable error %d\n",
                      err);
    }
    return err;
}

static int JK_METHOD
jk2_worker_ajp14_forwardSingleThread(jk_env_t *env, jk_worker_t *worker,
                                    jk_ws_service_t *s,
                                    jk_endpoint_t   *e )
{
    int err;

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "ajp14.forwardST() Before calling native channel\n");
    err=e->worker->channel->send( env, e->worker->channel, e,
                                  e->request );
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "ajp14.forwardST() After %d\n",err);
    
    
    return JK_TRUE;
}
     
static int JK_METHOD
jk2_worker_ajp14_service1(jk_env_t *env, jk_worker_t *w,
                        jk_ws_service_t *s,
                        jk_endpoint_t   *e )
{
    int err;

    if( ( e== NULL ) || ( s == NULL ) ) {
	env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.service() NullPointerException\n");
	return JK_FALSE;
    }

    e->currentRequest=s;
    
    /* Prepare the messages we'll use.*/ 
    e->request->reset( env, e->request );
    e->reply->reset( env, e->reply );
    e->post->reset( env, e->post );
    
    e->uploadfd	 = -1;		/* not yet used, later ;) */
    e->reuse = JK_FALSE;
    s->is_recoverable_error = JK_TRUE;
    /* Up to now, we can recover */
    e->recoverable = JK_TRUE;

    s->left_bytes_to_send = s->content_length;
    s->content_read=0;

    /* 
     * We get here initial request (in reqmsg)
     */
    err=jk2_serialize_request13(env, e->request, s, e);
    if (err!=JK_TRUE) {
	s->is_recoverable_error = JK_FALSE;                
	env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.service(): error marshaling\n");
	return JK_FALSE;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "ajp14.service() %s\n", e->worker->mbean->name);

    if( w->channel->beforeRequest != NULL ) {
        w->channel->beforeRequest( env, w->channel, w, e, s );
    }
    
    /* First message for this request */
    if( w->channel->is_stream == JK_TRUE ) {
        err=jk2_worker_ajp14_forwardStream( env, w, s, e );
    } else {
        err=jk2_worker_ajp14_forwardSingleThread( env, w, s, e );
    }

    if( w->channel->afterRequest != NULL ) {
        w->channel->afterRequest( env, w->channel, w, e, s );
    }

    e->currentRequest=NULL;
    
    return err;
}




static int JK_METHOD
jk2_worker_ajp14_done(jk_env_t *env, jk_worker_t *we, jk_endpoint_t *e)
{
    jk_worker_t *w;
    
    w= e->worker;

    if( e->cPool != NULL ) 
        e->cPool->reset(env, e->cPool);
    if (w->endpointCache != NULL ) {
        int err=0;
        err=w->endpointCache->put( env, w->endpointCache, e );
        if( err==JK_TRUE ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "ajp14.done() return to pool %s\n",
                          w->mbean->name );
            return JK_TRUE;
        }
    }

    /* No reuse or put() failed */

    jk2_close_endpoint(env, e);
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "ajp14.done() close endpoint %s\n",
                  w->mbean->name );
    
    return JK_TRUE;
}

static int JK_METHOD
jk2_worker_ajp14_getEndpoint(jk_env_t *env,
                            jk_worker_t *ajp14,
                            jk_endpoint_t **eP)
{
    jk_endpoint_t *e = NULL;
    jk_pool_t *endpointPool;
    
    if( ajp14->secret ==NULL ) {
    }

    if (ajp14->endpointCache != NULL ) {

        e=ajp14->endpointCache->get( env, ajp14->endpointCache );

        if (e!=NULL) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "ajp14.getEndpoint(): Reusing endpoint\n");
            *eP = e;
            return JK_TRUE;
        }
    }

    endpointPool = ajp14->pool->create( env, ajp14->pool, HUGE_POOL_SIZE );
    
    e = (jk_endpoint_t *)endpointPool->alloc(env, endpointPool,
                                              sizeof(jk_endpoint_t));
    if (e==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.get_endpoint OutOfMemoryException\n");
        return JK_FALSE;
    }

    e->pool = endpointPool;

    /* Init message storage areas.
     */
    e->request = jk2_msg_ajp_create( env, e->pool, 0);
    e->reply = jk2_msg_ajp_create( env, e->pool, 0);
    e->post = jk2_msg_ajp_create( env, e->pool, 0);
    
    e->reuse = JK_FALSE;

    e->cPool=endpointPool->create(env, endpointPool, HUGE_POOL_SIZE );

    e->worker = ajp14;
    e->channelData = NULL;
    
    *eP = e;
    return JK_TRUE;
}

/*
 * Serve the request, using AJP13/AJP14
 */
static int JK_METHOD
jk2_worker_ajp14_service(jk_env_t *env, jk_worker_t *w,
                        jk_ws_service_t *s)
{
    int err;
    jk_endpoint_t   *e;

    /* Get endpoint from the pool */
    jk2_worker_ajp14_getEndpoint( env, w, &e );

    err=jk2_worker_ajp14_service1( env, w, s, e );

    jk2_worker_ajp14_done( env, w, e);
    return err;
}


static int JK_METHOD
jk2_worker_ajp14_init(jk_env_t *env, jk_worker_t *ajp14)
{
    int  rc;

    if( ajp14->cache_sz == -1 )
        ajp14->cache_sz=JK_OBJCACHE_DEFAULT_SZ;

    if (ajp14->cache_sz > 0) {
        ajp14->endpointCache=jk2_objCache_create( env, ajp14->pool  );
        
        if( ajp14->endpointCache != NULL ) {
            rc=ajp14->endpointCache->init( env, ajp14->endpointCache,
                                           ajp14->cache_sz );
            if( rc!= JK_TRUE ) {
                ajp14->endpointCache=NULL;
            }
        }
    } else {
        ajp14->endpointCache=NULL;
    }

    if( ajp14->channelName == NULL ) {
        /* Use default channel */
        ajp14->channelName="channel.default";
    }

    ajp14->channel= env->getByName( env, ajp14->channelName );
    
    if( ajp14->channel == NULL ) {
        /* XXX  Create a default channel using socket/localhost/8009 ! */
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.validate(): no channel found %s\n",
                      ajp14->channelName);
        return JK_FALSE;
    }
    
    ajp14->channel->worker=ajp14;

    rc=ajp14->channel->init( env, ajp14->channel );
    if( rc != JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.init(): channel init failed\n");
        return rc;
    }

    return JK_TRUE;
}


static int JK_METHOD
jk2_worker_ajp14_destroy(jk_env_t *env, jk_worker_t *ajp14)
{
    int i;
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "ajp14.destroy()\n");

    if( ajp14->endpointCache != NULL ) {
        jk_endpoint_t *e;

        while( ajp14->endpointCache->count > 0 ) {
            
            e= ajp14->endpointCache->get( env, ajp14->endpointCache );
            
            if( e==NULL ) {
                // we finished all endpoints in the cache
                break;
            }
            
            jk2_close_endpoint(env, e);
        }
        ajp14->endpointCache->destroy( env, ajp14->endpointCache );

        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "ajp14.destroy() closed %d cached endpoints\n",
                      i);
    }

    ajp14->pool->close( env, ajp14->pool );

    return JK_TRUE;
}

int JK_METHOD jk2_worker_ajp14_factory( jk_env_t *env, jk_pool_t *pool,
                                        jk_bean_t *result,
                                        const char *type, const char *name)
{
    jk_worker_t *w=(jk_worker_t *)pool->calloc(env, pool, sizeof(jk_worker_t));

    if (name == NULL || w == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.factory() NullPointerException\n");
        return JK_FALSE;
    }
    w->pool = pool;
    w->cache_sz=-1;
    
    w->endpointCache= NULL;

    w->channel= NULL;
    w->secret= NULL;
   
    w->init= jk2_worker_ajp14_init;
    w->destroy=jk2_worker_ajp14_destroy;
    w->service = jk2_worker_ajp14_service;

    result->setAttribute= jk2_worker_ajp14_setAttribute;
    result->object = w;
    w->mbean=result;

    w->workerEnv=env->getByName( env, "workerEnv" );
    w->workerEnv->addWorker( env, w->workerEnv, w );
    
    return JK_TRUE;
}
