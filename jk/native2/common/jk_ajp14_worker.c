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
#include "jk_ajp14.h"

int JK_METHOD jk_worker_ajp14_factory( jk_env_t *env, jk_pool_t *pool, void **result,
                                       const char *type, const char *name);

static int JK_METHOD
jk_worker_ajp14_service(jk_endpoint_t   *e, jk_ws_service_t *s,
                        jk_logger_t     *l, int *is_recoverable_error);

static int JK_METHOD
jk_worker_ajp14_validate(jk_worker_t *pThis, jk_map_t    *props,
                         jk_workerEnv_t *we, jk_logger_t *l );

static int JK_METHOD
jk_worker_ajp14_done(jk_endpoint_t **e, jk_logger_t    *l);

static int JK_METHOD
jk_worker_ajp14_getEndpoint(jk_worker_t *_this, jk_endpoint_t **e,
                            jk_logger_t    *l);

static int JK_METHOD
jk_worker_ajp14_init(jk_worker_t *_this, jk_map_t    *props, 
                     jk_workerEnv_t *we, jk_logger_t *l);

static int JK_METHOD
jk_worker_ajp14_destroy(jk_worker_t **pThis, jk_logger_t *l);


#define AJP_DEF_RETRY_ATTEMPTS    (2)
#define AJP14_PROTO 14
#define AJP13_PROTO 13

#define AJP13_DEF_HOST            	("localhost")
#define AJP13_DEF_PORT            	(8009)
#define AJP14_DEF_HOST         	("localhost")
#define AJP14_DEF_PORT         	(8011)

/* -------------------- Impl -------------------- */

int JK_METHOD jk_worker_ajp14_factory( jk_env_t *env, jk_pool_t *pool,
                                       void **result,
                                       const char *type, const char *name)
{
    jk_logger_t *l=env->logger;
    jk_worker_t *w=(jk_worker_t *)pool->calloc(pool, sizeof(jk_worker_t));

    if (name == NULL || w == NULL) {
        l->jkLog(l, JK_LOG_ERROR, "ajp14.factory() NullPointerException\n");
        return JK_FALSE;
    }
    w->pool = pool;
    w->name = (char *)pool->pstrdup(pool, name);
    
    w->proto= AJP14_PROTO;

    w->endpointCache= NULL;
    w->connect_retry_attempts= AJP_DEF_RETRY_ATTEMPTS;

    w->channel= NULL;
   
    w->validate= jk_worker_ajp14_validate;
    w->init= jk_worker_ajp14_init;
    w->get_endpoint= jk_worker_ajp14_getEndpoint;
    w->destroy=jk_worker_ajp14_destroy;

    w->logon= NULL; 

    *result = w;

    l->jkLog(l, JK_LOG_INFO, "ajp14.factory() Created %s %s\n",
             w->name, name);

    return JK_TRUE;
}

/*
 * Initialize the worker.
 */
static int JK_METHOD
jk_worker_ajp14_validate(jk_worker_t *_this,
                         jk_map_t    *props,
                         jk_workerEnv_t *we,
                         jk_logger_t *l)
{
    int    port;
    char * host;
    int err;
    jk_worker_t *p = _this;
    jk_worker_t *aw;
    char * secret_key;
    int proto=AJP14_PROTO;

    aw = _this;
    secret_key = map_getStrProp( props, "worker", aw->name, "secretkey", NULL );
    
    if ((!secret_key) || (!strlen(secret_key))) {
        l->jkLog(l, JK_LOG_INFO,
               "ajp14.validate() No secretkey, use AJP13\n");
        proto=AJP13_PROTO;
        aw->proto= AJP13_PROTO;
        aw->logon= NULL; 
    }
    
    if (proto == AJP13_PROTO) {
	port = AJP13_DEF_PORT;
	host = AJP13_DEF_HOST;
    } else if (proto == AJP14_PROTO) {
	port = AJP14_DEF_PORT;
	host = AJP14_DEF_HOST;
    } else {
	l->jkLog(l, JK_LOG_ERROR,
                 "ajp14.validate() unknown protocol %d\n",proto);
	return JK_FALSE;
    } 
    if( _this->channel == NULL ) {
	/* Create a default channel */
	jk_env_t *env= we->env;

	_this->channel=env->getInstance(env, _this->pool,"channel", "socket" );

	if( _this->channel == NULL ) {
	    l->jkLog(l, JK_LOG_ERROR, "Error creating socket channel\n");
	    return JK_FALSE;
	}
    }
    
    _this->channel->setProperty( _this->channel, "defaultPort", "8007" );

    err=_this->channel->init( _this->channel, props, p->name, _this, l );

    if( err != JK_TRUE ) {
	l->jkLog(l, JK_LOG_ERROR, "ajp14.validate(): resolve failed\n");
	return err;
    }

    l->jkLog(l, JK_LOG_INFO, "ajp14.validate() %s\n", _this->name);
    
    return JK_TRUE;
}

