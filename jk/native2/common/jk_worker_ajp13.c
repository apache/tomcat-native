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
static char *jk2_worker_ajp14_getAttributeInfo[]={ "lb_factor", "lb_value", 
                                                   "route", "errorState", "graceful",
                                                   "epCount", "errorTime", NULL };

static char *jk2_worker_ajp14_multiValueInfo[]={"group", NULL };

static char *jk2_worker_ajp14_setAttributeInfo[]={"debug", "channel", "route",
                                                  "lb_factor", "level", NULL };


static void * JK_METHOD jk2_worker_ajp14_getAttribute(jk_env_t *env, jk_bean_t *bean, char *name ) {
    jk_worker_t *worker=(jk_worker_t *)bean->object;
    
    if( strcmp( name, "channelName" )==0 ) {
        if( worker->channel != NULL )
            return worker->channel->mbean->name;
        else
            return worker->channelName;
    } else if (strcmp( name, "route" )==0 ) {
        return worker->route;
    } else if (strcmp( name, "errorTime" )==0 ) {
        char *buf=env->tmpPool->calloc( env, env->tmpPool, 20 );
        sprintf( buf, "%d", worker->error_time );
        return buf;
    } else if (strcmp( name, "lb_value" )==0 ) {
        char *buf=env->tmpPool->calloc( env, env->tmpPool, 20 );
        sprintf( buf, "%d", worker->lb_value );
        return buf;
    } else if (strcmp( name, "lb_factor" )==0 ) {
        char *buf=env->tmpPool->calloc( env, env->tmpPool, 20 );
        sprintf( buf, "%d", worker->lb_factor );
        return buf;
    } else if (strcmp( name, "errorState" )==0 ) {
        if( worker->in_error_state ) 
            return "Y";
        else
            return NULL;
    } else if (strcmp( name, "graceful" )==0 ) {
        if( worker->mbean->disabled ) 
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
    } else if( strcmp( name, "tomcatId" )==0 ) {
        ajp14->route=value;
    } else if( strcmp( name, "route" )==0 ) {
        ajp14->route=value;
    } else if( strcmp( name, "group" )==0 ) {
        ajp14->groups->add( env, ajp14->groups, value, ajp14 );
    } else if( strcmp( name, "lb_factor" )==0 ) {
        ajp14->lb_factor=atoi( value );
    } else if( strcmp( name, "level" )==0 ) {
        ajp14->level=atoi( value );
    } else if( strcmp( name, "channel" )==0 ) {
        ajp14->channelName=value;
    } else {
        return JK_ERR;
    }

    return JK_OK;
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
    if (rc!=JK_OK )
        return JK_ERR;

    return JK_OK;
}


/*
 * Close the endpoint (clean buf and close socket)
 */
