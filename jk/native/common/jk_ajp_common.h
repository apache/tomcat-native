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
 * Description: common stuff for bi-directional protocol ajp13/ajp14.      *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_AJP_COMMON_H
#define JK_AJP_COMMON_H

#include "jk_service.h"
#include "jk_msg_buff.h"
#include "jk_mt.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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

/*
 * Request methods, coded as numbers instead of strings.
 * The list of methods was taken from Section 5.1.1 of RFC 2616,
 * RFC 2518, the ACL IETF draft, and the DeltaV IESG Proposed Standard.
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
 *                        | "REPORT"
 *                        | "VERSION-CONTROL"
 *                        | "CHECKIN"
 *                        | "CHECKOUT"
 *                        | "UNCHECKOUT"
 *                        | "SEARCH"
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
#define SC_M_REPORT             (unsigned char)16
#define SC_M_VERSION_CONTROL    (unsigned char)17
#define SC_M_CHECKIN            (unsigned char)18
#define SC_M_CHECKOUT           (unsigned char)19
#define SC_M_UNCHECKOUT         (unsigned char)20
#define SC_M_SEARCH             (unsigned char)21


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

/*
 * AJP13/AJP14 use same message structure
 */

#define AJP_DEF_RETRY_ATTEMPTS    (1)

#define AJP_HEADER_LEN            (4)
#define AJP_HEADER_SZ_LEN         (2)
#define CHUNK_BUFFER_PAD          (12)


struct jk_res_data {
    int         status;
    const char *msg;
    unsigned    num_headers;
    char      **header_names;
    char      **header_values;
};
typedef struct jk_res_data jk_res_data_t;

#include "jk_ajp14.h"

struct ajp_operation;
typedef struct ajp_operation ajp_operation_t;

struct ajp_endpoint;
typedef struct ajp_endpoint ajp_endpoint_t;

struct ajp_worker;
typedef struct ajp_worker ajp_worker_t;

struct ajp_worker {
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
    ajp_endpoint_t **ep_cache;

	int proto; /* PROTOCOL USED AJP13/AJP14 */

	jk_login_service_t *login;

    jk_worker_t worker; 

    /*
     * Post physical connect handler.
     * AJP14 will set here its login handler
     */ 
    int (* logon)(ajp_endpoint_t *ae,
                  jk_logger_t    *l);
}; 
 

/*
 * endpoint, the remote which will does the work
 */
struct ajp_endpoint {
    ajp_worker_t *worker;

    jk_pool_t pool;
    jk_pool_atom_t buf[BIG_POOL_SIZE];

	int proto;	/* PROTOCOL USED AJP13/AJP14 */

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
struct ajp_operation {
    jk_msg_buf_t    *request;   /* original request storage */
    jk_msg_buf_t    *reply;     /* reply storage (chuncked by ajp13 */
    jk_msg_buf_t    *post;      /* small post data storage area */
    int     uploadfd;           /* future persistant storage id */
    int     recoverable;        /* if exchange could be conducted on another TC */
};

/*
 * Functions
 */


int ajp_validate(jk_worker_t *pThis,
                 jk_map_t    *props,
                 jk_worker_env_t *we,
                 jk_logger_t *l,
                 int          proto);

int ajp_init(jk_worker_t *pThis,
             jk_map_t    *props,
             jk_worker_env_t *we,
             jk_logger_t *l,
             int          proto);

int ajp_destroy(jk_worker_t **pThis,
                jk_logger_t *l,
                int          proto);

int JK_METHOD ajp_done(jk_endpoint_t **e,
                       jk_logger_t    *l);

int ajp_get_endpoint(jk_worker_t    *pThis,
                     jk_endpoint_t **pend,
                     jk_logger_t    *l,
                     int             proto);

int ajp_connect_to_endpoint(ajp_endpoint_t *ae,
                            jk_logger_t    *l);

void ajp_close_endpoint(ajp_endpoint_t *ae,
                        jk_logger_t    *l);

int ajp_connection_tcp_send_message(ajp_endpoint_t *ae,
                                    jk_msg_buf_t   *msg,
                                    jk_logger_t    *l);

int ajp_connection_tcp_get_message(ajp_endpoint_t *ae,
                                   jk_msg_buf_t   *msg,
                                   jk_logger_t    *l);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_AJP_COMMON_H */
