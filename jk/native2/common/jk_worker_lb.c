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

/* Time to wait before retry... */
/* XXX make it longer - debugging only */
#define WAIT_BEFORE_RECOVER (5) 

#define ADDITINAL_WAIT_LOAD (20)


/* Find the biggest lb_value for all my workers.
 * This + ADDITIONAL_WAIT_LOAD will be set on all the workers
 * that recover after an error.
 */
static double jk2_get_max_lb(jk_worker_t *lb) 
{
    int i;
    double rc = 0.0;    

    for(i = 0 ; i < lb->num_of_workers ; i++) {
        if(!lb->lb_workers[i]->in_error_state) {
            if(lb->lb_workers[i]->lb_value > rc) {
                rc = lb->lb_workers[i]->lb_value;
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
static jk_worker_t *jk2_get_most_suitable_worker(jk_env_t *env, jk_worker_t *lb, 
                                                 jk_ws_service_t *s, int attempt)
{
    jk_worker_t *rc = NULL;
    double lb_min = 0.0;    
    int i;
    char *session_route;
    time_t now = 0;

    session_route = jk2_requtil_getSessionRoute(env, s);
       
    if(session_route) {
        for(i = 0 ; i < lb->num_of_workers ; i++) {
            if(0 == strcmp(session_route, lb->lb_workers[i]->route)) {
                if(attempt > 0 && lb->lb_workers[i]->in_error_state) {
                   break;
                } else {
                    return lb->lb_workers[i];
                 }
            }
        }
    }

    /** Get one worker that is ready */
    for(i = 0 ; i < lb->num_of_workers ; i++) {
        if(lb->lb_workers[i]->in_error_state) {
            /* Check if it's ready for recovery */
            /* if(!lb->lb_workers[i]->in_recovering) { */
            if( now==0 )
                now = time(NULL);
                
            if((now - lb->lb_workers[i]->error_time) > WAIT_BEFORE_RECOVER) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "lb.getWorker() timeout expired, reenable again %s\n", lb->
                              lb_workers[i]->mbean->name);
                
                lb->lb_workers[i]->in_recovering  = JK_TRUE;
                lb->lb_workers[i]->in_error_state=JK_FALSE;
                /* lb->lb_workers[i]->error_time     = now; */
                /* lb->lb_workers[i]->retry_count++; */
                /* rc = lb->lb_workers[i]; */

                /* Don't give bigger priority to recovered workers
                   break;
                */
            }
        }
        if( ! lb->lb_workers[i]->in_error_state ) {
            if(lb->lb_workers[i]->lb_value == 0 ) {
                /* That's the 'default' worker, it'll take all requests.
                 * All other workers are not used unless this is in error state.
                 *
                 * The 'break' will disable checking for recovery on other
                 * workers - but that doesn't matter as long as the default is alive.
                 */
                rc=lb->lb_workers[i];
                break;
            }
            if(lb->lb_workers[i]->lb_value < lb_min ||
               ( rc==NULL ) ) {
                lb_min = lb->lb_workers[i]->lb_value;
            rc = lb->lb_workers[i];
            }
        }
    }
        
    if ( rc==NULL ) {
        /* no workers found (rc is null), now try as hard as possible to get a
           worker anyway, pick one with largest error time.. */
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb.getWorker() All workers in error state, use the one with oldest error\n");
        
        for(i = 0 ; i < lb->num_of_workers ; i++) {
/*             if(lb->lb_workers[i]->in_error_state) { */
/*                 if(!lb->lb_workers[i]->in_recovering) { */
                    /* if the retry count is zero, that means the worker only
                       failed once, this is to e that the failed worker will
                       not continue to be retried over and over again.
                    */
/*                     if ( lb->lb_workers[i]->retry_count == 0 ) { */
                        if ( rc != NULL ) {
                            /* pick the oldest failed worker */
                            if ( lb->lb_workers[i]->error_time < rc->error_time ) {
                                rc = lb->lb_workers[i];
                            }
                        } else {
                            rc = lb->lb_workers[i];
                        }
/*                     } */
/*                 } */
/*             } else { */
                 /* This is a good worker - it may have come to life */ 
/*                 if(lb->lb_workers[i]->lb_value < lb_min || rc != NULL) { */
/*                     lb_min = lb->lb_workers[i]->lb_value; */
/*                     rc = lb->lb_workers[i]; */
/*                     break; */
/*                 } */
/*             } */
        }
    
        if ( rc  && rc->in_error_state ) {
/*             if(now==0) */
/*                 now = time(0); */
            rc->in_recovering  = JK_TRUE;
            rc->in_error_state  = JK_FALSE;
/*             rc->error_time     = now; */
/*             rc->retry_count++; */
        }
    }
    
    if(rc) {
        if( rc->lb_value != 0 ) {
            /* It it's the default, it'll remain the default - we don't
               increase the factor
            */
            rc->lb_value += rc->lb_factor;
        }
    }

    return rc;
}

/** Check the scoreboard, make updates in the 'live'
    config
*/
static int JK_METHOD jk2_lb_updateWorkers(jk_env_t *env,
                                          jk_worker_t *lb,
                                          jk_shm_t *shm)
{
    int rc;
    int i;

    if( shm== NULL || shm->head==NULL) return JK_ERR;
    
    JK_ENTER_CS(&lb->cs, rc);
    if(rc !=JK_TRUE) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb.updateWorkers() Can't enter critical section\n");
        return JK_ERR;
    }
    if( lb->ver == shm->head->lbVer ) {
        /* Was updated by some other thread */
        return JK_OK;
    }

    /* Walk the shm and update any changed worker */
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "lb.updateWorkers() Updating workers %d %d\n",
                  lb->ver, shm->head->lbVer);
    for( i=1; i<shm->head->lastSlot; i++ ) {
        jk_shm_slot_t *slot= shm->getSlot( env, shm, i );
        if( strncmp( slot->name, "TOMCAT:", 7 ) == 0 ) {
            /* */
            char *instanceId=slot->name+7;
            char *data=slot->data;
            jk_msg_t *msg;
            int chCnt;

            msg=jk2_msg_ajp_create2( env, env->tmpPool, slot->data, slot->size);
            chCnt=msg->getInt(env, msg );

            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "lb.updateWorkers() Reading %s %d channels \n",
                          slot->name, chCnt );

            if( chCnt == 0 ) {
                /* Remove all channels used by this tomcat instance */
                
            }
            /* Create all channels we don't have */
            /* XXX Not sure what's the best solution, we can do it in many ways */
            
        }
    }

    lb->ver = shm->head->lbVer;
        
    JK_LEAVE_CS(&lb->cs, rc);
    return JK_OK;
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

    /* reset all the retry counts to 0. XXX may be a problem if we have many workers ? */
