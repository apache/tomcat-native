/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2003 The Apache Software Foundation.          *
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
 * Description: AJP13 next generation Bi-directional protocol.
 *              Backward compatible with Ajp13
 * Author:      Henri Gomez <hgomez@apache.org>
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
#include "jk_requtil.h"
#include "jk_registry.h"

/* -------------------- Impl -------------------- */
static char *jk2_worker_ajp13_getAttributeInfo[]={ "lb_factor", "lb_value", "debug", "channel", "level",
                                                   "route", "routeRedirect", "errorState", "graceful", "groups", "disabled", 
                                                   "epCount", "errorTime", "connectTimeout", "replyTimeout",
                                                   "prepostTimeout", "recoveryOpts", NULL };

static char *jk2_worker_ajp13_multiValueInfo[]={"group", NULL };

static char *jk2_worker_ajp13_setAttributeInfo[]={"debug", "channel", "route", "routeRedirect","secretkey", "group", "graceful",
                                                  "disabled", "lb_factor", "level", "connectTimeout", "replyTimeout",
                                                   "prepostTimeout", "recoveryOpts", NULL };


static void * JK_METHOD jk2_worker_ajp13_getAttribute(jk_env_t *env, jk_bean_t *bean, char *name ) {
    jk_worker_t *worker=(jk_worker_t *)bean->object;
    
    if( strcmp( name, "channelName" )==0 ||
        strcmp( name, "channel" )==0   ) {
        if( worker->channel != NULL )
            return worker->channel->mbean->name;
        else
            return worker->channelName;
    } else if (strcmp( name, "route" )==0 ) {
        return worker->route;
    } else if (strcmp( name, "routeRedirect" )==0 ) {
        return worker->routeRedirect;
    } else if (strcmp( name, "debug" )==0 ) {
        return jk2_env_itoa( env,  bean->debug );
    } else if (strcmp( name, "groups" )==0 ) {
        return jk2_map_concatKeys( env, worker->groups, ":" );
    } else if (strcmp( name, "level" )==0 ) {
        return jk2_env_itoa( env,  worker->level );
    } else if (strcmp( name, "errorTime" )==0 ) {
        return jk2_env_itoa( env, worker->error_time );
    } else if (strcmp( name, "lb_value" )==0 ) {
        return jk2_env_itoa( env, worker->lb_value );
    } else if (strcmp( name, "lb_factor" )==0 ) {
        return jk2_env_itoa( env, worker->lb_factor );
    } else if (strcmp( name, "errorState" )==0 ) {
        return jk2_env_itoa( env, worker->in_error_state );
    } else if (strcmp( name, "graceful" )==0 ) {
        return jk2_env_itoa( env, worker->graceful );
    } else if (strcmp( name, "connectTimeout" )==0 ) {
        return jk2_env_itoa( env, worker->connect_timeout);
    } else if (strcmp( name, "replyTimeout" )==0 ) {
        return jk2_env_itoa( env, worker->reply_timeout);
    } else if (strcmp( name, "prepostTimeout" )==0 ) {
        return jk2_env_itoa( env, worker->prepost_timeout);
    } else if (strcmp( name, "recoveryOpts" )==0 ) {
        return jk2_env_itoa( env, worker->recovery_opts);
    } else if (strcmp( name, "disabled" )==0 ) {
        return jk2_env_itoa( env, bean->disabled );
    } else if (strcmp( name, "epCount" )==0 ) {
        if( worker->endpointCache==NULL ) return "0";
        return jk2_env_itoa( env, worker->endpointCache->count );
    } else {
        return NULL;
    }
}


/*
 * Initialize the worker
 */
static int JK_METHOD 
jk2_worker_ajp13_setAttribute(jk_env_t *env, jk_bean_t *mbean, 
                              char *name, void *valueP )
{
    jk_worker_t *ajp13=(jk_worker_t *)mbean->object;
    char *value=(char *)valueP;
           
    if( strcmp( name, "secretkey" )==0 ) {
        ajp13->secret = value;
    } else if( strcmp( name, "tomcatId" )==0 ) {
        ajp13->route=value;
    } else if( strcmp( name, "route" )==0 ) {
        ajp13->route=value;
    } else if( strcmp( name, "routeRedirect" )==0 ) {
        ajp13->routeRedirect=value;
    } else if( strcmp( name, "graceful" )==0 ) {
        ajp13->graceful=atoi( value );
    } else if( strcmp( name, "connectTimeout" )==0 ) {
        ajp13->connect_timeout=atoi( value );
    } else if( strcmp( name, "replyTimeout" )==0 ) {
        ajp13->reply_timeout=atoi( value );
    } else if( strcmp( name, "prepostTimeout" )==0 ) {
        ajp13->prepost_timeout=atoi( value );
    } else if( strcmp( name, "recoveryOpts" )==0 ) {
        ajp13->recovery_opts=atoi( value );
    } else if( strcmp( name, "disabled" )==0 ) {
        mbean->disabled=atoi( value );
    } else if( strcmp( name, "group" )==0 ) {
        ajp13->groups->add( env, ajp13->groups, value, ajp13 );
    } else if( strcmp( name, "lb_factor" )==0 ) {
        ajp13->lb_factor=atoi( value );
    } else if( strcmp( name, "level" )==0 ) {
        ajp13->level=atoi( value );
    } else if( strcmp( name, "channel" )==0 ) {
        ajp13->channelName=value;
    } else if( strcmp( name, "max_connections" )==0 ) {
        ajp13->maxEndpoints=atoi(value);
    } else {
        return JK_ERR;
    }

    return JK_OK;
}

/* Webserver ask container to take control (logon phase) */
#define JK_AJP13_PING               (unsigned char)8

/* Webserver check if container is alive, since container should respond by cpong */
#define JK_AJP13_CPING              (unsigned char)10

/* Container response to cping request */
#define JK_AJP13_CPONG              (unsigned char)9

/* 
 * Build the ping cmd. Tomcat will get control and will be able 
 * to send any command.
 *
 * +-----------------------+
 * | PING CMD (1 byte)     |
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
 * Build the cping cmd. Tomcat should respond by a cpong.
 *
 * +-----------------------+
 * | CPING CMD (1 byte)    |
 * +-----------------------+
 *
 * XXX Add optional Key/Value set .
 *  
 */
int jk2_serialize_cping(jk_env_t *env, jk_msg_t *msg,
                       jk_endpoint_t *ae)
{
    int rc;
    
    /* To be on the safe side */
    msg->reset(env, msg);

    rc= msg->appendByte( env, msg, JK_AJP13_CPING);
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
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "endpoint.close() %s\n",
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

/** Check if a channel is alive, send a cping and wait for a cpong
    during timeoutms
 */
static int jk2_check_alive(jk_env_t *env, jk_endpoint_t *ae, int timeout) {

    int err;
    jk_channel_t *channel=ae->worker->channel;
    jk_msg_t * msg=ae->reply;

    jk2_serialize_cping( env, msg, ae );
    err = ae->worker->channel->send( env, ae->worker->channel, ae, msg );

    if (err != JK_OK) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp13.checkalive() can't send cping request to %s\n", 
                      ae->worker->mbean->name);

        return JK_ERR;
    }
    
    if (ae->worker->channel->hasinput(env, ae->worker->channel, ae, 
                                      timeout) != JK_TRUE) {
                                      
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp13.checkalive() can't get cpong reply from %s in %d ms\n", 
                      ae->worker->mbean->name, timeout);

        return JK_ERR;
    }
    
    err = ae->worker->channel->recv( env, ae->worker->channel, ae, msg );

    if (err != JK_OK) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp13.checkalive() can't read cpong reply from %s\n", 
                      ae->worker->mbean->name);

        return JK_ERR;
    }
    
    return JK_OK;
}

/** Connect a channel, implementing the logging protocol if ajp13
 */
