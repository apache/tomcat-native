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
 * Mutex support.
 * 
 * @author Costin Manolache
 */

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_mutex.h"

#include "apr_proc_mutex.h"


static int JK_METHOD jk2_mutex_proc_init(jk_env_t *env, jk_bean_t  *mutexB)
{
    jk_mutex_t *jkMutex=mutexB->object;
    
    apr_proc_mutex_t *mutex;
    apr_lockmech_e mech=(apr_lockmech_e)jkMutex->mechanism;

    apr_pool_t *pool=(apr_pool_t *)env->getAprPool(env);
    
    apr_status_t  st;
    char *fname=jkMutex->fname;

    st=apr_proc_mutex_create( &mutex, fname, mech, pool );

    jkMutex->privateData=mutex;
    
    return st;
}

static int JK_METHOD 
jk2_mutex_proc_destroy(jk_env_t *env, jk_bean_t  *mutexB)
{
    jk_mutex_t *jkMutex=mutexB->object;
    
    apr_proc_mutex_t *mutex=(apr_proc_mutex_t *)jkMutex->privateData;
    apr_status_t  st = 0;

    if( mutex!= NULL )
        st=apr_proc_mutex_destroy( mutex );
    
    return st;
}

static int JK_METHOD 
jk2_mutex_proc_lock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    apr_proc_mutex_t *mutex=(apr_proc_mutex_t *)jkMutex->privateData;
    apr_status_t  st;
    
    st=apr_proc_mutex_lock( mutex );
    
    return st;
}

static int JK_METHOD 
jk2_mutex_proc_tryLock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    apr_proc_mutex_t *mutex=(apr_proc_mutex_t *)jkMutex->privateData;
    apr_status_t  st;
    
    st=apr_proc_mutex_trylock( mutex );
    
    return st;
}

static int JK_METHOD 
jk2_mutex_proc_unLock(jk_env_t *env, jk_mutex_t  *jkMutex)
{
    apr_proc_mutex_t *mutex=(apr_proc_mutex_t *)jkMutex->privateData;
    apr_status_t  st;
    
    st=apr_proc_mutex_unlock( mutex );
    
    return st;
}

static int JK_METHOD jk2_mutex_proc_setAttribute( jk_env_t *env, jk_bean_t *mbean, char *name, void *valueP ) {
    jk_mutex_t *mutex=(jk_mutex_t *)mbean->object;
    char *value=(char *)valueP;
    
    if( strcmp( "file", name ) == 0 ) {
    mutex->fname=value;
    } else if( strcmp( "mechanism", name ) == 0 ) {
    mutex->mechanism=atoi(value);
    } else {
    return JK_ERR;
    }
    return JK_OK;   

}

int JK_METHOD jk2_mutex_proc_factory( jk_env_t *env ,jk_pool_t *pool,
                               jk_bean_t *result,
                               const char *type, const char *name)
{
    jk_mutex_t *mutex;

    mutex=(jk_mutex_t *)pool->calloc(env, pool, sizeof(jk_mutex_t));

    if( mutex == NULL )
        return JK_ERR;

    mutex->pool=pool;
    mutex->privateData=NULL;

    result->setAttribute=jk2_mutex_proc_setAttribute;
    /* result->getAttribute=jk2_mutex_getAttribute; */
    mutex->mbean=result; 
    result->object=mutex;
    
    result->init=jk2_mutex_proc_init;
    result->destroy=jk2_mutex_proc_destroy;
    result->invoke=jk2_mutex_invoke;
    
    mutex->lock=jk2_mutex_proc_lock;
    mutex->tryLock=jk2_mutex_proc_tryLock;
    mutex->unLock=jk2_mutex_proc_unLock;
    
    return JK_OK;
}
