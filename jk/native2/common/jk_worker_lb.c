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
 * Author:      Costin Manolache
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_logger.h"
#include "jk_config.h"
#include "jk_env.h"
#include "jk_requtil.h"
#include "jk_mt.h"

#define DEFAULT_LB_FACTOR           (1.0)

/* Time to wait before retry... XXX make it configurable*/
#define WAIT_BEFORE_RECOVER (60) 

#define MAX_ATTEMPTS 3

#define NO_WORKER_MSG "The servlet container is temporary unavailable or beeing upgraded\n";

/** Find the best worker. In process, check if timeout expired
    for workers that failed in the past and give them another chance.

    This will check the JSESSIONID and forward to the right worker
    if in a session.

    It'll also adjust the load balancing factors.
*/
static jk_worker_t *jk2_get_most_suitable_worker(jk_env_t *env, jk_worker_t *lb, 
                                                 jk_ws_service_t *s, int attempt)
{
    jk_worker_t *rc = NULL;
    int lb_min = 0;    
    int lb_max = 0;    
    int i;
    int j;
    int level;
    int currentLevel=JK_LB_LEVELS - 1;
    char *session_route;
    time_t now = 0;

    session_route = jk2_requtil_getSessionRoute(env, s);
       
    if(session_route) {
        for( level=0; level<JK_LB_LEVELS; level++ ) {
            for(i = 0 ; i < lb->workerCnt[level]; i++) {
                jk_worker_t *w=lb->workerTables[level][i];
                
                if(w->route != NULL &&
                   0 == strcmp(session_route, w->route)) {
                    if(attempt > 0 && w->in_error_state) {
                        /* We already tried to revive this worker. */
                        break;
                    } else {
                        return w;
                    }
                }
            }
        }
    }

    /** Get one worker that is ready
     */
    for( level=0; level<JK_LB_LEVELS; level++ ) {
        for(i = 0 ; i < lb->workerCnt[level] ; i++) {
            jk_worker_t *w=lb->workerTables[level][i];

            if( w->mbean->disabled ) continue;
            if( w->in_error_state ) continue;

            if( rc==NULL ) {
                rc=w;
                currentLevel=level;
                lb_min=w->lb_value;
                continue;
            }
            
            if( w->lb_value < lb_min ) {
                lb_min = w->lb_value;
                rc = w;
                currentLevel=level;
            }
        }

        if( rc!=NULL ) {
            /* We found a valid worker on the current level, don't worry about the
               higher levels.
            */
            break;
        }

        if( lb->hwBalanceErr > 0 ) {
            /* don't go to higher levels - we'll return an error
             */
            currentLevel=0;
            break;
        }
    }

    /** Reenable workers in error state if the timeout has passed.
     *  Don't bother with 'higher' levels, since we'll never try them.
     */
    for( level=0; level<=currentLevel; level++ ) {

        for(i = 0 ; i < lb->workerCnt[level] ; i++) {
            jk_worker_t *w=lb->workerTables[level][i];

            if( w->mbean->disabled ) continue;
        
            if(w->in_error_state) {
                /* Check if it's ready for recovery */
                if( now==0 ) now = time(NULL);
                
                if((now - w->error_time) > WAIT_BEFORE_RECOVER) {
                    env->l->jkLog(env, env->l, JK_LOG_ERROR,
                                  "lb.getWorker() reenable %s\n", w->mbean->name);
                    w->in_error_state = JK_FALSE;

                    /* Find max lb */
                    if( lb_max ==0 ) {
                        for( j=0; j<lb->workerCnt[level]; j++ ) {
                            if( lb->workerTables[level][j]->lb_value > lb_max ) {
                                lb_max=lb->workerTables[level][j]->lb_value;
                            }
                        }
                    }
                    w->lb_value = lb_max;
                }
            }
        }
    }

    /* If no active worker is found, we'll try all workers in error_state,
    */
    if ( rc==NULL ) {
        /* no workers found (rc is null), now try as hard as possible to get a
           worker anyway, pick one with largest error time.. */
        int error_workers=0;
        
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "lb.getWorker() All workers in error state, use the one with oldest error\n");
        
        for( level=0; level<JK_LB_LEVELS; level++ ) {
            for(i = 0 ; i < lb->workerCnt[level] ; i++) {
                jk_worker_t *w=lb->workerTables[level][i];

                if( w->mbean->disabled == JK_TRUE ) continue;

                error_workers++;

                if( rc==NULL ) {
                    rc= w;
                    currentLevel=level;
                    continue;
                }
                /* pick the oldest failed worker */
                if ( w->error_time < rc->error_time ) {
                    currentLevel=level;
                    rc = w;
                }
            }

            if( lb->hwBalanceErr > 0 ) {
                /* Don't try higher levels, only level=0 */
                break;
            }
        }

        if( attempt >= error_workers ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "lb.getWorker() We tried all possible workers %d\n", attempt );
            return NULL;

        }
    }
    
    if(rc!=NULL) {
        /* It it's the default, it'll remain the default - we don't
           increase the factor
        */
        rc->in_error_state  = JK_FALSE;
        if( rc->lb_value != 0 ) {
            int newValue=rc->lb_value + rc->lb_factor;
            
            if( newValue > 255 ) {
                rc->lb_value=rc->lb_factor;
                /* Roll over. This has 2 goals:
                   - avoid the lb factor becoming too big, and give a chance to run to
                   workers that were in error state ( I think it's cleaner than looking for "max" )
                   - the actual lb_value will be 1 byte. Even on the craziest platform, that
                   will be an atomic write. We do a lot of operations on lb_value in a MT environment,
                   and the chance of reading something inconsistent is considerable. Since APR
                   will not support atomic - and adding a CS would cost too much, this is actually
                   a good solution.

                   Note that lb_value is not used for anything critical - just to balance the load,
                   the worst that may happen is having a worker stay idle for 255 requests.
                */
                for(i = 0 ; i < lb->workerCnt[currentLevel] ; i++) {
                    jk_worker_t *w=lb->workerTables[currentLevel][i];
                    w->lb_value=w->lb_factor;
                }
            } else {
                rc->lb_value=newValue;
            }
        }
    }

    return rc;
}


/** Get the best worker and forward to it.
    Since we don't directly connect to anything, there's no
    need for an endpoint.
*/
static int JK_METHOD jk2_lb_service(jk_env_t *env,
                                    jk_worker_t *lb,
                                    jk_ws_service_t *s)
{
    int attempt=0;
    int i;
    jk_workerEnv_t *wEnv=lb->workerEnv;
    
    if( s==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb.service() NullPointerException\n");
        return JK_ERR;
    }

    /* you can not recover on another load balancer */
    s->realWorker=NULL;


    /* Check for configuration updates
     */
    if( wEnv->shm != NULL && wEnv->shm->head != NULL ) {
        /* We have shm, let's check for updates. This is just checking one
           memory location, no lock involved. It is possible we'll read it
           while somebody else is changing - but that's ok, we just check for
           equality.
        */
        /* We should check this periodically
         */
        if( wEnv->config->ver != wEnv->shm->head->lbVer ) {
            wEnv->config->update(env, wEnv->config, NULL );
            wEnv->config->ver = wEnv->shm->head->lbVer;
        }
    }
    
    while(1) {
        jk_worker_t *rec;
        int rc;

        rec=jk2_get_most_suitable_worker(env, lb, s, attempt);
        attempt++;
        
        s->is_recoverable_error = JK_FALSE;

        if(rec == NULL) {
            /* NULL record, no more workers left ... */
            env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                          "lb_worker.service() all workers in error or disabled state\n");
            /* set hwBalanceErr status */
            if( lb->hwBalanceErr != 0 ) {
                s->status=lb->hwBalanceErr;
            } else {
                s->status=lb->noWorkerCode; /* SERVICE_UNAVAILABLE is the default */
            }

            if( s->status == 302 ) {
                s->headers_out->put(env, s->headers_out,
                                    "Location", lb->noWorkerMsg, NULL);
                s->head(env, s );
            } else {
                s->headers_out->put(env, s->headers_out,
                                    "Content-Type", "text/html", NULL);
                s->head(env, s );
                s->jkprintf(env, s, lb->noWorkerMsg );
            }
            
            s->afterRequest( env, s);
            return JK_ERR;
        }

        if( lb->mbean->debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "lb.service() try %s\n", rec->mbean->name );
        
        if( rec->route==NULL ) {
            rec->route=rec->mbean->localName;
        }
        
        s->jvm_route = rec->route;

        s->realWorker = rec;

        rc = rec->service(env, rec, s);

        if(rc==JK_OK) {                        
            rec->in_error_state = JK_FALSE;
            rec->error_time     = 0;
            /* the endpoint that succeeded is saved for done() */
            return JK_OK;
        }
        
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "lb.service() worker failed %s\n", rec->mbean->name );
        
        /*
         * Service failed !!!
         *
         * Time for fault tolerance (if possible)...
         */
        rec->in_error_state = JK_TRUE;
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
        if( lb->mbean->debug > 0 ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                          "lb_worker.service() try other hosts\n");
        }
    }
    return JK_ERR;
}

/** Init internal structures.
    Called any time the config changes
*/
static int JK_METHOD jk2_lb_refresh(jk_env_t *env, jk_worker_t *lb)
{
    int currentWorker=0;
    int i;
    int level;
    int num_of_workers=lb->lbWorkerMap->size( env, lb->lbWorkerMap);

    for(i = 0 ; i < num_of_workers ; i++) {
        char *name = lb->lbWorkerMap->nameAt( env, lb->lbWorkerMap, i);
        jk_worker_t *w= env->getByName( env, name );
        int level=0;
        int pos=0;

        if( w== NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "lb_worker.init(): no worker found %s\n", name);
            continue;
        }

        if( w->mbean->disabled ) continue;
        
        level=w->level;
        
        /* It's like disabled */
        if( level >= JK_LB_LEVELS ) continue;

        pos=lb->workerCnt[level]++;
        
        lb->workerTables[level][pos]=w;

        w->lb_value = w->lb_factor;
        w->in_error_state = JK_FALSE;
    }
    
    return JK_OK;
}


static char *jk2_worker_lb_multiValueInfo[]={"worker", NULL };
static char *jk2_worker_lb_setAttributeInfo[]={"hwBalanceErr", "noWorkerMsg", "noWorkerCode", NULL };

static int JK_METHOD jk2_lb_setAttribute(jk_env_t *env, jk_bean_t *mbean, 
                                         char *name, void *valueP)
{
    jk_worker_t *lb=mbean->object;
    char *value=valueP;
    int err;
    char **worker_names;
    unsigned num_of_workers;
    unsigned i = 0;
    char *tmp;
    
    if( strcmp( name, "worker") == 0 ) {
        if( lb->lbWorkerMap->get( env, lb->lbWorkerMap, name) != NULL ) {
            /* Already added */
            return JK_OK;
        }
        name = lb->mbean->pool->pstrdup(env, lb->mbean->pool, name);
        lb->lbWorkerMap->add(env, lb->lbWorkerMap, name, "");
        
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "lb_worker.setAttribute(): Adding to %s: %s\n", lb->mbean->localName, name);

        jk2_lb_refresh( env, lb );
        return JK_OK;
    } else if( strcmp( name, "noWorkerMsg") == 0 ) {
        lb->noWorkerMsg=value;
    } else if( strcmp( name, "noWorkerCode") == 0 ) {
        lb->noWorkerCode=atoi( value );
    } else if( strcmp( name, "hwBalanceErr") == 0 ) {
        lb->hwBalanceErr=atoi( value );
    }

    return JK_ERR;
}


static int JK_METHOD jk2_lb_init(jk_env_t *env, jk_bean_t *bean)
{
    jk_worker_t *lb=bean->object;
    int err;
    char **worker_names;
    int i = 0;
    char *tmp;
    int num_of_workers=lb->lbWorkerMap->size( env, lb->lbWorkerMap);

    err=jk2_lb_refresh(env, lb );
    if( err != JK_OK )
        return err;

    /*     if( lb->workerEnv->shm != NULL && lb->workerEnv->shm->head != NULL)  */
    /*         jk2_lb_updateWorkers(env, lb, lb->workerEnv->shm); */

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "lb.init() %s %d workers\n",
                  lb->mbean->name, num_of_workers );
    
    return JK_OK;
}

static int JK_METHOD jk2_lb_destroy(jk_env_t *env, jk_bean_t *bean)
{
    jk_worker_t *w=bean->object;
    /* Workers are destroyed by the workerEnv. It is possible
       that a worker is part of more than a lb.
       Nothing to clean up so far.
    */
    return JK_OK;
}


int JK_METHOD jk2_worker_lb_factory(jk_env_t *env,jk_pool_t *pool,
                                    jk_bean_t *result, char *type, char *name)
{
    jk_worker_t *w;
    int i;
    
    if(NULL == name ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb_worker.factory() NullPointerException\n");
        return JK_ERR;
    }
    
    w = (jk_worker_t *)pool->calloc(env, pool, sizeof(jk_worker_t));

    if(w==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb_worker.factory() OutOfMemoryException\n");
        return JK_ERR;
    }

    w->worker_private = NULL;
    w->service        = jk2_lb_service;
    
    for( i=0; i<JK_LB_LEVELS; i++ ) {
        w->workerCnt[i]=0;
    }
    
    jk2_map_default_create(env,&w->lbWorkerMap, pool);

    result->init           = jk2_lb_init;
    result->destroy        = jk2_lb_destroy;
    result->setAttribute=jk2_lb_setAttribute;
    result->multiValueInfo=jk2_worker_lb_multiValueInfo;
    result->setAttributeInfo=jk2_worker_lb_setAttributeInfo;
    result->object=w;
    w->mbean=result;

    w->hwBalanceErr=0;
    w->noWorkerCode=503;
    w->noWorkerMsg=NO_WORKER_MSG;

    w->workerEnv=env->getByName( env, "workerEnv" );
    w->workerEnv->addWorker( env, w->workerEnv, w );
    
    return JK_OK;
}

