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
 * Run worker. It'll execute a process and monitor it. Derived from 
 * the good old jserv.
 *
 * 
 *
 * @author Costin Manolache
 */

#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_logger.h"
#include "jk_env.h"
#include "jk_requtil.h"
#include "jk_registry.h"

static int JK_METHOD jk2_worker_run_service(jk_env_t *env, jk_worker_t *_this,
                                            jk_ws_service_t *s)
{
    /* I should display a status page for the monitored processes
     */
    env->l->jkLog(env, env->l, JK_LOG_INFO, "run.service()\n");

    /* Generate the header */
    s->status = 500;
    s->msg = "Not supported";
    s->headers_out->put(env, s->headers_out,
                        "Content-Type", "text/html", NULL);

    s->head(env, s);

    s->afterRequest(env, s);
    return JK_OK;
}


int JK_METHOD jk2_worker_run_factory(jk_env_t *env, jk_pool_t *pool,
                                     jk_bean_t *result,
                                     const char *type, const char *name)
{
    jk_worker_t *_this;

    if (NULL == name) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "run_worker.factory() NullPointerException\n");
        return JK_ERR;
    }

    _this = (jk_worker_t *)pool->calloc(env, pool, sizeof(jk_worker_t));

    if (_this == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "run_worker.factory() OutOfMemoryException\n");
        return JK_ERR;
    }

    _this->service = jk2_worker_run_service;

    result->object = _this;
    _this->mbean = result;

    _this->workerEnv = env->getByName(env, "workerEnv");
    _this->workerEnv->addWorker(env, _this->workerEnv, _this);

    return JK_OK;
}
