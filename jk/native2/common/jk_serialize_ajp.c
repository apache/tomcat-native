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
 * Serializing requests.
 *
 * @author: Gal Shachor <shachor@il.ibm.com>                           
 * @author: Henri Gomez <hgomez@slib.fr>
 * @author: Costin Manolache
 */


#include "jk_global.h"
#include "jk_handler.h"
#include "jk_channel.h"
#include "jk_env.h"
#include "jk_requtil.h"
#include "jk_msg.h"
#include "jk_ajp14.h"

/*
 * Forward a request from the web server to the servlet container.
 */
#define JK_AJP13_FORWARD_REQUEST    (unsigned char)2

/* Important: ajp13 protocol has the strange habit of sending
   a second ( untyped ) message imediately following the request,
   with a first chunk of POST body. This is nice for small post
   requests, since it avoids a roundtrip, but it's horrible
   because it brakes the model. So we'll have to remember this
   as an exception to the rule as long as ajp13 is alive
*/

#define JK_AJP13_SHUTDOWN           (unsigned char)7

#define JK_AJP13_PING           (unsigned char)8

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
int jk_serialize_ping(jk_env_t *env, jk_msg_t *msg,
                      jk_endpoint_t *ae)
{
    int rc;
    
    /* To be on the safe side */
    msg->reset(env, msg);

    /* SHUTDOWN CMD */
    rc= msg->appendByte( env, msg, JK_AJP13_PING);
    if (rc!=JK_TRUE )
        return JK_FALSE;

    return JK_TRUE;
}


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
/* only in if JkOptions +ForwardKeySize */
#define SC_A_SSL_KEY_SIZE       (unsigned char)11		
#define SC_A_ARE_DONE           (unsigned char)0xFF



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
int jk_serialize_request13(jk_env_t *env, jk_msg_t *msg,
                           jk_ws_service_t *s )
{
    unsigned char method;
    int i;
    int headerCount;

    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "Into ajp_marshal_into_msgb\n");

    if (!jk_requtil_getMethodId(env, s->method, &method)) { 
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "Error ajp_marshal_into_msgb - No such method %s\n",
                      s->method);
        return JK_FALSE;
    }

    headerCount=s->headers_in->size(env, s->headers_in);
    
    if (msg->appendByte(env, msg, JK_AJP13_FORWARD_REQUEST)  ||
        msg->appendByte(env, msg, method)               ||
        msg->appendString(env, msg, s->protocol)        ||
        msg->appendString(env, msg, s->req_uri)         ||
        msg->appendString(env, msg, s->remote_addr)     ||
        msg->appendString(env, msg, s->remote_host)     ||
        msg->appendString(env, msg, s->server_name)     ||
        msg->appendInt(env, msg, (unsigned short)s->server_port) ||
        msg->appendByte(env, msg, (unsigned char)(s->is_ssl)) ||
        msg->appendInt(env, msg, (unsigned short)(headerCount))) {

        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "handle.request()  Error serializing the message head\n");
        return JK_FALSE;
    }

    for (i = 0 ; i < headerCount ; i++) {
        unsigned short sc;

        char *name=s->headers_in->nameAt(env, s->headers_in, i);

        if (jk_requtil_getHeaderId(env, name, &sc)) {
            if (msg->appendInt(env, msg, sc)) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "handle.request() Error serializing header id\n");
                return JK_FALSE;
            }
        } else {
            if (msg->appendString(env, msg, name)) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "handle.request() Error serializing header name\n");
                return JK_FALSE;
            }
        }
        
        if (msg->appendString(env, msg,
                              s->headers_in->valueAt( env, s->headers_in, i))) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing header value\n");
            return JK_FALSE;
        }
    }

    if (s->remote_user) {
        if (msg->appendByte(env, msg, SC_A_REMOTE_USER) ||
            msg->appendString(env, msg, s->remote_user)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing user name\n");
            return JK_FALSE;
        }
    }
    if (s->auth_type) {
        if (msg->appendByte(env, msg, SC_A_AUTH_TYPE) ||
            msg->appendString(env, msg, s->auth_type)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing auth type\n");
            return JK_FALSE;
        }
    }
    if (s->query_string) {
        if (msg->appendByte(env, msg, SC_A_QUERY_STRING) ||
            msg->appendString(env, msg, s->query_string)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing query string\n");
            return JK_FALSE;
        }
    }
    /* XXX This can be sent only on startup ( ajp14 ) */
     
    if (s->jvm_route) {
        if (msg->appendByte(env, msg, SC_A_JVM_ROUTE) ||
            msg->appendString(env, msg, s->jvm_route)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing worker id\n");
            return JK_FALSE;
        }
    }
    
    if (s->ssl_cert_len) {
        if (msg->appendByte(env, msg, SC_A_SSL_CERT) ||
            msg->appendString(env, msg, s->ssl_cert)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing SSL cert\n");
            return JK_FALSE;
        }
    }

    if (s->ssl_cipher) {
        if (msg->appendByte(env, msg, SC_A_SSL_CIPHER) ||
            msg->appendString(env, msg, s->ssl_cipher)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing SSL cipher\n");
            return JK_FALSE;
        }
    }
    if (s->ssl_session) {
        if (msg->appendByte(env, msg, SC_A_SSL_SESSION) ||
            msg->appendString(env, msg, s->ssl_session)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing SSL session\n");
            return JK_FALSE;
        }
    }

    /*
     * ssl_key_size is required by Servlet 2.3 API
     * added support only in ajp14 mode
     * JFC removed: ae->proto == AJP14_PROTO
     */
    if (s->ssl_key_size != -1) {
        if (msg->appendByte(env, msg, SC_A_SSL_KEY_SIZE) ||
            msg->appendInt(env, msg, (unsigned short) s->ssl_key_size)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing SSL key size\n");
            return JK_FALSE;
        }
    }


    if (s->attributes->size( env,  s->attributes) > 0) {
        for (i = 0 ; i < s->attributes->size( env,  s->attributes) ; i++) {
            char *name=s->attributes->nameAt( env,  s->attributes, i);
            char *val=s->attributes->nameAt( env, s->attributes, i);
            if (msg->appendByte(env, msg, SC_A_REQ_ATTRIBUTE) ||
                msg->appendString(env, msg, name ) ||
                msg->appendString(env, msg, val)) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                         "handle.request() Error serializing attribute %s=%s\n",
                         name, val);
                return JK_FALSE;
            }
        }
    }

    if (msg->appendByte(env, msg, SC_A_ARE_DONE)) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                 "handle.request() Error serializing end marker\n");
        return JK_FALSE;
    }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "handle.request() request serialized\n");
    return JK_TRUE;
}


