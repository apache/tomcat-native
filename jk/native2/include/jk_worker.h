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
 * Description: Workers controller header file                             *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           * 
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_WORKER_H
#define JK_WORKER_H

#include "jk_env.h"
#include "jk_pool.h"
#include "jk_logger.h"
#include "jk_service.h"
#include "jk_endpoint.h"
#include "jk_map.h"
#include "jk_mt.h"
#include "jk_uriMap.h"
#include "jk_objCache.h"
#include "jk_msg.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct jk_worker;
struct jk_endpoint;
struct jk_env;
struct jk_objCache;
struct jk_msg;
struct jk_map;
typedef struct jk_worker jk_worker_t;

/*
 * The worker 'class', which represents something to which the web server
 * can delegate requests. 
 *
 * This can mean communicating with a particular servlet engine instance,
 * using a particular protocol.  A single web server instance may have
 * multiple workers communicating with a single servlet engine (it could be
 * using ajp12 for some requests and ajp13/ajp14 for others).  Or, a single web
 * server instance could have multiple workers communicating with different
 * servlet engines using the same protocol (it could be load balancing
 * among many engines, using ajp13/ajp14 for all communication).
 *
 * There is also a load balancing worker (jk_lb_worker.c), which itself
 * manages a group of workers.
 *
 * As with all the core jk classes, this is essentially an abstract base
 * class which is implemented/extended by classes which are specific to a
 * particular protocol (or request-handling system).  By using an abstract
 * base class in this manner, plugins can be written for different servers
 * (e.g. IIS, Apache) without the plugins having to worry about which
 * protocol they are talking.
 *
 * This particular OO-in-C system uses a 'worker_private' pointer to
 * point to the protocol-specific data/functions.  So in the subclasses, the
 * methods do most of their work by getting their hands on the
 * worker_private pointer and then using that to get at the functions for
 * their protocol.
 *
 * Try imagining this as a 'public abstract class', and the
 * worker_private pointer as a sort of extra 'this' reference.  Or
 * imagine that you are seeing the internal vtables of your favorite OO
 * language.  Whatever works for you.
 *
 * See jk_ajp14_worker.c, jk_worker_status for examples.  
 *
 */
struct jk_worker {

    struct jk_workerEnv *workerEnv;
    char *name;
    char *type;

    /** The id of the tomcat instance we connect to. We may have multiple
        workers connecting to a single tomcat. If no route is defined,
        the worker name will be the route name. The route can be the
        name of another worker. 
     */
    char *route;

    /** Pool for worker specific informations.
        In future we may start/stop/reload workers at runtime, but that's
        far away
    */
    struct jk_pool *pool;

    /* 
     * A 'this' pointer which is used by the subclasses of this class to
     * point to data/functions which are specific to a given protocol 
     * (e.g. ajp12 or ajp13 or ajp14).  
     */
    void *worker_private;
    
    /** Communication channel used by the worker 
     */
    struct jk_channel *channel;
    char *channelName;

    /** Reuse the endpoint and it's connection
     */
    struct jk_objCache *endpointCache;
    int cache_sz;

    /** Request pool cache. XXX We may use a pool of requests.
     */
    struct jk_objCache *rPoolCache;
    
    /* Private key used to connect to the remote side2.*/
    char * secret;

    /* Each worker can be part of a load-balancer scheme.
     * The information can be accessed by other components -
     * for example to report status, etc.
     */
    double  lb_factor;
    double  lb_value;
    int     in_error_state;
    int     in_recovering;
    time_t  error_time;
    /** Last exception recorded on this worker, the reason for
     *  which this worker is in error state and can't perform.
     */
    struct jk_exception *lastError;

    /** If num_of_workers > 0 this is an load balancing worker
     */
    jk_worker_t **lb_workers;
    struct jk_map *lbWorkerMap;
    int num_of_workers;

    int (JK_METHOD *setProperty)(struct jk_env *env, jk_worker_t *_this,
                                 char *name, char *value );

    char *(JK_METHOD *getProperty)(struct jk_env *env, jk_worker_t *_this,
                                   char *name );
    
    /*
     * Do whatever initialization needs to be done to start this worker up.
     * Configuration options are passed in via the props parameter.  
     *
     * You can skip this by setting it to NULL.
     */
    int (JK_METHOD *init)(struct jk_env *env, jk_worker_t *_this);

    /*
     * Shutdown this worker. XXX Some cleanup must be made by default
     * by workerEnv, so we don't need to duplicate the code.
     */
    int (JK_METHOD *destroy)(struct jk_env *env, jk_worker_t *_thisP );

    /*
     * Forward a request to the servlet engine.  The request is described
     * by the jk_ws_service_t object.  I'm not sure exactly how
     * is_recoverable_error is being used.  
     */
    int (JK_METHOD *service)(struct jk_env *env,
                             struct jk_worker *_this,
                             struct jk_ws_service *s);
    
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_WORKER_H */
