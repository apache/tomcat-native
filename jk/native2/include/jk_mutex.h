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

#ifndef JK_MUTEX_H
#define JK_MUTEX_H

#include "jk_global.h"
#include "jk_env.h"
#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_msg.h"
#include "jk_service.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
struct jk_mutex {
    struct jk_bean *mbean;

    struct jk_pool *pool;

    /** Name of the mutex */
    char *fname;

    /** APR mechanism */
    int mechanism;

    /** 
     */
    int (JK_METHOD *lock)(struct jk_env *env, struct jk_mutex *mutex);

    /** 
     */
    int (JK_METHOD *tryLock)(struct jk_env *env, struct jk_mutex *mutex);

    /** 
     */
    int (JK_METHOD *unLock)(struct jk_env *env, struct jk_mutex *mutex);

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

int JK_METHOD jk2_mutex_invoke(struct jk_env *env, struct jk_bean *bean, struct jk_endpoint *ep, int code,
                               struct jk_msg *msg, int raw);

    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif 
