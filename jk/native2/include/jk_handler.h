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
struct jk_env;
    
typedef int (JK_METHOD *jk_handler_callback)(struct jk_env *env,
                                             struct jk_msg *msg,
                                             struct jk_ws_service *r,
                                             struct jk_endpoint *ae);

struct jk_handler;
typedef struct jk_handler jk_handler_t;

struct jk_handler {
    struct jk_workerEnv *workerEnv;

    char *name;
    int messageId;
    jk_handler_callback callback;
};
                                        
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif 
