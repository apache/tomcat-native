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
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
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
        rc = JK_FALSE;
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
                    return JK_FALSE;
                }
            } else if ('\0' == header_name[6]) {
                *sc = SC_ACCEPT;
            } else {
                return JK_FALSE;
            }
        } else if (!strcasecmp(header_name, "authorization")) {
            *sc = SC_AUTHORIZATION;
        } else {
            return JK_FALSE;
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
            return JK_FALSE;
        }
        break;
        
    case 'h':
    case 'H':
        if(!strcasecmp(header_name, "host")) {
            *sc = SC_HOST;
        } else {
            return JK_FALSE;
        }
        break;
        
    case 'p':
    case 'P':
        if(!strcasecmp(header_name, "pragma")) {
            *sc = SC_PRAGMA;
        } else {
            return JK_FALSE;
        }
        break;
        
    case 'r':
    case 'R':
        if(!strcasecmp(header_name, "referer")) {
            *sc = SC_REFERER;
        } else {
            return JK_FALSE;
        }
        break;
        
    case 'u':
    case 'U':
        if(!strcasecmp(header_name, "user-agent")) {
            *sc = SC_USER_AGENT;
        } else {
            return JK_FALSE;
        }
        break;
        
    default:
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "requtil.getHeaderId() long header %s\n", header_name);

        return JK_FALSE;
    }
    /* Never reached */
    return JK_TRUE;
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
        if(!s->read(env, s, buf + rdlen, len - rdlen, &this_time)) {
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
}
