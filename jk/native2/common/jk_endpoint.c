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
 * Endpoint represents a connection between the jk server and client.
 *  ( tomcat and apache )
 *
 * @author Costin Manolache
 */

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_channel.h"
#include "jk_msg.h"
#include "jk_logger.h"
#include "jk_handler.h"
#include "jk_service.h"
#include "jk_env.h"
#include "jk_objCache.h"
#include "jk_registry.h"


static int JK_METHOD jk2_endpoint_init(jk_env_t *env, jk_bean_t *bean)
{
    jk_endpoint_t *ep = (jk_endpoint_t *)bean->object;
    jk_stat_t *stats;
    jk_workerEnv_t *wEnv = ep->workerEnv;

    /* alloc it inside the shm if possible */
    if (wEnv->epStat == NULL) {
        if (wEnv->shm != NULL && wEnv->childId >= 0) {
            char shmName[128];
            apr_snprintf(shmName, 128, "epStat.%d", wEnv->childId);

            wEnv->epStat =
                wEnv->shm->createSlot(env, wEnv->shm, shmName, 8096);
            if (wEnv->epStat == NULL) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "workerEnv.init() create slot %s failed\n",
                              shmName);
                /* If epStat is NULL - no statistics will be collected, but the server should still work.
                 */
                /*return JK_ERR; */
            }
            else {
                wEnv->epStat->structCnt = 0;
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                              "workerEnv.init() create slot %s\n", shmName);
            }
        }
    }

    if (wEnv->epStat != NULL && wEnv->childId >= 0) {
        jk_stat_t *statArray = (jk_stat_t *)wEnv->epStat->data;
        stats = &statArray[ep->mbean->id];
        ep->workerEnv->epStat->structSize = sizeof(jk_stat_t);
        ep->workerEnv->epStat->structCnt = ep->mbean->id + 1;
        if (ep->worker != NULL && ep->worker->mbean->debug > 0)
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "SHM stats %d %#lx %#lx %s %s childId=%d\n",
                          ep->mbean->id, ep->workerEnv->epStat->data, stats,
                          ep->mbean->localName, ep->mbean->name,
                          ep->workerEnv->childId);
    }
    else {
        stats =
            (jk_stat_t *)ep->mbean->pool->calloc(env, ep->mbean->pool,
                                                 sizeof(jk_stat_t));
        if (ep->worker != NULL && ep->worker->mbean->debug > 0)
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "Local stats %d %#lx %d\n", ep->mbean->id,
                          ep->workerEnv->epStat, ep->workerEnv->childId);
    }

    ep->stats = stats;

    ep->stats->reqCnt = 0;
    ep->stats->errCnt = 0;
    ep->stats->maxTime = 0;
    ep->stats->totalTime = 0;

    bean->state = JK_STATE_INIT;

    return JK_OK;
}

static char *getAttInfo[] = { "id", NULL };

static void *JK_METHOD jk2_endpoint_getAttribute(jk_env_t *env,
                                                 jk_bean_t *bean, char *name)
{
    jk_endpoint_t *ep = (jk_endpoint_t *)bean->object;

    if (strcmp(name, "id") == 0) {
        return "1";
    }
    else if (strcmp("inheritGlobals", name) == 0) {
        return "";
    }
    return NULL;
}



int JK_METHOD
jk2_endpoint_factory(jk_env_t *env, jk_pool_t *pool,
                     jk_bean_t *result, const char *type, const char *name)
{
    jk_endpoint_t *e = (jk_endpoint_t *)pool->calloc(env, pool,
                                                     sizeof(jk_endpoint_t));
    int epId;

    if (e == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "endpoint.factory() OutOfMemoryException\n");
        return JK_ERR;
    }

    /* Init message storage areas. Protocol specific.
     */
    e->request = jk2_msg_ajp_create(env, pool, 0);
    e->reply = jk2_msg_ajp_create(env, pool, 0);
    e->post = jk2_msg_ajp_create(env, pool, 0);

    e->readBuf = pool->alloc(env, pool, 8096);
    e->bufPos = 0;

    result->init = jk2_endpoint_init;

    e->sd = -1;
    e->recoverable = JK_TRUE;
    e->cPool = pool->create(env, pool, HUGE_POOL_SIZE);
    e->stats = NULL;
    e->channelData = NULL;
    e->currentRequest = NULL;
    e->worker = NULL;
    epId = atoi(result->localName);

    result->object = e;
    result->getAttributeInfo = getAttInfo;
    result->getAttribute = jk2_endpoint_getAttribute;
    e->mbean = result;

    e->workerEnv = env->getByName(env, "workerEnv");

    return JK_OK;
}
