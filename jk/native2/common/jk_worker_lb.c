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

#if APR_HAS_THREADS
#include "apr_thread_proc.h"
#endif

#define DEFAULT_LB_FACTOR           (1.0)

/* Time to wait before retry... XXX make it configurable*/
#define WAIT_BEFORE_RECOVER 60 

#define MAX_ATTEMPTS 3

#define NO_WORKER_MSG "The servlet container is temporary unavailable or being upgraded\n";

#define STICKY_SESSION 1

typedef struct {
    struct  jk_mutex *cs;
    int     attempts;
    int     recovery;
    int     timeout;
    int     sticky_session; 
    time_t  error_time;

} jk_worker_lb_private_t;

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
    char *session_route = NULL;
    char *routeRedirect=NULL;
    time_t now = 0;
    jk_worker_lb_private_t *lb_priv = lb->worker_private;

    if(lb_priv->sticky_session) {
        session_route = jk2_requtil_getSessionRoute(env, s);
    }

    if( lb->mbean->debug > 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "lb.get_worker %d Session=%s\n", lb_priv->sticky_session, (session_route? session_route: "NULL"));
    }
    if(session_route) {
        for( level=0; level<JK_LB_LEVELS; level++ ) {
            for(i = 0 ; i < lb->workerCnt[level]; i++) {
                jk_worker_t *w=lb->workerTables[level][i];
                
                if(w->route != NULL &&
                   0 == strcmp(session_route, w->route)) {
                    if( w->routeRedirect != NULL ) {
                        /* Session was migrated to another worker */
                        routeRedirect=w->routeRedirect;
                        break;
                    }
                    
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
    /* We have a session - but the worker is in error state
       or has a "redirect".
       Try the new worker.
    */
    if(routeRedirect != NULL) {
        for( level=0; level<JK_LB_LEVELS; level++ ) {
            for(i = 0 ; i < lb->workerCnt[level]; i++) {
                jk_worker_t *w=lb->workerTables[level][i];
                
                if(w->route != NULL &&
                   0 == strcmp(routeRedirect, w->route)) {
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
            if( w->graceful ) continue;
            if( w->in_error_state ) continue;
            if( w->lb_disabled ) continue;

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
            if( w->lb_disabled ) continue;
        
            if(w->in_error_state) {
                /* Check if it's ready for recovery */
                if( now==0 ) now = time(NULL);
                
                if((now - w->error_time) > lb_priv->recovery) {
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
                if( w->graceful ) continue;
                if( w->lb_disabled ) continue;

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
    jk_workerEnv_t *wEnv=lb->workerEnv;
    jk_worker_lb_private_t *lb_priv = lb->worker_private;
    jk_worker_t *rec = NULL;
    
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

    /* Initialize here the recovery POST buffer */
    s->reco_buf = jk2_msg_ajp_create( env, s->pool, 0);
    s->reco_status = RECO_INITED;
    
    while(1) {
        int rc;

        /* Since there is potential worker state change
         * make the find best worker call thread safe 
         */
        if (lb_priv->cs != NULL)
            lb_priv->cs->lock(env, lb_priv->cs);
        rec=jk2_get_most_suitable_worker(env, lb, s, attempt);

        if (lb_priv->cs != NULL)
            lb_priv->cs->unLock(env, lb_priv->cs);
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

            /* Don't touch headers if noErrorHeader set to TRUE, ie ErrorDocument in Apache via mod_alias */
            if (! lb->noErrorHeader) {
                /* XXX: I didn't understand the 302, since s->status is lb->hwBalanceErr or lb->noWorkerCode */
                /* Both could be set in workers2.properties so ..... */ 
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
            }            
            s->afterRequest( env, s);
            lb_priv->error_time = time(NULL);
            return JK_ERR;
        }

        if( lb->mbean->debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
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
            return JK_OK;
        }

        /* If this is a browser connection error dont't check other
         * workers.             
         */
        if (rc == JK_HANDLER_ERROR) {
            rec->in_error_state = JK_FALSE;
            rec->error_time     = 0;
            return JK_HANDLER_ERROR;
        }

        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "lb.service() worker failed %d for %s\n", rc, rec->mbean->name );
        
        /*
         * Service failed !!!
         *
         * Time for fault tolerance (if possible)...
         */
        rec->in_error_state = JK_TRUE;
        rec->error_time     = time(NULL);
        
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
            env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
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
    int num_of_workers=lb->lbWorkerMap->size( env, lb->lbWorkerMap);

    for(i = 0 ; i < num_of_workers ; i++) {
        char *name = lb->lbWorkerMap->nameAt( env, lb->lbWorkerMap, i);
        jk_worker_t *w= env->getByName( env, name );
        int level=0;
        int pos;
        int workerCnt;

        if( w== NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "lb_worker.init(): no worker found %s\n", name);
            continue;
        }

        if( w->mbean->disabled ) continue;
        
        level=w->level;
        
        /* It's like disabled */
        if( level >= JK_LB_LEVELS ) continue;

        /* check if worker is already in the table */
        workerCnt = lb->workerCnt[level];
        for(pos = 0 ; pos < workerCnt ; pos++) {
            if( lb->workerTables[level][pos] == w ) {
                break;
            }
        }

        if( pos == workerCnt ) {
            if( pos == JK_LB_MAX_WORKERS ) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "lb_worker.init(): maximum lb workers reached %s\n", name);
                continue;
            }
            pos=lb->workerCnt[level]++;
        
            lb->workerTables[level][pos]=w;

            w->lb_value = w->lb_factor;
            w->in_error_state = JK_FALSE;
        }
    }
    
    return JK_OK;
}


static char *jk2_worker_lb_multiValueInfo[]={"worker", NULL };
static char *jk2_worker_lb_setAttributeInfo[]={"attempts", "stickySession", "recovery", "timeout",
                                               "hwBalanceErr", "noErrorHeader", "noWorkerMsg", "noWorkerCode", "worker", NULL };

static char *jk2_worker_lb_getAttributeInfo[]={"attempts", "stickySession", "recovery", "timeout",
                                               "hwBalanceErr", "noErrorHeader", "noWorkerMsg", "noWorkerCode", "workers", NULL };


static void * JK_METHOD jk2_lb_getAttribute(jk_env_t *env, jk_bean_t *mbean, 
                                            char *name)
{
    jk_worker_t *lb=mbean->object;
    unsigned i = 0;
    jk_worker_lb_private_t *lb_priv = lb->worker_private;
    
    if( strcmp( name, "workers") == 0 ) {
        return jk2_map_concatKeys( env, lb->lbWorkerMap, ":" );
    } else if( strcmp( name, "noWorkerMsg") == 0 ) {
        return lb->noWorkerMsg;
    } else if( strcmp( name, "noWorkerCode") == 0 ) {
        return jk2_env_itoa( env, lb->noWorkerCode);
    } else if( strcmp( name, "hwBalanceErr") == 0 ) {
        return jk2_env_itoa( env, lb->hwBalanceErr);
    } else if( strcmp( name, "noErrorHeader") == 0 ) {
        return jk2_env_itoa( env, lb->noErrorHeader);
    } else if( strcmp( name, "timeout") == 0 ) {
        return jk2_env_itoa( env, lb_priv->timeout);
    } else if( strcmp( name, "recovery") == 0 ) {
        return jk2_env_itoa( env, lb_priv->recovery);
    } else if( strcmp( name, "stickySession") == 0 ) {
        return jk2_env_itoa( env, lb_priv->sticky_session);
    } else if( strcmp( name, "attempts") == 0 ) {
        return jk2_env_itoa( env, lb_priv->attempts);
    }
    return NULL;
}

static int JK_METHOD jk2_lb_setAttribute(jk_env_t *env, jk_bean_t *mbean, 
                                         char *name, void *valueP)
{
    jk_worker_t *lb=mbean->object;
    char *value=valueP;
    unsigned i = 0;
    jk_worker_lb_private_t *lb_priv = lb->worker_private;
    
    if( strcmp( name, "worker") == 0 ) {
        if( lb->lbWorkerMap->get( env, lb->lbWorkerMap, value) != NULL ) {
            /* Already added */
            return JK_OK;
        }
        value = lb->mbean->pool->pstrdup(env, lb->mbean->pool, value);
        lb->lbWorkerMap->add(env, lb->lbWorkerMap, value, "");

        if( lb->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "lb_worker.setAttribute(): Adding to %s: %s\n", lb->mbean->localName, value);

        jk2_lb_refresh( env, lb );
        return JK_OK;
    } else if( strcmp( name, "noWorkerMsg") == 0 ) {
        lb->noWorkerMsg=value;
    } else if( strcmp( name, "noWorkerCode") == 0 ) {
        lb->noWorkerCode=atoi( value );
    } else if( strcmp( name, "hwBalanceErr") == 0 ) {
        lb->hwBalanceErr=atoi( value );
    } else if( strcmp( name, "noErrorHeader") == 0 ) {
        lb->noErrorHeader=atoi( value );
    } else if( strcmp( name, "timeout") == 0 ) {
        lb_priv->timeout=atoi( value );
    } else if( strcmp( name, "recovery") == 0 ) {
        lb_priv->recovery=atoi( value );
    } else if( strcmp( name, "stickySession") == 0 ) {
        lb_priv->sticky_session=atoi( value );
    } else if( strcmp( name, "attempts") == 0 ) {
        lb_priv->attempts=atoi( value );
    }
    return JK_ERR;
}


static int JK_METHOD jk2_lb_init(jk_env_t *env, jk_bean_t *bean)
{
    jk_worker_t *lb=bean->object;
    int err;
    int i = 0;
    int num_of_workers=lb->lbWorkerMap->size( env, lb->lbWorkerMap);

    err=jk2_lb_refresh(env, lb );
    if( err != JK_OK )
        return err;

    /*     if( lb->workerEnv->shm != NULL && lb->workerEnv->shm->head != NULL)  */
    /*         jk2_lb_updateWorkers(env, lb, lb->workerEnv->shm); */
    if( lb->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "lb.init() %s %d workers\n",
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
    jk_bean_t *jkb;
    jk_worker_lb_private_t *worker_private;

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
    
    jkb=env->createBean2(env, pool,"threadMutex", NULL);
    if( jkb != NULL ) {
        w->cs=jkb->object;
        jkb->init(env, jkb );
    }

    worker_private = (jk_worker_lb_private_t *)pool->calloc(env,
                          pool, sizeof(jk_worker_lb_private_t));
    
    jkb=env->createBean2(env, pool,"threadMutex", NULL);
    if( jkb != NULL ) {
        worker_private->cs=jkb->object;
        jkb->init(env, jkb );
    }
    worker_private->attempts = MAX_ATTEMPTS;
    worker_private->recovery = WAIT_BEFORE_RECOVER;
    worker_private->sticky_session = STICKY_SESSION;
    w->worker_private = worker_private;
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
    /* Let Apache handle the error via ErrorDocument and mod_alias */
    w->noErrorHeader=1;
    w->workerEnv=env->getByName( env, "workerEnv" );
    w->workerEnv->addWorker( env, w->workerEnv, w );

    return JK_OK;
}
