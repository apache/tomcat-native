/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2003 The Apache Software Foundation.          *
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

#elif defined( WIN32 )
/*-------------------- Windows - @deprecated, use APR if possible -------------------- */

#include <windows.h>

static int JK_METHOD jk2_mutex_thread_init(jk_env_t *env, jk_bean_t  *mutexB)
{
    jk_mutex_t *jkMutex=mutexB->object;
    mutexB->state=JK_STATE_INIT;
    InitializeCriticalSection( & jkMutex->threadMutex );
    return JK_OK;
}

static int JK_METHOD 
jk2_mutex_thread_destroy(jk_env_t *env, jk_bean_t  *mutexB)
{
    jk_mutex_t *jkMutex=mutexB->object;

    mutexB->state=JK_STATE_NEW;
    if( jkMutex==NULL ) return JK_ERR;
    DeleteCriticalSection( & jkMutex->threadMutex );
    return JK_OK;
}

static int JK_METHOD 
jk2_mutex_thread_lock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    EnterCriticalSection( &jkMutex->threadMutex );
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
    LeaveCriticalSection( &jkMutex->threadMutex );
    return JK_OK;
}


#elif defined( _REENTRANT )
/*-------------------- PThread - @deprecated, use APR if possible -------------------- */

#include <pthread.h>

static int JK_METHOD jk2_mutex_thread_init(jk_env_t *env, jk_bean_t  *mutexB)
{
    jk_mutex_t *jkMutex=mutexB->object;
    mutexB->state=JK_STATE_INIT;

    return pthread_mutex_init( &jkMutex->threadMutex, NULL );
}

static int JK_METHOD 
jk2_mutex_thread_destroy(jk_env_t *env, jk_bean_t  *mutexB)
{
    jk_mutex_t *jkMutex=mutexB->object;
    mutexB->state=JK_STATE_NEW;

    if( jkMutex==NULL ) return JK_ERR;
    return pthread_mutex_destroy( & jkMutex->threadMutex);
}

static int JK_METHOD 
jk2_mutex_thread_lock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    return pthread_mutex_lock( & jkMutex->threadMutex );
}

static int JK_METHOD 
jk2_mutex_thread_tryLock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    return JK_OK;
}

static int JK_METHOD 
jk2_mutex_thread_unLock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    return pthread_mutex_unlock( & jkMutex->threadMutex );
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