static void jk2_close_endpoint(jk_env_t *env, jk_endpoint_t *ae)
{
    if( ae->worker->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_INFO, "endpoint.close() %s\n",
                      ae->worker->mbean->name);

    /*     ae->reuse = JK_FALSE; */
    if( ae->worker->channel != NULL )
        ae->worker->channel->close( env, ae->worker->channel, ae );
    
    ae->sd=-1;
    ae->recoverable=JK_TRUE;
    
    ae->cPool->reset( env, ae->cPool );
    /* ae->cPool->close( env, ae->cPool ); */

    /* Don't touch the ae->pool, the object has important
       statistics */
    /* ae->pool->reset( env, ae->pool ); */
    /*     ae->pool->close( env, ae->pool ); */
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
        return JK_ERR;
    }

    if( ae->worker->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp14.connect() %s %s\n", ae->worker->channelName, channel->mbean->name );
    
    err=channel->open( env, channel, ae );

    if( err != JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.connect() failed %s\n", ae->worker->mbean->name );
        return JK_ERR;
    }

    /* We just reconnected, reset error state
     */
    ae->worker->in_error_state=JK_FALSE;

#ifdef HAS_APR
    ae->stats->connectedTime=apr_time_now();
#endif
    
    /** XXX use a 'connected' field */
    if( ae->sd == -1 ) ae->sd=0;
    
    /* Check if we must execute a logon after the physical connect */
    if (ae->worker->secret == NULL)
        return JK_OK;

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
    if( err != JK_OK ) {
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
    int err=JK_OK;
    
    /*
     * Try to send the request on a valid endpoint. If one endpoint
     * fails, close the channel and try again ( maybe tomcat was restarted )
     * 
     * XXX JK_RETRIES could be replaced by the number of workers in
     * a load-balancing configuration 
     */
    for(attempt = 0 ; attempt < JK_RETRIES ;attempt++) {
        jk_channel_t *channel= worker->channel;

        if( e->sd == -1 ) {
            err=jk2_worker_ajp14_connect(env, e);
            if( err!=JK_OK ) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "ajp14.service() failed to connect endpoint errno=%d %s\n",
                              errno, strerror( errno ));

                e->worker->in_error_state=JK_TRUE;
                return err;
            }
        }

        err=e->worker->channel->send( env, e->worker->channel, e,
                                      e->request );

        if( e->worker->mbean->debug > 10 )
            e->request->dump( env, e->request, "Sent" );
        
        if (err==JK_OK ) {
            /* We sent the request, have valid endpoint */
            break;
        }

        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.service() error sending, reconnect %s %d %d %s\n",
                      e->worker->channelName, err, errno, strerror(errno));

        channel->close( env, channel, e );
        e->sd=-1;
    }
    return JK_OK;
}


static int JK_METHOD
jk2_worker_ajp14_forwardStream(jk_env_t *env, jk_worker_t *worker,
                              jk_ws_service_t *s,
                              jk_endpoint_t   *e )
{
    int err;

    e->recoverable = JK_TRUE;
    s->is_recoverable_error = JK_TRUE;

    err=jk2_worker_ajp14_sendAndReconnect( env, worker, s, e );
    if( err!=JK_OK )
        return err;
    
    /* We should have a channel now, send the post data */

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

        if( e->worker->mbean->debug > 10 )
            e->request->dump( env, e->request, "Post head" );

        if (err != JK_OK ) {
            /* the browser stop sending data, no need to recover */
            e->recoverable = JK_FALSE;
            s->is_recoverable_error = JK_FALSE;
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp14.service() Error receiving initial post \n");
            return JK_ERR;
        }
        err= e->worker->channel->send( env, e->worker->channel, e,
                                       e->post );
        if( err != JK_OK ) {
            e->recoverable = JK_FALSE;
            s->is_recoverable_error = JK_FALSE;
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp14.service() Error receiving initial post \n");
            return JK_ERR;
        }
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
        return JK_ERR;
    }

    if( err != JK_OK ) {
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

    if( e->worker->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp14.forwardST() Before calling native channel %s\n",
                      e->worker->channel->mbean->name);
    err=e->worker->channel->send( env, e->worker->channel, e,
                                  e->request );
    if( e->worker->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp14.forwardST() After %d\n",err);
    
    
    return err;
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
	return JK_ERR;
    }
    
    if( w->channel==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp14.service() no channel defined, error in init\n", w->mbean->name);
        return JK_ERR;
    }
    
    e->currentRequest=s;

    /* XXX configurable ? */
    strncpy( e->stats->active, s->req_uri, 64);
    
    /* Prepare the messages we'll use.*/ 
    e->request->reset( env, e->request );
    e->reply->reset( env, e->reply );
    e->post->reset( env, e->post );
    
    s->is_recoverable_error = JK_TRUE;
    /* Up to now, we can recover */
    e->recoverable = JK_TRUE;

    s->left_bytes_to_send = s->content_length;
    s->content_read=0;

    /* 
     * We get here initial request (in reqmsg)
     */
    err=jk2_serialize_request13(env, e->request, s, e);
    if (err!=JK_OK) {
	s->is_recoverable_error = JK_FALSE;                
	env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.service(): error marshaling\n");
	return JK_ERR;
    }

    if( w->mbean->debug > 0 ) 
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

    if( w->mbean->debug > 0 ) 
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
    int rc=JK_OK;
    
    w= e->worker;
    
    if( e->cPool != NULL ) 
        e->cPool->reset(env, e->cPool);

    if( w->endpointCache == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "ajp14.done() No pool\n");
        return JK_ERR;
    }
    
    if( ! e->recoverable ||  w->in_error_state ) {
        jk2_close_endpoint(env, e);
        /*     if( w->mbean->debug > 0 )  */
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp14.done() close endpoint %s error_state %d\n",
                      w->mbean->name, w->in_error_state );
    }
        
    rc=w->endpointCache->put( env, w->endpointCache, e );
    if( rc!=JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "ajp14.done() Error recycling ep\n");
        return rc;        
    }
    if( w->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp14.done() return to pool %s\n",
                      w->mbean->name );
    return JK_OK;
}