/*
 * Close the endpoint (clean buf and close socket)
 */
static void jk_close_endpoint(jk_endpoint_t *ae,
                              jk_logger_t    *l)
{
    l->jkLog(l, JK_LOG_INFO, "endpoint.close() %s\n", ae->worker->name);

    ae->reuse = JK_FALSE;
    ae->pool->reset( ae->pool );
    ae->pool->close( ae->pool );
    ae->cPool->reset( ae->cPool );
    ae->cPool->close( ae->cPool );
    ae->worker->channel->close( ae->worker->channel, ae );
}

/** Connect a channel, implementing the logging protocol if ajp14
 */
static int jk_worker_ajp14_connect(jk_endpoint_t *ae, jk_logger_t *l) {
    jk_channel_t *channel=ae->worker->channel;
    jk_msg_t *msg;
    
    int err=channel->open( channel, ae );

    if( err != JK_TRUE ) {
        l->jkLog(l, JK_LOG_ERROR,
                 "ajp14.connect() failed %s\n", ae->worker->name );
        return JK_FALSE;
    }

    /* Check if we must execute a logon after the physical connect */
    if (ae->worker->logon == NULL)
        return JK_TRUE;

    /* Do the logon process */
    l->jkLog(l, JK_LOG_INFO, "ajp14.connect() logging in\n" );

    /* use the reply buffer - it's a new channel, it is cetainly not
     in use. The request and post buffers are probably in use if this
    is a reconnect */
    msg=ae->reply;

    jk_serialize_ping( msg, ae );
    
    err = msg->send( msg, ae );

    /* Move to 'slave' mode, listening to messages */
    err=ae->worker->workerEnv->processCallbacks( ae->worker->workerEnv,
                                                 ae, NULL);

    /* Anything but OK - the login failed
     */
    if( err != JK_TRUE ) {
        jk_close_endpoint( ae, l );
    }
    return err;
}


/*
 * Serve the request, using AJP13/AJP14
 */
