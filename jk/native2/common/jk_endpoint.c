/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2002 The Apache Software Foundation.          *
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


static int JK_METHOD jk2_endpoint_init(jk_env_t *env, jk_bean_t *bean ) {
    jk_endpoint_t *ep=(jk_endpoint_t *)bean->object;
    jk_stat_t *stats;
    jk_workerEnv_t *wEnv=ep->workerEnv;

    /* alloc it inside the shm if possible */
    if( wEnv->epStat==NULL ) {
        if( wEnv->shm != NULL && wEnv->childId >= 0 ) {
            char shmName[128];
            snprintf( shmName, 128, "epStat.%d", wEnv->childId );
            
            wEnv->epStat=wEnv->shm->createSlot( env, wEnv->shm, shmName, 8096 );
            if (wEnv->epStat==NULL) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "workerEnv.init() create slot %s failed\n",  shmName );
                return JK_ERR;
            }
            wEnv->epStat->structCnt=0;
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "workerEnv.init() create slot %s\n",  shmName );
        }
    }

    if( wEnv->epStat != NULL && wEnv->childId >= 0 ) {
        jk_stat_t *statArray=(jk_stat_t *)wEnv->epStat->data;
        stats= & statArray[ ep->mbean->id ];
        ep->workerEnv->epStat->structSize=sizeof( jk_stat_t );
        ep->workerEnv->epStat->structCnt=ep->mbean->id + 1;
        if( ep->worker != NULL && ep->worker->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "SHM stats %d %#lx %#lx %s %s childId=%d\n", ep->mbean->id,
                          ep->workerEnv->epStat->data, stats,
                          ep->mbean->localName, ep->mbean->name, ep->workerEnv->childId);
    } else {
        stats = (jk_stat_t *)ep->mbean->pool->calloc( env, ep->mbean->pool, sizeof( jk_stat_t ) );
        if( ep->worker != NULL && ep->worker->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "Local stats %d %#lx %d\n", ep->mbean->id, ep->workerEnv->epStat, ep->workerEnv->childId );
    }
    
    ep->stats=stats;

    ep->stats->reqCnt=0;
    ep->stats->errCnt=0;
#ifdef HAS_APR
    ep->stats->maxTime=0;
    ep->stats->totalTime=0;
#endif

	return JK_OK;
}

int JK_METHOD
jk2_endpoint_factory( jk_env_t *env, jk_pool_t *pool,
                          jk_bean_t *result,
                      const char *type, const char *name)
{
    jk_endpoint_t *e = (jk_endpoint_t *)pool->calloc(env, pool,
                                                    sizeof(jk_endpoint_t));
    int epId;

    if (e==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "endpoint.factory() OutOfMemoryException\n");
        return JK_ERR;
    }
    
    /* Init message storage areas. Protocol specific.
     */
    e->request = jk2_msg_ajp_create( env, pool, 0);
    e->reply = jk2_msg_ajp_create( env, pool, 0);
    e->post = jk2_msg_ajp_create( env, pool, 0);

    e->readBuf=pool->alloc( env, pool, 8096 );
    e->bufPos=0;
    
    result->init=jk2_endpoint_init;
    
    e->sd=-1;
    e->recoverable=JK_TRUE;
    e->cPool=pool->create(env, pool, HUGE_POOL_SIZE );
    e->stats = NULL;
    e->channelData = NULL;
    e->currentRequest = NULL;
    e->worker=NULL;
    epId=atoi( result->localName );
    
    result->object = e;
    e->mbean=result;

    e->workerEnv=env->getByName( env, "workerEnv" );
    
    return JK_OK;
}