static int jk2_worker_ajp13_connect(jk_env_t *env, jk_endpoint_t *ae) {
    jk_channel_t *channel=ae->worker->channel;
    jk_msg_t *msg;
    
    int err;

    if( channel==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp13.connect() no channel %s\n", ae->worker->mbean->name );
        return JK_ERR;
    }

    if( ae->worker->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "ajp13.connect() %s %s\n", ae->worker->channelName, channel->mbean->name );
    
    err=channel->open( env, channel, ae );

    if( err != JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp13.connect() failed %s\n", ae->worker->mbean->name );
        return JK_ERR;
    }

    /* We just reconnected, reset error state
     */
    ae->worker->in_error_state=JK_FALSE;

    ae->stats->connectedTime=apr_time_now();
    
    /** XXX use a 'connected' field */
    if( ae->sd == -1 ) ae->sd=0;
    
    if (ae->worker->connect_timeout != 0 ) {
        if (jk2_check_alive(env, ae, ae->worker->connect_timeout) != JK_OK)
            return JK_ERR;
    }

    /* Check if we must execute a logon after the physical connect */
    if (ae->worker->secret == NULL)
        return JK_OK;

    /* Do the logon process */
    env->l->jkLog(env, env->l, JK_LOG_INFO, "ajp13.connect() logging in\n" );

    /* use the reply buffer - it's a new channel, it is certainly not
     in use. The request and post buffers are probably in use if this
    is a reconnect */
    msg=ae->reply;

    /* send a ping message to told container to take control (logon phase) */
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


static int JK_METHOD
jk2_worker_ajp13_forwardStream(jk_env_t *env, jk_worker_t *worker,
                              jk_ws_service_t *s,
                              jk_endpoint_t   *e )
{
    int err=JK_OK;
    int attempt;
    int has_post_body=JK_FALSE;
    jk_channel_t *channel= worker->channel;

    e->recoverable = JK_TRUE;
    s->is_recoverable_error = JK_TRUE;

    /*
     * Try to send the request on a valid endpoint. If one endpoint
     * fails, close the channel and try again ( maybe tomcat was restarted )
     * 
     * XXX JK_RETRIES could be replaced by the number of workers in
     * a load-balancing configuration 
     */
    for(attempt = 0 ; attempt < JK_RETRIES ;attempt++) {

        if( e->sd == -1 ) {
            err=jk2_worker_ajp13_connect(env, e);
            if( err!=JK_OK ) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "ajp13.service() failed to connect endpoint errno=%d %s\n",
                              errno, strerror( errno ));
                e->worker->in_error_state=JK_TRUE;
                return err;
            }
            if( worker->mbean->debug > 0 )
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "ajp13.service() connecting to endpoint \n");
        }

        err=e->worker->channel->send( env, e->worker->channel, e,
                                      e->request );

        if( e->worker->mbean->debug > 10 )
            e->request->dump( env, e->request, "Sent" );
        
        if (err!=JK_OK ) {
            /* Can't send - bad endpoint, try again */
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp13.service() error sending, reconnect %s %d %d %s\n",
                          e->worker->channelName, err, errno, strerror(errno));
            jk2_close_endpoint(env, e );
            continue;
        }

        /* We should have a channel now, send the post data */
        
        /* Prepare to send some post data ( ajp13 proto ). We do that after the
           request was sent ( we're receiving data from client, can be slow, no
           need to delay - we can do that in paralel. ( not very sure this is
           very usefull, and it brakes the protocol ) ! */

    /* || s->is_chunked - this can't be done here. The original protocol sends the first
       chunk of post data ( based on Content-Length ), and that's what the java side expects.
       Sending this data for chunked would break other ajp13 serers.

       Note that chunking will continue to work - using the normal read.
    */
        if (has_post_body  || s->left_bytes_to_send > 0 || s->reco_status == RECO_FILLED) {
            /* We never sent any POST data and we check it we have to send at
             * least of block of data (max 8k). These data will be kept in reply
             * for resend if the remote Tomcat is down, a fact we will learn only
             * doing a read (not yet) 
             */
            
            /* If we have the service recovery buffer FILLED and we're in first attempt */
            /* recopy the recovery buffer in post instead of reading it from client */
            if ( s->reco_status == RECO_FILLED && (attempt==0) ) {
		    	/* Get in post buf the previously saved POST */
		    	
		    	if (s->reco_buf->copy(env, s->reco_buf, e->post) < 0) {
		                s->is_recoverable_error = JK_FALSE;
		                env->l->jkLog(env, env->l, JK_LOG_ERROR,
		                              "ajp13.service() can't use the LB recovery buffer, aborting\n");
	                return JK_ERR;
		    	}		    	

                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "ajp13.service() using the LB recovery buffer\n");
            }
            else
            {
	            if( attempt==0 )
	                err=jk2_serialize_postHead( env, e->post, s, e );
	            else
	                err=JK_OK; /* We already have the initial body chunk */
	            
	            if( e->worker->mbean->debug > 10 )
	                e->request->dump( env, e->request, "Post head" );
	            
	            if (err != JK_OK ) {
	                /* the browser stop sending data, no need to recover */
	                /* e->recoverable = JK_FALSE; */
	                s->is_recoverable_error = JK_FALSE;
	                env->l->jkLog(env, env->l, JK_LOG_ERROR,
	                              "ajp13.service() Error receiving initial post %d %d %d\n", err, errno, attempt);
	                return JK_ERR;
	            }

				/* If a recovery buffer exist (LB mode), save here the post buf */
				if (s->reco_status == RECO_INITED) {
			    	/* Save the post for recovery if needed */
			    	if (e->post->copy(env, e->post, s->reco_buf) < 0) {
		                s->is_recoverable_error = JK_FALSE;
		                env->l->jkLog(env, env->l, JK_LOG_ERROR,
		                              "ajp13.service() can't save the LB recovery buffer, aborting\n");
		                return JK_ERR;
			    	}
			    	else	
			    		s->reco_status = RECO_FILLED;
				}
			}
			
            has_post_body=JK_TRUE;
            err= e->worker->channel->send( env, e->worker->channel, e,
                                           e->post );
            if( err != JK_OK ) {
                /* e->recoverable = JK_FALSE; */
                /*                 s->is_recoverable_error = JK_FALSE; */
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "ajp13.service() Error sending initial post %d %d %d\n", err, errno, attempt);
                jk2_close_endpoint(env, e);
                continue;
                /*  return JK_ERR; */
            }
        }
        
        err = e->worker->workerEnv->processCallbacks(env, e->worker->workerEnv,
                                                     e, s);
        
        /* if we can't get reply, check if no recover flag was set 
         * if is_recoverable_error is cleared, we have started received 
         * upload data and we must consider that operation is no more recoverable
         */
        if (err!=JK_OK && ! e->recoverable ) {
            s->is_recoverable_error = JK_FALSE;
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp13.service() ajpGetReply unrecoverable error %d\n",
                          err);
            /* The connection is compromised, need to close it ! */
            e->worker->in_error_state = 1;
            return JK_ERR;
        }
        
        if( err != JK_OK ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "ajp13.service() ajpGetReply recoverable error %d\n",
                          err);
            jk2_close_endpoint(env, e ); 
       }

        if( err==JK_OK )
            return err;
    }
    return err;
}