static int JK_METHOD
jk_worker_ajp14_service(jk_endpoint_t   *e, 
                        jk_ws_service_t *s,
                        jk_logger_t     *l,
                        int  *is_recoverable_error)
{
    int err;
    int attempt;

    if( ( e== NULL ) 
	|| ( s == NULL )
        || ! is_recoverable_error ) {
	l->jkLog(l, JK_LOG_ERROR, "ajp14.service() NullPointerException\n");
	return JK_FALSE;
    }

    /* Prepare the messages we'll use.*/ 
    e->request->reset( e->request );
    e->reply->reset( e->reply );
    e->post->reset( e->post );
    
    e->recoverable = JK_TRUE;
    e->uploadfd	 = -1;		/* not yet used, later ;) */
    e->reuse = JK_FALSE;
    *is_recoverable_error = JK_TRUE;
    /* Up to now, we can recover */
    e->recoverable = JK_TRUE;

    s->left_bytes_to_send = s->content_length;
    s->content_read=0;

    /* 
     * We get here initial request (in reqmsg)
     */
    err=jk_serialize_request13(e->request, s);
    if (err!=JK_TRUE) {
	*is_recoverable_error = JK_FALSE;                
	l->jkLog(l, JK_LOG_ERROR, "ajp14.service(): error marshaling\n");
	return JK_FALSE;
    }

    /* Prepare to send some post data ( ajp13 proto ) */
    if (s->is_chunked || s->left_bytes_to_send > 0) {
        /* We never sent any POST data and we check it we have to send at
	 * least of block of data (max 8k). These data will be kept in reply
	 * for resend if the remote Tomcat is down, a fact we will learn only
	 * doing a read (not yet) 
	 */
        err=jk_serialize_postHead( e->post, s, e );

        if (err != JK_TRUE ) {
            /* the browser stop sending data, no need to recover */
            e->recoverable = JK_FALSE;
            l->jkLog(l, JK_LOG_ERROR,
                     "ajp14.service() Error receiving initial post data\n");
            return JK_FALSE;
        }
    }
        
    l->jkLog(l, JK_LOG_DEBUG, "ajp14.service() %s\n", e->worker->name);

    /*
     * Try to send the request on a valid endpoint. If one endpoint
     * fails, close the channel and try again ( maybe tomcat was restarted )
     * 
     * XXX JK_RETRIES could be replaced by the number of workers in
     * a load-balancing configuration 
     */
    for(attempt = 0 ; attempt < e->worker->connect_retry_attempts ; attempt++) {
        jk_channel_t *channel=e->worker->channel;

        err=e->request->send( e->request, e);

	if (err==JK_TRUE ) {
            /* We sent the request, have valid endpoint */
            break;
        }

        if( e->recoverable != JK_TRUE ) {
            l->jkLog(l, JK_LOG_ERROR,
                     "ajp14.service() error sending request %s, giving up\n",
                     e->worker->name);
            return JK_FALSE;
        }
        
        l->jkLog(l, JK_LOG_ERROR,
                 "ajp14.service() error sending, retry on a new endpoint %s\n",
                 e->worker->name);

        channel->close( channel, e );

        err=jk_worker_ajp14_connect(e, l); 

        if( err != JK_TRUE ) {
            l->jkLog(l, JK_LOG_ERROR,
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
    
    /* We should have a channel now, send the post data */
    *is_recoverable_error = JK_TRUE;
    e->recoverable = JK_TRUE;
    
    err= e->post->send( e->post, e );

    err = e->worker->workerEnv->processCallbacks(e->worker->workerEnv,
                                                e, s);
    
    /* if we can't get reply, check if no recover flag was set 
     * if is_recoverable_error is cleared, we have started received 
     * upload data and we must consider that operation is no more recoverable
     */
    if (! e->recoverable) {
        *is_recoverable_error = JK_FALSE;
        l->jkLog(l, JK_LOG_ERROR,
                 "ajp14.service() ajpGetReply unrecoverable error %d\n",
                 err);
        return JK_FALSE;
    }
    
    l->jkLog(l, JK_LOG_ERROR,
             "ajp14.service() ajpGetReply recoverable error %d\n", err);

    return err;
}


static int JK_METHOD
jk_worker_ajp14_done(jk_endpoint_t **eP,jk_logger_t    *l)
{
    jk_endpoint_t *e;
    jk_worker_t *w;
    
    if (eP==NULL || *eP==NULL ) {
        l->jkLog(l, JK_LOG_ERROR, "ajp14.done() NullPointerException\n");
        return JK_FALSE;
    }

    e = *eP;

    w= e->worker;

    if( e->cPool != NULL ) 
        e->cPool->reset(e->cPool);

    if (w->endpointCache != NULL ) {
        int err=0;
        
        err=w->endpointCache->put( w->endpointCache, e );
        if( err==JK_TRUE ) {
            l->jkLog(l, JK_LOG_INFO, "ajp14.done() return to pool %s\n",
                     w->name );
            return JK_TRUE;
        }
    }

    /* No reuse or put() failed */

    jk_close_endpoint(e, l);
    *eP = NULL;
    l->jkLog(l, JK_LOG_INFO, "ajp14.done() close endpoint %s\n",
             w->name );
    
    return JK_TRUE;
}

static int JK_METHOD
jk_worker_ajp14_getEndpoint(jk_worker_t *_this,
                            jk_endpoint_t **e,
                            jk_logger_t    *l)
{
    jk_endpoint_t *ae = NULL;
    jk_pool_t *endpointPool;
    
    if( _this->secret ==NULL ) {
    }

    if (_this->endpointCache != NULL ) {

        ae=_this->endpointCache->get( _this->endpointCache );

        if (ae!=NULL) {
            l->jkLog(l, JK_LOG_INFO, "ajp14.getEndpoint(): Reusing endpoint\n");
            *e = ae;
            return JK_TRUE;
        }
    }

    endpointPool = _this->pool->create( _this->pool, HUGE_POOL_SIZE );
    
    ae = (jk_endpoint_t *)endpointPool->alloc(endpointPool,
                                              sizeof(jk_endpoint_t));
    if (ae==NULL) {
        l->jkLog(l, JK_LOG_ERROR, "ajp14.get_endpoint OutOfMemoryException\n");
        return JK_FALSE;
    }

    ae->pool = endpointPool;

    /* Init message storage areas.
     */
    ae->request = jk_msg_ajp_create( ae->pool, l, 0);
    ae->reply = jk_msg_ajp_create( ae->pool, l, 0);
    ae->post = jk_msg_ajp_create( ae->pool, l, 0);
    
    ae->reuse = JK_FALSE;

    ae->cPool=endpointPool->create( endpointPool, HUGE_POOL_SIZE );

    ae->worker = _this;
    ae->proto = _this->proto;
    ae->channelData = NULL;
    ae->service = jk_worker_ajp14_service;
    ae->done = jk_worker_ajp14_done;
    
    *e = ae;
    return JK_TRUE;
}

static int JK_METHOD
jk_worker_ajp14_init(jk_worker_t *_this,
                     jk_map_t    *props, 
                     jk_workerEnv_t *we,
                     jk_logger_t *l)
{
    jk_endpoint_t *ae;
    int             rc;
    char * secret_key;
    int proto=AJP14_PROTO;
    int cache_sz;

    secret_key = map_getStrProp( props, "worker", _this->name,
                                 "secretkey", NULL );
    
    if( secret_key==NULL ) {
        proto=AJP13_PROTO;
        _this->proto= AJP13_PROTO;
        _this->logon= NULL; 
    } else {
        /* Set Secret Key (used at logon time) */	
        _this->secret = strdup(secret_key);
    }


    /* start the connection cache */
    cache_sz = map_getIntProp( props, "worker", _this->name, "cachesize",
                               JK_OBJCACHE_DEFAULT_SZ );

    if (cache_sz > 0) {
        int err;
        _this->endpointCache=jk_objCache_create( _this->pool, l );

        if( _this->endpointCache != NULL ) {
            err=_this->endpointCache->init( _this->endpointCache, cache_sz );
            if( err!= JK_TRUE ) {
                _this->endpointCache=NULL;
            }
        }
    } else {
        _this->endpointCache=NULL;
    }

    /* Set WebServerName (used at logon time) */
    if( we->server_name == NULL )
        we->server_name = _this->pool->pstrdup(we->pool,we->server_name);

    if (_this->secret == NULL) {
        /* No extra initialization for AJP13 */
        l->jkLog(l, JK_LOG_INFO, "ajp14.init() ajp13 mode %s\n",_this->name);
        return JK_TRUE;
    }

    /* -------------------- Ajp14 discovery -------------------- */

    /* (try to) connect with the worker and get any information
     *  If the worker is down, we'll use the cached data and update
     *  at the first connection
     *
     *  Tomcat can triger a 'ping' at startup to force us to
     *  update.
     */
    
    if (jk_worker_ajp14_getEndpoint(_this, &ae, l) == JK_FALSE) 
        return JK_FALSE; 
    
    rc=jk_worker_ajp14_connect(ae, l);

    
    if ( rc == JK_TRUE) {
        l->jkLog( l, JK_LOG_INFO, "ajp14.init() %s connected ok\n",
                  _this->name );
    } else {
        l->jkLog(l, JK_LOG_INFO, "ajp14.init() delayed connection %s\n",
                 _this->name);
    }
    return JK_TRUE;
}


static int JK_METHOD
jk_worker_ajp14_destroy(jk_worker_t **pThis, jk_logger_t *l)
{
    jk_worker_t *aw = *pThis;
    int i;
    
    l->jkLog(l, JK_LOG_DEBUG, "ajp14.destroy()\n");

    if( aw->endpointCache != NULL ) {
        
        for( i=0; i< aw->endpointCache->ep_cache_sz; i++ ) {
            jk_endpoint_t *e;
            
            e= aw->endpointCache->get( aw->endpointCache );
            
            if( e==NULL ) {
                // we finished all endpoints in the cache
                break;
            }
            
            jk_close_endpoint(e, l);
        }
        aw->endpointCache->destroy( aw->endpointCache );

        l->jkLog(l, JK_LOG_DEBUG, "ajp14.destroy() closed %d cached endpoints\n",
                 i);
    }

    return JK_TRUE;
}