/** The inital BODY chunk 
 */
int jk_serialize_postHead(jk_env_t *env, jk_msg_t   *msg,
                          jk_ws_service_t  *r,
                          jk_endpoint_t *e)
{
    int len = r->left_bytes_to_send;

    if(len > AJP13_MAX_SEND_BODY_SZ) {
        len = AJP13_MAX_SEND_BODY_SZ;
    }
    if(len <= 0) {
        len = 0;
        return JK_TRUE;
    }

    len=msg->appendFromServer( env, msg, r, e, len );
    /* the right place to add file storage for upload */
    if (len >= 0) {
        r->content_read += len;
        return JK_TRUE;
    }                  
            
    env->l->jkLog(env, env->l, JK_LOG_ERROR,
             "handler.marshapPostHead() - error len=%d\n", len);
    return JK_FALSE;	    
}

/* 
 * Build the Shutdown Cmd. ( no need to send any secret key,
 * we already authenticated when opening the channel )
 *
 * +-----------------------+
 * | SHUTDOWN CMD (1 byte) |
 * +-----------------------+
 *
 */
int jk_serialize_shutdown(jk_env_t *env, jk_msg_t *msg,
                          jk_ws_service_t  *r)
{
    int rc;
    
    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "Into ajp14_marshal_shutdown_into_msgb\n");

    /* To be on the safe side */
    msg->reset(env, msg);

    /* SHUTDOWN CMD */
    rc= msg->appendByte( env, msg, JK_AJP13_SHUTDOWN);
    if (rc!=JK_TRUE )
        return JK_FALSE;

    return JK_TRUE;
}




