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

/* ==================== Dispatch messages from java ==================== */

/** Called by java. Will call the right mutex method.
 */
int JK_METHOD jk2_mutex_invoke(jk_env_t *env, jk_bean_t *bean,
                               jk_endpoint_t *ep, int code, jk_msg_t *msg,
                               int raw)
{
    jk_mutex_t *mutex = (jk_mutex_t *)bean->object;
    int rc = JK_OK;

    if (mutex->mbean->debug > 0)
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "mutex.%d() \n", code);

    switch (code) {
    case MUTEX_LOCK:{
            if (mutex->mbean->debug > 0)
                env->l->jkLog(env, env->l, JK_LOG_DEBUG, "mutex.lock()\n");
            return mutex->lock(env, mutex);
        }
    case MUTEX_TRYLOCK:{
            if (mutex->mbean->debug > 0)
                env->l->jkLog(env, env->l, JK_LOG_DEBUG, "mutex.trylock()\n");
            return mutex->tryLock(env, mutex);
        }
    case MUTEX_UNLOCK:{
            if (mutex->mbean->debug > 0)
                env->l->jkLog(env, env->l, JK_LOG_DEBUG, "mutex.unlock()\n");
            return mutex->unLock(env, mutex);
        }
    }                           /* switch */
    return JK_ERR;
}
