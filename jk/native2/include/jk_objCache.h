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

#ifndef JK_OBJCACHE_H
#define JK_OBJCACHE_H

#include "jk_global.h"
#include "jk_env.h"
#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_msg.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

    struct jk_env;
    struct jk_objCache;
    struct jk_logger;
    struct jk_pool;
    typedef struct jk_objCache jk_objCache_t;

#define JK_OBJCACHE_DEFAULT_SZ          (128)


    jk_objCache_t *jk2_objCache_create(struct jk_env *env,
                                       struct jk_pool *pool);

/**
 * Simple object cache ( or pool for java people - don't confuse with the
 *  mem pool ).
 *
 * Used to avoid creating expensive objects, like endpoints ( which would
 * require TCP connection on startup ).
 *
 * This is very simple - only one object kind, no expiry.
 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           
 * Author:      Henri Gomez <hgomez@apache.org>
 * Author:      Costin Manolache
*/
    struct jk_objCache
    {
    /** Return an object to the pool.
     *  @return JK_FALSE the object can't be taken back, caller must free it.
     */
        int (*put) (struct jk_env * env, jk_objCache_t *_this, void *obj);

        void *(*get) (struct jk_env * env, jk_objCache_t *_this);

        int (*init) (struct jk_env * env, jk_objCache_t *_this,
                     int cacheSize);

        int (*destroy) (struct jk_env * env, jk_objCache_t *_this);

    /** Cache max size. -1 for unbound ( i.e. growing ). */
        int maxSize;

        /* Current size of the table */
        int size;

    /** Number of elements in the cache.
     *  Postition where next element will be inserted.
     */
        int count;

        /* Sync.
         */
        struct jk_mutex *cs;

    /** Objects in the cache */
        void **data;
        struct jk_pool *pool;
    };

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif
