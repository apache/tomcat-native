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
#include "jk_msg_buff.h"
#include "jk_ajp14.h" 
#include "jk_logger.h"
#include "jk_service.h"
#include "jk_env.h"

int JK_METHOD jk_worker_ajp14_factory( jk_env_t *env, void **result,
                                       char *type, char *name);

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


#define	JK_RETRIES 3

/* -------------------- Impl -------------------- */

int JK_METHOD jk_worker_ajp14_factory( jk_env_t *env, void **result,
                                       char *type, char *name)
{
    jk_logger_t *l=env->logger;
    jk_worker_t *w=(jk_worker_t *)malloc(sizeof(jk_worker_t));
   
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14_worker_factory\n");

    if (name == NULL || w == NULL) {
        l->jkLog(l, JK_LOG_ERROR, "In ajp14_worker_factory, NULL parameters\n");
        return JK_FALSE;
    }

    w->name = strdup(name);
    
    w->proto= AJP14_PROTO;

    w->login= (jk_login_service_t *)malloc(sizeof(jk_login_service_t));

    if (w->login == NULL) {
        l->jkLog(l, JK_LOG_ERROR,
               "In ajp14_worker_factory, malloc failed for login area\n");
        return JK_FALSE;
    }
	
    memset(w->login, 0, sizeof(jk_login_service_t));
    
    w->login->negociation=
        (AJP14_CONTEXT_INFO_NEG | AJP14_PROTO_SUPPORT_AJP14_NEG);
    w->login->web_server_name=NULL; /* must be set in init */
    
    w->ep_cache_sz= 0;
    w->ep_cache= NULL;
    w->connect_retry_attempts= AJP_DEF_RETRY_ATTEMPTS;

    w->channel= NULL;
   
    w->validate= jk_worker_ajp14_validate;
    w->init= jk_worker_ajp14_init;
    w->get_endpoint= jk_worker_ajp14_getEndpoint;
    w->destroy=jk_worker_ajp14_destroy;

    w->logon= NULL; 

    *result = w;

    return JK_TRUE;
}

/*
 * service is now splitted in ajp_send_request and ajp_get_reply
 * much more easier to do errors recovery
 *
 * We serve here the request, using AJP13/AJP14 (e->proto)
 *
 */
static int JK_METHOD
jk_worker_ajp14_service(jk_endpoint_t   *e, 
                        jk_ws_service_t *s,
                        jk_logger_t     *l,
                        int  *is_recoverable_error)
{
    int i;
    int err;
    
    l->jkLog(l, JK_LOG_DEBUG, "Into jk_endpoint_t::service\n");

    if( ( e== NULL ) 
	|| ( s == NULL )
        || ! is_recoverable_error ) {
	l->jkLog(l, JK_LOG_ERROR, "jk_endpoint_t::service: NULL parameters\n");
	return JK_FALSE;
    }
	
    e->request = jk_b_new(&(e->pool));
    jk_b_set_buffer_size(e->request, DEF_BUFFER_SZ); 
    jk_b_reset(e->request);
    
    e->reply = jk_b_new(&(e->pool));
    jk_b_set_buffer_size(e->reply, DEF_BUFFER_SZ);
    jk_b_reset(e->reply); 
	
    e->post = jk_b_new(&(e->pool));
    jk_b_set_buffer_size(e->post, DEF_BUFFER_SZ);
    jk_b_reset(e->post); 
    
    e->recoverable = JK_TRUE;
    e->uploadfd	 = -1;		/* not yet used, later ;) */
    
    e->left_bytes_to_send = s->content_length;
    e->reuse = JK_FALSE;
    *is_recoverable_error = JK_TRUE;
    
    /* 
     * We get here initial request (in reqmsg)
     */
    if (!jk_handler_request_marshal(e->request, s, l, e)) {
	*is_recoverable_error = JK_FALSE;                
	l->jkLog(l, JK_LOG_ERROR, "jk_endpoint_t::service: error marshaling\n");
	return JK_FALSE;
    }
    
    /* 
     * JK_RETRIES could be replaced by the number of workers in
     * a load-balancing configuration 
     */
    for (i = 0; i < JK_RETRIES; i++) {
	/*
	 * We're using reqmsg which hold initial request
	 * if Tomcat is stopped or restarted, we will pass reqmsg
	 * to next valid tomcat. 
	 */
	err=ajp_send_request(e, s, l);
	if (err!=JK_TRUE ) {
	    l->jkLog(l, JK_LOG_ERROR, 
		   "In jk_endpoint_t::service, ajp_send_request"
		   "failed in send loop %d\n", i);
	} else {
	    /* If we have the no recoverable error, it's probably because the sender (browser)
	     * stop sending data before the end (certainly in a big post)
	     */
	    if (! e->recoverable) {
		*is_recoverable_error = JK_FALSE;
		l->jkLog(l, JK_LOG_ERROR, "In jk_endpoint_t::service,"
		       "ajp_send_request failed without recovery in send loop %d\n", i);
		return JK_FALSE;
	    }
	    
	    /* Up to there we can recover */
	    *is_recoverable_error = JK_TRUE;
	    e->recoverable = JK_TRUE;
	    
	    if (ajp_get_reply(e, s, l))
		return (JK_TRUE);
	    
	    /* if we can't get reply, check if no recover flag was set 
	     * if is_recoverable_error is cleared, we have started received 
	     * upload data and we must consider that operation is no more recoverable
	     */
	    if (! e->recoverable) {
		*is_recoverable_error = JK_FALSE;
		l->jkLog(l, JK_LOG_ERROR, "In jk_endpoint_t::service,"
		       "ajp_get_reply failed without recovery in send loop %d\n", i);
		return JK_FALSE;
	    }
	    
	    l->jkLog(l, JK_LOG_ERROR, "In jk_endpoint_t::service,"
		   "ajp_get_reply failed in send loop %d\n", i);
	}
	/* Try again to connect */
	{
	    jk_channel_t *channel=e->worker->channel;
	    
	    l->jkLog(l, JK_LOG_ERROR, "Error sending request, reconnect\n");
	    err=channel->close( channel, e );
	    err=channel->open( channel, e );
	    if( err != JK_TRUE ) {
		l->jkLog(l, JK_LOG_ERROR, "Reconnect failed\n");
		return err;
	    }
	}
    }
    
    return JK_FALSE;
}

/*
 * Validate the worker (ajp13/ajp14)
 */
static int JK_METHOD
jk_worker_ajp14_validate(jk_worker_t *pThis,
                         jk_map_t    *props,
                         jk_workerEnv_t *we,
                         jk_logger_t *l)
{
    int    port;
    char * host;
    int err;
    jk_worker_t *p = pThis;
    jk_worker_t *aw;
    char * secret_key;
    int proto=AJP14_PROTO;

    aw = pThis;
    secret_key = map_getStrProp( props, "worker", aw->name, "secretkey", NULL );
    
    if ((!secret_key) || (!strlen(secret_key))) {
        l->jkLog(l, JK_LOG_ERROR,
               "No secretkey, defaulting to unauthenticated AJP13\n");
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
    if( pThis->channel == NULL ) {
	/* Create a default channel */
	jk_env_t *env= we->env;

	jk_env_objectFactory_t fac = 
	    (jk_env_objectFactory_t)env->getFactory(env, "channel", "socket" );
	l->jkLog( l, JK_LOG_DEBUG, "Got socket channel factory \n");

	err=fac( env, (void **)&pThis->channel, "channel", "socket" );
	if( err != JK_TRUE ) {
	    l->jkLog(l, JK_LOG_ERROR, "Error creating socket factory\n");
	    return err;
	}
	l->jkLog(l, JK_LOG_ERROR, "Got channel %lx %lx\n", pThis, pThis->channel);
    }
    
    pThis->channel->setProperty( pThis->channel, "defaultPort", "8007" );

    err=pThis->channel->init( pThis->channel, props, p->name, pThis, l );
    if( err != JK_TRUE ) {
	l->jkLog(l, JK_LOG_ERROR, "ajp14.validate(): resolve failed\n");
	return err;
    }
    
    return JK_TRUE;
}


static int JK_METHOD
jk_worker_ajp14_done(jk_endpoint_t **e,jk_logger_t    *l)
{
    l->jkLog(l, JK_LOG_DEBUG, "Into jk_endpoint_t::done\n");

    if (e && *e ) {
        jk_endpoint_t *p = *e;
        int reuse_ep = p->reuse;

        ajp_reset_endpoint(p);

        if(reuse_ep) {
            jk_worker_t *w = p->worker;
            if(w->ep_cache_sz) {
                int rc;
                JK_ENTER_CS(&w->cs, rc);
                if(rc) {
                    int i;

                    for(i = 0 ; i < w->ep_cache_sz ; i++) {
                        if(!w->ep_cache[i]) {
                            w->ep_cache[i] = p;
                            break;
                        }
                    }
                    JK_LEAVE_CS(&w->cs, rc);
                    if(i < w->ep_cache_sz) {
			l->jkLog(l, JK_LOG_DEBUG, "Return endpoint to pool\n");
                        return JK_TRUE;
                    }
                }
            }
        }

        ajp_close_endpoint(p, l);
        *e = NULL;

        return JK_TRUE;
    }

    l->jkLog(l, JK_LOG_ERROR, "In jk_endpoint_t::done, NULL parameters\n");
    return JK_FALSE;
}

static int JK_METHOD
jk_worker_ajp14_getEndpoint(jk_worker_t *_this,
                            jk_endpoint_t **e,
                            jk_logger_t    *l)
{
    jk_endpoint_t *ae = NULL;

    if( _this->login->secret_key ==NULL ) {
    }

    l->jkLog(l, JK_LOG_DEBUG, "ajp14.get_endpoint() %lx %lx\n", _this, _this->channel );

    if (_this->ep_cache_sz) {
        int rc;
        JK_ENTER_CS(&_this->cs, rc);
        if (rc) {
            int i;
            
            for (i = 0 ; i < _this->ep_cache_sz ; i++) {
                if (_this->ep_cache[i]) {
                    ae = _this->ep_cache[i];
                    _this->ep_cache[i] = NULL;
                    break;
                }
            }
            JK_LEAVE_CS(&_this->cs, rc);
            if (ae) {
                l->jkLog(l, JK_LOG_DEBUG, "Reusing endpoint\n");
                *e = ae;
                return JK_TRUE;
            }
        }
    }

    ae = (jk_endpoint_t *)malloc(sizeof(jk_endpoint_t));
    if (!ae) {
        l->jkLog(l, JK_LOG_ERROR, "ajp14.get_endpoint OutOfMemoryException\n");
        return JK_FALSE;
    }
    
    ae->reuse = JK_FALSE;
    jk_open_pool(&ae->pool, ae->buf, sizeof(ae->buf));
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

    secret_key = map_getStrProp( props, "worker", _this->name, "secretkey", NULL );
    
    if( secret_key==NULL ) {
        proto=AJP13_PROTO;
        _this->proto= AJP13_PROTO;
        _this->logon= NULL; 
    } else {
        /* Set Secret Key (used at logon time) */	
        _this->login->secret_key = strdup(secret_key);
    }

    l->jkLog(l, JK_LOG_DEBUG, "ajp14.init()\n");

    /* start the connection cache */
    cache_sz = map_getIntProp( props, "worker", _this->name, "cachesize",
                               AJP13_DEF_CACHE_SZ );
    if (cache_sz > 0) {
        _this->ep_cache =
            (jk_endpoint_t **)malloc(sizeof(jk_endpoint_t *) * cache_sz);
        
        if(_this->ep_cache) {
            int i;
            _this->ep_cache_sz = cache_sz;
            for(i = 0 ; i < cache_sz ; i++) {
                _this->ep_cache[i] = NULL;
            }
            JK_INIT_CS(&(_this->cs), i);
            if (!i) {
                return JK_FALSE;
            }
        }
    }
    
    if (_this->login->secret_key == NULL) {
        /* No extra initialization for AJP13 */
        return JK_TRUE;
    }

    /* -------------------- Ajp14 discovery -------------------- */
    
    /* Set WebServerName (used at logon time) */
    _this->login->web_server_name = strdup(we->server_name);
    
    if (_this->login->web_server_name == NULL) {
        l->jkLog(l, JK_LOG_ERROR, "can't malloc web_server_name\n");
        return JK_FALSE;
    }
    
/*     if (get_endpoint(_this, &ae, l) == JK_FALSE) */
/*         return JK_FALSE; */

    /* Temporary commented out. Will be added back after fixing
       few more things ( like what happens if apache is started before tomcat).
     */
/*     if (ajp_connect_to_endpoint(ae, l) == JK_TRUE) { */
        
 	/* connection stage passed - try to get context info 
 	 * this is the long awaited autoconf feature :) 
 	 */ 
/*         rc = discovery(ae, we, l); */
/*         ajp_close_endpoint(ae, l); */
/*         return rc; */
/*     } */
    
    return JK_TRUE;
}


static int JK_METHOD
jk_worker_ajp14_destroy(jk_worker_t **pThis, jk_logger_t *l)
{
    jk_worker_t *aw = *pThis;
    
    if (aw->login != NULL &&
        aw->login->secret_key != NULL ) {
        /* Ajp14-specific initialization */
        if (aw->login->web_server_name) {
            free(aw->login->web_server_name);
            aw->login->web_server_name = NULL;
        }
        
        if (aw->login->secret_key) {
            free(aw->login->secret_key);
            aw->login->secret_key = NULL;
        }
        
        free(aw->login);
        aw->login = NULL;
    }


    l->jkLog(l, JK_LOG_DEBUG, "ajp14.destroy()\n");

    free(aw->name);
        
    l->jkLog(l, JK_LOG_DEBUG, "ajp14.destroy() %d endpoints to close\n",
             aw->ep_cache_sz);

    if(aw->ep_cache_sz > 0 ) {
        int i;
        for(i = 0 ; i < aw->ep_cache_sz ; i++) {
            if(aw->ep_cache[i]) {
                ajp_close_endpoint(aw->ep_cache[i], l);
            }
        }
        free(aw->ep_cache);
        JK_DELETE_CS(&(aw->cs), i);
    }

    if (aw->login) {
        free(aw->login);
        aw->login = NULL;
    }

    free(aw);
    return JK_TRUE;
}
