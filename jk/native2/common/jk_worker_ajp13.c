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
static char *myAttInfo[]={ "channelName", "route", "errorState", "recovering",
                           "epCount", NULL };

static void * JK_METHOD jk2_worker_ajp14_getAttribute(jk_env_t *env, jk_bean_t *bean, char *name ) {
    jk_worker_t *worker=(jk_worker_t *)bean->object;
    
    if( strcmp( name, "channelName" )==0 ) {
        if( worker->channel != NULL )
            return worker->channel->mbean->name;
        else
            return worker->channelName;
    } else if (strcmp( name, "route" )==0 ) {
        return worker->route;
    } else if (strcmp( name, "errorState" )==0 ) {
        if( worker->in_error_state ) 
            return "Y";
        else
            return NULL;
    } else if (strcmp( name, "recovering" )==0 ) {
        if( worker->in_recovering ) 
            return "Y";
        else
            return NULL;
    } else if (strcmp( name, "epCount" )==0 ) {
        char *result;
        if( worker->endpointCache==NULL ) return "0";
        result=env->tmpPool->calloc( env, env->tmpPool, 6 );
        sprintf( result, "%d", worker->endpointCache->count );
        return result;
    } else {
        return NULL;
    }
}

/*
 * Initialize the worker.
 */
static int JK_METHOD 
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
        ajp14->channelName=value;
    } else {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp14.setProperty() Unknown property %s %s %s\n", mbean->name, name, value );
        return JK_FALSE;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "ajp14.setProperty() %s %s %s\n", mbean->name, name, value );
    
    return JK_TRUE;
}

#define JK_AJP13_PING               (unsigned char)8

/* 
 * Build the ping cmd. Tomcat will get control and will be able 
 * to send any command.
 *
 * +-----------------------+
 * | PING CMD (1 byte) |
 * +-----------------------+
 *
 * XXX Add optional Key/Value set .
 *  
 */
int jk2_serialize_ping(jk_env_t *env, jk_msg_t *msg,
                       jk_endpoint_t *ae)
{
    int rc;
    
    /* To be on the safe side */
    msg->reset(env, msg);

    rc= msg->appendByte( env, msg, JK_AJP13_PING);
    if (rc!=JK_TRUE )
        return JK_FALSE;

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
    if( ae->worker->channel != NULL )
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
    
    int err;

    if( channel==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.connect() no channel %s\n", ae->worker->mbean->name );
        return JK_FALSE;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "ajp14.connect() %s %s\n", ae->worker->channelName, channel->mbean->name );
    
    err=channel->open( env, channel, ae );

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
                "ajp14.service() error sending, reconnect %s %s\n",
                      e->worker->mbean->name, e->worker->channelName);

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

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "ajp14.service() processing callbacks\n");

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

    if( w->channel==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp14.service() no channel defined, error in init\n", w->mbean->name);
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
                  "ajp14.service() %s\n", w->channelName);

    if( w->channel->beforeRequest != NULL ) {
        w->channel->beforeRequest( env, w->channel, w, e, s );
    }
    
    /* First message for this request */
    if( w->channel->is_stream == JK_TRUE ) {
        err=jk2_worker_ajp14_forwardStream( env, w, s, e );
    } else {
        err=jk2_worker_ajp14_forwardSingleThread( env, w, s, e );
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "ajp14.service() done %s\n", e->worker->mbean->name);

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
    jk_bean_t *jkb;
    
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

    jkb=env->createBean2( env,ajp14->pool,  "endpoint", NULL );
    if( jkb==NULL )
        return JK_FALSE;
    e = (jk_endpoint_t *)jkb->object;
    e->worker = ajp14;

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
        /* No "channel" was specified. Default to a channel, using
           the local part of the worker name to construct it. The type
           of the channel will be unix socket if a / is found, jni if
           the name is jni, and socket otherwise.
           
           If the channle is not found, create one.
        */
        char *localName=strchr( ajp14->mbean->name, ':' );
        if( localName==NULL || localName[1]=='\0' ) {
            /* No local part, use the defaults */
            ajp14->channelName="channel.socket";
        } else {
            char *prefix;
            localName++;
            if( strcmp( localName, "jni" ) == 0 ) {
                /* Easy one */
                ajp14->channelName="channel.jni";
            }  else {
                if( strchr( localName, '/' )) {
                    prefix="channel.apr:";
                } else {
                    /* We could do more - if other channels are defined and
                       we can guess it */
                    prefix="channel.socket:";
                }
                ajp14->channelName=ajp14->pool->calloc( env, ajp14->pool, strlen( localName )+
                                                        strlen( prefix ) + 2 );
                strcpy( ajp14->channelName, prefix );
                strcat( ajp14->channelName, localName );
            }
        }
    }

    if( ajp14->channel == NULL ) {
        ajp14->channel= env->getByName( env, ajp14->channelName );
    }
    
    if( ajp14->channel == NULL ) {
        jk_bean_t * chB=env->createBean( env, ajp14->workerEnv->pool, ajp14->channelName);
        if( chB==NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp14.init(): can't create channel %s\n",
                          ajp14->channelName);
            return JK_FALSE;
        }
        ajp14->channel = chB->object;

        if( ajp14->channel == NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp14.init(): no channel found %s\n",
                          ajp14->channelName);
            return JK_FALSE;
        }
        rc=ajp14->workerEnv->initChannel( env, ajp14->workerEnv, ajp14->channel );
        if( rc != JK_TRUE ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp14.init(): channel init failed\n");
        }
    }

    ajp14->channel->worker=ajp14;


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
    result->getAttribute= jk2_worker_ajp14_getAttribute;
    result->object = w;
    w->mbean=result;

    w->workerEnv=env->getByName( env, "workerEnv" );
    w->workerEnv->addWorker( env, w->workerEnv, w );

    result->getAttributeInfo=myAttInfo;
    
    return JK_TRUE;
}