static int JK_METHOD
jk2_worker_ajp14_getEndpoint(jk_env_t *env,
                            jk_worker_t *ajp14,
                            jk_endpoint_t **eP)
{
    jk_endpoint_t *e = NULL;
    jk_pool_t *endpointPool;
    jk_bean_t *jkb;
    int csOk;
    
    if( ajp14->secret ==NULL ) {
    }

    if( ajp14->endpointCache == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "ajp14.getEndpoint() No pool\n");
        return JK_ERR;
    }

    e=ajp14->endpointCache->get( env, ajp14->endpointCache );

    if (e!=NULL) {
        *eP = e;
        return JK_OK;
    }

    JK_ENTER_CS(&ajp14->cs, csOk);
    if( !csOk ) return JK_ERR;

    {
        jkb=env->createBean2( env, ajp14->mbean->pool,  "endpoint", NULL );
        if( jkb==NULL ) {
            JK_LEAVE_CS( &ajp14->cs, csOk );
            return JK_ERR;
        }
    
        e = (jk_endpoint_t *)jkb->object;
        e->worker = ajp14;
        e->sd=-1;

        ajp14->endpointMap->add( env, ajp14->endpointMap, jkb->localName, jkb );
                                 
        *eP = e;

        ajp14->workerEnv->addEndpoint( env, ajp14->workerEnv, e );
    }
    JK_LEAVE_CS( &ajp14->cs, csOk );
        
    if( ajp14->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp14.getEndpoint(): Created endpoint %s %s \n",
                      ajp14->mbean->name, jkb->name);
    
    return JK_OK;
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

#ifdef HAS_APR
    if( s->uriEnv->timing == JK_TRUE ) {
        e->stats->startTime=s->startTime;
        e->stats->jkStartTime=e->stats->connectedTime=apr_time_now();
        if(e->stats->startTime==0)
            e->stats->startTime=e->stats->jkStartTime;
    }
#endif
    
    err=jk2_worker_ajp14_service1( env, w, s, e );

    /* One error doesn't mean the whole worker is in error state.
     */
    if( err==JK_OK ) {
        e->stats->reqCnt++;
    } else {
        e->stats->errCnt++;
    }

#ifdef HAS_APR
    if( s->uriEnv->timing == JK_TRUE ) {
        apr_time_t reqTime;

        e->stats->endTime=apr_time_now();
        reqTime=e->stats->endTime - e->stats->startTime;
    
        e->stats->totalTime += reqTime;
        if( e->stats->maxTime < reqTime )
            e->stats->maxTime=reqTime;
    }
#endif
    jk2_worker_ajp14_done( env, w, e);
    return err;
}


