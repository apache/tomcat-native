/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil-*- */
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
 * Description: Definitions of the objects used during the service step.   *
 *              These are the web server (ws) the worker and the connection*
 *              JVM connection point                                       *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Dan Milstein <danmil@shore.net>                            *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_SERVICE_H
#define JK_SERVICE_H

#include "jk_global.h"
#include "jk_map.h"
#include "jk_env.h"
#include "jk_workerEnv.h"
#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_uriMap.h"
#include "jk_worker.h"
#include "jk_endpoint.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    
struct jk_ws_service;
struct jk_endpoint;
struct jk_worker;
struct jk_workerEnv;
struct jk_channel;
struct jk_pool;
struct jk_env;
typedef struct jk_ws_service jk_ws_service_t;

/*
 * The web server service 'class'.  An instance of this class is created
 * for each request which is forwarded from the web server to the servlet
 * container.  Contains the basic information about the request
 * (e.g. protocol, req_uri, etc), and also contains a series of methods
 * which provide access to core web server functionality (start_response,
 * read, write).  This class might be more accurately called ws_request.
 *
 * As with all the core jk classes, this is essentially an abstract base
 * class which is implemented/extended by classes which are specific to a
 * particular web server.  By using an abstract base class in this manner,
 * workers can be written for different protocols (e.g. ajp12, ajp13, ajp14)
 * without the workers having to worry about which web server they are
 * talking to.
 *
 * This particular OO-in-C system uses a 'ws_private' pointer to point to
 * the platform-specific data.  So in the subclasses, the methods do most
 * of their work by getting their hands on the ws_private pointer and then
 * using that to get at the correctly formatted data and functions for
 * their platform.
 *
 * Try imagining this as a 'public abstract class', and the ws_private
 * pointer as a sort of extra 'this' reference.  Or imagine that you are
 * seeing the internal vtables of your favorite OO language.  Whatever
 * works for you.
 *
 * See apache1.3/mod_jk2.c and iis/jk_isapi_plugin.c for examples.  
 */
struct jk_ws_service {
    struct jk_workerEnv *workerEnv;

    /* JK_TRUE if a 'recoverable' error happened. That means a
     * lb worker can retry on a different worker, without
     * loosing any information. If JK_FALSE, an error will be reported
     * to the client
     */
    int is_recoverable_error;

    struct jk_worker *realWorker;
    
    /* 
     * A 'this' pointer which is used by the subclasses of this class to
     * point to data which is specific to a given web server platform
     * (e.g. Apache or IIS).  
     */
    void *ws_private;

    int response_started;
    int read_body_started;
    
    /*
     * Provides memory management.  All data specific to this request is
     * allocated within this pool, which can then be reclaimed at the end
     * of the request handling cycle. 
     *
     * Alive as long as the request is alive.
     * You can use endpoint pool for communication - it is recycled.
     */
    struct jk_pool *pool;

    /* 
     * CGI Environment needed by servlets
     */
    char    *method;        
    char    *protocol;      
    char    *req_uri;       
    char    *remote_addr;   
    char    *remote_host;   
    char    *remote_user;   
    char    *auth_type;     
    char    *query_string;  
    char    *server_name;   
    unsigned server_port;   
    char    *server_software;
    unsigned content_length;    /* integer that represents the content  */
                                /* length should be 0 if unknown.        */
    unsigned is_chunked;        /* 1 if content length is unknown (chunked rq) */
    unsigned no_more_chunks;    /* 1 if last chunk has been read */
    unsigned content_read;      /* number of bytes read */

    /*
     * SSL information
     *
     * is_ssl       - True if request is in ssl connection
     * ssl_cert     - If available, base64 ASN.1 encoded client certificates.
     * ssl_cert_len - Length of ssl_cert, 0 if certificates are not available.
     * ssl_cipher   - The ssl cipher suite in use.
     * ssl_session  - The ssl session string
     *
     * In some servers it is impossible to extract all this information, in this 
     * case, we are passing NULL.
     */
    int      is_ssl;
    char     *ssl_cert;
    unsigned ssl_cert_len;
    char     *ssl_cipher;
    char     *ssl_session;

	/*
	 * SSL extra information for Servlet 2.3 API
	 * 
	 * ssl_key_size - ssl key size in use
	 */
	int		ssl_key_size;
	
    /** Incoming headers */
    struct jk_map *headers_in;

    /*
     * Request attributes. 
     *
     * These attributes that were extracted from the web server and are 
     * sent to Tomcat.
     *
     * The developer should be able to read them from the ServletRequest
     * attributes. Tomcat is required to append org.apache.tomcat. to 
     * these attrinbute names.
     */
    struct jk_map *attributes;

    /*
     * The jvm route is in use when the adapter load balance among
     * several JVMs. It is the ID of a specific JVM in the load balance
     * group. We are using this variable to implement JVM session 
     * affinity
     */
    char    *jvm_route;

    /* Response informations. As in apache, we don't use a separate
       structure for response.
     */
    int         status;
    const char *msg;
    struct jk_map *headers_out;

    /* Count remaining bytes ( original content length minus what was sent */
    int left_bytes_to_send;
    
    /*
     * Callbacks into the web server.  For each, the first argument is
     * essentially a 'this' pointer.  All return JK_TRUE on success
     * and JK_FALSE on failure.
     */

    /* Initialize the service structure
     */
    int (*init)( struct jk_env *env, jk_ws_service_t *_this,
                 struct jk_worker *w, void *serverObj);

    /* Post request cleanup.
     */
    void (*afterRequest)( struct jk_env *env, jk_ws_service_t *_this );
    
    /*
     * Set the response head in the server structures. This will be called
     * before the first write.
     */
    int (JK_METHOD *head)(struct jk_env *env, jk_ws_service_t *s);

    /*
     * Read a chunk of the request body into a buffer.  Attempt to read len
     * bytes into the buffer.  Write the number of bytes actually read into
     * actually_read.  
     */
    int (JK_METHOD *read)(struct jk_env *env, jk_ws_service_t *s,
                          void *buffer,
                          unsigned len,
                          unsigned *actually_read);

    /*
     * Write a chunk of response data back to the browser.
     */
    int (JK_METHOD *write)(struct jk_env *env, jk_ws_service_t *s,
                           const void *buffer, int len);

    /*
     * Flush the output buffers.
     */
    int (JK_METHOD *flush)(struct jk_env *env, jk_ws_service_t *s );
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_SERVICE_H */
