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
 * Utils for processing various request components
 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           
 * Author:      Henri Gomez <hgomez@slib.fr>                               
 * Author:      Costin Manolache
 */

/* XXX make them virtual methods, allow servers to override
 */

#include "jk_global.h"
#include "jk_channel.h"
#include "jk_env.h"
#include "jk_requtil.h"

#define CHUNK_BUFFER_PAD          (12)

static const char *response_trans_headers[] = {
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
#define SC_A_SECRET             (unsigned char)12
#define SC_A_ARE_DONE           (unsigned char)0xFF

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


/** Get header value using a lookup table. 
 *
 *
 * long_res_header_for_sc
 */
const char *jk2_requtil_getHeaderById(jk_env_t *env, int sc) 
{
    const char *rc = NULL;
    if(sc <= SC_RES_HEADERS_NUM && sc > 0) {
        rc = response_trans_headers[sc - 1];
    }

    return rc;
}

/**
 * Get method id. 
 *
 * sc_for_req_method
 */
int jk2_requtil_getMethodId(jk_env_t *env, const char *method,
                           unsigned char *sc) 
{
    int rc = JK_OK;
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
    } else if(0 == strcmp(method, "VERSION-CONTROL")) {
        *sc = SC_M_VERSION_CONTROL;
    } else if(0 == strcmp(method, "CHECKIN")) {
        *sc = SC_M_CHECKIN;
    } else if(0 == strcmp(method, "CHECKOUT")) {
        *sc = SC_M_CHECKOUT;
    } else if(0 == strcmp(method, "UNCHECKOUT")) {
        *sc = SC_M_UNCHECKOUT;
    } else if(0 == strcmp(method, "SEARCH")) {
        *sc = SC_M_SEARCH;
    } else {
        rc = JK_ERR;
    }

    return rc;
}

/**
 * Get header id.
 *
 * sc_for_req_header
 */
int  jk2_requtil_getHeaderId(jk_env_t *env, const char *header_name,
                            unsigned short *sc) 
{
/*     char lowerCased[30]; */
    
/*     if( strlen( header_name ) > 30 ) */
/*         return JK_FALSE; */
/*     strncpy( lowerCased, header_name,  30 ); */
    
    
    switch(header_name[0]) {
    case 'a':
    case 'A':
        if(strncasecmp( header_name, "accept", 6 ) == 0 ) {
            if ('-' == header_name[6]) {
                if(!strcasecmp(header_name + 7, "charset")) {
                    *sc = SC_ACCEPT_CHARSET;
                } else if(!strcasecmp(header_name + 7, "encoding")) {
                    *sc = SC_ACCEPT_ENCODING;
                } else if(!strcasecmp(header_name + 7, "language")) {
                    *sc = SC_ACCEPT_LANGUAGE;
                } else {
                    return JK_ERR;
                }
            } else if ('\0' == header_name[6]) {
                *sc = SC_ACCEPT;
            } else {
                return JK_ERR;
            }
        } else if (!strcasecmp(header_name, "authorization")) {
            *sc = SC_AUTHORIZATION;
        } else {
            return JK_ERR;
        }
        break;
        
    case 'c':
    case 'C':
        if(!strcasecmp(header_name, "cookie")) {
            *sc = SC_COOKIE;
        } else if(!strcasecmp(header_name, "connection")) {
            *sc = SC_CONNECTION;
        } else if(!strcasecmp(header_name, "content-type")) {
            *sc = SC_CONTENT_TYPE;
        } else if(!strcasecmp(header_name, "content-length")) {
            *sc = SC_CONTENT_LENGTH;
        } else if(!strcasecmp(header_name, "cookie2")) {
            *sc = SC_COOKIE2;
        } else {
            return JK_ERR;
        }
        break;
        
    case 'h':
    case 'H':
        if(!strcasecmp(header_name, "host")) {
            *sc = SC_HOST;
        } else {
            return JK_ERR;
        }
        break;
        
    case 'p':
    case 'P':
        if(!strcasecmp(header_name, "pragma")) {
            *sc = SC_PRAGMA;
        } else {
            return JK_ERR;
        }
        break;
        
    case 'r':
    case 'R':
        if(!strcasecmp(header_name, "referer")) {
            *sc = SC_REFERER;
        } else {
            return JK_ERR;
        }
        break;
        
    case 'u':
    case 'U':
        if(!strcasecmp(header_name, "user-agent")) {
            *sc = SC_USER_AGENT;
        } else {
            return JK_ERR;
        }
        break;
        
    default:
/*         env->l->jkLog(env, env->l, JK_LOG_INFO,  */
/*                       "requtil.getHeaderId() long header %s\n", header_name); */

        return JK_ERR;
    }
    return JK_OK;
}

/** Retrieve the cookie with the given name
 */
char *jk2_requtil_getCookieByName(jk_env_t *env, jk_ws_service_t *s,
                                  const char *name)
{
    int i;
    jk_map_t *headers=s->headers_in;

    /* XXX use 'get' - and make sure jk_map has support for
       case insensitive search */
    for(i = 0 ; i < headers->size( NULL, headers ) ; i++) {
        if(0 == strcasecmp(headers->nameAt( NULL, headers, i), "cookie")) {

            char *id_start;
            for(id_start = strstr( headers->valueAt( NULL, headers, i ), name) ; 
                id_start ; 
                id_start = strstr(id_start + 1, name)) {
                if('=' == id_start[strlen(name)]) {
                    /*
                     * Session cookie was found, get it's value
                     */
                    id_start += (1 + strlen(name));
                    if(strlen(id_start)) {
                        char *id_end;
                        id_start = s->pool->pstrdup(env, s->pool, id_start);
                        if(id_end = strchr(id_start, ';')) {
                            *id_end = '\0';
                        }
                        return id_start;
                    }
                }
            }
        }
    }

    return NULL;
}

/* Retrieve the parameter with the given name
 */
char *jk2_requtil_getPathParam(jk_env_t *env, jk_ws_service_t *s,
                              const char *name)
{
    char *id_start = NULL;
    for(id_start = strstr(s->req_uri, name) ; 
        id_start ; 
        id_start = strstr(id_start + 1, name)) {
        if('=' == id_start[strlen(name)]) {
            /*
             * Session path-cookie was found, get it's value
             */
            id_start += (1 + strlen(name));
            if(strlen(id_start)) {
                char *id_end;
                id_start = s->pool->pstrdup(env, s->pool, id_start);
                /* 
                 * The query string is not part of req_uri, however
                 * to be on the safe side lets remove the trailing query 
                 * string if appended...
                 */
                if(id_end = strchr(id_start, '?')) { 
                    *id_end = '\0';
                }
                return id_start;
            }
        }
    }
  
    return NULL;
}

/** Retrieve session id from the cookie or the parameter                      
 * (parameter first)
 */
char *jk2_requtil_getSessionId(jk_env_t *env, jk_ws_service_t *s)
{
    char *val;
    val = jk2_requtil_getPathParam(env, s, JK_PATH_SESSION_IDENTIFIER);
    if(!val) {
        val = jk2_requtil_getCookieByName(env, s, JK_SESSION_IDENTIFIER);
    }
    return val;
}

/** Extract the 'route' from the session id. The route is
 *  the id of the worker that generated the session and where all
 *  further requests in that session will be sent.
*/
char *jk2_requtil_getSessionRoute(jk_env_t *env, jk_ws_service_t *s)
{
    char *sessionid = jk2_requtil_getSessionId(env, s);
    char *ch;

    if(!sessionid) {
        return NULL;
    }

    /*
     * Balance parameter is appended to the end
     */  
    ch = strrchr(sessionid, '.');
    if(!ch) {
        return 0;
    }
    ch++;
    if(*ch == '\0') {
        return NULL;
    }
    return ch;
}

/*
 * Read data from the web server.
 *
 * Socket API didn't garanty all the data will be kept in a single 
 * read, so we must loop up to all awaited data are received 
 */
int jk2_requtil_readFully(jk_env_t *env, jk_ws_service_t *s,
                         unsigned char *buf,
                         unsigned  len)
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
        if(s->read(env, s, buf + rdlen, len - rdlen, &this_time)) {
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


/* -------------------- Printf writing -------------------- */

#define JK_BUF_SIZE 4096

static int jk2_requtil_createBuffer(jk_env_t *env, 
                             jk_ws_service_t *s)
{
    int bsize=JK_BUF_SIZE;
    
    s->outSize=bsize;
    s->outBuf=(char *)s->pool->alloc( env, s->pool, bsize );

    return JK_OK;
}

static void jk2_requtil_printf(jk_env_t *env, jk_ws_service_t *s, char *fmt, ...)
{
    va_list vargs;
    int ret=0;

    if( s->outBuf==NULL ) {
        jk2_requtil_createBuffer( env, s );
    }
    
    va_start(vargs,fmt);
    s->outPos=0; /* Temp - we don't buffer */
    ret=vsnprintf(s->outBuf + s->outPos, s->outSize - s->outPos, fmt, vargs);
    va_end(vargs);

    s->write( env, s, s->outBuf, strlen(s->outBuf) );
}

/* -------------------- Request serialization -------------------- */
/* XXX optimization - this can be overriden by server to avoid
   multiple copies
*/
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
int jk2_serialize_request13(jk_env_t *env, jk_msg_t *msg,
                            jk_ws_service_t *s,
                            jk_endpoint_t *ae)
{
    unsigned char method;
    int i;
    int headerCount;
    int rc;
    int debug=0;

    if( s->uriEnv != NULL ) {
        debug=s->uriEnv->mbean->debug;
    }
    
    rc=jk2_requtil_getMethodId(env, s->method, &method);
    if (rc!=JK_OK) { 
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "Error ajp_marshal_into_msgb - No such method %s\n",
                      s->method);
        return JK_ERR;
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
        return JK_ERR;
    }

    for (i = 0 ; i < headerCount ; i++) {
        unsigned short sc;

        char *name=s->headers_in->nameAt(env, s->headers_in, i);

        if (jk2_requtil_getHeaderId(env, name, &sc) == JK_OK) {
            /*  env->l->jkLog(env, env->l, JK_LOG_INFO, */
            /*                "serialize.request() Add headerId %s %d\n", name, sc); */
            if (msg->appendInt(env, msg, sc)) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "serialize.request() Error serializing header id\n");
                return JK_ERR;
            }
        } else {
            if( debug > 0 )
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                              "serialize.request() Add headerName %s\n", name);
            if (msg->appendString(env, msg, name)) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "serialize.request() Error serializing header name\n");
                return JK_ERR;
            }
        }
        
        if (msg->appendString(env, msg,
                               s->headers_in->valueAt( env, s->headers_in, i))) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "serialize.request() Error serializing header value\n");
            return JK_ERR;
        }
    }

    if (s->remote_user) {
        if (msg->appendByte(env, msg, SC_A_REMOTE_USER) ||
            msg->appendString(env, msg, s->remote_user)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "serialize.request() Error serializing user name\n");
            return JK_ERR;
        }
    }
    if (s->auth_type) {
        if (msg->appendByte(env, msg, SC_A_AUTH_TYPE) ||
            msg->appendString(env, msg, s->auth_type)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing auth type\n");
            return JK_ERR;
        }
    }
    if (s->query_string) {
        if (msg->appendByte(env, msg, SC_A_QUERY_STRING) ||
            msg->appendString(env, msg, s->query_string)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing query string\n");
            return JK_ERR;
        }
    }
    /* XXX This can be sent only on startup ( ajp14 ) */
     
    if (s->jvm_route) {
        if ( msg->appendByte(env, msg, SC_A_JVM_ROUTE) ||
             msg->appendString(env, msg, s->jvm_route)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing worker id\n");
            return JK_ERR;
        }
    }
    
    if (s->ssl_cert_len) {
        if ( msg->appendByte(env, msg, SC_A_SSL_CERT) ||
             msg->appendString(env, msg, s->ssl_cert)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing SSL cert\n");
            return JK_ERR;
        }
    }

    if (s->ssl_cipher) {
        if ( msg->appendByte(env, msg, SC_A_SSL_CIPHER) ||
             msg->appendString(env, msg, s->ssl_cipher)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing SSL cipher\n");
            return JK_ERR;
        }
    }
    if (s->ssl_session) {
        if ( msg->appendByte(env, msg, SC_A_SSL_SESSION) ||
             msg->appendString(env, msg, s->ssl_session)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing SSL session\n");
            return JK_ERR;
        }
    }

    /*
     * ssl_key_size is required by Servlet 2.3 API
     * added support only in ajp14 mode
     * JFC removed: ae->proto == AJP14_PROTO
     */
    if (s->ssl_key_size != -1) {
        if ( msg->appendByte(env, msg, SC_A_SSL_KEY_SIZE) ||
             msg->appendInt(env, msg, (unsigned short) s->ssl_key_size)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing SSL key size\n");
            return JK_ERR;
        }
    }

    if (ae->worker->secret ) {
        if ( msg->appendByte(env, msg, SC_A_SECRET) ||
             msg->appendString(env, msg, ae->worker->secret )) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "handle.request() Error serializing secret\n");
            return JK_ERR;
        }
    }


    if (s->attributes->size( env,  s->attributes) > 0) {
        for (i = 0 ; i < s->attributes->size( env,  s->attributes) ; i++) {
            char *name=s->attributes->nameAt( env,  s->attributes, i);
            char *val=s->attributes->nameAt( env, s->attributes, i);
            if ( msg->appendByte(env, msg, SC_A_REQ_ATTRIBUTE) ||
                 msg->appendString(env, msg, name ) ||
                 msg->appendString(env, msg, val)) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                         "handle.request() Error serializing attribute %s=%s\n",
                         name, val);
                return JK_ERR;
            }
        }
    }

    if ( msg->appendByte(env, msg, SC_A_ARE_DONE)) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                 "handle.request() Error serializing end marker\n");
        return JK_ERR;
    }

    
    if( debug > 0  )
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "serialize.request() serialized %s\n", s->req_uri);

    /*  msg->dump( env, msg, "Dump: " ); */
    return JK_OK;
}


/** The inital BODY chunk 
 */
int jk2_serialize_postHead(jk_env_t *env, jk_msg_t   *msg,
                           jk_ws_service_t  *r,
                           jk_endpoint_t *ae)
{
    int len = r->left_bytes_to_send;

    if(len > AJP13_MAX_SEND_BODY_SZ) {
        len = AJP13_MAX_SEND_BODY_SZ;
    }
    if(len <= 0) {
        len = 0;
        return JK_OK;
    }

    len=msg->appendFromServer( env, msg, r, ae, len );
    /* the right place to add file storage for upload */
    if (len >= 0) {
        r->content_read += len;
        return JK_OK;
    }                  
            
    env->l->jkLog(env, env->l, JK_LOG_ERROR,
             "handler.marshapPostHead() - error len=%d\n", len);
    return JK_ERR;	    
}

/* -------------------- Request encoding -------------------- */
/* Moved from IIS adapter */

#define T_OS_ESCAPE_PATH	(4)

static const unsigned char test_char_table[256] = {
    0,14,14,14,14,14,14,14,14,14,15,14,14,14,14,14,14,14,14,14,
    14,14,14,14,14,14,14,14,14,14,14,14,14,0,7,6,1,6,1,1,
    9,9,1,0,8,0,0,10,0,0,0,0,0,0,0,0,0,0,8,15,
    15,8,15,15,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,15,15,15,7,0,7,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,15,7,15,1,14,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6 
};

#define TEST_CHAR(c, f)	(test_char_table[(unsigned)(c)] & (f))

static const char c2x_table[] = "0123456789abcdef";

static unsigned char *c2x(unsigned what, unsigned char *where)
{
    *where++ = '%';
    *where++ = c2x_table[what >> 4];
    *where++ = c2x_table[what & 0xf];
    return where;
}

int jk_requtil_escapeUrl(const char *path, char *dest, int destsize)
{
    const unsigned char *s = (const unsigned char *)path;
    unsigned char *d = (unsigned char *)dest;
    unsigned char *e = dest + destsize - 1;
    unsigned char *ee = dest + destsize - 3;
    unsigned c;

    while ((c = *s)) {
	if (TEST_CHAR(c, T_OS_ESCAPE_PATH)) {
            if (d >= ee )
                return JK_ERR;
	    d = c2x(c, d);
	}
	else {
            if (d >= e )
                return JK_ERR;
	    *d++ = c;
	}
	++s;
    }
    *d = '\0';
    return JK_OK;
}

/* XXX Make it a default checking in uri worker map
 */
int jk_requtil_uriIsWebInf(char *uri)
{
    char *c = uri;
    while(*c) {
        *c = tolower(*c);
        c++;
    }                    
    if(strstr(uri, "web-inf")) {
        return JK_TRUE;
    }
    if(strstr(uri, "meta-inf")) {
        return JK_TRUE;
    }

    return JK_FALSE;
}