static int JK_METHOD
jk2_worker_ajp13_forwardSingleThread(jk_env_t *env, jk_worker_t *worker,
                                    jk_ws_service_t *s,
                                    jk_endpoint_t   *e )
{
    int err;

    if( e->worker->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "ajp13.forwardST() Before calling native channel %s\n",
                      e->worker->channel->mbean->name);
    err=e->worker->channel->send( env, e->worker->channel, e,
                                  e->request );
    if( e->worker->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "ajp13.forwardST() After %d\n",err);
    
    /* I assume no unrecoverable error can happen here - we're in a single thread,
       so things are simpler ( at least in this area ) */
    return err;
}
     
static int JK_METHOD
jk2_worker_ajp13_service1(jk_env_t *env, jk_worker_t *w,
                        jk_ws_service_t *s,
                        jk_endpoint_t   *e )
{
    int err;

    if( ( e== NULL ) || ( s == NULL ) ) {
    env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp13.service() NullPointerException\n");
    return JK_ERR;
    }
    
    if( w->channel==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp13.service() no channel defined, error in init\n", w->mbean->name);
        return JK_ERR;
    }
    
    e->currentRequest=s;

    /* XXX configurable ? */
    strncpy( e->stats->active, s->req_uri, 64);
    /* Be sure this is null terminated if it's a long url */
    e->stats->active[63] = '\0';
    
    /* Prepare the messages we'll use.*/ 
    e->request->reset( env, e->request );
    e->reply->reset( env, e->reply );
    e->post->reset( env, e->post );
    
    s->is_recoverable_error = JK_TRUE;
    /* Up to now, we can recover */
    e->recoverable = JK_TRUE;

    s->left_bytes_to_send = s->content_length;
    s->content_read=0;

    if (w->prepost_timeout != 0) {
        if (jk2_check_alive(env, e, e->worker->prepost_timeout) != JK_OK)
            return JK_ERR;
    }
    
    /* 
     * We get here initial request (in reqmsg)
     */
    err=jk2_serialize_request13(env, e->request, s, e);
    if (err!=JK_OK) {
    s->is_recoverable_error = JK_FALSE;                
    env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp13.service(): error marshaling\n");
    return JK_ERR;
    }

    if( w->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "ajp13.service() %s\n", w->channelName);

    if( w->channel->beforeRequest != NULL ) {
        w->channel->beforeRequest( env, w->channel, w, e, s );
    }
    
    /* First message for this request */
    if( w->channel->is_stream == JK_TRUE ) {
        err=jk2_worker_ajp13_forwardStream( env, w, s, e );
    } else {
        err=jk2_worker_ajp13_forwardSingleThread( env, w, s, e );
    }
    if (err != JK_OK){
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
              "ajp13.service() Error  forwarding %s %d %d\n", e->worker->mbean->name,
                      e->recoverable, e->worker->in_error_state);
    }

    if( w->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "ajp13.service() done %s\n", e->worker->mbean->name);

    if( w->channel->afterRequest != NULL ) {
        w->channel->afterRequest( env, w->channel, w, e, s );
    }

    e->currentRequest=NULL;
    
    return err;
}




