/*
 * Copyright (c) 1997-2001 The Java Apache Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * 4. The names "Apache JServ", "Apache JServ Servlet Engine" and 
 *    "Java Apache Project" must not be used to endorse or promote products 
 *    derived from this software without prior written permission.
 *
 * 5. Products derived from this software may not be called "Apache JServ"
 *    nor may "Apache" nor "Apache JServ" appear in their names without 
 *    prior written permission of the Java Apache Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *    
 * THIS SOFTWARE IS PROVIDED BY THE JAVA APACHE PROJECT "AS IS" AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE JAVA APACHE PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Java Apache Group. For more information
 * on the Java Apache Project and the Apache JServ Servlet Engine project,
 * please see <http://java.apache.org/>.
 *
 */

/***************************************************************************
 * Description: Bi-directional protocol.                                   *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Author:      Costin <costin@costin.dnt.ro>                              *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_connect.h"
#include "jk_util.h"
#include "jk_msg_buff.h"
#include "jk_ajp13.h"
#include "jk_mt.h"

#define AJP_DEF_HOST            ("localhost")
#define AJP_DEF_PORT            (8008)
#define READ_BUF_SIZE           (8*1024)
#define DEF_RETRY_ATTEMPTS      (1)
#define DEF_CACHE_SZ            (1)
#define JK_INTERNAL_ERROR       (-2)
#define MAX_SEND_BODY_SZ        (DEF_BUFFER_SZ - 6)
#define AJP13_HEADER_LEN	(4)
#define AJP13_HEADER_SZ_LEN	(2)

struct ajp13_operation;
typedef struct ajp13_operation ajp13_operation_t;

struct ajp13_endpoint;
typedef struct ajp13_endpoint ajp13_endpoint_t;

struct ajp13_worker {
    struct sockaddr_in worker_inet_addr; /* Contains host and port */
    unsigned connect_retry_attempts;
    char *name; 

    /*
     * Open connections cache...
     *
     * 1. Critical section object to protect the cache.
     * 2. Cache size.
     * 3. An array of "open" endpoints.
     */
    JK_CRIT_SEC cs;
    unsigned ep_cache_sz;
    ajp13_endpoint_t **ep_cache;

    jk_worker_t worker; 
};
typedef struct ajp13_worker ajp13_worker_t;

struct ajp13_endpoint { 
    ajp13_worker_t *worker;

    jk_pool_t pool;
    jk_pool_atom_t buf[BIG_POOL_SIZE];

    int sd;
    int reuse;
    jk_endpoint_t endpoint;

    unsigned left_bytes_to_send;
};

/* 
 * little struct to avoid multiples ptr passing
 * this struct is ready to hold upload file fd
 * to add upload persistant storage
 */
struct ajp13_operation {
	jk_msg_buf_t 	*request;	/* original request storage */
	jk_msg_buf_t	*reply;		/* reply storage (chuncked by ajp13 */
	int		uploadfd;	/* future persistant storage id */
	int		recoverable;	/* if exchange could be conducted on another TC */
};

static void reset_endpoint(ajp13_endpoint_t *ep)
{
    ep->reuse = JK_FALSE; 
    jk_reset_pool(&(ep->pool));
}

static void close_endpoint(ajp13_endpoint_t *ep)
{
    reset_endpoint(ep);
    if(ep->sd > 0) {
        jk_close_socket(ep->sd);
    } 
    free(ep);
}


static void reuse_connection(ajp13_endpoint_t *ep,
                             jk_logger_t *l)
{
    ajp13_worker_t *w = ep->worker;

    if(w->ep_cache_sz) {
        int rc;
        JK_ENTER_CS(&w->cs, rc);
        if(rc) {
            unsigned i;
                    
            for(i = 0 ; i < w->ep_cache_sz ; i++) {
                if(w->ep_cache[i]) {
                    ep->sd = w->ep_cache[i]->sd;
                    w->ep_cache[i]->sd = -1;
                    close_endpoint(w->ep_cache[i]);
                    w->ep_cache[i] = NULL;
                    break;
                 }
            }
            JK_LEAVE_CS(&w->cs, rc);
        }
    }
}

static void connect_to_tomcat(ajp13_endpoint_t *ep,
                              jk_logger_t *l)
{
    unsigned attempt;

    for(attempt = 0 ; attempt < ep->worker->connect_retry_attempts ; attempt++) {
        ep->sd = jk_open_socket(&ep->worker->worker_inet_addr, 
                               JK_TRUE, 
                               l);
        if(ep->sd >= 0) {
            jk_log(l, 
                   JK_LOG_DEBUG, 
                   "In jk_endpoint_t::connect_to_tomcat, connected sd = %d\n", ep->sd);
            return;
        }
    }    

    jk_log(l, 
           JK_LOG_ERROR, 
           "In jk_endpoint_t::connect_to_tomcat, failed errno = %d\n", errno);
}

static int connection_tcp_send_message(ajp13_endpoint_t *ep, 
                                       jk_msg_buf_t *msg, 
                                       jk_logger_t *l ) 
{
    jk_b_end(msg);
    
    jk_dump_buff(l, JK_LOG_DEBUG, "sending to ajp13", msg);

    if(0 > jk_tcp_socket_sendfull(ep->sd, 
                                  jk_b_get_buff(msg),
                                  jk_b_get_len(msg))) {
        return JK_FALSE;
    }

    return JK_TRUE;
}

static int connection_tcp_get_message(ajp13_endpoint_t *ep, 
                                      jk_msg_buf_t *msg, 
                                      jk_logger_t *l) 
{
    unsigned char head[AJP13_HEADER_LEN];
    int rc;
    int msglen;

    rc = jk_tcp_socket_recvfull(ep->sd, head, AJP13_HEADER_LEN);

    if(rc < 0) {
        jk_log(l, JK_LOG_ERROR, 
               "connection_tcp_get_message: Error - jk_tcp_socket_recvfull failed\n");
        return JK_FALSE;
    }
    
    if((head[0] != 'A') || (head[1] != 'B' )) {
        jk_log(l, JK_LOG_ERROR, 
               "connection_tcp_get_message: Error - Wrong message format\n");
	    return JK_FALSE;
    }

    msglen  = ((head[2]&0xff)<<8);
    msglen += (head[3] & 0xFF);

    if(msglen > jk_b_get_size(msg)) {
        jk_log(l, JK_LOG_ERROR, 
               "connection_tcp_get_message: Error - Wrong message size\n");
	    return JK_FALSE;
    }
    
    jk_b_set_len(msg, msglen);
    jk_b_set_pos(msg, 0); 

    rc = jk_tcp_socket_recvfull(ep->sd, jk_b_get_buff(msg), msglen);
    if(rc < 0) {
        jk_log(l, JK_LOG_ERROR, 
               "connection_tcp_get_message: Error - jk_tcp_socket_recvfull failed\n");
        return JK_FALSE;
    }
        
    jk_dump_buff(l, JK_LOG_DEBUG, "received from ajp13", msg);
    return JK_TRUE;
}

static int read_fully_from_server(jk_ws_service_t *s, 
                                  unsigned char *buf, 
                                  unsigned len)
{
    unsigned rdlen = 0;

    while(rdlen < len) {
        unsigned this_time = 0;
        if(!s->read(s, buf + rdlen, len - rdlen, &this_time)) {
            return -1;
        }

        if(0 == this_time) {
            break;
        }
	    rdlen += this_time;
    }

    return (int)rdlen;
}

static int read_into_msg_buff(ajp13_endpoint_t *ep,
                              jk_ws_service_t *r, 
                              jk_msg_buf_t *msg, 
                              jk_logger_t *l,
                              unsigned len)
{
    unsigned char *read_buf = jk_b_get_buff(msg);                                                                                       

    jk_b_reset(msg);
    
    read_buf += AJP13_HEADER_LEN;    /* leave some space for the buffer headers */
    read_buf += AJP13_HEADER_SZ_LEN; /* leave some space for the read length */

    if(read_fully_from_server(r, read_buf, len) < 0) {
        jk_log(l, JK_LOG_ERROR, 
               "read_into_msg_buff: Error - read_fully_from_server failed\n");
        return JK_FALSE;                        
    } 
    
    ep->left_bytes_to_send -= len;

    if(0 != jk_b_append_int(msg, (unsigned short)len)) {
        jk_log(l, JK_LOG_ERROR, 
               "read_into_msg_buff: Error - jk_b_append_int failed\n");
        return JK_FALSE;
    }

    jk_b_set_len(msg, jk_b_get_len(msg) + len);
    
    return JK_TRUE;
}

static int ajp13_process_callback(jk_msg_buf_t *msg, 
                                  ajp13_endpoint_t *ep,
                                  jk_ws_service_t *r, 
                                  jk_logger_t *l) 
{
    int code = (int)jk_b_get_byte(msg);

    switch(code) {
        case JK_AJP13_SEND_HEADERS:
            {
                jk_res_data_t res;
                if(!ajp13_unmarshal_response(msg,
                                             &res,
                                             &ep->pool,
                                             l)) {
                    jk_log(l, JK_LOG_ERROR, 
                           "Error ajp13_process_callback - ajp13_unmarshal_response failed\n");
                    return JK_AJP13_ERROR;
                }
                if(!r->start_response(r, 
                                      res.status, 
                                      res.msg, 
                                      (const char * const *)res.header_names,
                                      (const char * const *)res.header_values,
                                      res.num_headers)) {
                    jk_log(l, JK_LOG_ERROR, 
                           "Error ajp13_process_callback - start_response failed\n");
                    return JK_INTERNAL_ERROR;
                }
            }
	    break;

        case JK_AJP13_SEND_BODY_CHUNK:
            {
	            unsigned len = (unsigned)jk_b_get_int(msg);
                if(!r->write(r, jk_b_get_buff(msg) + jk_b_get_pos(msg), len)) {
                    jk_log(l, JK_LOG_ERROR, 
                           "Error ajp13_process_callback - write failed\n");
                    return JK_INTERNAL_ERROR;
                }
            }
	    break;

        case JK_AJP13_GET_BODY_CHUNK:
            {
		unsigned len = (unsigned)jk_b_get_int(msg);

                if(len > MAX_SEND_BODY_SZ) {
                    len = MAX_SEND_BODY_SZ;
                }
                if(len > ep->left_bytes_to_send) {
                    len = ep->left_bytes_to_send;
                }
		if(len < 0) {
		    len = 0;
		}

		/* the right place to add file storage for upload */
		if(read_into_msg_buff(ep, r, msg, l, len)) {
		    r->content_read += len;
		    return JK_AJP13_HAS_RESPONSE;
		}                  

		jk_log(l, JK_LOG_ERROR, 
		       "Error ajp13_process_callback - read_into_msg_buff failed\n");
		return JK_INTERNAL_ERROR;	    
            }
	    break;

        case JK_AJP13_END_RESPONSE:
            {
                ep->reuse = (int)jk_b_get_byte(msg);
                
                if((ep->reuse & 0X01) != ep->reuse) {
                    /*
                     * Strange protocol error.
                     */
                    ep->reuse = JK_FALSE;
                }
            }
            return JK_AJP13_END_RESPONSE;
	    break;

        default:
	        jk_log(l, 
                   JK_LOG_ERROR,
		           "Error ajp13_process_callback - Invalid code: %d\n", code);
	        return JK_AJP13_ERROR;
    }
    
    return JK_AJP13_NO_RESPONSE;
}

/* -------------------- Method -------------------- */
static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t *props,                            
                              jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::validate\n");

    if(pThis && pThis->worker_private) {        
        ajp13_worker_t *p = pThis->worker_private;
        int port = jk_get_worker_port(props, 
                                      p->name,
                                      AJP_DEF_PORT);

        char *host = jk_get_worker_host(props, 
                                        p->name,
                                        AJP_DEF_HOST);

        jk_log(l, 
               JK_LOG_DEBUG, 
               "In jk_worker_t::validate for worker %s contact is %s:%d\n", 
               p->name, host, port);
	
        if(port > 1024 && host) {
            if(jk_resolve(host, (short)port, &p->worker_inet_addr)) {
                return JK_TRUE;
            }
            jk_log(l, JK_LOG_ERROR, "In jk_worker_t::validate, resolve failed\n");
        }
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::validate, Error %s %d\n", host, port);
    } else {
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::validate, NULL parameters\n");
    }
    
    return JK_FALSE;
}


static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t *props, 
                          jk_logger_t *l)
{
    /* 
     * start the connection cache
     */
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::init\n");

    if(pThis && pThis->worker_private) {        
        ajp13_worker_t *p = pThis->worker_private;
        int cache_sz = jk_get_worker_cache_size(props, 
                                                p->name,
                                                DEF_CACHE_SZ);

        if(cache_sz > 0) {
            p->ep_cache = 
                (ajp13_endpoint_t **)malloc(sizeof(ajp13_endpoint_t *) * cache_sz);
            if(p->ep_cache) {
                int i;
                p->ep_cache_sz = cache_sz;
                for(i = 0 ; i < cache_sz ; i++) {
                    p->ep_cache[i] = NULL;
                }
                JK_INIT_CS(&(p->cs), i);
                if(i) {
                    return JK_TRUE;
                }
            }
        }        
    } else {
        jk_log(l, 
               JK_LOG_ERROR, 
               "In jk_worker_t::init, NULL parameters\n");
    }
    
    return JK_FALSE;
}


static int JK_METHOD destroy(jk_worker_t **pThis,
                             jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::destroy\n");
    if(pThis && *pThis && (*pThis)->worker_private) {
        ajp13_worker_t *private_data = (*pThis)->worker_private;
        
        free(private_data->name);

        if(private_data->ep_cache_sz) {
            unsigned i;
            for(i = 0 ; i < private_data->ep_cache_sz ; i++) {
                if(private_data->ep_cache[i]) {
                    reset_endpoint(private_data->ep_cache[i]);
                    close_endpoint(private_data->ep_cache[i]);
                }                
            }
            free(private_data->ep_cache);
            JK_DELETE_CS(&(private_data->cs), i);
        }

        free(private_data);

        return JK_TRUE;
    }

    jk_log(l, JK_LOG_ERROR, "In jk_worker_t::destroy, NULL parameters\n");
    return JK_FALSE;
}


static int JK_METHOD done(jk_endpoint_t **e,
                          jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_endpoint_t::done\n");
    if(e && *e && (*e)->endpoint_private) {
        ajp13_endpoint_t *p = (*e)->endpoint_private;
        int reuse_ep = p->reuse;

        reset_endpoint(p);

        if(reuse_ep) {
            ajp13_worker_t *w = p->worker;
            if(w->ep_cache_sz) {
                int rc;
                JK_ENTER_CS(&w->cs, rc);
                if(rc) {
                    unsigned i;
                    
                    for(i = 0 ; i < w->ep_cache_sz ; i++) {
                        if(!w->ep_cache[i]) {
                            w->ep_cache[i] = p;
                            break;
                        }
                    }
                    JK_LEAVE_CS(&w->cs, rc);
                    if(i < w->ep_cache_sz) {
                        return JK_TRUE;
                    }
                }
            }
        }

        close_endpoint(p);
        *e = NULL;

        return JK_TRUE;
    }

    jk_log(l, 
           JK_LOG_ERROR, 
           "In jk_endpoint_t::done, NULL parameters\n");
    return JK_FALSE;
}

/*
 * send request to Tomcat via Ajp13
 * - first try to find reuseable socket
 * - if no one available, try to connect
 * - send request, but send must be see as asynchronous,
 *   since send() call will return noerror about 95% of time
 *   Hopefully we'll get more information on next read.
 * 
 * nb: reqmsg is the original request msg buffer
 *     repmsg is the reply msg buffer which could be scratched
 */
static int send_request(jk_endpoint_t *e,
						jk_ws_service_t *s,
						jk_logger_t *l,
						ajp13_endpoint_t *p,
						ajp13_operation_t *op)
{
	/* Up to now, we can recover */
	op->recoverable = JK_TRUE;

	/*
	 * First try to reuse open connections...
	*/
	while((p->sd > 0) && !connection_tcp_send_message(p, op->request, l)) {
		jk_log(l, JK_LOG_ERROR, "Error sending request try another pooled connection\n");
		jk_close_socket(p->sd);
		p->sd = -1;
		reuse_connection(p, l);
	}

	/*
	 * If we failed to reuse a connection, try to reconnect.
	 */
	if(p->sd < 0) {
		connect_to_tomcat(p, l);
		if(p->sd >= 0) {
		/*
		 * After we are connected, each error that we are going to
		 * have is probably unrecoverable
		 */
		if(!connection_tcp_send_message(p, op->request, l)) {
			jk_log(l, JK_LOG_ERROR, "Error sending request on a fresh connection\n");
			return JK_FALSE;
		}
		} else {
			jk_log(l, JK_LOG_ERROR, "Error connecting to the Tomcat process.\n");
			return JK_FALSE;
		}
	}

	/*
	 * From now on an error means that we have an internal server error
	 * or Tomcat crashed. In any case we cannot recover this.
	 */

	jk_log(l, JK_LOG_DEBUG, "send_request 2: request body to send %d - request body to resend %d\n", 
		p->left_bytes_to_send, jk_b_get_len(op->reply) - AJP13_HEADER_LEN);

	/*
	 * POST recovery job is done here.
	 * It's not very fine to have posted data in reply but that's the only easy
	 * way to do that for now. Sharing the reply is really a bad solution but
	 * it will works for POST DATA less than 8k.
	 * We send here the first part of data which was sent previously to the
	 * remote Tomcat
	 */
	if(jk_b_get_len(op->reply) > AJP13_HEADER_LEN) {
		if(!connection_tcp_send_message(p, op->reply, l)) {
			jk_log(l, JK_LOG_ERROR, "Error resending request body\n");
			return JK_FALSE;
		}
	}
	else
	{
		/* We never sent any POST data and we check it we have to send at
		 * least of block of data (max 8k). These data will be kept in reply
		 * for resend if the remote Tomcat is down, a fact we will learn only
		 * doing a read (not yet) 
	 	 */
		if(p->left_bytes_to_send > 0) {
			unsigned len = p->left_bytes_to_send;
			if(len > MAX_SEND_BODY_SZ) 
				len = MAX_SEND_BODY_SZ;
            		if(!read_into_msg_buff(p, s, op->reply, l, len)) {
				/* the browser stop sending data, no need to recover */
				op->recoverable = JK_FALSE;
				return JK_FALSE;
			}
			s->content_read = len;
			if(!connection_tcp_send_message(p, op->reply, l)) {
				jk_log(l, JK_LOG_ERROR, "Error sending request body\n");
				return JK_FALSE;
			}  
		}
	}
	return (JK_TRUE);
}

/*
 * get replies from Tomcat via Ajp13
 * We will know only at read time if the remote host closed
 * the connection (half-closed state - FIN-WAIT2). In that case
 * we must close our side of the socket and abort emission.
 * We will need another connection to send the request
 * There is need of refactoring here since we mix 
 * reply reception (tomcat -> apache) and request send (apache -> tomcat)
 * and everything using the same buffer (repmsg)
 * ajp13 is async but handling read/send this way prevent nice recovery
 * In fact if tomcat link is broken during upload (browser -> apache -> tomcat)
 * we'll loose data and we'll have to abort the whole request.
 */
static int get_reply(jk_endpoint_t *e,
                        jk_ws_service_t *s,
                        jk_logger_t *l,
			ajp13_endpoint_t *p,
			ajp13_operation_t *op)
{
	/* Start read all reply message */
	while(1) {
		int rc = 0;

		if(!connection_tcp_get_message(p, op->reply, l)) {
			jk_log(l, JK_LOG_ERROR, "Error reading reply\n");
			/* we just can't recover, unset recover flag */
			return JK_FALSE;
		}

		rc = ajp13_process_callback(op->reply, p, s, l);

		/* no more data to be sent, fine we have finish here */
       		if(JK_AJP13_END_RESPONSE == rc)
        		return JK_TRUE;
        	else if(JK_AJP13_HAS_RESPONSE == rc) {
        	/* 
        	 * in upload-mode there is no second chance since
        	 * we may have allready send part of uploaded data 
        	 * to Tomcat.
        	 * In this case if Tomcat connection is broken we must 
        	 * abort request and indicate error.
        	 * A possible work-around could be to store the uploaded
        	 * data to file and replay for it
        	 */
 			op->recoverable = JK_FALSE; 
			rc = connection_tcp_send_message(p, op->reply, l);
        		if (rc < 0) {
				jk_log(l, JK_LOG_ERROR, "Error sending request data %d\n", rc);
               	 		return JK_FALSE;
			}
		} else if(rc < 0) {
			return (JK_FALSE); /* XXX error */
		}
	}
}

#define	JK_RETRIES 3

/*
 * service is now splitted in send_request and get_reply
 * much more easier to do errors recovery
 */
static int JK_METHOD service(jk_endpoint_t *e, 
                             jk_ws_service_t *s,
                             jk_logger_t *l,
                             int *is_recoverable_error)
{
	int i;
	ajp13_operation_t	oper;
	ajp13_operation_t	*op = &oper;

	jk_log(l, JK_LOG_DEBUG, "Into jk_endpoint_t::service\n");

	if(e && e->endpoint_private && s && is_recoverable_error) {
		ajp13_endpoint_t *p = e->endpoint_private;
       		op->request = jk_b_new(&(p->pool));
		jk_b_set_buffer_size(op->request, DEF_BUFFER_SZ); 
		jk_b_reset(op->request);
       
		op->reply = jk_b_new(&(p->pool));
		jk_b_set_buffer_size(op->reply, DEF_BUFFER_SZ);
		jk_b_reset(op->reply); 
		
		op->recoverable = JK_TRUE;
		op->uploadfd	 = -1;		/* not yet used, later ;) */

		p->left_bytes_to_send = s->content_length;
		p->reuse = JK_FALSE;
		*is_recoverable_error = JK_TRUE;

		/* 
		 * We get here initial request (in reqmsg)
		 */
		if(!ajp13_marshal_into_msgb(op->request, s, l)) {
			*is_recoverable_error = JK_FALSE;                
			return JK_FALSE;
		}

		/* 
		 * JK_RETRIES could be replaced by the number of workers in
		 * a load-balancing configuration 
		 */
		for (i = 0; i < JK_RETRIES; i++)
		{
			/*
			 * We're using reqmsg which hold initial request
			 * if Tomcat is stopped or restarted, we will pass reqmsg
			 * to next valid tomcat. 
			 */
			if (send_request(e, s, l, p, op)) {

				/* If we have the no recoverable error, it's probably because the sender (browser)
				 * stop sending data before the end (certainly in a big post)
				 */
				if (! op->recoverable) {
					*is_recoverable_error = JK_FALSE;
					jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, send_request failed without recovery in send loop %d\n", i);
					return JK_FALSE;
				}

				/* Up to there we can recover */
				*is_recoverable_error = JK_TRUE;
				op->recoverable = JK_TRUE;

				if (get_reply(e, s, l, p, op))
					return (JK_TRUE);

				/* if we can't get reply, check if no recover flag was set 
				 * if is_recoverable_error is cleared, we have started received 
				 * upload data and we must consider that operation is no more recoverable
				 */
				if (! op->recoverable) {
					*is_recoverable_error = JK_FALSE;
					jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, get_reply failed without recovery in send loop %d\n", i);
					return JK_FALSE;
				}
				
				jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, get_reply failed in send loop %d\n", i);
			}
			else
				jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, send_request failed in send loop %d\n", i);
		
			jk_close_socket(p->sd);
			p->sd = -1;
			reuse_connection(p, l);
		}
	} else {
        	jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, NULL parameters\n");
	}

	return JK_FALSE;
}

