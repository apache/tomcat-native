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
#include "jk_util.h"
#include "jk_ajp13.h"
#include "jk_ajp14.h"
#include "jk_ajp_common.h"
#include "jk_connect.h"


const char *response_trans_headers[] = {
    "Content-Type", 
    "Content-Language", 
    "Content-Length", 
    "Date", 
    "Last-Modified", 
    "Location", 
    "Set-Cookie", 
    "Set-Cookie2", 
    "Servlet-Engine", 
    "Status", 
    "WWW-Authenticate"
};

static const char *long_res_header_for_sc(int sc) 
{
    const char *rc = NULL;
    if(sc <= SC_RES_HEADERS_NUM && sc > 0) {
        rc = response_trans_headers[sc - 1];
    }

    return rc;
}


static int sc_for_req_method(const char    *method,
                             unsigned char *sc) 
{
    int rc = JK_TRUE;
    if(0 == strcmp(method, "GET")) {
        *sc = SC_M_GET;
    } else if(0 == strcmp(method, "POST")) {
        *sc = SC_M_POST;
    } else if(0 == strcmp(method, "HEAD")) {
        *sc = SC_M_HEAD;
    } else if(0 == strcmp(method, "PUT")) {
        *sc = SC_M_PUT;
    } else if(0 == strcmp(method, "DELETE")) {
        *sc = SC_M_DELETE;
    } else if(0 == strcmp(method, "OPTIONS")) {
        *sc = SC_M_OPTIONS;
    } else if(0 == strcmp(method, "TRACE")) {
        *sc = SC_M_TRACE;
    } else if(0 == strcmp(method, "PROPFIND")) {
	*sc = SC_M_PROPFIND;
    } else if(0 == strcmp(method, "PROPPATCH")) {
	*sc = SC_M_PROPPATCH;
    } else if(0 == strcmp(method, "MKCOL")) {
	*sc = SC_M_MKCOL;
    } else if(0 == strcmp(method, "COPY")) {
	*sc = SC_M_COPY;
    } else if(0 == strcmp(method, "MOVE")) {
	*sc = SC_M_MOVE;
    } else if(0 == strcmp(method, "LOCK")) {
	*sc = SC_M_LOCK;
    } else if(0 == strcmp(method, "UNLOCK")) {
	*sc = SC_M_UNLOCK;
    } else if(0 == strcmp(method, "ACL")) {
	*sc = SC_M_ACL;
    } else if(0 == strcmp(method, "REPORT")) {
	*sc = SC_M_REPORT;
    } else {
        rc = JK_FALSE;
    }

    return rc;
}

static int sc_for_req_header(const char     *header_name,
                             unsigned short *sc) 
{
    switch(header_name[0]) {
        case 'a':
            if('c' ==header_name[1] &&
               'c' ==header_name[2] &&
               'e' ==header_name[3] &&
               'p' ==header_name[4] &&
               't' ==header_name[5]) {
                if ('-' == header_name[6]) {
                    if(!strcmp(header_name + 7, "charset")) {
                        *sc = SC_ACCEPT_CHARSET;
                    } else if(!strcmp(header_name + 7, "encoding")) {
                        *sc = SC_ACCEPT_ENCODING;
                    } else if(!strcmp(header_name + 7, "language")) {
                        *sc = SC_ACCEPT_LANGUAGE;
                    } else {
                        return JK_FALSE;
                    }
                } else if ('\0' == header_name[6]) {
                    *sc = SC_ACCEPT;
                } else {
                    return JK_FALSE;
                }
            } else if (!strcmp(header_name, "authorization")) {
                *sc = SC_AUTHORIZATION;
            } else {
                return JK_FALSE;
            }
        break;

        case 'c':
            if(!strcmp(header_name, "cookie")) {
                *sc = SC_COOKIE;
            } else if(!strcmp(header_name, "connection")) {
                *sc = SC_CONNECTION;
            } else if(!strcmp(header_name, "content-type")) {
                *sc = SC_CONTENT_TYPE;
            } else if(!strcmp(header_name, "content-length")) {
                *sc = SC_CONTENT_LENGTH;
            } else if(!strcmp(header_name, "cookie2")) {
                *sc = SC_COOKIE2;
            } else {
                return JK_FALSE;
            }
        break;

        case 'h':
            if(!strcmp(header_name, "host")) {
                *sc = SC_HOST;
            } else {
                return JK_FALSE;
            }
        break;

        case 'p':
            if(!strcmp(header_name, "pragma")) {
                *sc = SC_PRAGMA;
            } else {
                return JK_FALSE;
            }
        break;

        case 'r':
            if(!strcmp(header_name, "referer")) {
                *sc = SC_REFERER;
            } else {
                return JK_FALSE;
            }
        break;

        case 'u':
            if(!strcmp(header_name, "user-agent")) {
                *sc = SC_USER_AGENT;
            } else {
                return JK_FALSE;
            }
        break;

        default:
            return JK_FALSE;
    }

    return JK_TRUE;
}


/*
 * Message structure
 *
 *
AJPV13_REQUEST/AJPV14_REQUEST=
    request_prefix (1) (byte)
    method         (byte)
    protocol       (string)
    req_uri        (string)
    remote_addr    (string)
    remote_host    (string)
    server_name    (string)
    server_port    (short)
    is_ssl         (boolean)
    num_headers    (short)
    num_headers*(req_header_name header_value)

    ?context       (byte)(string)
    ?servlet_path  (byte)(string)
    ?remote_user   (byte)(string)
    ?auth_type     (byte)(string)
    ?query_string  (byte)(string)
    ?jvm_route     (byte)(string)
    ?ssl_cert      (byte)(string)
    ?ssl_cipher    (byte)(string)
    ?ssl_session   (byte)(string)
    ?ssl_key_size  (byte)(int)		only AJP14
    request_terminator (byte)
    ?body          content_length*(var binary)

 */

static int ajp_marshal_into_msgb(jk_msg_buf_t    *msg,
                                 jk_ws_service_t *s,
                                 jk_logger_t     *l,
					        	 ajp_endpoint_t  *ae)
{
    unsigned char method;
    unsigned i;

    jk_log(l, JK_LOG_DEBUG, "Into ajp_marshal_into_msgb\n");

    if (!sc_for_req_method(s->method, &method)) { 
        jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - No such method %s\n", s->method);
        return JK_FALSE;
    }

    if (jk_b_append_byte(msg, JK_AJP13_FORWARD_REQUEST)  ||
        jk_b_append_byte(msg, method)               ||
        jk_b_append_string(msg, s->protocol)        ||
        jk_b_append_string(msg, s->req_uri)         ||
        jk_b_append_string(msg, s->remote_addr)     ||
        jk_b_append_string(msg, s->remote_host)     ||
        jk_b_append_string(msg, s->server_name)     ||
        jk_b_append_int(msg, (unsigned short)s->server_port) ||
        jk_b_append_byte(msg, (unsigned char)(s->is_ssl)) ||
        jk_b_append_int(msg, (unsigned short)(s->num_headers))) {

        jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the message begining\n");
        return JK_FALSE;
    }

    for (i = 0 ; i < s->num_headers ; i++) {
        unsigned short sc;

        if (sc_for_req_header(s->headers_names[i], &sc)) {
            if (jk_b_append_int(msg, sc)) {
                jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the header name\n");
                return JK_FALSE;
            }
        } else {
            if (jk_b_append_string(msg, s->headers_names[i])) {
                jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the header name\n");
                return JK_FALSE;
            }
        }
        
        if (jk_b_append_string(msg, s->headers_values[i])) {
            jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the header value\n");
            return JK_FALSE;
        }
    }

    if (s->remote_user) {
        if (jk_b_append_byte(msg, SC_A_REMOTE_USER) ||
            jk_b_append_string(msg, s->remote_user)) {
            jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the remote user\n");
            return JK_FALSE;
        }
    }
    if (s->auth_type) {
        if (jk_b_append_byte(msg, SC_A_AUTH_TYPE) ||
            jk_b_append_string(msg, s->auth_type)) {
            jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the auth type\n");
            return JK_FALSE;
        }
    }
    if (s->query_string) {
        if (jk_b_append_byte(msg, SC_A_QUERY_STRING) ||
            jk_b_append_string(msg, s->query_string)) {
            jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the query string\n");
            return JK_FALSE;
        }
    }
    if (s->jvm_route) {
        if (jk_b_append_byte(msg, SC_A_JVM_ROUTE) ||
            jk_b_append_string(msg, s->jvm_route)) {
            jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the jvm route\n");
            return JK_FALSE;
        }
    }
    if (s->ssl_cert_len) {
        if (jk_b_append_byte(msg, SC_A_SSL_CERT) ||
            jk_b_append_string(msg, s->ssl_cert)) {
            jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the SSL certificates\n");
            return JK_FALSE;
        }
    }

    if (s->ssl_cipher) {
        if (jk_b_append_byte(msg, SC_A_SSL_CIPHER) ||
            jk_b_append_string(msg, s->ssl_cipher)) {
            jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the SSL ciphers\n");
            return JK_FALSE;
        }
    }
    if (s->ssl_session) {
        if (jk_b_append_byte(msg, SC_A_SSL_SESSION) ||
            jk_b_append_string(msg, s->ssl_session)) {
            jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the SSL session\n");
            return JK_FALSE;
        }
    }

    /*
     * ssl_key_size is required by Servlet 2.3 API
     * added support only in ajp14 mode
     */
    if ((ae->proto == AJP14_PROTO) && (s->ssl_key_size != -1)) {
        if (jk_b_append_byte(msg, SC_A_SSL_KEY_SIZE) ||
            jk_b_append_int(msg, (unsigned short) s->ssl_key_size)) {
            jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the SSL key size\n");
            return JK_FALSE;
        }
    }


    if (s->num_attributes > 0) {
        for (i = 0 ; i < s->num_attributes ; i++) {
            if (jk_b_append_byte(msg, SC_A_REQ_ATTRIBUTE)       ||
                jk_b_append_string(msg, s->attributes_names[i]) ||
                jk_b_append_string(msg, s->attributes_values[i])) {
                jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending attribute %s=%s\n",
                      s->attributes_names[i], s->attributes_values[i]);
                return JK_FALSE;
            }
        }
    }

    if (jk_b_append_byte(msg, SC_A_ARE_DONE)) {
        jk_log(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the message end\n");
        return JK_FALSE;
    }
    
    jk_log(l, JK_LOG_DEBUG, "ajp_marshal_into_msgb - Done\n");
    return JK_TRUE;
}

