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

#include "jk_logger.h"
#include "jk_service.h"
#include "jk_endpoint.h"
#include "jk_map.h"
#include "jk_mt.h"
#include "jk_uriMap.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct jk_worker;
struct jk_endpoint;
struct jk_env;
typedef struct jk_worker jk_worker_t;

    
/*
 * The login structure
 */
typedef struct jk_login_service jk_login_service_t;
#define AJP14_ENTROPY_SEED_LEN		32	/* we're using MD5 => 32 chars */
#define AJP14_COMPUTED_KEY_LEN		32  /* we're using MD5 also */

struct jk_login_service {

    /*
     *  Pointer to web-server name
     */
    char * web_server_name;
    
    /*
     * Pointer to servlet-engine name
     */
    char * servlet_engine_name;
    
    /*
     * Pointer to secret key
     */
    char * secret_key;
    
    /*
     * Received entropy seed
     */
    char entropy[AJP14_ENTROPY_SEED_LEN + 1];
    
    /*
     * Computed key
     */
    char computed_key[AJP14_COMPUTED_KEY_LEN + 1];
    
    /*
     *  What we want to negociate
     */
    unsigned long negociation;
    
    /*
     * What we received from servlet engine 
     */
    unsigned long negociated;
};                                

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
 * Web servers are configured to forward requests to a given worker.  To
 * handle those requests, the worker's get_endpoint method is called, and
 * then the service() method of that endpoint is called.
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
 * See jk_ajp14_worker.c, jk_ajp13_worker.c and jk_ajp12_worker.c for examples.  
 */
struct jk_worker {

    struct jk_workerEnv *workerEnv;
    char *name;
    
    /* 
     * A 'this' pointer which is used by the subclasses of this class to
     * point to data/functions which are specific to a given protocol 
     * (e.g. ajp12 or ajp13 or ajp14).  
     */
    void *worker_private;
    
    /* XXX Add name and all other common properties !!! 
     */

    /** Communication channle used by the worker 
     */
    struct jk_channel *channel;

    /* XXX Stuff from ajp, some is generic, some not - need to
       sort out after */
    struct sockaddr_in worker_inet_addr; /* Contains host and port */
    unsigned connect_retry_attempts;
 
    /* 
     * Open connections cache...
     *
     * 1. Critical section object to protect the cache.
     * 2. Cache size. 
     * 3. An array of "open" endpoints.
     */
    JK_CRIT_SEC cs;
    unsigned ep_cache_sz;
    struct jk_endpoint **ep_cache;

    int proto;
    struct jk_login_service *login;
 
    int (* logon)(struct jk_endpoint *ae,
                  jk_logger_t    *l);


    /*
     * For all of the below (except destroy), the first argument is
     * essentially a 'this' pointer.  
     */

    /*
     * Given a worker which is in the process of being created, and a list
     * of configuration options (or 'properties'), check to see if it the
     * options are.  This will always be called before the init() method.
     * The init/validate distinction is a bit hazy to me.
     * See jk_ajp13_worker.c/jk_ajp14_worker.c and jk_worker.c->wc_create_worker() 
     */
    int (JK_METHOD *validate)(jk_worker_t *_this,
                              jk_map_t *props, 
                              struct jk_workerEnv *we,
                              jk_logger_t *l);

    /*
     * Do whatever initialization needs to be done to start this worker up.
     * Configuration options are passed in via the props parameter.  
     */
    int (JK_METHOD *init)(jk_worker_t *w,
                          jk_map_t *props, 
                          struct jk_workerEnv *we,
                          jk_logger_t *l);


    /*
     * Obtain an endpoint to service a particular request.  A pointer to
     * the endpoint is stored in pend.  
     */
    int (JK_METHOD *get_endpoint)(jk_worker_t *w,
                                  struct jk_endpoint **pend,
                                  jk_logger_t *l);

    /*
     * Shutdown this worker.  The first argument is not a 'this' pointer,
     * but rather a pointer to 'this', so that the object can be free'd (I
     * think -- though that doesn't seem to be happening.  Hmmm).  
     */
    int (JK_METHOD *destroy)(jk_worker_t **w,
                             jk_logger_t *l);
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_WORKER_H */
