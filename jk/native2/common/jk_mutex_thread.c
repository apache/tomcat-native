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
 * Thread mutex support. Wrapper around apr. Old code in jk_mt.h used if no APR.
 * 
 * @author Costin Manolache
 */

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_mutex.h"

/* XXX TODO:
   - In a JNI environment it is _essential_ we use the same locking mechanism as
   java. We need to add a check to see if we have a JNIEnv, and use the java
   locking abstractions.
*/

#if APR_HAS_THREADS

#include "apr_thread_mutex.h"

static int JK_METHOD jk2_mutex_thread_init(jk_env_t *env, jk_bean_t  *mutexB)
{
    jk_mutex_t *jkMutex=mutexB->object;
    mutexB->state=JK_STATE_INIT;

    return apr_thread_mutex_create( &jkMutex->threadMutex,
                                    0,
                                    (apr_pool_t *)env->getAprPool(env) );
}

static int JK_METHOD 
jk2_mutex_thread_destroy(jk_env_t *env, jk_bean_t  *mutexB)
{
    jk_mutex_t *jkMutex=mutexB->object;

    mutexB->state=JK_STATE_NEW;
    if( jkMutex==NULL || jkMutex->threadMutex==NULL ) return JK_ERR;

    return apr_thread_mutex_destroy( jkMutex->threadMutex);
}

static int JK_METHOD 
jk2_mutex_thread_lock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    if( jkMutex==NULL || jkMutex->threadMutex==NULL ) return JK_ERR;
    return apr_thread_mutex_lock( jkMutex->threadMutex );
}

static int JK_METHOD 
jk2_mutex_thread_tryLock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    if( jkMutex==NULL || jkMutex->threadMutex==NULL ) return JK_ERR;
    return apr_thread_mutex_trylock( jkMutex->threadMutex );
}

static int JK_METHOD 
jk2_mutex_thread_unLock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    if( jkMutex==NULL || jkMutex->threadMutex==NULL ) return JK_ERR;
    return apr_thread_mutex_unlock( jkMutex->threadMutex );
}

#else /* All 3 cases we cover - apr and the 2 fallbacks */

/*-------------------- No locking -------------------- */

static int JK_METHOD jk2_mutex_thread_init(jk_env_t *env, jk_bean_t  *mutexB)
{
    mutexB->state=JK_STATE_INIT;
    return JK_OK;
}

static int JK_METHOD 
jk2_mutex_thread_destroy(jk_env_t *env, jk_bean_t  *mutexB)
{
    mutexB->state=JK_STATE_NEW;
    return JK_OK;
}

static int JK_METHOD 
jk2_mutex_thread_lock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    return JK_OK;
}

static int JK_METHOD 
jk2_mutex_thread_tryLock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    return JK_OK;
}

static int JK_METHOD 
jk2_mutex_thread_unLock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    return JK_OK;
}

#endif

int JK_METHOD jk2_mutex_thread_factory( jk_env_t *env ,jk_pool_t *pool,
                                        jk_bean_t *result,
                                        const char *type, const char *name)
{
    jk_mutex_t *mutex;

    mutex=(jk_mutex_t *)pool->calloc(env, pool, sizeof(jk_mutex_t));

    if( mutex == NULL )
        return JK_ERR;

    mutex->pool=pool;

    mutex->mbean=result; 
    result->object=mutex;
    
    result->init=jk2_mutex_thread_init;
    result->destroy=jk2_mutex_thread_destroy;
    result->invoke=jk2_mutex_invoke;
    
    mutex->lock=jk2_mutex_thread_lock;
    mutex->tryLock=jk2_mutex_thread_tryLock;
    mutex->unLock=jk2_mutex_thread_unLock;

    return JK_OK;
}