/*
AJPV13_RESPONSE/AJPV14_RESPONSE:=
    response_prefix (2)
    status          (short)
    status_msg      (short)
    num_headers     (short)
    num_headers*(res_header_name header_value)
    *body_chunk
    terminator      boolean <! -- recycle connection or not  -->

req_header_name := 
    sc_req_header_name | (string)

res_header_name := 
    sc_res_header_name | (string)

header_value :=
    (string)

body_chunk :=
    length  (short)
    body    length*(var binary)

 */


static int ajp_unmarshal_response(jk_msg_buf_t   *msg,
                                  jk_res_data_t  *d,
						          ajp_endpoint_t *ae,
                                  jk_logger_t    *l)
{
	jk_pool_t * p = &ae->pool;

    d->status = jk_b_get_int(msg);

    if (!d->status) {
        jk_log(l, JK_LOG_ERROR, "Error ajp_unmarshal_response - Null status\n");
        return JK_FALSE;
    }

    d->msg = (char *)jk_b_get_string(msg);

    jk_log(l, JK_LOG_DEBUG, "ajp_unmarshal_response: status = %d\n", d->status);

    d->num_headers = jk_b_get_int(msg);
    d->header_names = d->header_values = NULL;

    jk_log(l, JK_LOG_DEBUG, "ajp_unmarshal_response: Number of headers is = %d\n", d->num_headers);

    if (d->num_headers) {
        d->header_names = jk_pool_alloc(p, sizeof(char *) * d->num_headers);
        d->header_values = jk_pool_alloc(p, sizeof(char *) * d->num_headers);

        if (d->header_names && d->header_values) {
            unsigned i;
            for(i = 0 ; i < d->num_headers ; i++) {
                unsigned short name = jk_b_pget_int(msg, jk_b_get_pos(msg)) ;
                
                if ((name & 0XFF00) == 0XA000) {
                    jk_b_get_int(msg);
                    name = name & 0X00FF;
                    if (name <= SC_RES_HEADERS_NUM) {
                        d->header_names[i] = (char *)long_res_header_for_sc(name);
                    } else {
                        jk_log(l, JK_LOG_ERROR, "Error ajp_unmarshal_response - No such sc (%d)\n", name);
                        return JK_FALSE;
                    }
                } else {
                    d->header_names[i] = (char *)jk_b_get_string(msg);
                    if (!d->header_names[i]) {
                        jk_log(l, JK_LOG_ERROR, "Error ajp_unmarshal_response - Null header name\n");
                        return JK_FALSE;
                    }
                }

                d->header_values[i] = (char *)jk_b_get_string(msg);
                if (!d->header_values[i]) {
                    jk_log(l, JK_LOG_ERROR, "Error ajp_unmarshal_response - Null header value\n");
                    return JK_FALSE;
                }

                jk_log(l, JK_LOG_DEBUG, "ajp_unmarshal_response: Header[%d] [%s] = [%s]\n", 
                       i,
                       d->header_names[i],
                       d->header_values[i]);
            }
        }
    }

    return JK_TRUE;
}


/*
 * Reset the endpoint (clean buf)
 */

static void ajp_reset_endpoint(ajp_endpoint_t *ae)
{
    ae->reuse = JK_FALSE;
    jk_reset_pool(&(ae->pool));
}

/*
 * Close the endpoint (clean buf and close socket)
 */

void ajp_close_endpoint(ajp_endpoint_t *ae,
                        jk_logger_t    *l)
{
	jk_log(l, JK_LOG_DEBUG, "In jk_endpoint_t::ajp_close_endpoint\n");

    ajp_reset_endpoint(ae);
    jk_close_pool(&(ae->pool));

    if (ae->sd > 0) { 
        jk_close_socket(ae->sd);
		jk_log(l, JK_LOG_DEBUG, "In jk_endpoint_t::ajp_close_endpoint, closed sd = %d\n", ae->sd);
		ae->sd = -1; /* just to avoid twice close */
	}

    free(ae);
}


/*
 * Try to reuse a previous connection
 */

static void ajp_reuse_connection(ajp_endpoint_t *ae,
                                 jk_logger_t    *l)
{
    ajp_worker_t *aw = ae->worker;

    if (aw->ep_cache_sz) {
        int rc;
        JK_ENTER_CS(&aw->cs, rc);
        if (rc) {
            unsigned i;

            for (i = 0 ; i < aw->ep_cache_sz ; i++) {
                if (aw->ep_cache[i]) {
                    ae->sd = aw->ep_cache[i]->sd;
                    aw->ep_cache[i]->sd = -1;
                    ajp_close_endpoint(aw->ep_cache[i], l);
                    aw->ep_cache[i] = NULL;
                    break;
                 }
            }
            JK_LEAVE_CS(&aw->cs, rc);
        }
    }
}


int ajp_connect_to_endpoint(ajp_endpoint_t *ae,
                            jk_logger_t    *l)
{
    unsigned attempt;

    for(attempt = 0 ; attempt < ae->worker->connect_retry_attempts ; attempt++) {
        ae->sd = jk_open_socket(&ae->worker->worker_inet_addr, JK_TRUE, l);
        if(ae->sd >= 0) {
            jk_log(l, JK_LOG_DEBUG, "In jk_endpoint_t::ajp_connect_to_endpoint, connected sd = %d\n", ae->sd);

			/* Check if we must execute a logon after the physical connect */
			if (ae->worker->logon != NULL)
				return (ae->worker->logon(ae, l));

			return JK_TRUE;
        }
    }

    jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::ajp_connect_to_endpoint, failed errno = %d\n", errno);
	return JK_FALSE;
}

/*
 * Send a message to endpoint, using corresponding PROTO HEADER
 */

int ajp_connection_tcp_send_message(ajp_endpoint_t *ae,
                                    jk_msg_buf_t   *msg,
                                    jk_logger_t    *l)
{
	if (ae->proto == AJP13_PROTO) {
    	jk_b_end(msg, AJP13_WS_HEADER);
		jk_dump_buff(l, JK_LOG_DEBUG, "sending to ajp13", msg);
	}
	else if (ae->proto == AJP14_PROTO) {
		jk_b_end(msg, AJP14_WS_HEADER);
		jk_dump_buff(l, JK_LOG_DEBUG, "sending to ajp14", msg);
	}
	else {
		jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::ajp_connection_tcp_send_message, unknown protocol %d, supported are AJP13/AJP14\n", ae->proto);
		return JK_FALSE;
	}

    if(0 > jk_tcp_socket_sendfull(ae->sd, jk_b_get_buff(msg), jk_b_get_len(msg))) {
        return JK_FALSE;
    }

    return JK_TRUE;
}

/*
 * Receive a message from endpoint, checking PROTO HEADER
 */

int ajp_connection_tcp_get_message(ajp_endpoint_t *ae,
                                   jk_msg_buf_t   *msg,
                                   jk_logger_t    *l)
{
    unsigned char head[AJP_HEADER_LEN];
    int           rc;
    int           msglen;
	unsigned int  header;

	if ((ae->proto != AJP13_PROTO) && (ae->proto != AJP14_PROTO)) {
		jk_log(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message: Can't handle unknown protocol %d\n", ae->proto);
		return JK_FALSE;
	}

    rc = jk_tcp_socket_recvfull(ae->sd, head, AJP_HEADER_LEN);

    if(rc < 0) {
        jk_log(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message: Error - jk_tcp_socket_recvfull failed\n");
        return JK_FALSE;
    }

	header = ((unsigned int)head[0] << 8) | head[1];
  
	if (ae->proto == AJP13_PROTO) {
    	if (header != AJP13_SW_HEADER) {

			if (header == AJP14_SW_HEADER)
				jk_log(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message: Error - received AJP14 reply on an AJP13 connection\n");
			else
        		jk_log(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message: Error - Wrong message format 0x%04x\n", header);

        	return JK_FALSE;
    	}
	}
	else if (ae->proto == AJP14_PROTO) {
		if (header != AJP14_SW_HEADER) {

			if (header == AJP13_SW_HEADER)
				jk_log(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message: Error - received AJP13 reply on an AJP14 connection\n");
			else
				jk_log(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message: Error - Wrong message format 0x%04x\n", header);

			return JK_FALSE;
		}
	}	

    msglen  = ((head[2]&0xff)<<8);
    msglen += (head[3] & 0xFF);

    if(msglen > jk_b_get_size(msg)) {
        jk_log(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message: Error - Wrong message size\n");
        return JK_FALSE;
    }

    jk_b_set_len(msg, msglen);
    jk_b_set_pos(msg, 0);

    rc = jk_tcp_socket_recvfull(ae->sd, jk_b_get_buff(msg), msglen);
    if(rc < 0) {
        jk_log(l, JK_LOG_ERROR, "ajp_connection_tcp_get_message: Error - jk_tcp_socket_recvfull failed\n");
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


/*
 * Read data from AJP13/AJP14 protocol 
 */

static int ajp_read_into_msg_buff(ajp_endpoint_t  *ae,
                                  jk_ws_service_t *r,
                                  jk_msg_buf_t    *msg,
								  unsigned 		   len,
                                  jk_logger_t     *l)
{
    unsigned char *read_buf = jk_b_get_buff(msg);

    jk_b_reset(msg);

    read_buf += AJP_HEADER_LEN;    /* leave some space for the buffer headers */
    read_buf += AJP_HEADER_SZ_LEN; /* leave some space for the read length */

    if (ajp_read_fully_from_server(r, read_buf, len) < 0) {
        jk_log(l, JK_LOG_ERROR, "ajp_read_into_msg_buff: Error - ajp_read_fully_from_server failed\n");
        return JK_FALSE;
    }

    ae->left_bytes_to_send -= len;

    if(0 != jk_b_append_int(msg, (unsigned short)len)) {
        jk_log(l, JK_LOG_ERROR, "ajp_read_into_msg_buff: Error - jk_b_append_int failed\n");
        return JK_FALSE;
    }

    jk_b_set_len(msg, jk_b_get_len(msg) + len);

    return JK_TRUE;
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
static int ajp_send_request(jk_endpoint_t *e,
		     				jk_ws_service_t *s,
						    jk_logger_t *l,
						    ajp_endpoint_t *ae,
						    ajp_operation_t *op)
{
	/* Up to now, we can recover */
	op->recoverable = JK_TRUE;

	/*
	 * First try to reuse open connections...
	*/
	while ((ae->sd > 0) && ! ajp_connection_tcp_send_message(ae, op->request, l)) {
		jk_log(l, JK_LOG_ERROR, "Error sending request try another pooled connection\n");
		jk_close_socket(ae->sd);
		ae->sd = -1;
		ajp_reuse_connection(ae, l);
	}

	/*
	 * If we failed to reuse a connection, try to reconnect.
	 */
	if (ae->sd < 0) {
		if (ajp_connect_to_endpoint(ae, l) == JK_TRUE) {
		/*
		 * After we are connected, each error that we are going to
		 * have is probably unrecoverable
		 */
		if (!ajp_connection_tcp_send_message(ae, op->request, l)) {
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

	jk_log(l, JK_LOG_DEBUG, "ajp_send_request 2: request body to send %d - request body to resend %d\n", 
		ae->left_bytes_to_send, jk_b_get_len(op->reply) - AJP_HEADER_LEN);

	/*
	 * POST recovery job is done here.
	 * It's not very fine to have posted data in reply but that's the only easy
	 * way to do that for now. Sharing the reply is really a bad solution but
	 * it will works for POST DATA less than 8k.
	 * We send here the first part of data which was sent previously to the
	 * remote Tomcat
	 */
	if (jk_b_get_len(op->post) > AJP_HEADER_LEN) {
		if(!ajp_connection_tcp_send_message(ae, op->post, l)) {
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
		if (ae->left_bytes_to_send > 0) {
			unsigned len = ae->left_bytes_to_send;
			if (len > AJP13_MAX_SEND_BODY_SZ) 
				len = AJP13_MAX_SEND_BODY_SZ;
            		if (! ajp_read_into_msg_buff(ae, s, op->post, len, l)) {
				/* the browser stop sending data, no need to recover */
				op->recoverable = JK_FALSE;
				return JK_FALSE;
			}
			s->content_read = len;
			if (!ajp_connection_tcp_send_message(ae, op->reply, l)) {
				jk_log(l, JK_LOG_ERROR, "Error sending request body\n");
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
                                ajp_endpoint_t *ae,
                                jk_ws_service_t *r, 
                                jk_logger_t *l) 
{
    int code = (int)jk_b_get_byte(msg);

    switch(code) {
        case JK_AJP13_SEND_HEADERS:
            {
                jk_res_data_t res;
                if (!ajp_unmarshal_response(msg, &res, ae, l)) {
                    jk_log(l, JK_LOG_ERROR, "Error ajp_process_callback - ajp_unmarshal_response failed\n");
                    return JK_AJP13_ERROR;
                }
                if (!r->start_response(r, 
                                       res.status, 
                                       res.msg, 
                                       (const char * const *)res.header_names,
                                       (const char * const *)res.header_values,
                                       res.num_headers)) {
                    jk_log(l, JK_LOG_ERROR, "Error ajp_process_callback - start_response failed\n");
                    return JK_CLIENT_ERROR;
                }
            }
	    break;

        case JK_AJP13_SEND_BODY_CHUNK:
            {
	            unsigned len = (unsigned)jk_b_get_int(msg);
                if(!r->write(r, jk_b_get_buff(msg) + jk_b_get_pos(msg), len)) {
                    jk_log(l, JK_LOG_ERROR, "Error ajp_process_callback - write failed\n");
                    return JK_CLIENT_ERROR;
                }
            }
	    break;

        case JK_AJP13_GET_BODY_CHUNK:
            {
		unsigned len = (unsigned)jk_b_get_int(msg);

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
		if (ajp_read_into_msg_buff(ae, r, msg, len, l)) {
		    r->content_read += len;
		    return JK_AJP13_HAS_RESPONSE;
		}                  

		jk_log(l, JK_LOG_ERROR, "Error ajp_process_callback - ajp_read_into_msg_buff failed\n");
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
	        jk_log(l, JK_LOG_ERROR, "Error ajp_process_callback - Invalid code: %d\n", code);
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
static int ajp_get_reply(jk_endpoint_t *e,
                         jk_ws_service_t *s,
                         jk_logger_t *l,
			             ajp_endpoint_t *p,
			             ajp_operation_t *op)
{
	/* Start read all reply message */
	while(1) {
		int rc = 0;

		if(!ajp_connection_tcp_get_message(p, op->reply, l)) {
			jk_log(l, JK_LOG_ERROR, "Error reading reply\n");
			/* we just can't recover, unset recover flag */
			return JK_FALSE;
		}

		rc = ajp_process_callback(op->reply, op->post, p, s, l);

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
			rc = ajp_connection_tcp_send_message(p, op->post, l);
        		if (rc < 0) {
				jk_log(l, JK_LOG_ERROR, "Error sending request data %d\n", rc);
               	 		return JK_FALSE;
			}
        } else if(JK_FATAL_ERROR == rc) {
          /*
           * we won't be able to gracefully recover from this so
           * set recoverable to false and get out.
           */
            op->recoverable = JK_FALSE;
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

#define	JK_RETRIES 3

/*
 * service is now splitted in ajp_send_request and ajp_get_reply
 * much more easier to do errors recovery
 *
 * We serve here the request, using AJP13/AJP14 (e->proto)
 *
 */
int JK_METHOD ajp_service(jk_endpoint_t   *e, 
                jk_ws_service_t *s,
                jk_logger_t     *l,
                int             *is_recoverable_error)
{
	int i;
	ajp_operation_t	oper;
	ajp_operation_t	*op = &oper;

	jk_log(l, JK_LOG_DEBUG, "Into jk_endpoint_t::service\n");

	if(e && e->endpoint_private && s && is_recoverable_error) {
		ajp_endpoint_t *p = e->endpoint_private;
       		op->request = jk_b_new(&(p->pool));
		jk_b_set_buffer_size(op->request, DEF_BUFFER_SZ); 
		jk_b_reset(op->request);
       
		op->reply = jk_b_new(&(p->pool));
		jk_b_set_buffer_size(op->reply, DEF_BUFFER_SZ);
		jk_b_reset(op->reply); 
		
		op->post = jk_b_new(&(p->pool));
		jk_b_set_buffer_size(op->post, DEF_BUFFER_SZ);
		jk_b_reset(op->post); 
		
		op->recoverable = JK_TRUE;
		op->uploadfd	 = -1;		/* not yet used, later ;) */

		p->left_bytes_to_send = s->content_length;
		p->reuse = JK_FALSE;
		*is_recoverable_error = JK_TRUE;

		/* 
		 * We get here initial request (in reqmsg)
		 */
		if (!ajp_marshal_into_msgb(op->request, s, l, p)) {
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
			if (ajp_send_request(e, s, l, p, op)) {

				/* If we have the no recoverable error, it's probably because the sender (browser)
				 * stop sending data before the end (certainly in a big post)
				 */
				if (! op->recoverable) {
					*is_recoverable_error = JK_FALSE;
					jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, ajp_send_request failed without recovery in send loop %d\n", i);
					return JK_FALSE;
				}

				/* Up to there we can recover */
				*is_recoverable_error = JK_TRUE;
				op->recoverable = JK_TRUE;

				if (ajp_get_reply(e, s, l, p, op))
					return (JK_TRUE);

				/* if we can't get reply, check if no recover flag was set 
				 * if is_recoverable_error is cleared, we have started received 
				 * upload data and we must consider that operation is no more recoverable
				 */
				if (! op->recoverable) {
					*is_recoverable_error = JK_FALSE;
					jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, ajp_get_reply failed without recovery in send loop %d\n", i);
					return JK_FALSE;
				}
				
				jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, ajp_get_reply failed in send loop %d\n", i);
			}
			else
				jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, ajp_send_request failed in send loop %d\n", i);
		
			jk_close_socket(p->sd);
			p->sd = -1;
			ajp_reuse_connection(p, l);
		}
	} else {
        	jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, NULL parameters\n");
	}

	return JK_FALSE;
}
/*
 * Validate the worker (ajp13/ajp14)
 */

int ajp_validate(jk_worker_t *pThis,
                 jk_map_t    *props,
                 jk_worker_env_t *we,
                 jk_logger_t *l,
			  	 int          proto)
{
	int    port;
	char * host;

    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::validate\n");

	if (proto == AJP13_PROTO) {
		port = AJP13_DEF_PORT;
		host = AJP13_DEF_HOST;
	}
	else if (proto == AJP14_PROTO) {
		port = AJP14_DEF_PORT;
		host = AJP14_DEF_HOST;
	}
	else {
		jk_log(l, JK_LOG_DEBUG, "In jk_worker_t::validate unknown protocol %d\n", proto);
		return JK_FALSE;
	} 

    if (pThis && pThis->worker_private) {
        ajp_worker_t *p = pThis->worker_private;
        port = jk_get_worker_port(props, p->name, port);
        host = jk_get_worker_host(props, p->name, host);

        jk_log(l, JK_LOG_DEBUG, "In jk_worker_t::validate for worker %s contact is %s:%d\n", p->name, host, port);

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


int ajp_init(jk_worker_t *pThis,
             jk_map_t    *props,
             jk_worker_env_t *we,
             jk_logger_t *l,
			 int          proto)
{
	int cache;
	
    /*
     * start the connection cache
     */
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::init\n");

    if (proto == AJP13_PROTO) {
        cache = AJP13_DEF_CACHE_SZ;
    }
    else if (proto == AJP14_PROTO) {
        cache = AJP13_DEF_CACHE_SZ;
    }
    else {
        jk_log(l, JK_LOG_DEBUG, "In jk_worker_t::validate unknown protocol %d\n", proto);
        return JK_FALSE;
    }

    if (pThis && pThis->worker_private) {
        ajp_worker_t *p = pThis->worker_private;
        int cache_sz = jk_get_worker_cache_size(props, p->name, cache);
        if (cache_sz > 0) {
            p->ep_cache = (ajp_endpoint_t **)malloc(sizeof(ajp_endpoint_t *) * cache_sz);
            if(p->ep_cache) {
                int i;
                p->ep_cache_sz = cache_sz;
                for(i = 0 ; i < cache_sz ; i++) {
                    p->ep_cache[i] = NULL;
                }
                JK_INIT_CS(&(p->cs), i);
                if (i) {
                    return JK_TRUE;
                }
            }
        }
    } else {
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::init, NULL parameters\n");
    }

    return JK_FALSE;
}

int ajp_destroy(jk_worker_t **pThis,
                jk_logger_t *l,
				int          proto)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::destroy\n");

    if (pThis && *pThis && (*pThis)->worker_private) {
        ajp_worker_t *aw = (*pThis)->worker_private;

       	free(aw->name);

		jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::destroy up to %d endpoint to close\n", aw->ep_cache_sz);

        if(aw->ep_cache_sz) {
            unsigned i;
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

    jk_log(l, JK_LOG_ERROR, "In jk_worker_t::destroy, NULL parameters\n");
    return JK_FALSE;
}

int JK_METHOD ajp_done(jk_endpoint_t **e,
                       jk_logger_t    *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_endpoint_t::done\n");

    if (e && *e && (*e)->endpoint_private) {
        ajp_endpoint_t *p = (*e)->endpoint_private;
        int reuse_ep = p->reuse;

        ajp_reset_endpoint(p);

        if(reuse_ep) {
            ajp_worker_t *w = p->worker;
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

        ajp_close_endpoint(p, l);
        *e = NULL;

        return JK_TRUE;
    }

    jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::done, NULL parameters\n");
    return JK_FALSE;
}

int ajp_get_endpoint(jk_worker_t    *pThis,
                     jk_endpoint_t **je,
                     jk_logger_t    *l,
					 int			 proto)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::get_endpoint\n");

    if (pThis && pThis->worker_private && je) {
        ajp_worker_t   *aw = pThis->worker_private;
        ajp_endpoint_t *ae = NULL;

        if (aw->ep_cache_sz) {
            int rc;
            JK_ENTER_CS(&aw->cs, rc);
            if (rc) {
                unsigned i;
    
                for (i = 0 ; i < aw->ep_cache_sz ; i++) {
                    if (aw->ep_cache[i]) {
                        ae = aw->ep_cache[i];
                        aw->ep_cache[i] = NULL;
                        break;
                    }
                }
                JK_LEAVE_CS(&aw->cs, rc);
                if (ae) {
                    *je = &ae->endpoint;
                    return JK_TRUE;
                }
            }
        }

        ae = (ajp_endpoint_t *)malloc(sizeof(ajp_endpoint_t));
        if (ae) {
            ae->sd = -1;
            ae->reuse = JK_FALSE;
            jk_open_pool(&ae->pool, ae->buf, sizeof(ae->buf));
            ae->worker = pThis->worker_private;
            ae->endpoint.endpoint_private = ae;
			ae->proto = proto;
            ae->endpoint.service = ajp_service;
            ae->endpoint.done = ajp_done;
            *je = &ae->endpoint;
            return JK_TRUE;
        }
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::get_endpoint, malloc failed\n");
    } else {
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::get_endpoint, NULL parameters\n");
    }

    return JK_FALSE;
}