static int JK_METHOD get_endpoint(jk_worker_t *pThis,
                                  jk_endpoint_t **pend,
                                  jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::get_endpoint\n");

    if(pThis && pThis->worker_private && pend) {        
        ajp13_worker_t *p = pThis->worker_private;
        ajp13_endpoint_t *ep = NULL;

        if(p->ep_cache_sz) {
            int rc;
            JK_ENTER_CS(&p->cs, rc);
            if(rc) {
                unsigned i;
                
                for(i = 0 ; i < p->ep_cache_sz ; i++) {
                    if(p->ep_cache[i]) {
                        ep = p->ep_cache[i];
                        p->ep_cache[i] = NULL;
                        break;
                    }
                }
                JK_LEAVE_CS(&p->cs, rc);
                if(ep) {
                    *pend = &ep->endpoint;
                    return JK_TRUE;
                }
            }
        } 

        ep = (ajp13_endpoint_t *)malloc(sizeof(ajp13_endpoint_t));
        if(ep) {
            ep->sd = -1;         
            ep->reuse = JK_FALSE;
            jk_open_pool(&ep->pool, ep->buf, sizeof(ep->buf));
            ep->worker = pThis->worker_private;
            ep->endpoint.endpoint_private = ep;
            ep->endpoint.service = service;
            ep->endpoint.done = done;
            *pend = &ep->endpoint;
            return JK_TRUE;
        }
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::get_endpoint, malloc failed\n");
    } else {
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::get_endpoint, NULL parameters\n");
    }

    return JK_FALSE;
}



int JK_METHOD ajp13_worker_factory(jk_worker_t **w,
                                   const char *name,
                                   jk_logger_t *l)
{
    ajp13_worker_t *private_data = 
            (ajp13_worker_t *)malloc(sizeof(ajp13_worker_t));
    
    jk_log(l, 
           JK_LOG_DEBUG, 
           "Into ajp13_worker_factory\n");
    if(NULL == name || NULL == w) {
        jk_log(l, 
               JK_LOG_ERROR, 
               "In ajp13_worker_factory, NULL parameters\n");
	    return JK_FALSE;
    }
        
    if(!private_data) {
        jk_log(l, JK_LOG_ERROR, "In ajp13_worker_factory, NULL parameters\n");
	    return JK_FALSE;
    }

    private_data->name = strdup(name);          
    
    if(!private_data->name) {
	    free(private_data);
	    jk_log(l, JK_LOG_ERROR, "In ajp13_worker_factory, malloc failed\n");
	    return JK_FALSE;
    } 


    private_data->ep_cache_sz            = 0;
    private_data->ep_cache               = NULL;
    private_data->connect_retry_attempts = DEF_RETRY_ATTEMPTS;
    private_data->worker.worker_private  = private_data;
    
    private_data->worker.validate        = validate;
    private_data->worker.init            = init;
    private_data->worker.get_endpoint    = get_endpoint;
    private_data->worker.destroy         = destroy;
    
    *w = &private_data->worker;
    return JK_TRUE;
}
