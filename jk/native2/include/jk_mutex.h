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

#ifndef JK_MUTEX_H
#define JK_MUTEX_H

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
    struct jk_mutex;

#if APR_HAS_THREADS
#include "apr_thread_mutex.h"
#elif defined( WIN32 )
#include <windows.h>
#elif defined( _REENTRANT )
#include <pthread.h>
#endif


    typedef struct jk_mutex jk_mutex_t;

#define MUTEX_LOCK 4
#define MUTEX_TRYLOCK 5
#define MUTEX_UNLOCK 6

/**
 *  Interprocess mutex support. This is a wrapper to APR.
 *
 * @author Costin Manolache
 */
    struct jk_mutex
    {
        struct jk_bean *mbean;

        struct jk_pool *pool;

    /** Name of the mutex */
        char *fname;

    /** APR mechanism */
        int mechanism;

    /** 
     */
        int (JK_METHOD * lock) (struct jk_env * env, struct jk_mutex * mutex);

    /** 
     */
        int (JK_METHOD * tryLock) (struct jk_env * env,
                                   struct jk_mutex * mutex);

    /** 
     */
        int (JK_METHOD * unLock) (struct jk_env * env,
                                  struct jk_mutex * mutex);

        /* Private data */
        void *privateData;

#if APR_HAS_THREADS
        apr_thread_mutex_t *threadMutex;
#elif defined( WIN32 )
        CRITICAL_SECTION threadMutex;
#elif defined( _REENTRANT )
        pthread_mutex_t threadMutex;
#else
        void *threadMutex;
#endif
    };

    int JK_METHOD jk2_mutex_invoke(struct jk_env *env, struct jk_bean *bean,
                                   struct jk_endpoint *ep, int code,
                                   struct jk_msg *msg, int raw);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif
