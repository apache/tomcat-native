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

/** Will return endpoint specific runtime properties
 *
 *    uri          The uri that is beeing processed, NULL if the endpoing is inactive
 *    channelName  The channel that is using this endpoint
 *    workerName   The name of the worker using this endpoint
 *    requests     Number of requests beeing serviced by this
 *    avg_time, max_time
 *    
 */
static void * JK_METHOD jk2_endpoint_getAttribute(jk_env_t *env, jk_bean_t *bean, char *name ) {
    return NULL;
}

int JK_METHOD
jk2_endpoint_factory( jk_env_t *env, jk_pool_t *pool,
                      jk_bean_t *result,
                      const char *type, const char *name)
{
    jk_pool_t *endpointPool = pool->create( env, pool, HUGE_POOL_SIZE );
    jk_endpoint_t *e = (jk_endpoint_t *)endpointPool->alloc(env, endpointPool,
                                                            sizeof(jk_endpoint_t));
    if (e==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "endpoint.factory() OutOfMemoryException\n");
        return JK_FALSE;
    }
    
    e->pool = endpointPool;

    /* Init message storage areas. Protocol specific.
     */
    e->request = jk2_msg_ajp_create( env, e->pool, 0);
    e->reply = jk2_msg_ajp_create( env, e->pool, 0);
    e->post = jk2_msg_ajp_create( env, e->pool, 0);
    
    e->reuse = JK_FALSE;

    e->cPool=endpointPool->create(env, endpointPool, HUGE_POOL_SIZE );

    e->channelData = NULL;
    
    //    w->init= jk2_worker_ajp14_init;
    //    w->destroy=jk2_worker_ajp14_destroy;

    result->getAttribute= jk2_endpoint_getAttribute;
    result->object = e;
    e->mbean=result;

    return JK_TRUE;
}
