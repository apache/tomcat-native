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
 * Description: common stuff for bi-directional protocols ajp13/ajp14.     *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/


#include "jk_global.h"
#include "jk_channel.h"
#include "jk_env.h"
#include "jk_requtil.h"

/* Private methods */

static int ajp_read_fully_from_server(jk_ws_service_t *s,
                                      unsigned char   *buf,
                                      unsigned         len);

static int ajp_process_callback(jk_msg_buf_t *msg, 
                                jk_msg_buf_t *pmsg,
                                jk_endpoint_t *ae,
                                jk_ws_service_t *r, 
                                jk_logger_t *l);

/*
 * Reset the endpoint (clean buf)
 */
void ajp_reset_endpoint(jk_endpoint_t *ae)
{
    ae->reuse = JK_FALSE;
    ae->pool.reset( &ae->pool );
}

/*
 * Close the endpoint (clean buf and close socket)
 */
void ajp_close_endpoint(jk_endpoint_t *ae,
                        jk_logger_t    *l)
{
    l->jkLog(l, JK_LOG_DEBUG, "In jk_endpoint_t::ajp_close_endpoint\n");

    ajp_reset_endpoint(ae);
    ae->pool.close( &ae->pool );
    {
	jk_channel_t *channel=ae->worker->channel;
	int err=channel->close( channel, ae );
    }
    free(ae);
}

int ajp_connect_to_endpoint(jk_endpoint_t *ae,
                            jk_logger_t    *l)
{
    int attempt;

    for(attempt = 0 ; attempt < ae->worker->connect_retry_attempts ; attempt++) {
	jk_channel_t *channel=ae->worker->channel;
        int err=channel->open( channel, ae );
	l->jkLog(l, JK_LOG_DEBUG, "ajp_connect_to_endpoint: connected %lx\n", ae );
	if( err == JK_TRUE ) {
	    /* Check if we must execute a logon after the physical connect */
	    if (ae->worker->logon != NULL)
		return (ae->worker->logon(ae, l));
	    
	    return JK_TRUE;
        }
    }

    l->jkLog(l, JK_LOG_ERROR, "In jk_endpoint_t::ajp_connect_to_endpoint, failed errno = %d\n", errno);
    return JK_FALSE; 
}

/*
 * Send a message to endpoint, using corresponding PROTO HEADER
 */
int ajp_connection_tcp_send_message(jk_endpoint_t *ae,
                                    jk_msg_buf_t   *msg,
                                    jk_logger_t    *l)
{
    jk_b_end(msg, AJP13_WS_HEADER);
    jk_dump_buff(l, JK_LOG_DEBUG, "sending to ajp13", msg);
    {
	int err;
	jk_channel_t *channel=ae->worker->channel;
    
	err=channel->send( channel, ae, 
			   jk_b_get_buff(msg), jk_b_get_len(msg) );
	if( err!=JK_TRUE ) {
	    return err;
	}
    }

    return JK_TRUE;
}

/*
 * Receive a message from endpoint, checking PROTO HEADER
 */

int ajp_connection_tcp_get_message(jk_endpoint_t *ae,
                                   jk_msg_buf_t   *msg,
                                   jk_logger_t    *l)
{
    unsigned char head[AJP_HEADER_LEN];
    int           rc;
    int           msglen;
    unsigned int  header;

    if ((ae->proto != AJP13_PROTO) && (ae->proto != AJP14_PROTO)) {
	l->jkLog(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message:"
                 " Can't handle unknown protocol %d\n", ae->proto);
	return JK_FALSE;
    }
    {
	jk_channel_t *channel=ae->worker->channel;
    
	rc=channel->recv( channel, ae, 
                          head, AJP_HEADER_LEN );
    }

    if(rc < 0) {
        l->jkLog(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message: Error - jk_tcp_socket_recvfull failed\n");
        return JK_FALSE;
    }

    header = ((unsigned int)head[0] << 8) | head[1];
    
    if (header != AJP13_SW_HEADER) {
        l->jkLog(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message:"
                 "Error - Wrong message format 0x%04x\n", header);
        return JK_FALSE;
    }

    msglen  = ((head[2]&0xff)<<8);
    msglen += (head[3] & 0xFF);

    if(msglen > jk_b_get_size(msg)) {
        l->jkLog(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message: Error - Wrong message size\n");
        return JK_FALSE;
    }

    jk_b_set_len(msg, msglen);
    jk_b_set_pos(msg, 0);

    {
	jk_channel_t *channel=ae->worker->channel;
    
	rc=channel->recv( channel, ae, 
                          jk_b_get_buff(msg), msglen);
    }
    if(rc < 0) {
        l->jkLog(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message: Error - jk_tcp_socket_recvfull failed\n");
        return JK_FALSE;
    }

    if (ae->proto == AJP13_PROTO) 
    	jk_dump_buff(l, JK_LOG_DEBUG, "received from ajp13", msg);
    else if (ae->proto == AJP14_PROTO)
	jk_dump_buff(l, JK_LOG_DEBUG, "received from ajp14", msg);
    
    return JK_TRUE;
}

/*
 * Read all the data from the socket.
 *
 * Socket API didn't garanty all the data will be kept in a single 
 * read, so we must loop up to all awaited data are received 
 */

static int ajp_read_fully_from_server(jk_ws_service_t *s,
                                      unsigned char   *buf,
                                      unsigned         len)
{
    unsigned rdlen = 0;
    unsigned padded_len = len;

    if (s->is_chunked && s->no_more_chunks) {
	return 0;
    }
    if (s->is_chunked) {
        /* Corner case: buf must be large enough to hold next
         * chunk size (if we're on or near a chunk border).
         * Pad the length to a reasonable value, otherwise the
         * read fails and the remaining chunks are tossed.
         */
        padded_len = (len < CHUNK_BUFFER_PAD) ?
            len : len - CHUNK_BUFFER_PAD;
    }

    while(rdlen < padded_len) {
        unsigned this_time = 0;
        if(!s->read(s, buf + rdlen, len - rdlen, &this_time)) {
            return -1;
        }

        if(0 == this_time) {
	    if (s->is_chunked) {
		s->no_more_chunks = 1; /* read no more */
	    }
            break;
        }
        rdlen += this_time;
    }

    return (int)rdlen;
}


/*
 * Read data from AJP13/AJP14 protocol
 * Returns -1 on error, else number of bytes read
 */

int ajp_read_into_msg_buff(jk_endpoint_t  *ae,
                           jk_ws_service_t *r,
                           jk_msg_buf_t    *msg,
                           int            len,
                           jk_logger_t     *l)
{
    unsigned char *read_buf = jk_b_get_buff(msg);

    jk_b_reset(msg);

    read_buf += AJP_HEADER_LEN;    /* leave some space for the buffer headers */
    read_buf += AJP_HEADER_SZ_LEN; /* leave some space for the read length */

    /* Pick the max size since we don't know the content_length */
    if (r->is_chunked && len == 0) {
	len = AJP13_MAX_SEND_BODY_SZ;
    }

    if ((len = ajp_read_fully_from_server(r, read_buf, len)) < 0) {
        l->jkLog(l, JK_LOG_ERROR, "ajp_read_into_msg_buff: Error - ajp_read_fully_from_server failed\n");
        return -1;
    }

    if (!r->is_chunked) {
	ae->left_bytes_to_send -= len;
    }

    if (len > 0) {
	/* Recipient recognizes empty packet as end of stream, not
	   an empty body packet */
        if(0 != jk_b_append_int(msg, (unsigned short)len)) {
            l->jkLog(l, JK_LOG_ERROR, 
                     "read_into_msg_buff: Error - jk_b_append_int failed\n");
            return -1;
	}
    }

    jk_b_set_len(msg, jk_b_get_len(msg) + len);

    return len;
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
int ajp_send_request(jk_endpoint_t *e,
                     jk_ws_service_t *s,
                     jk_logger_t *l)
{
    int err=JK_TRUE;
    
    /* Up to now, we can recover */
    e->recoverable = JK_TRUE;

    /*
     * First try to reuse open connections...
     */
    {
        jk_channel_t *channel=e->worker->channel;
        err=ajp_connection_tcp_send_message(e, e->request, l);
        if( err != JK_TRUE ) {
            l->jkLog(l, JK_LOG_ERROR, "Error sending request, close endpoint\n");
            channel->close( channel, e );
        }
    }

    /*
     * If we failed to reuse a connection, try to reconnect.
     */
    if( err != JK_TRUE ) {
        err=ajp_connect_to_endpoint(e, l);
	if ( err != JK_TRUE) {
	    l->jkLog(l, JK_LOG_ERROR, "Error connecting to the Tomcat process.\n");
	    return JK_FALSE;
        }
        /*
         * After we are connected, each error that we are going to
         * have is probably unrecoverable
         */
        err=ajp_connection_tcp_send_message(e, e->request, l);
        if (err != JK_TRUE) {
            l->jkLog(l, JK_LOG_ERROR, "Error sending request on a fresh connection\n");
            return JK_FALSE;
        }
    }
    
    /*
     * From now on an error means that we have an internal server error
     * or Tomcat crashed. In any case we cannot recover this.
     */
    
    l->jkLog(l, JK_LOG_DEBUG, "ajp_send_request 2: request body to send %d - request body to resend %d\n", 
             e->left_bytes_to_send, jk_b_get_len(e->reply) - AJP_HEADER_LEN);
    
    /*
     * POST recovery job is done here.
     * It's not very fine to have posted data in reply but that's the only easy
     * way to do that for now. Sharing the reply is really a bad solution but
     * it will works for POST DATA less than 8k.
     * We send here the first part of data which was sent previously to the
     * remote Tomcat
     */
    if (jk_b_get_len(e->post) > AJP_HEADER_LEN) {
        err=ajp_connection_tcp_send_message(e, e->post, l);
	if(err != JK_TRUE) {
	    l->jkLog(l, JK_LOG_ERROR, "Error resending request body\n");
	    return JK_FALSE;
	}
    } else {
	/* We never sent any POST data and we check it we have to send at
	 * least of block of data (max 8k). These data will be kept in reply
	 * for resend if the remote Tomcat is down, a fact we will learn only
	 * doing a read (not yet) 
	 */
	if (s->is_chunked || e->left_bytes_to_send > 0) {
	    int len = e->left_bytes_to_send;
	    if (len > AJP13_MAX_SEND_BODY_SZ) 
		len = AJP13_MAX_SEND_BODY_SZ;
            len = ajp_read_into_msg_buff(e, s, e->post, len, l);
	    if (len < 0) {
		/* the browser stop sending data, no need to recover */
		e->recoverable = JK_FALSE;
		return JK_FALSE;
	    }
	    s->content_read = len;
            err=ajp_connection_tcp_send_message(e, e->post, l);
	    if (err!=JK_TRUE) {
		l->jkLog(l, JK_LOG_ERROR, "Error sending request body\n");
		return JK_FALSE;
	    }  
	}
    }
    return (JK_TRUE);
}

/*
 * What to do with incoming data (dispatcher)
 */

static int ajp_process_callback(jk_msg_buf_t *msg, 
                                jk_msg_buf_t *pmsg,
                                jk_endpoint_t *ae,
                                jk_ws_service_t *r, 
                                jk_logger_t *l) 
{
    int code = (int)jk_b_get_byte(msg);
    int err;

    switch(code) {
    case JK_AJP13_SEND_HEADERS:
        err=ajp_handle_response( msg, r, ae, l );
        return err;
        break;
        
    case JK_AJP13_SEND_BODY_CHUNK:
        return ajp_handle_sendChunk( msg, r, ae, l);
        break;
        
    case JK_AJP13_GET_BODY_CHUNK:
        {
            /* XXX Is it signed or not ? */
            int len = jk_b_get_int(msg);
            
            if(len > AJP13_MAX_SEND_BODY_SZ) {
                len = AJP13_MAX_SEND_BODY_SZ;
            }
            if(len > ae->left_bytes_to_send) {
                len = ae->left_bytes_to_send;
            }
            if(len < 0) {
                len = 0;
            }
            
            /* the right place to add file storage for upload */
            if ((len = ajp_read_into_msg_buff(ae, r, msg, len, l)) >= 0) {
                r->content_read += len;
                return JK_AJP13_HAS_RESPONSE;
            }                  
            
            l->jkLog(l, JK_LOG_ERROR, "Error ajp_process_callback - ajp_read_into_msg_buff failed\n");
            return JK_INTERNAL_ERROR;	    
        }
        break;
        
    case JK_AJP13_END_RESPONSE:
        {
            ae->reuse = (int)jk_b_get_byte(msg);
            
            if((ae->reuse & 0X01) != ae->reuse) {
                /*
                 * Strange protocol error.
                 */
                ae->reuse = JK_FALSE;
            }
        }
        return JK_AJP13_END_RESPONSE;
        break;
        
    default:
        l->jkLog(l, JK_LOG_ERROR, "Error ajp_process_callback - Invalid code: %d\n", code);
        jk_dump_buff(l, JK_LOG_ERROR, "Message: ", msg);
        return JK_AJP13_ERROR;
    }
    
    return JK_AJP13_NO_RESPONSE;
}

/*
 * get replies from Tomcat via Ajp13/Ajp14
 * We will know only at read time if the remote host closed
 * the connection (half-closed state - FIN-WAIT2). In that case
 * we must close our side of the socket and abort emission.
 * We will need another connection to send the request
 * There is need of refactoring here since we mix 
 * reply reception (tomcat -> apache) and request send (apache -> tomcat)
 * and everything using the same buffer (repmsg)
 * ajp13/ajp14 is async but handling read/send this way prevent nice recovery
 * In fact if tomcat link is broken during upload (browser -> apache -> tomcat)
 * we'll loose data and we'll have to abort the whole request.
 */
int ajp_get_reply(jk_endpoint_t *e,
                  jk_ws_service_t *s,
                  jk_logger_t *l)
{
    /* Start read all reply message */
    while(1) {
        int rc = 0;
        
        if(!ajp_connection_tcp_get_message(e, e->reply, l)) {
            l->jkLog(l, JK_LOG_ERROR, "Error reading reply\n");
            /* we just can't recover, unset recover flag */
            return JK_FALSE;
        }
        
        rc = ajp_process_callback(e->reply, e->post, e, s, l);
        
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
            e->recoverable = JK_FALSE; 
            rc = ajp_connection_tcp_send_message(e, e->post, l);
            if (rc < 0) {
                l->jkLog(l, JK_LOG_ERROR, "Error sending request data %d\n", rc);
                return JK_FALSE;
            }
        } else if(JK_FATAL_ERROR == rc) {
            /*
             * we won't be able to gracefully recover from this so
             * set recoverable to false and get out.
             */
            e->recoverable = JK_FALSE;
            return JK_FALSE;
        } else if(JK_CLIENT_ERROR == rc) {
            /*
             * Client has stop talking to us, so get out.
             * We assume this isn't our fault, so just a normal exit.
             * In most (all?)  cases, the ajp13_endpoint::reuse will still be
             * false here, so this will be functionally the same as an
             * un-recoverable error.  We just won't log it as such.
             */
            return JK_TRUE;
        } else if(rc < 0) {
            return (JK_FALSE); /* XXX error */
        }
    }
}

