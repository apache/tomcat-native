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

#ifndef JK_WORKERENV_H
#define JK_WORKERENV_H

#include "jk_logger.h"
#include "jk_endpoint.h"
#include "jk_worker.h"
#include "jk_map.h"
#include "jk_uriMap.h"
#include "jk_webapp.h"
#include "jk_handler.h"
#include "jk_service.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct jk_worker;
struct jk_endpoint;
struct jk_env;
struct jk_uriMap;
struct jk_webapp;
struct jk_map;
struct jk_handler;
struct jk_ws_service;

/*
 * Jk configuration and global methods. 
 * 
 * Env Information to be provided to worker at init time
 * With AJP14 support we need to have access to many informations
 * about web-server, uri to worker map....
 */
struct jk_workerEnv {
    /* The URI to WORKER map. Set via JkMount in httpd.conf,
       uriworker.properties or autoconfiguration.

       It is empty if 'native' mapping is used ( SetHandler )
    */
    /*     struct jk_map *uri_to_context; */

    /* Use this pool for all global settings
     */
    struct jk_pool *pool;
    
    /* Workers hashtable. You can also access workers by id
     */
    struct jk_map *worker_map;

    /** Number of workers that are configured. XXX same as
        size( worker_map )
    */
    int num_of_workers;

    /** Worker that will be used by default, if no other
        worker is specified. Usefull for SetHandler or
        to avoid the lookup
    */
    struct jk_worker *defaultWorker;
    
    /* Web-Server we're running on (Apache/IIS/NES).
     */
    char *server_name;
    
    /* XXX Virtual server handled - "*" is all virtual
     */
    char *virtual;

    /** Initialization properties, set via native options or workers.properties.
     */
    struct jk_map *init_data;

    /** Root env, used to register object types, etc
     */
    struct jk_env *env;

    /*
     * Log options. Extracted from init_data.
     */
    char *log_file;
    int  log_level;

    /** Global logger for jk messages. Set at init.
     */
    jk_logger_t *l;

    /*
     * Worker stuff
     */
    char     *worker_file;

    char     *secret_key;
    /*     jk_map_t *automount; */

    struct jk_uriMap *uriMap;

    struct jk_webapp *rootWebapp;
    struct jk_map *webapps;

    /** If 'global' server mappings will be visible in virtual hosts
        as well. XXX Not sure this is needed
    */
    int      mountcopy;
    
    int was_initialized;

    /*
     * SSL Support
     */
    int  ssl_enable;
    char *https_indicator;
    char *certs_indicator;
    char *cipher_indicator;
    char *session_indicator;  /* Servlet API 2.3 requirement */
    char *key_size_indicator; /* Servlet API 2.3 requirement */

    /*
     * Jk Options
     */
    int options;

    /** Old REUSE_WORKER define. Instead of using an object pool, use
        thread data to recycle the connection. */
    int perThreadWorker;
    
    /*
     * Environment variables support
     */
    int envvars_in_use;
    jk_map_t * envvars;

    /* Handlers. This is a dispatch table for messages, for
     * each message id we have an entry containing the jk_handler_t.
     * lastMessageId is the size of the table.
     */
    struct jk_handler **handlerTable;
    int lastMessageId;
    
    /** Private data, associated with the 'real' server
     *  server_rec * in apache
     */
    void *_private;
    
    /* -------------------- Methods -------------------- */
    
    /** Get worker by name
     */
    struct jk_worker *(*getWorkerForName)(struct jk_workerEnv *_this,
                                          const char *name);

    
    struct jk_worker *(*createWorker)(struct jk_workerEnv *_this,
                                      const char *name, 
                                      struct jk_map *init_data);

    struct jk_webapp *(*createWebapp)(struct jk_workerEnv *_this,
                                      const char *vhost,
                                      const char *name, 
                                      struct jk_map *init_data);

    int (*processCallbacks)(struct jk_workerEnv *_this,
                            struct jk_endpoint *e,
                            struct jk_ws_service *r );

    /**
     *  Init the workers, prepare the worker environment.
     * 
     *  Replaces wc_open
     */
    int (*init)(struct jk_workerEnv *_this);

    /** Close all workers, clean up
     *
     */
    void (*close)(struct jk_workerEnv *_this);
};


typedef struct jk_workerEnv jk_workerEnv_t;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_WORKERENV_H */
