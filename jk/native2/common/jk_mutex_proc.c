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
 * Mutex support.
 * 
 * @author Costin Manolache
 */

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_mutex.h"

#ifdef HAS_APR

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
    apr_status_t  st;

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

#else

int JK_METHOD jk2_mutex_proc_factory( jk_env_t *env ,jk_pool_t *pool,
                                      jk_bean_t *result,
                                      const char *type, const char *name)
{
    result->disabled=1;
    return JK_OK;
}
#endif
