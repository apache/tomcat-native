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

#ifndef JK_CHANNEL_H
#define JK_CHANNEL_H

#include "jk_global.h"
#include "jk_env.h"
#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_msg.h"
#include "jk_service.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

    struct jk_env;
    struct jk_worker;
    struct jk_endpoint;
    struct jk_channel;

#define CH_OPEN 4
#define CH_CLOSE 5
#define CH_READ 6
#define CH_WRITE 7
#define CH_HASINPUT 8

    typedef struct jk_channel jk_channel_t;

/**
 * Abstraction (interface) for sending/receiving blocks of data ( packets ).
 * This will be used to replace the current TCP/socket code and allow other
 * transports. JNI, shmem, doors and other mechanisms can be tryed out, and 
 * we can also have a gradual transition to APR. The current tcp-based transport
 * will be refactored to this interface.
 * 
 * XXX Experimental
 * 
 * Issues:
 *  - Should the transport also check the packet integrity ( envelope ) ?
 *  - This is supposed to work for send/receive sequences, and the buffer
 *   shouldn't be modified until a send/receive is completed ( we want to
 *   avoid memcpy )
 *  - We need to extend/generalize the mechanisms in 'worker' to support other
 *   types of internal interfaces.
 *  - this interface is using get/setProperty model, like in Java beans ( well, 
 *    just a generic get/set method, no individual setters ). This is different
 *    from worker, which gets a map at startup. This model should also allow 
 *    run-time queries for status, management, etc - but it may be too complex.
 * 
 * @author Costin Manolache
 */
    struct jk_channel
    {
        struct jk_bean *mbean;

        /* JK_TRUE if the channel is 'stream' based, i.e. it works using
           send() followed by blocking reads().
           XXX make it type and define an enum of supported types ?

           The only alternative right now is JNI ( and doors ), where
           a single thread is used. After the first packet is sent the
           java side takes control and directly dispatch messages using the
           jni  ( XXX review - it would be simple with continuations, but
           send/receive flow is hard to replicate on jni ) 
         */
        int is_stream;

        struct jk_workerEnv *workerEnv;
        struct jk_worker *worker;
        char *workerName;

        int serverSide;

    /** Open the communication channel
     */
        int (JK_METHOD * open) (struct jk_env * env, jk_channel_t *_this,
                                struct jk_endpoint * endpoint);

    /** Close the communication channel
     */
        int (JK_METHOD * close) (struct jk_env * env, jk_channel_t *_this,
                                 struct jk_endpoint * endpoint);

  /** Send a packet
   */
        int (JK_METHOD * send) (struct jk_env * env, jk_channel_t *_this,
                                struct jk_endpoint * endpoint,
                                struct jk_msg * msg);

    /** Receive a packet
     */
        int (JK_METHOD * recv) (struct jk_env * env, jk_channel_t *_this,
                                struct jk_endpoint * endpoint,
                                struct jk_msg * msg);

    /** Check if something is available in input on the communication channel
     */
        int (JK_METHOD * hasinput) (struct jk_env * env, jk_channel_t *_this,
                                    struct jk_endpoint * endpoint,
                                    int timeout);

    /** Called before request processing, to initialize resources.
        All following calls will be in the same thread.
     */
        int (JK_METHOD * beforeRequest) (struct jk_env * env,
                                         jk_channel_t *_this,
                                         struct jk_worker * worker,
                                         struct jk_endpoint * endpoint,
                                         struct jk_ws_service * r);

    /** Called after request processing. Used to be worker.done()
     */
        int (JK_METHOD * afterRequest) (struct jk_env * env,
                                        jk_channel_t *_this,
                                        struct jk_worker * worker,
                                        struct jk_endpoint * endpoint,
                                        struct jk_ws_service * r);

    /** Obtain the channel status code
     */
        int (JK_METHOD * status) (struct jk_env * env,
                                  struct jk_worker * worker,
                                  jk_channel_t *_this);

        void *_privatePtr;
    };

    int JK_METHOD jk2_channel_invoke(struct jk_env *env, struct jk_bean *bean,
                                     struct jk_endpoint *ep, int code,
                                     struct jk_msg *msg, int raw);


    int JK_METHOD jk2_channel_setAttribute(struct jk_env *env,
                                           struct jk_bean *mbean, char *name,
                                           void *valueP);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif
