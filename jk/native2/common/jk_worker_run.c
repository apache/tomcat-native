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
    s->status=500;
    s->msg="Not supported";
    s->headers_out->put(env, s->headers_out,
                        "Content-Type", "text/html", NULL);

    s->head(env, s );

    s->afterRequest( env, s);
    return JK_OK;
}

static int JK_METHOD jk2_worker_run_destroy(jk_env_t *env, jk_worker_t *w)
{
    int i = 0;

    if(w==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "run_worker.destroy() NullPointerException\n");
        return JK_ERR;
    }

    w->pool->close(env, w->pool);    

    return JK_OK;
}


int JK_METHOD jk2_worker_run_factory(jk_env_t *env, jk_pool_t *pool,
                                     jk_bean_t *result,
                                     const char *type, const char *name)
{
    jk_worker_t *_this;
    
    if(NULL == name ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "run_worker.factory() NullPointerException\n");
        return JK_ERR;
    }
    
    _this = (jk_worker_t *)pool->calloc(env, pool, sizeof(jk_worker_t));

    if(_this==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "run_worker.factory() OutOfMemoryException\n");
        return JK_ERR;
    }

    _this->pool=pool;

    _this->service        = jk2_worker_run_service;
    
    result->object=_this;
    _this->mbean=result;

    _this->workerEnv=env->getByName( env, "workerEnv" );
    _this->workerEnv->addWorker( env, _this->workerEnv, _this );

    return JK_OK;
}