/*     for(i = 0 ; i < lb->num_of_workers ; i++) { */
/*         lb->lb_workers[i]->retry_count = 0; */
/*     } */

    if( wEnv->shm != NULL && wEnv->shm->head != NULL ) {
        /* We have shm, let's check for updates. This is just checking one
           memory location, no lock involved. It is possible we'll read it
           while somebody else is changing - but that's ok, we just check for
           equality.
        */
        if( lb->ver != wEnv->shm->head->lbVer ) {
            jk2_lb_updateWorkers(env, lb, wEnv->shm);
        }
    }
    
    while(1) {
        jk_worker_t *rec;
        int rc;

        /* Prevent loops */
        if( attempt > lb->num_of_workers + 1 ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "lb.service() max attempts exceeded %d\n", attempt);
            return JK_ERR;
        }
        
        if( lb->num_of_workers==1 ) {
            /* A single worker - no need to search */
            rec=lb->lb_workers[0];
        } else {
            rec=jk2_get_most_suitable_worker(env, lb, s, attempt++);
        }
        
        s->is_recoverable_error = JK_FALSE;

        if(rec == NULL) {
            /* NULL record, no more workers left ... */
            env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                          "lb_worker.service() No suitable workers left \n");
            return JK_ERR;
        }
                
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "lb.service() try %s\n", rec->mbean->name );
        
        s->jvm_route =  rec->route;
 
        rec->reqCnt++;
        
        rc = rec->service(env, rec, s);

        if(rc==JK_OK) {                        
            if(rec->in_recovering) {
                /* Don't change '0' XXX A special flag may avoid those strange tests */
                if( rec->lb_value != 0 ) 
                    rec->lb_value = jk2_get_max_lb(rec) + ADDITINAL_WAIT_LOAD;
            }
            rec->in_error_state = JK_FALSE;
            rec->in_recovering  = JK_FALSE;
            rec->retry_count    = 0;
            rec->error_time     = 0;
            rec->errCnt++;
            /* the endpoint that succeeded is saved for done() */
            s->realWorker = rec;
            return JK_OK;
        }
        
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "lb.service() worker failed\n");
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
    return JK_ERR;
}