static char x2c(const char *what)
{
    register char digit;

    digit = ((what[0] >= 'A') ? ((what[0] & 0xdf) - 'A') + 10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A') + 10 : (what[1] - '0'));
    return (digit);
}

int jk_requtil_unescapeUrl(char *url)
{
    register int x, y, badesc, badpath;

    badesc = 0;
    badpath = 0;
    for (x = 0, y = 0; url[y]; ++x, ++y) {
        if (url[y] != '%')
            url[x] = url[y];
        else {
            if (!isxdigit(url[y + 1]) || !isxdigit(url[y + 2])) {
                badesc = 1;
                url[x] = '%';
            }
            else {
                url[x] = x2c(&url[y + 1]);
                y += 2;
                if (url[x] == '/' || url[x] == '\0')
                    badpath = 1;
            }
        }
    }
    url[x] = '\0';
    if (badesc)
        return -1;
    else if (badpath)
        return -2;
    else
        return JK_OK;
}

void jk_requtil_getParents(char *name)
{
    int l, w;

    /* Four paseses, as per RFC 1808 */
    /* a) remove ./ path segments */

    for (l = 0, w = 0; name[l] != '\0';) {
        if (name[l] == '.' && name[l + 1] == '/' && (l == 0 || name[l - 1] == '/'))
            l += 2;
        else
            name[w++] = name[l++];
    }

    /* b) remove trailing . path, segment */
    if (w == 1 && name[0] == '.')
        w--;
    else if (w > 1 && name[w - 1] == '.' && name[w - 2] == '/')
        w--;
    name[w] = '\0';

    /* c) remove all xx/../ segments. (including leading ../ and /../) */
    l = 0;

    while (name[l] != '\0') {
        if (name[l] == '.' && name[l + 1] == '.' && name[l + 2] == '/' &&
            (l == 0 || name[l - 1] == '/')) {
            register int m = l + 3, n;

            l = l - 2;
            if (l >= 0) {
                while (l >= 0 && name[l] != '/')
                    l--;
                l++;
            }
            else
                l = 0;
            n = l;
            while ((name[n] = name[m]))
                (++n, ++m);
        }
        else
            ++l;
    }

    /* d) remove trailing xx/.. segment. */
    if (l == 2 && name[0] == '.' && name[1] == '.')
        name[0] = '\0';
    else if (l > 2 && name[l - 1] == '.' && name[l - 2] == '.' && name[l - 3] == '/') {
        l = l - 4;
        if (l >= 0) {
            while (l >= 0 && name[l] != '/')
                l--;
            l++;
        }
        else
            l = 0;
        name[l] = '\0';
    }
}




static const char begin_cert [] = 
	"-----BEGIN CERTIFICATE-----\r\n";

static const char end_cert [] = 
	"-----END CERTIFICATE-----\r\n";

static const char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int jk_requtil_base64CertLen(int len)
{
    int n = ((len + 2) / 3 * 4) + 1; // base64 encoded size
    n += (n + 63 / 64) * 2; // add CRLF's
    n += sizeof(begin_cert) + sizeof(end_cert) - 2;  // add enclosing strings.
    return n;
}

int jk_requtil_base64EncodeCert(char *encoded,
                                const unsigned char *string, int len)
{
    int i,c;
    char *p;
    const char *t;
    
    p = encoded;

    t = begin_cert;
    while (*t != '\0')
        *p++ = *t++;
    
    c = 0;
    for (i = 0; i < len - 2; i += 3) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        *p++ = basis_64[((string[i] & 0x3) << 4) |
                        ((int) (string[i + 1] & 0xF0) >> 4)];
        *p++ = basis_64[((string[i + 1] & 0xF) << 2) |
                        ((int) (string[i + 2] & 0xC0) >> 6)];
        *p++ = basis_64[string[i + 2] & 0x3F];
        c += 4;
        if ( c >= 64 ) {
            *p++ = '\r';
            *p++ = '\n';
            c = 0;
		}
    }
    if (i < len) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        if (i == (len - 1)) {
            *p++ = basis_64[((string[i] & 0x3) << 4)];
            *p++ = '=';
        }
        else {
            *p++ = basis_64[((string[i] & 0x3) << 4) |
                            ((int) (string[i + 1] & 0xF0) >> 4)];
            *p++ = basis_64[((string[i + 1] & 0xF) << 2)];
        }
        *p++ = '=';
        c++;
    }
    if ( c != 0 ) {
        *p++ = '\r';
        *p++ = '\n';
    }

	t = end_cert;
	while (*t != '\0')
		*p++ = *t++;

    *p++ = '\0';
    return p - encoded;
}




/** Initialize the request 
 * 
 * jk_init_ws_service
 */ 
void jk2_requtil_initRequest(jk_env_t *env, jk_ws_service_t *s)
{
    s->ws_private           = NULL;
    s->method               = NULL;
    s->protocol             = NULL;
    s->req_uri              = NULL;
    s->remote_addr          = NULL;
    s->remote_host          = NULL;
    s->remote_user          = NULL;
    s->auth_type            = NULL;
    s->query_string         = NULL;
    s->server_name          = NULL;
    s->server_port          = 80;
    s->server_software      = NULL;
    s->content_length       = 0;
    s->is_chunked           = 0;
    s->no_more_chunks       = 0;
    s->content_read         = 0;
    s->is_ssl               = JK_FALSE;
    s->ssl_cert             = NULL;
    s->ssl_cert_len         = 0;
    s->ssl_cipher           = NULL;
    s->ssl_session          = NULL;
    s->jvm_route            = NULL;
    s->uriEnv               = NULL;
    s->outBuf               = NULL;
    s->msg                  = NULL;
    
    s->jkprintf=jk2_requtil_printf;
}