static int JK_METHOD
jk2_worker_ajp13_done(jk_env_t *env, jk_worker_t *we, jk_endpoint_t *e)
{
    jk_worker_t *w;
    int rc=JK_OK;
    
    w= e->worker;
    
    if( e->cPool != NULL ) 
        e->cPool->reset(env, e->cPool);

    if( w->endpointCache == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "ajp13.done() No pool\n");
        return JK_ERR;
    }
    
    if(  w->in_error_state ) {
        jk2_close_endpoint(env, e);
        /*     if( w->mbean->debug > 0 )  */
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "ajp13.done() close endpoint %s error_state %d\n",
                      w->mbean->name, w->in_error_state );
    }
        
    rc=w->endpointCache->put( env, w->endpointCache, e );
    if( rc!=JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "ajp13.done() Error recycling ep\n");
        return rc;        
    }
    if( w->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "ajp13.done() return to pool %s\n",
                      w->mbean->name );
    return JK_OK;
}

static int JK_METHOD
jk2_worker_ajp13_getEndpoint(jk_env_t *env,
                            jk_worker_t *ajp13,
                            jk_endpoint_t **eP)
{
    jk_endpoint_t *e = NULL;
    jk_bean_t *jkb;
    int ret;
    
    if( ajp13->secret ==NULL ) {
    }

    if( ajp13->endpointCache == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "ajp13.getEndpoint() No pool\n");
        return JK_ERR;
    }

    e=ajp13->endpointCache->get( env, ajp13->endpointCache );

    if (e!=NULL) {
        *eP = e;
        return JK_OK;
    }

    if( ajp13->cs != NULL ) 
        ajp13->cs->lock( env, ajp13->cs );

    {
        if (ajp13->maxEndpoints && 
            ajp13->maxEndpoints <= ajp13->endpointMap->size(env, ajp13->endpointMap)) {
            /* The maximum number of connections is reached */
            ajp13->in_max_epcount = JK_TRUE;
            if( ajp13->cs != NULL ) 
                ajp13->cs->unLock( env, ajp13->cs );
            if( ajp13->mbean->debug > 0 )
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "ajp13.getEndpoint(): maximum %d endpoints for %s reached\n",
                              ajp13->maxEndpoints, 
                              ajp13->mbean->name);
            return JK_ERR;
        }
        ajp13->in_max_epcount = JK_FALSE;

        jkb=env->createBean2( env, ajp13->mbean->pool,  "endpoint", NULL );
        if( jkb==NULL ) {
            if( ajp13->cs != NULL ) 
                ajp13->cs->unLock( env, ajp13->cs );
            return JK_ERR;
        }
    
        e = (jk_endpoint_t *)jkb->object;
        e->worker = ajp13;
        e->sd=-1;

        ajp13->endpointMap->add( env, ajp13->endpointMap, jkb->localName, jkb );
                                 
        *eP = e;

        ret = ajp13->workerEnv->addEndpoint( env, ajp13->workerEnv, e );
    }
    if( ajp13->cs != NULL ) 
        ajp13->cs->unLock( env, ajp13->cs );
        
    if( ajp13->mbean->debug > 0 ) {
        if (ret==JK_OK)
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "ajp13.getEndpoint(): Created endpoint %s %s \n",
                      ajp13->mbean->name, jkb->name);
        else
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "ajp13.getEndpoint(): endpoint creation %s %s failed\n",
                      ajp13->mbean->name, jkb->name);
    }
    
    return ret;
}

/*
 * Serve the request, using AJP13/AJP13
 */
static int JK_METHOD
jk2_worker_ajp13_service(jk_env_t *env, jk_worker_t *w,
                        jk_ws_service_t *s)
{
    int err;
    jk_endpoint_t   *e;

    /* Get endpoint from the pool */
    err=jk2_worker_ajp13_getEndpoint( env, w, &e );
    if  (err!=JK_OK)
      return err;

    if ((w!=NULL) && (w->channel!=NULL) && (w->channel->status!=NULL)) {
        err = w->channel->status(env, w, w->channel); 
        if  (err!=JK_OK) {
            jk2_worker_ajp13_done( env, w, e);
            return err;
        }
    }

    if( s->uriEnv!=NULL && s->uriEnv->timing == JK_TRUE ) {
        e->stats->startTime=s->startTime;
        e->stats->jkStartTime=e->stats->connectedTime=apr_time_now();
        if(e->stats->startTime==0)
            e->stats->startTime=e->stats->jkStartTime;
    }
    e->stats->workerId=w->mbean->id;
    
    err=jk2_worker_ajp13_service1( env, w, s, e );

    /* One error doesn't mean the whole worker is in error state.
     */
    if( err==JK_OK ) {
        e->stats->reqCnt++;
    } else {
        e->stats->errCnt++;
    }

    if( s->uriEnv!=NULL && s->uriEnv->timing == JK_TRUE ) {
        apr_time_t reqTime;

        e->stats->endTime=apr_time_now();
        reqTime=e->stats->endTime - e->stats->startTime;
    
        e->stats->totalTime += reqTime;
        if( e->stats->maxTime < reqTime )
            e->stats->maxTime=reqTime;
    }
    jk2_worker_ajp13_done( env, w, e);
    return err;
}


static int JK_METHOD
jk2_worker_ajp13_init(jk_env_t *env, jk_bean_t *bean )
{
    jk_worker_t *ajp13=bean->object;
    int  rc;
    int size;
    int i;

    if(ajp13->channel != NULL &&
       ajp13->channel->mbean->debug > 0 ) {
        ajp13->mbean->debug = ajp13->channel->mbean->debug;
    }

    if(ajp13->channel != NULL &&
       ajp13->channel->mbean->disabled  )
        ajp13->mbean->disabled = JK_TRUE;
    
    ajp13->endpointCache=jk2_objCache_create( env, ajp13->mbean->pool  );
        
    if( ajp13->endpointCache == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp13.init(): error creating endpoint cache\n");
        return JK_ERR;
    }
    
    /* Will grow */
    rc=ajp13->endpointCache->init( env, ajp13->endpointCache, -1 );
    if( rc!= JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp13.init(): error creating endpoint cache\n");
        return JK_ERR;
    }

    if( ajp13->channel == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp13.init(): No channel %s\n", ajp13->mbean->localName);
        /* That's ok - it may be added later */
        /*         return JK_ERR; */
    }

    if( ajp13->route==NULL ) {
        /* Default - eventually the naming convention should become mandatory */
        ajp13->route=bean->localName;
    }
    
    /* Find the groups we are member on and add ourself in
     */
    size=ajp13->groups->size( env, ajp13->groups );
    if( size==0 ) {
        /* No explicit groups, it'll go to default lb */
        jk_worker_t *lb=ajp13->workerEnv->defaultWorker;
        
        lb->mbean->setAttribute(env, lb->mbean, "worker",
                                ajp13->mbean->name);
        if( ajp13->mbean->debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "ajp13.init(): Adding %s to default lb\n", ajp13->mbean->localName);
    } else {
        for( i=0; i<size; i++ ) {
            char *name= ajp13->groups->nameAt( env, ajp13->groups, i );
            jk_worker_t *lb;

            if( ajp13->mbean->debug > 0 )
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "ajp13.init(): Adding %s to %s\n",
                              ajp13->mbean->localName, name);
            if (strncmp(name, "lb:", 3) == 0) {
                lb= env->getByName( env, name );
                if( lb==NULL ) {
                    /* Create the lb group */
                    if( ajp13->mbean->debug > 0 ) 
                        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                                      "ajp13.init(): Automatically creating the group %s\n",
                                      name);
                    env->createBean( env, ajp13->workerEnv->mbean->pool, name );
                    lb= env->getByName( env, name );
                    if( lb==NULL ) {
                        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                                      "ajp13.init(): Failed to create %s\n", name);
                        return JK_ERR;
                    }
                }
            }
            else {
                lb= env->getByName2( env, "lb", name );
                if( lb==NULL ) {
                    /* Create the lb group */
                    if( ajp13->mbean->debug > 0 ) 
                        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                                      "ajp13.init(): Automatically creating the group %s\n",
                                      name);
                    env->createBean2( env, ajp13->workerEnv->mbean->pool, "lb", name );
                    lb= env->getByName2( env, "lb", name );
                    if( lb==NULL ) {
                        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                                      "ajp13.init(): Failed to create %s\n", name);
                        return JK_ERR;
                    }
                }
            }
            lb->mbean->setAttribute(env, lb->mbean, "worker",
                                    ajp13->mbean->name);
        }

    }
    
    /* XXX Find any 'group' property - find or create an lb for that
       and register it
    */
    return JK_OK;
}


static int JK_METHOD
jk2_worker_ajp13_destroy(jk_env_t *env, jk_bean_t *bean)
{
    jk_worker_t *ajp13=bean->object;
    int i;

    if( ajp13->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "ajp13.destroy()\n");
    
    if( ajp13->endpointCache != NULL ) {
        jk_endpoint_t *e;
        i=ajp13->endpointCache->count;
        while( ajp13->endpointCache->count > 0 ) {
            
            e= ajp13->endpointCache->get( env, ajp13->endpointCache );
            
            if( e==NULL ) {
                /* we finished all endpoints in the cache */
                break;
            }
            
            
            jk2_close_endpoint(env, e);
        }
        ajp13->endpointCache->destroy( env, ajp13->endpointCache );

        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "ajp13.destroy() closed %d cached endpoints\n",
                      i);
    }

    return JK_OK;
}

int JK_METHOD jk2_worker_ajp13_factory( jk_env_t *env, jk_pool_t *pool,
                                        jk_bean_t *result,
                                        const char *type, const char *name)
{
    jk_worker_t *w=(jk_worker_t *)pool->calloc(env, pool, sizeof(jk_worker_t));
    jk_bean_t *jkb;

    if (name == NULL || w == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp13.factory() NullPointerException\n");
        return JK_ERR;
    }

    jk2_map_default_create(env, &w->groups, pool);
    jk2_map_default_create(env, &w->endpointMap, pool);

    jkb=env->createBean2(env, pool,"threadMutex", NULL);
    if( jkb != NULL ) {
        w->cs=jkb->object;
        jkb->init(env, jkb );
    }
    
    w->endpointCache= NULL;

    w->channel= NULL;
    w->secret= NULL;

    w->lb_factor=1;
    w->graceful=0;
    w->service = jk2_worker_ajp13_service;

    result->setAttribute= jk2_worker_ajp13_setAttribute;
    result->getAttribute= jk2_worker_ajp13_getAttribute;
    result->init= jk2_worker_ajp13_init;
    result->destroy=jk2_worker_ajp13_destroy;

    result->getAttributeInfo=jk2_worker_ajp13_getAttributeInfo;
    result->multiValueInfo=jk2_worker_ajp13_multiValueInfo;
    result->setAttributeInfo=jk2_worker_ajp13_setAttributeInfo;

    result->object = w;
    w->mbean=result;

    w->workerEnv=env->getByName( env, "workerEnv" );
    w->workerEnv->addWorker( env, w->workerEnv, w );

    return JK_OK;
}