/** Init internal structures.
    Called any time the config changes
*/
static int JK_METHOD jk2_lb_refresh(jk_env_t *env, jk_worker_t *lb)
{
    int currentWorker=0;
    int i;
    lb->num_of_workers=lb->lbWorkerMap->size( env, lb->lbWorkerMap);

    if( lb->lb_workers_size < lb->num_of_workers ) {
        if( lb->lb_workers_size==0 ) {
            lb->lb_workers_size=10;
        } else {
            lb->lb_workers_size = 2 * lb->lb_workers_size;
        }
        lb->lb_workers =
            lb->mbean->pool->alloc(env, lb->mbean->pool, 
                                   lb->lb_workers_size * sizeof(jk_worker_t *));
        if(!lb->lb_workers) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "lb_worker.validate(): OutOfMemoryException\n");
            return JK_ERR;
        }
    }    

    for(i = 0 ; i < lb->num_of_workers ; i++) {
        char *name = lb->lbWorkerMap->nameAt( env, lb->lbWorkerMap, i);
        jk_worker_t *w= env->getByName( env, name );
        if( w== NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "lb_worker.init(): no worker found %s\n", name);
           lb->num_of_workers--;
            continue;
        }
        
        if( w->lb_factor != 0 ) {
            w->lb_factor = 1/ w->lb_factor;
            lb->lb_workers[currentWorker]=w;
        } else {
            /* If == 0, then this is the default worker. Switch it with the first
               worker to avoid looking too much for it.
             */
            jk_worker_t *first=lb->lb_workers[0];
            lb->lb_workers[0]=w;
            lb->lb_workers[currentWorker]=first;
        }
        

        /* 
         * Allow using lb in fault-tolerant mode.
         * Just set lbfactor in worker.properties to 0 to have 
         * a worker used only when principal is down or session route
         * point to it. Provided by Paul Frieden <pfrieden@dchain.com>
         */
        w->lb_value = w->lb_factor;
        w->in_error_state = JK_FALSE;
        w->in_recovering  = JK_FALSE;
        w->retry_count  = 0;

        currentWorker++;
    }
    return JK_OK;
}

static int JK_METHOD jk2_lb_addWorker(jk_env_t *env, jk_worker_t *lb, 
                                      char *name)
{
    name = lb->mbean->pool->pstrdup(env, lb->mbean->pool, name);
    lb->lbWorkerMap->add(env, lb->lbWorkerMap, name, "");
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "lb_worker.setAttribute(): Adding %s %s\n", lb->mbean->name, name);

}

static int JK_METHOD jk2_lb_setProperty(jk_env_t *env, jk_bean_t *mbean, 
                                        char *name, void *valueP)
{
    jk_worker_t *lb=mbean->object;
    char *value=valueP;
    int err;
    char **worker_names;
    unsigned num_of_workers;
    unsigned i = 0;
    char *tmp;
    
    if( strcmp( name, "balanced_workers") == 0 ) {
        worker_names=jk2_config_split( env,  lb->mbean->pool,
                                       value, NULL, &num_of_workers );
        if( worker_names==NULL || num_of_workers==0 ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "lb_worker.validate(): no defined workers\n");
            return JK_ERR;
        }
        for(i = 0 ; i < num_of_workers ; i++) {
            jk2_lb_addWorker( env, lb, worker_names[i]);
        }
        jk2_lb_refresh( env, lb );
        return JK_OK;
    } else if( strcmp( name, "worker") == 0 ) {
        jk2_lb_addWorker( env, lb, value);
        jk2_lb_refresh( env, lb );
        return JK_OK;
    }
    return JK_ERR;
}


static int JK_METHOD jk2_lb_init(jk_env_t *env, jk_worker_t *lb)
{
    int err;
    char **worker_names;
    int i = 0;
    char *tmp;

    int num_of_workers=lb->lbWorkerMap->size( env, lb->lbWorkerMap);

    err=jk2_lb_refresh(env, lb );
    if( err != JK_OK )
        return err;

    lb->ver=0;
    if( lb->workerEnv->shm != NULL && lb->workerEnv->shm->head != NULL) 
        jk2_lb_updateWorkers(env, lb, lb->workerEnv->shm);

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "lb.init() %s %d workers\n",
                  lb->mbean->name, lb->num_of_workers );
    
    return JK_OK;
}

static int JK_METHOD jk2_lb_destroy(jk_env_t *env, jk_worker_t *w)
{
    int i = 0;

    if(w==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb_worker.destroy() NullPointerException\n");
        return JK_ERR;
    }

    /* Workers are destroyed by the workerEnv. It is possible
       that a worker is part of more than a lb.
    */
    /*
    for(i = 0 ; i < w->num_of_workers ; i++) {
        w->lb_workers[i]->destroy( env, w->lb_workers[i]);
    }
    */

    return JK_OK;
}


int JK_METHOD jk2_worker_lb_factory(jk_env_t *env,jk_pool_t *pool,
                                    jk_bean_t *result, char *type, char *name)
{
    jk_worker_t *w;
    
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

    w->lb_workers = NULL;
    w->num_of_workers = 0;
    w->worker_private = NULL;
    w->init           = jk2_lb_init;
    w->destroy        = jk2_lb_destroy;
    w->service        = jk2_lb_service;
   
    jk2_map_default_create(env,&w->lbWorkerMap, pool);

    result->setAttribute=jk2_lb_setProperty;
    result->object=w;
    w->mbean=result;

    w->workerEnv=env->getByName( env, "workerEnv" );
    w->workerEnv->addWorker( env, w->workerEnv, w );
    
    return JK_OK;
}

