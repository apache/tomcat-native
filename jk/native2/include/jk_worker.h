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

/* Number of lb levels/priorities. Workers are grouped by the level,
   lower levels will allways be prefered. If all workers in a level are
   in error state, we move to the next leve.
*/
#define JK_LB_LEVELS 4
#define JK_LB_MAX_WORKERS 256

/* XXX Separate this in 2 structures: jk_lb.h and jk_ajp.h.
   Using 'worker' as a generic term is confusing, the objects are very different.
 */
    
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
    struct jk_bean *mbean;
    
    struct jk_workerEnv *workerEnv;

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

    /** Reuse the endpoint and it's connection. The cache will contain
        the 'unused' workers. It's size may be used to select
        an worker by the lb.
     */
    struct jk_objCache *endpointCache;
    
    /* All endpoints for this worker. Each endpoint is long-lived,
       the size of the map will represent the maximum number of connections
       active so far.
     */
    struct jk_map *endpointMap;

    /** Request pool cache. XXX We may use a pool of requests.
     */
    struct jk_objCache *rPoolCache;
    
    /* Private key used to connect to the remote side2.*/
    char * secret;

    JK_CRIT_SEC cs;
    /* -------------------- Information used for load balancing ajp workers -------------------- */

    /** The id of the tomcat instance we connect to. We may have multiple
        workers connecting to a single tomcat. If no route is defined,
        the worker name will be the route name. The route can be the
        name of another worker. 
     */
    char *route;

    /** lb groups in which this worker belongs */
    struct jk_map *groups;

    /* Each worker can be part of a load-balancer scheme.
     * The information can be accessed by other components -
     * for example to report status, etc.
     */
    int  lb_factor;
    int  lb_value;

    /* Time when the last error occured on this worker */
    time_t  error_time;

    /* In error state. Will return to normal state after a timeout
     *  ( number of requests or time ), if no other worker is active
     *  or when the configuration changes.
     */
    int in_error_state;
    
    /* Worker priority.
     * Workers with lower priority are allways preffered - regardless of lb_value
     * This is user to represent 'local' workers ( we can call it 'proximity' or 'distance' )
     */
    int level;
    
    /* -------------------- Information specific to the lb worker -------------------- */

    /** Load balanced workers. Maps name->worker, used at config time.
     *  When the worker is initialized or refreshed it'll set the runtime
     *  tables.
     */
    struct jk_map *lbWorkerMap;

    /** Support for some hardware load-balancing schemes.
        If this is set, jk2 will only forward new requests to local
        workers ( level=0 ). All other workers will get requests
        with a session id.

        The value of this field will be used as a status code if
        all local workers are down. This way the front load-balancers
        will know this server shouldn't be used for new requests.
    */
    int hwBalanceErr;

    /* Message to display when no tomcat instance is available
     * if status=302, this is a redirect location.
     */
    char *noWorkerMsg;
    int  noWorkerCode;
    
    int workerCnt[JK_LB_LEVELS];
    jk_worker_t *workerTables[JK_LB_LEVELS][JK_LB_MAX_WORKERS];
    
    /* -------------------- Methods supported by all workers -------------------- */

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