static int JK_METHOD
jk2_worker_ajp14_init(jk_env_t *env, jk_bean_t *bean )
{
    jk_worker_t *ajp14=bean->object;
    int  rc;
    int size;
    int i;

    if(ajp14->channel != NULL &&
       ajp14->channel->mbean->debug > 0 ) {
        ajp14->mbean->debug = ajp14->channel->mbean->debug;
    }

    if(ajp14->channel != NULL &&
       ajp14->channel->mbean->disabled  )
        ajp14->mbean->disabled = JK_TRUE;
    
    ajp14->endpointCache=jk2_objCache_create( env, ajp14->mbean->pool  );
        
    if( ajp14->endpointCache == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.init(): error creating endpoint cache\n");
        return JK_ERR;
    }
    
    /* Will grow */
    rc=ajp14->endpointCache->init( env, ajp14->endpointCache, -1 );
    if( rc!= JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.init(): error creating endpoint cache\n");
        return JK_ERR;
    }

    if( ajp14->channel == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.init(): No channel %s\n", ajp14->mbean->localName);
        return JK_ERR;
    }

    
    /* Find the groups we are member on and add ourself in
     */
    size=ajp14->groups->size( env, ajp14->groups );
    if( size==0 ) {
        /* No explicit groups, it'll go to default lb */
        jk_worker_t *lb=ajp14->workerEnv->defaultWorker;
        
        lb->mbean->setAttribute(env, lb->mbean, "worker",
                                ajp14->mbean->name);
        if( ajp14->mbean->debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "ajp14.init(): Adding %s to default lb\n", ajp14->mbean->localName);
    } else {
        for( i=0; i<size; i++ ) {
            char *name= ajp14->groups->nameAt( env, ajp14->groups, i );
            jk_worker_t *lb;

            if( ajp14->mbean->debug > 0 )
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                              "ajp14.init(): Adding %s to %s\n",
                              ajp14->mbean->localName, name);
            lb= env->getByName2( env, "worker.lb", name );
            if( lb==NULL ) {
                /* Create the lb group */
                if( ajp14->mbean->debug > 0 ) 
                    env->l->jkLog(env, env->l, JK_LOG_INFO,
                                  "ajp14.init(): Automatically creating the group %s\n",
                                  name);
                env->createBean2( env, ajp14->workerEnv->mbean->pool, "worker.lb", name );
                lb= env->getByName2( env, "worker.lb", name );
                if( lb==NULL ) {
                    env->l->jkLog(env, env->l, JK_LOG_ERROR,
                                  "ajp14.init(): Failed to create %s\n", name);
                    return JK_ERR;
                }
            }
            lb->mbean->setAttribute(env, lb->mbean, "worker",
                                    ajp14->mbean->name);
        }

    }
    
    /* XXX Find any 'group' property - find or create an lb for that
       and register it
    */
    return JK_OK;
}


static int JK_METHOD
jk2_worker_ajp14_destroy(jk_env_t *env, jk_bean_t *bean)
{
    jk_worker_t *ajp14=bean->object;
    int i;

    if( ajp14->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp14.destroy()\n");
    
    if( ajp14->endpointCache != NULL ) {
        jk_endpoint_t *e;
        i=ajp14->endpointCache->count;
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

    return JK_OK;
}

int JK_METHOD jk2_worker_ajp14_factory( jk_env_t *env, jk_pool_t *pool,
                                        jk_bean_t *result,
                                        const char *type, const char *name)
{
    jk_worker_t *w=(jk_worker_t *)pool->calloc(env, pool, sizeof(jk_worker_t));
    int i;

    if (name == NULL || w == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.factory() NullPointerException\n");
        return JK_ERR;
    }

    jk2_map_default_create(env, &w->groups, pool);
    jk2_map_default_create(env, &w->endpointMap, pool);

    JK_INIT_CS(&(w->cs), i);
    if (i!=JK_TRUE) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "objCache.create(): Can't init CS\n");
    }
    
    w->endpointCache= NULL;

    w->channel= NULL;
    w->secret= NULL;
   
    w->service = jk2_worker_ajp14_service;

    result->setAttribute= jk2_worker_ajp14_setAttribute;
    result->getAttribute= jk2_worker_ajp14_getAttribute;
    result->init= jk2_worker_ajp14_init;
    result->destroy=jk2_worker_ajp14_destroy;

    result->getAttributeInfo=jk2_worker_ajp14_getAttributeInfo;
    result->multiValueInfo=jk2_worker_ajp14_multiValueInfo;
    result->setAttributeInfo=jk2_worker_ajp14_setAttributeInfo;

    result->object = w;
    w->mbean=result;

    w->workerEnv=env->getByName( env, "workerEnv" );
    w->workerEnv->addWorker( env, w->workerEnv, w );

    
    return JK_OK;
}
