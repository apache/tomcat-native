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

/***************************************************************************
 * Description: Load balancer worker, knows how to load balance among      *
 *              several workers.                                           *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Based on:                                                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_logger.h"
#include "jk_env.h"
#include "jk_requtil.h"

#define DEFAULT_LB_FACTOR           (1.0)

/* Time to wait before retry... */
#define WAIT_BEFORE_RECOVER (60*1) 

#define ADDITINAL_WAIT_LOAD (20)

int JK_METHOD jk2_worker_lb_factory(jk_env_t *env, jk_pool_t *pool,
                                   void **result,char *type, char *name);



/* Find the biggest lb_value for all my workers.
 * This + ADDITIONAL_WAIT_LOAD will be set on all the workers
 * that recover after an error.
 */
static double jk2_get_max_lb(jk_worker_t *p) 
{
    int i;
    double rc = 0.0;    

    for(i = 0 ; i < p->num_of_workers ; i++) {
        if(!p->lb_workers[i]->in_error_state) {
            if(p->lb_workers[i]->lb_value > rc) {
                rc = p->lb_workers[i]->lb_value;
            }
        }            
    }
    return rc;
}

/** Find the best worker. In process, check if timeout expired
    for workers that failed in the past and give them another chance.

    This will check the JSESSIONID and forward to the right worker
    if in a session.

    It'll also adjust the load balancing factors.
*/
static jk_worker_t *jk2_get_most_suitable_worker(jk_env_t *env, jk_worker_t *p, 
                                                 jk_ws_service_t *s, int attempt)
{
    jk_worker_t *rc = NULL;
    double lb_min = 0.0;    
    int i;
    char *session_route = jk2_requtil_getSessionRoute(env, s);
       
    if(session_route) {
        for(i = 0 ; i < p->num_of_workers ; i++) {
            if(0 == strcmp(session_route, p->lb_workers[i]->name)) {
                if(attempt > 0 && p->lb_workers[i]->in_error_state) {
                   break;
                } else {
                    return p->lb_workers[i];
                }
            }
        }
    }

    for(i = 0 ; i < p->num_of_workers ; i++) {
        if(p->lb_workers[i]->in_error_state) {
            if(!p->lb_workers[i]->in_recovering) {
                time_t now = time(0);
                
                if((now - p->lb_workers[i]->error_time) > WAIT_BEFORE_RECOVER) {
                    
                    p->lb_workers[i]->in_recovering  = JK_TRUE;
                    p->lb_workers[i]->error_time     = now;
                    rc = p->lb_workers[i];

                    break;
                }
            }
        } else {
            if(p->lb_workers[i]->lb_value < lb_min || !rc) {
                lb_min = p->lb_workers[i]->lb_value;
                rc = p->lb_workers[i];
            }
        }            
    }

    if(rc) {
        rc->lb_value += rc->lb_factor;                
    }

    return rc;
}


/** Get the best worker and forward to it.
    Since we don't directly connect to anything, there's no
    need for an endpoint.
*/
static int JK_METHOD jk2_lb_service(jk_env_t *env,
                                    jk_worker_t *w,
                                    jk_ws_service_t *s)
{
    int attempt=0;

    if( s==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb.service() NullPointerException\n");
        return JK_FALSE;
    }

    /* you can not recover on another load balancer */
    s->realWorker=NULL;

    while(1) {
        jk_worker_t *rec = jk2_get_most_suitable_worker(env, w, s, attempt++);
        int rc;

        s->is_recoverable_error = JK_FALSE;

        if(rec == NULL) {
            /* NULL record, no more workers left ... */
            env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                          "lb_worker.service() No suitable workers left \n");
            return JK_FALSE;
        }
                
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "lb.service() try %s\n", rec->name );
        
        s->jvm_route = s->pool->pstrdup(env, s->pool,  rec->name);

        rc = rec->service(env, rec, s);

        if(rc==JK_TRUE) {                        
            if(rec->in_recovering) {
                rec->lb_value = jk2_get_max_lb(rec) + ADDITINAL_WAIT_LOAD;
            }
            rec->in_error_state = JK_FALSE;
            rec->in_recovering  = JK_FALSE;
            rec->error_time     = 0;
            /* the endpoint that succeeded is saved for done() */
            s->realWorker = rec;
            return JK_TRUE;
        }
        
        /*
         * Service failed !!!
         *
         * Time for fault tolerance (if possible)...
         */
        rec->in_error_state = JK_TRUE;
        rec->in_recovering  = JK_FALSE;
        rec->error_time     = time(0);
        
        if(!s->is_recoverable_error) {
            /* Error is not recoverable - break with an error. */
            env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                          "lb.service() unrecoverable error...\n");
            break;
        }
        
        /* 
         * Error is recoverable by submitting the request to
         * another worker... Lets try to do that.
         */
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "lb_worker.service() try other host\n");
    }
    return JK_FALSE;
}

static int JK_METHOD jk2_lb_setProperty(jk_env_t *env, jk_worker_t *_this,
                                        char *name, char *value)
{
    int err;
    char **worker_names;
    unsigned num_of_workers;
    unsigned i = 0;
    char *tmp;

    /* XXX Add one-by-one */
    
    if( strcmp( name, "balanced_workers") == 0 ) {
        worker_names=jk2_map_split( env, NULL, _this->pool,
                                    value, NULL, &num_of_workers );
        if( worker_names==NULL || num_of_workers==0 ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "lb_worker.validate(): no defined workers\n");
            return JK_FALSE;
        }
        for(i = 0 ; i < num_of_workers ; i++) {
            char *name = _this->pool->pstrdup(env, _this->pool, worker_names[i]);
            _this->lbWorkerMap->add(env, _this->lbWorkerMap, name, "");
        }
        return JK_TRUE;
    }
    return JK_FALSE;
}
    
static int JK_METHOD jk2_lb_init(jk_env_t *env, jk_worker_t *_this)
{
    int err;
    char **worker_names;
    int i = 0;
    int currentWorker=0;
    char *tmp;

    int num_of_workers=_this->lbWorkerMap->size( env, _this->lbWorkerMap);
    _this->lb_workers =
        _this->pool->alloc(env, _this->pool, 
                           num_of_workers * sizeof(jk_worker_t *));
    
    if(!_this->lb_workers) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb_worker.validate(): OutOfMemoryException\n");
        return JK_FALSE;
    }

    for(i = 0 ; i < num_of_workers ; i++) {
        char *name = _this->lbWorkerMap->nameAt( env, _this->lbWorkerMap, i);
        jk_worker_t *w= _this->workerEnv->getWorkerForName( env, _this->workerEnv, name );
        if( w== NULL )
            continue;
        
        _this->lb_workers[currentWorker]=w;
        
        tmp=jk2_map_getStrProp( env, _this->workerEnv->initData, "worker", name, "lbfactor", NULL );

        if( tmp==NULL ) 
            _this->lb_workers[currentWorker]->lb_factor = DEFAULT_LB_FACTOR;
        else 
            _this->lb_workers[currentWorker]->lb_factor = atof( tmp );
        
        _this->lb_workers[currentWorker]->lb_factor =
            1/ _this->lb_workers[currentWorker]->lb_factor;

        /* 
         * Allow using lb in fault-tolerant mode.
         * Just set lbfactor in worker.properties to 0 to have 
         * a worker used only when principal is down or session route
         * point to it. Provided by Paul Frieden <pfrieden@dchain.com>
         */
        _this->lb_workers[currentWorker]->lb_value =
            _this->lb_workers[currentWorker]->lb_factor;
        _this->lb_workers[currentWorker]->in_error_state = JK_FALSE;
        _this->lb_workers[currentWorker]->in_recovering  = JK_FALSE;
        currentWorker++;
    }
    
    _this->num_of_workers=currentWorker;

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "lb.validate() %s %d workers\n",
                  _this->name, _this->num_of_workers );

    return JK_TRUE;
}

static int JK_METHOD jk2_lb_destroy(jk_env_t *env, jk_worker_t *w)
{
    int i = 0;

    if(w==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb_worker.destroy() NullPointerException\n");
        return JK_FALSE;
    }

    /* Workers are destroyed by the workerEnv. It is possible
       that a worker is part of more than a lb.
    */
    /*
    for(i = 0 ; i < w->num_of_workers ; i++) {
        w->lb_workers[i]->destroy( env, w->lb_workers[i]);
    }
    */

    w->pool->close(env, w->pool);    

    return JK_TRUE;
}


int JK_METHOD jk2_worker_lb_factory(jk_env_t *env,jk_pool_t *pool,
                                   void **result, char *type, char *name)
{
    jk_worker_t *_this;
    
    if(NULL == name ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb_worker.factory() NullPointerException\n");
        return JK_FALSE;
    }
    
    _this = (jk_worker_t *)pool->calloc(env, pool, sizeof(jk_worker_t));

    if(_this==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb_worker.factory() OutOfMemoryException\n");
        return JK_FALSE;
    }

    _this->name=name;
    _this->pool=pool;

    _this->lb_workers = NULL;
    _this->num_of_workers = 0;
    _this->worker_private = NULL;
    _this->setProperty    = jk2_lb_setProperty;
    _this->init           = jk2_lb_init;
    _this->destroy        = jk2_lb_destroy;
    _this->service        = jk2_lb_service;
   
    jk2_map_default_create(env,&_this->lbWorkerMap, _this->pool);

    *result=_this;

    /*     env->l->jkLog(env, env->l, JK_LOG_INFO, */
    /*                   "lb_worker.factory() New lb worker\n"); */
    
    /* name, pool will be set by workerEnv ( our factory ) */
    
    return JK_TRUE;
}

