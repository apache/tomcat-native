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
 * Marshalling for request elements.
 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           
 * Author:      Henri Gomez <hgomez@slib.fr>                               
 */


#include "jk_global.h"
#include "jk_ajp14.h"
#include "jk_channel.h"
#include "jk_env.h"
#include "jk_requtil.h"
#include "jk_msg_buff.h"

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
#define SC_A_SSL_KEY_SIZE       (unsigned char)11		/* only in if JkOptions +ForwardKeySize */
#define SC_A_ARE_DONE           (unsigned char)0xFF

int jk_handler_request_marshal(jk_msg_buf_t    *msg,
                               jk_ws_service_t *s,
                               jk_logger_t     *l,
                               jk_endpoint_t  *ae);

/**
  Message structure
 
 
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
    ?ssl_key_size  (byte)(int)		via JkOptions +ForwardKeySize
    request_terminator (byte)
    ?body          content_length*(var binary)

    Was: ajp_marshal_into_msgb
 */
int jk_handler_request_marshal(jk_msg_buf_t    *msg,
                               jk_ws_service_t *s,
                               jk_logger_t     *l,
                               jk_endpoint_t  *ae)
{
    unsigned char method;
    unsigned i;

    l->jkLog(l, JK_LOG_DEBUG, "Into ajp_marshal_into_msgb\n");

    if (!jk_requtil_getMethodId(s->method, &method)) { 
        l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - No such method %s\n", s->method);
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

        l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the message begining\n");
        return JK_FALSE;
    }

    for (i = 0 ; i < s->num_headers ; i++) {
        unsigned short sc;

        if (jk_requtil_getHeaderId(s->headers_names[i], &sc)) {
            if (jk_b_append_int(msg, sc)) {
                l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the header name\n");
                return JK_FALSE;
            }
        } else {
            if (jk_b_append_string(msg, s->headers_names[i])) {
                l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the header name\n");
                return JK_FALSE;
            }
        }
        
        if (jk_b_append_string(msg, s->headers_values[i])) {
            l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the header value\n");
            return JK_FALSE;
        }
    }

    if (s->remote_user) {
        if (jk_b_append_byte(msg, SC_A_REMOTE_USER) ||
            jk_b_append_string(msg, s->remote_user)) {
            l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the remote user\n");
            return JK_FALSE;
        }
    }
    if (s->auth_type) {
        if (jk_b_append_byte(msg, SC_A_AUTH_TYPE) ||
            jk_b_append_string(msg, s->auth_type)) {
            l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the auth type\n");
            return JK_FALSE;
        }
    }
    if (s->query_string) {
        if (jk_b_append_byte(msg, SC_A_QUERY_STRING) ||
            jk_b_append_string(msg, s->query_string)) {
            l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the query string\n");
            return JK_FALSE;
        }
    }
    if (s->jvm_route) {
        if (jk_b_append_byte(msg, SC_A_JVM_ROUTE) ||
            jk_b_append_string(msg, s->jvm_route)) {
            l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the jvm route\n");
            return JK_FALSE;
        }
    }
    if (s->ssl_cert_len) {
        if (jk_b_append_byte(msg, SC_A_SSL_CERT) ||
            jk_b_append_string(msg, s->ssl_cert)) {
            l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the SSL certificates\n");
            return JK_FALSE;
        }
    }

    if (s->ssl_cipher) {
        if (jk_b_append_byte(msg, SC_A_SSL_CIPHER) ||
            jk_b_append_string(msg, s->ssl_cipher)) {
            l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the SSL ciphers\n");
            return JK_FALSE;
        }
    }
    if (s->ssl_session) {
        if (jk_b_append_byte(msg, SC_A_SSL_SESSION) ||
            jk_b_append_string(msg, s->ssl_session)) {
            l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the SSL session\n");
            return JK_FALSE;
        }
    }

    /*
     * ssl_key_size is required by Servlet 2.3 API
     * added support only in ajp14 mode
     * JFC removed: ae->proto == AJP14_PROTO
     */
    if (s->ssl_key_size != -1) {
        if (jk_b_append_byte(msg, SC_A_SSL_KEY_SIZE) ||
            jk_b_append_int(msg, (unsigned short) s->ssl_key_size)) {
            l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the SSL key size\n");
            return JK_FALSE;
        }
    }


    if (s->num_attributes > 0) {
        for (i = 0 ; i < s->num_attributes ; i++) {
            if (jk_b_append_byte(msg, SC_A_REQ_ATTRIBUTE)       ||
                jk_b_append_string(msg, s->attributes_names[i]) ||
                jk_b_append_string(msg, s->attributes_values[i])) {
                l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending attribute %s=%s\n",
                      s->attributes_names[i], s->attributes_values[i]);
                return JK_FALSE;
            }
        }
    }

    if (jk_b_append_byte(msg, SC_A_ARE_DONE)) {
        l->jkLog(l, JK_LOG_ERROR, "Error ajp_marshal_into_msgb - Error appending the message end\n");
        return JK_FALSE;
    }
    
    l->jkLog(l, JK_LOG_DEBUG, "ajp_marshal_into_msgb - Done\n");
    return JK_TRUE;
}
