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
 * Description: Definitions of the endpoint.
 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           
 * Author:      Dan Milstein <danmil@shore.net>                            
 * Author:      Henri Gomez <hgomez@slib.fr>                               
 * Version:     $Revision$                                          
 ***************************************************************************/

#ifndef JK_ENDPOINT_H
#define JK_ENDPOINT_H

#include "jk_global.h"
#include "jk_env.h"
#include "jk_map.h"
#include "jk_service.h"
#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_uriMap.h"
#include "jk_msg.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    
struct jk_endpoint;
struct jk_stat;
struct jk_ws_service;
struct jk_logger;
struct jk_map;
typedef struct jk_endpoint   jk_endpoint_t;
typedef struct jk_stat   jk_stat_t;

/* XXX replace worker with channel, endpoints are specific to channels not workers */
    
/*
 * The endpoint 'class', which represents one end of a connection to the
 * servlet engine.  Basically, supports nothing other than forwarding the
 * request to the servlet engine.  Endpoints can be persistent (as with
 * ajp13/ajp14, where a single connection is reused many times), or can last for a
 * single request (as with ajp12, where a new connection is created for
 * every request).
 *
 * An endpoint for a given protocol is obtained by the web server plugin
 * from a worker object for that protocol.  See below for details.
 *
 * As with all the core jk classes, this is essentially an abstract base
 * class which is implemented/extended by classes which are specific to a
 * particular protocol.  By using an abstract base class in this manner,
 * plugins can be written for different servers (e.g. IIS, Apache) without
 * the plugins having to worry about which protocol they are talking.
 *
 * This particular OO-in-C system uses a 'endpoint_private' pointer to
 * point to the protocol-specific data/functions.  So in the subclasses, the
 * methods do most of their work by getting their hands on the
 * endpoint_private pointer and then using that to get at the functions for
 * their protocol.
 *
 * Try imagining this as a 'public abstract class', and the
 * endpoint_private pointer as a sort of extra 'this' reference.  Or
 * imagine that you are seeing the internal vtables of your favorite OO
 * language.  Whatever works for you.
 *
 * See jk_ajp13_worker.c/jk_ajp14_worker.c and jk_ajp12_worker.c for examples.  
 */
struct jk_endpoint {
    struct jk_bean *mbean;

    /* Parent
     */
    struct jk_worker *worker;

    struct jk_workerEnv *workerEnv;

    /** Data specific to a channel connection. Usually a struct
     * containing info about the active connection ( sd, jniEnv, etc ).
     */
    void *channelData;

    /* Channel-specific data. Usually a file descriptor. ( avoids
       using a struct for typical socket channels )
     */
    int sd;

    /** Current request beeing processed.
     *  Used by JNI worker mostly ( XXX remove it, pass it explicitely )
     */
    struct jk_ws_service *currentRequest;

    /** Connection pool. Used to store temporary data. It'll be
     *  recycled after each transaction.
     *  XXX Do we need it ? env->tmpPool can be used as well.
     */
    struct jk_pool *cPool;

    /* Buffers */

    /* Incoming messages ( from tomcat ). Will be overriten after each
       message, you must save any data you want to keep.
     */
    struct jk_msg *reply;

    /* Outgoing messages ( from server ). If the handler will return
       JK_HANDLER_RESPONSE this message will be sent to tomcat
    */
    struct jk_msg *post;
    
    /* original request storage ( XXX do we need it ? )
     */
    struct jk_msg *request; 

    char *readBuf;
    int bufPos;
    
    /* JK_TRUE if the endpoint can be reused ( after an error ).
     * JK_FALSE if we must close the connection - for example we have
     * packet/marshalling error.
     * This is different from req->is_recoverable_error, which just
     * specifies that the request can be sent to another tomcat.
     * Set by workerEnv if a handler returns JK_HANDLER_ERROR.
     */
    int     recoverable;        

    /* The struct will be created in shm if available
     */
    struct jk_stat *stats;
};

/** Statistics collected per endpoint
 */
struct jk_stat {
    /* Number of requests served by this worker and the number of errors */
    int reqCnt;
    int errCnt;

    int connected;

    int workerId;
    /* Active request
     */
    char active[64];

#ifdef HAS_APR
    /* Time when this endpoint has opened a connection to
       tomcat
    */
    apr_time_t connectedTime;
    /* Total time ( for average - divide by reqCnt )
       and maxTime for requests.
    */
    apr_time_t totalTime;
    apr_time_t maxTime;
    /* Last request - time of start, time when we start the ajp protocol, end time
     */
    apr_time_t startTime;
    apr_time_t jkStartTime;
    apr_time_t endTime;
#endif
};

    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_ENDPOINT_H */
