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

#ifndef JK_REQ_H
#define JK_REQ_H

#include "jk_global.h"
#include "jk_env.h"
#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_endpoint.h"
#include "jk_service.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

    struct jk_env;
    struct jk_msg;
    typedef struct jk_msg jk_msg_t;

    struct jk_endpoint;
    struct jk_ws_service;
    struct jk_logger;

#define DEF_BUFFER_SZ (8*1024)
#define AJP13_MAX_SEND_BODY_SZ      (DEF_BUFFER_SZ - 6)

/**
 * Abstract interface to marshalling. Different encodings and
 * communication mechanisms can be supported.
 *
 * This object is recyclable, but is not thread safe - it can
 * handle a single message at a time.
 *
 * It is created by a channel( XXX endpoint ? )
 * and can be sent/received only on that channel.
 *
 * XXX Lifecycle: on send the buffer will be reused after send
 *     On receive - it will be recycled after reset() or equiv.
 *     Same as on the java side.
 *
 * @author Costin Manolache
 */
    struct jk_msg
    {
    /** Human-readable method name */
        char *name;

    /** Method id - to be sent in the packet
     */
        int id;

    /** Header length for this message
     */
        int headerLength;

        /*
         * Prepare the buffer for a new invocation 
         */
        void (*reset) (struct jk_env * env, struct jk_msg * _this);

        /*
         * Finalize the buffer before sending - set length fields, etc
         */
        void (*end) (struct jk_env * env, struct jk_msg * _this);

        int (*checkHeader) (struct jk_env * env, struct jk_msg * _this,
                            struct jk_endpoint * e);

        /*
         * Dump the buffer header
         *   @param err Message text
         */
        void (*dump) (struct jk_env * env, struct jk_msg * _this, char *err);

        int (*appendByte) (struct jk_env * env, struct jk_msg * _this,
                           unsigned char val);

        int (*appendBytes) (struct jk_env * env, struct jk_msg * _this,
                            const unsigned char *param, const int len);

        int (*appendInt) (struct jk_env * env, struct jk_msg * _this,
                          const unsigned short val);

        int (*appendLong) (struct jk_env * env, struct jk_msg * _this,
                           const unsigned long val);

        int (*appendString) (struct jk_env * env, struct jk_msg * _this,
                             const char *param);

        int (*appendAsciiString) (struct jk_env * env, struct jk_msg * _this,
                                  const char *param);

        int (*appendMap) (struct jk_env * env, struct jk_msg * _this,
                          struct jk_map * map);

        unsigned char (*getByte) (struct jk_env * env, struct jk_msg * _this);

        unsigned short (*getInt) (struct jk_env * env, struct jk_msg * _this);

    /** Look at the next int, without reading it
     */
        unsigned short (*peekInt) (struct jk_env * env,
                                   struct jk_msg * _this);

        unsigned long (*getLong) (struct jk_env * env, struct jk_msg * _this);

    /** Return a string. 
        The buffer is internal to the message, you must save
        or make sure the message lives long enough.
     */
        char *(*getString) (struct jk_env * env, struct jk_msg * _this);

    /** Return a byte[] and it's length.
     *  The buffer is internal to the message, you must save
     * or make sure the message lives long enough.
     */
        unsigned char *(*getBytes) (struct jk_env * env,
                                    struct jk_msg * _this, int *len);

    /** Read a map structure from the message. The map is encoded
     *  as an int count and then the NV pairs.
     *
     *  The content will not be copied - but point to the msg's buffer.
     *  If you want to use the map after the msg becomes invalid, you need
     *  to copy it.
     */
        int (*getMap) (struct jk_env * env, struct jk_msg * _this,
                       struct jk_map * map);

    /** 
     * Special method. Will read data from the server and add them as
     * bytes. It is equivalent with jk2_requtil_readFully() in a buffer
     * and then jk_msg_appendBytes(), except that we use directly the
     * internal buffer.
     *
     * Returns -1 on error, else number of bytes read
     */
        int (*appendFromServer) (struct jk_env * env,
                                 struct jk_msg * _this,
                                 struct jk_ws_service * r,
                                 struct jk_endpoint * ae, int len);

    /** 
     * Clone the msg buf into another one, sort of clone.
     *
     * Returns -1 on error, else number of bytes copied
     */
        int (*copy) (struct jk_env * env,
                     struct jk_msg * _this, struct jk_msg * dst);

        void *_privatePtr;

        /* Temporary, don't use */
        struct jk_pool *pool;

        unsigned char *buf;
        int pos;
        int len;
        int maxlen;

        /* JK_TRUE if the message is sent/received by the server ( tomcat ).
         */
        int serverSide;
    };

/* Temp */
    jk_msg_t *jk2_msg_ajp_create(struct jk_env *env, struct jk_pool *p,
                                 int buffSize);

    jk_msg_t *jk2_msg_ajp_create2(struct jk_env *env, struct jk_pool *pool,
                                  char *buf, int buffSize);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif
