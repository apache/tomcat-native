/*
 * Copyright (c) 1997-1999 The Java Apache Project.  All rights reserved.
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
 * Description: Experimental bi-directionl protocol handler.               *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/


#include "jk_global.h"
#include "jk_util.h"
#include "jk_ajp13.h"

/*
 * Conditional request attributes
 * 
 */
#define SC_A_CONTEXT            (unsigned char)1
#define SC_A_SERVLET_PATH       (unsigned char)2
#define SC_A_REMOTE_USER        (unsigned char)3
#define SC_A_AUTH_TYPE          (unsigned char)4
#define SC_A_QUERY_STRING       (unsigned char)5
#define SC_A_JVM_ROUTE          (unsigned char)6
#define SC_A_SSL_CERT           (unsigned char)7
#define SC_A_SSL_CIPHER         (unsigned char)8
#define SC_A_SSL_SESSION        (unsigned char)9
#define SC_A_REQ_ATTRIBUTE      (unsigned char)10
#define SC_A_ARE_DONE           (unsigned char)0xFF

/*
 * Request methods, coded as numbers instead of strings.
 * The list of methods was taken from Section 5.1.1 of RFC 2616,
 * RFC 2518, and the ACL IETF draft.
 *          Method        = "OPTIONS"
 *                        | "GET"    
 *                        | "HEAD"   
 *                        | "POST"   
 *                        | "PUT"    
 *                        | "DELETE" 
 *                        | "TRACE"  
 *                        | "PROPFIND"
 *                        | "PROPPATCH"
 *                        | "MKCOL"
 *                        | "COPY"
 *                        | "MOVE"
 *                        | "LOCK"
 *                        | "UNLOCK"
 *                        | "ACL"
 * 
 */
#define SC_M_OPTIONS            (unsigned char)1
#define SC_M_GET                (unsigned char)2
#define SC_M_HEAD               (unsigned char)3
#define SC_M_POST               (unsigned char)4
#define SC_M_PUT                (unsigned char)5
#define SC_M_DELETE             (unsigned char)6
#define SC_M_TRACE              (unsigned char)7
#define SC_M_PROPFIND           (unsigned char)8
#define SC_M_PROPPATCH          (unsigned char)9
#define SC_M_MKCOL              (unsigned char)10
#define SC_M_COPY               (unsigned char)11
#define SC_M_MOVE               (unsigned char)12
#define SC_M_LOCK               (unsigned char)13
#define SC_M_UNLOCK             (unsigned char)14
#define SC_M_ACL		(unsigned char)15


/*
 * Frequent request headers, these headers are coded as numbers
 * instead of strings.
 * 
 * Accept
 * Accept-Charset
 * Accept-Encoding
 * Accept-Language
 * Authorization
 * Connection
 * Content-Type
 * Content-Length
 * Cookie
 * Cookie2
 * Host
 * Pragma
 * Referer
 * User-Agent
 * 
 */

#define SC_ACCEPT               (unsigned short)0xA001
#define SC_ACCEPT_CHARSET       (unsigned short)0xA002
#define SC_ACCEPT_ENCODING      (unsigned short)0xA003
#define SC_ACCEPT_LANGUAGE      (unsigned short)0xA004
#define SC_AUTHORIZATION        (unsigned short)0xA005
#define SC_CONNECTION           (unsigned short)0xA006
#define SC_CONTENT_TYPE         (unsigned short)0xA007
#define SC_CONTENT_LENGTH       (unsigned short)0xA008
#define SC_COOKIE               (unsigned short)0xA009    
#define SC_COOKIE2              (unsigned short)0xA00A
#define SC_HOST                 (unsigned short)0xA00B
#define SC_PRAGMA               (unsigned short)0xA00C
#define SC_REFERER              (unsigned short)0xA00D
#define SC_USER_AGENT           (unsigned short)0xA00E

/*
 * Frequent response headers, these headers are coded as numbers
 * instead of strings.
 * 
 * Content-Type
 * Content-Language
 * Content-Length
 * Date
 * Last-Modified
 * Location
 * Set-Cookie
 * Servlet-Engine
 * Status
 * WWW-Authenticate
 * 
 */

#define SC_RESP_CONTENT_TYPE        (unsigned short)0xA001
#define SC_RESP_CONTENT_LANGUAGE    (unsigned short)0xA002
#define SC_RESP_CONTENT_LENGTH      (unsigned short)0xA003
#define SC_RESP_DATE                (unsigned short)0xA004
#define SC_RESP_LAST_MODIFIED       (unsigned short)0xA005
#define SC_RESP_LOCATION            (unsigned short)0xA006
#define SC_RESP_SET_COOKIE          (unsigned short)0xA007
#define SC_RESP_SET_COOKIE2         (unsigned short)0xA008
#define SC_RESP_SERVLET_ENGINE      (unsigned short)0xA009
#define SC_RESP_STATUS              (unsigned short)0xA00A
#define SC_RESP_WWW_AUTHENTICATE    (unsigned short)0xA00B
#define SC_RES_HEADERS_NUM          11

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

const char *long_res_header_for_sc(int sc) 
{
    const char *rc = NULL;
    if(sc <= SC_RES_HEADERS_NUM && sc > 0) {
        rc = response_trans_headers[sc - 1];
    }

    return rc;
}


int sc_for_req_method(const char *method,
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
    } else {
        rc = JK_FALSE;
    }

    return rc;
}

int sc_for_req_header(const char *header_name,
                      unsigned short *sc) 
{
    switch(header_name[0]) {
        case 'a':
            if('c' ==header_name[1] &&
               'c' ==header_name[2] &&
               'e' ==header_name[3] &&
               'p' ==header_name[4] &&
               't' ==header_name[5]) {
                if('-' == header_name[6]) {
                    if(!strcmp(header_name + 7, "charset")) {
                        *sc = SC_ACCEPT_CHARSET;
                    } else if(!strcmp(header_name + 7, "encoding")) {
                        *sc = SC_ACCEPT_ENCODING;
                    } else if(!strcmp(header_name + 7, "language")) {
                        *sc = SC_ACCEPT_LANGUAGE;
                    } else {
                        return JK_FALSE;
                    }
                } else if('\0' == header_name[6]) {
                    *sc = SC_ACCEPT;
                } else {
                    return JK_FALSE;
                }
            } else if(!strcmp(header_name, "authorization")) {
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
AJPV13_REQUEST:=
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
    request_terminator (byte)
    ?body          content_length*(var binary)

 */

int ajp13_marshal_into_msgb(jk_msg_buf_t *msg,
                            jk_ws_service_t *s,
                            jk_logger_t *l)
{
    unsigned char method;
    unsigned i;

    jk_log(l, JK_LOG_DEBUG, 
           "Into ajp13_marshal_into_msgb\n");

    if(!sc_for_req_method(s->method, &method)) { 
        jk_log(l, JK_LOG_ERROR, 
               "Error ajp13_marshal_into_msgb - No such method %s\n", s->method);
        return JK_FALSE;
    }

    if(0 != jk_b_append_byte(msg, JK_AJP13_FORWARD_REQUEST)  ||
       0 != jk_b_append_byte(msg, method)               ||
       0 != jk_b_append_string(msg, s->protocol)        ||
       0 != jk_b_append_string(msg, s->req_uri)         ||
       0 != jk_b_append_string(msg, s->remote_addr)     ||
       0 != jk_b_append_string(msg, s->remote_host)     ||
       0 != jk_b_append_string(msg, s->server_name)     ||
       0 != jk_b_append_int(msg, (unsigned short)s->server_port) ||
       0 != jk_b_append_byte(msg, (unsigned char)(s->is_ssl)) ||
       0 != jk_b_append_int(msg, (unsigned short)(s->num_headers))) {

        jk_log(l, JK_LOG_ERROR, 
               "Error ajp13_marshal_into_msgb - Error appending the message begining\n");

        return JK_FALSE;
    }

    for(i = 0 ; i < s->num_headers ; i++) {
        unsigned short sc;

        if(sc_for_req_header(s->headers_names[i], &sc) ) {
            if(0 != jk_b_append_int(msg, sc)) {
                jk_log(l, JK_LOG_ERROR, 
                       "Error ajp13_marshal_into_msgb - Error appending the header name\n");
                return JK_FALSE;
            }
        } else {
            if(0 != jk_b_append_string(msg, s->headers_names[i])) {
                jk_log(l, JK_LOG_ERROR, 
                       "Error ajp13_marshal_into_msgb - Error appending the header name\n");
                return JK_FALSE;
            }
        }
        
        if(0 != jk_b_append_string(msg, s->headers_values[i])) {
            jk_log(l, JK_LOG_ERROR, 
                   "Error ajp13_marshal_into_msgb - Error appending the header value\n");
            return JK_FALSE;
        }
    }

    if(s->remote_user) {
        if(0 != jk_b_append_byte(msg, SC_A_REMOTE_USER) ||
           0 != jk_b_append_string(msg, s->remote_user)) {
            jk_log(l, JK_LOG_ERROR, 
                   "Error ajp13_marshal_into_msgb - Error appending the remote user\n");

            return JK_FALSE;
        }
    }
    if(s->auth_type) {
        if(0 != jk_b_append_byte(msg, SC_A_AUTH_TYPE) ||
           0 != jk_b_append_string(msg, s->auth_type)) {
            jk_log(l, JK_LOG_ERROR, 
                   "Error ajp13_marshal_into_msgb - Error appending the auth type\n");

            return JK_FALSE;
        }
    }
    if(s->query_string) {
        if(0 != jk_b_append_byte(msg, SC_A_QUERY_STRING) ||
           0 != jk_b_append_string(msg, s->query_string)) {
            jk_log(l, JK_LOG_ERROR, 
                   "Error ajp13_marshal_into_msgb - Error appending the query string\n");

            return JK_FALSE;
        }
    }
    if(s->jvm_route) {
        if(0 != jk_b_append_byte(msg, SC_A_JVM_ROUTE) ||
           0 != jk_b_append_string(msg, s->jvm_route)) {
            jk_log(l, JK_LOG_ERROR, 
                   "Error ajp13_marshal_into_msgb - Error appending the jvm route\n");

            return JK_FALSE;
        }
    }
    if(s->ssl_cert_len) {
        if(0 != jk_b_append_byte(msg, SC_A_SSL_CERT) ||
           0 != jk_b_append_string(msg, s->ssl_cert)) {
            jk_log(l, JK_LOG_ERROR, 
                   "Error ajp13_marshal_into_msgb - Error appending the SSL certificates\n");

            return JK_FALSE;
        }
    }

    if(s->ssl_cipher) {
        if(0 != jk_b_append_byte(msg, SC_A_SSL_CIPHER) ||
           0 != jk_b_append_string(msg, s->ssl_cipher)) {
            jk_log(l, JK_LOG_ERROR, 
                   "Error ajp13_marshal_into_msgb - Error appending the SSL ciphers\n");

            return JK_FALSE;
        }
    }
    if(s->ssl_session) {
        if(0 != jk_b_append_byte(msg, SC_A_SSL_SESSION) ||
           0 != jk_b_append_string(msg, s->ssl_session)) {
            jk_log(l, JK_LOG_ERROR, 
                   "Error ajp13_marshal_into_msgb - Error appending the SSL session\n");

            return JK_FALSE;
        }
    }

    if(s->num_attributes > 0) {
        for(i = 0 ; i < s->num_attributes ; i++) {
            if(0 != jk_b_append_byte(msg, SC_A_REQ_ATTRIBUTE)       ||
               0 != jk_b_append_string(msg, s->attributes_names[i]) ||
               0 != jk_b_append_string(msg, s->attributes_values[i])) {
                jk_log(l, JK_LOG_ERROR, 
                   "Error ajp13_marshal_into_msgb - Error appending attribute %s=%s\n",
                   s->attributes_names[i], s->attributes_values[i]);
                return JK_FALSE;
            }
        }
    }

    if(0 != jk_b_append_byte(msg, SC_A_ARE_DONE)) {
        jk_log(l, JK_LOG_ERROR, 
               "Error ajp13_marshal_into_msgb - Error appending the message end\n");
        return JK_FALSE;
    }
    
    jk_log(l, JK_LOG_DEBUG, 
           "ajp13_marshal_into_msgb - Done\n");
    return JK_TRUE;
}

/*
AJPV13_RESPONSE:=
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


int ajp13_unmarshal_response(jk_msg_buf_t *msg,
                             jk_res_data_t *d,
                             jk_pool_t *p,
                             jk_logger_t *l)
{
    d->status = jk_b_get_int(msg);

    if(!d->status) {
        jk_log(l, JK_LOG_ERROR, 
               "Error ajp13_unmarshal_response - Null status\n");

        return JK_FALSE;
    }

    d->msg = (char *)jk_b_get_string(msg);

    jk_log(l, JK_LOG_DEBUG, 
           "ajp13_unmarshal_response: status = %d\n",
           d->status);

    d->num_headers = jk_b_get_int(msg);
    d->header_names = d->header_values = NULL;

    jk_log(l, JK_LOG_DEBUG, 
           "ajp13_unmarshal_response: Number of headers is = %d\n",
           d->num_headers);

    if(d->num_headers) {
        d->header_names = jk_pool_alloc(p, sizeof(char *) * d->num_headers);
        d->header_values = jk_pool_alloc(p, sizeof(char *) * d->num_headers);

        if(d->header_names && d->header_values) {
            unsigned i;
            for(i = 0 ; i < d->num_headers ; i++) {
                unsigned short name = jk_b_pget_int(msg, jk_b_get_pos(msg)) ;
                
                if((name & 0XFF00) == 0XA000) {
                    jk_b_get_int(msg);
                    name = name & 0X00FF;
                    if(name <= SC_RES_HEADERS_NUM) {
                        d->header_names[i] = (char *)long_res_header_for_sc(name);
                    } else {
                        jk_log(l, JK_LOG_ERROR, 
                               "Error ajp13_unmarshal_response - No such sc (%d)\n",
			       name);

                        return JK_FALSE;
                    }
                } else {
                    d->header_names[i] = (char *)jk_b_get_string(msg);
                    if(!d->header_names[i]) {
                        jk_log(l, JK_LOG_ERROR, 
                               "Error ajp13_unmarshal_response - Null header name\n");

                        return JK_FALSE;
                    }
                }

                d->header_values[i] = (char *)jk_b_get_string(msg);
                if(!d->header_values[i]) {
                    jk_log(l, JK_LOG_ERROR, 
                           "Error ajp13_unmarshal_response - Null header value\n");

                    return JK_FALSE;
                }

                jk_log(l, JK_LOG_DEBUG, 
                    "ajp13_unmarshal_response: Header[%d] [%s] = [%s]\n",
                    i,
                    d->header_names[i],
                    d->header_values[i]);
            }
        }
    }

    return JK_TRUE;
}

int ajp13_marshal_shutdown_into_msgb(jk_msg_buf_t *msg,
                                     jk_pool_t *p,
                                     jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, 
           "Into ajp13_marshal_shutdown_into_msgb\n");
    
    /* To be on the safe side */
    jk_b_reset(msg);

    /*
     * Just a single byte with s/d command.
     */
    if(0 != jk_b_append_byte(msg, JK_AJP13_SHUTDOWN)) {
        return JK_FALSE;
    }

    return JK_TRUE;
}
