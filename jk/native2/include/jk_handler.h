/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * Handle jk messages. Each handler will register a number of
 * callbacks for certain message types. 
 *
 * This is based on a simple generalization of the code in ajp13,
 * with the goal of making the system extensible for ajp14.
 *
 * @author Costin Manolache
 */

#ifndef JK_HANDLER_H
#define JK_HANDLER_H

#include "jk_global.h"
#include "jk_env.h"
#include "jk_map.h"
#include "jk_workerEnv.h"
#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_uriMap.h"
#include "jk_worker.h"
#include "jk_endpoint.h"
#include "jk_msg.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Return codes from the handler method
 */

/**
 * Message does not have a response, jk will continue
 * to wait.
 */
#define JK_HANDLER_OK        0

/** Message requires a response. The handler will prepare
 *  the response in e->post message.
 */
#define JK_HANDLER_RESPONSE  1

/** This is the last message ( in a sequence ). The original
 *  transaction is now completed.
 */
#define JK_HANDLER_LAST      2

/** An error ocurred during handler execution. The processing
 *  will be interrupted, but the connection remains open.
 *  After an error handler we should continue receiving messages,
 *  but ignore them and send an error message on the first ocassion.
 */
#define JK_HANDLER_ERROR     3

/** A fatal error ocurred, we should close the channel
 *  and report the error. This should be used if something unexpected,
 *  from which we can't recover happens. ( for example an unexpected packet,
 *  an invalid code, etc ).
 */
#define JK_HANDLER_FATAL     4

struct jk_msg;
struct jk_ws_service;
struct jk_endpoint;
struct jk_logger;
struct jk_workerEnv;
struct jk_env;
    
typedef int (JK_METHOD *jk_handler_callback)(struct jk_env *env,
                                             void *target,
                                             struct jk_endpoint *ae,
                                             struct jk_msg *msg);

struct jk_handler;
typedef struct jk_handler jk_handler_t;

struct jk_handler {
    struct jk_workerEnv *workerEnv;

    char *name;
    int messageId;

    jk_handler_callback callback;

    int (JK_METHOD *init)( struct jk_env *env, struct jk_handler *handler,
                 struct jk_workerEnv *workerEnv);
};
                                        
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif 
