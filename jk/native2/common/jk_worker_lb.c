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

#define MAX_ATTEMPTS 3

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
        
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "lb.getWorker() All workers in error state, use the one with oldest error\n");
        
        for( level=0; level<JK_LB_LEVELS; level++ ) {
            for(i = 0 ; i < lb->workerCnt[level] ; i++) {
                jk_worker_t *w=lb->workerTables[level][i];

                if( w->mbean->disabled == JK_TRUE ) continue;

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

/* Remove all channels used by this tomcat instance */
static int jk2_worker_lb_disableInstance( jk_env_t *env,
                                          jk_worker_t *lb,
                                          char *instanceId )
{
    int i;
    int level;
    
    for( level=0; level<JK_LB_LEVELS; level++ ) {
        for(i = 0 ; i < lb->workerCnt[level] ; i++) {
            jk_worker_t *w=lb->workerTables[level][i];

            if( w->route != NULL &&
                strcmp( w->route, instanceId ) == 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                              "lb.updateWorkers() Gracefull shutdown %s %s\n",
                              w->channel->mbean->name, instanceId );
                w->in_error_state= JK_TRUE;
                w->mbean->disabled = JK_TRUE;
            }
        }
    }
    return JK_OK;
}

static int jk2_worker_lb_registerChannel( jk_env_t *env,
                                          jk_worker_t *lb,
                                          char *instanceId,
                                          jk_msg_t *msg, jk_map_t *groups)
{
    char *chName;
    jk_map_t *chProp;
    int i;
    int found=JK_FALSE;
    jk_config_t *config;
    char *tmpBuf;
    jk_bean_t *chBean;
    int rc=JK_OK;
    int level;

    jk2_map_default_create(env, &chProp, env->tmpPool);

    chName=msg->getString( env, msg );
    if( chName==NULL ) 
        rc=JK_ERR;
    
    if( rc==JK_OK )
        rc=msg->getMap( env, msg, chProp );
    
    if( rc!=JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb.updateWorkers() can't read channel data %s %s\n",
                      chName, instanceId);
        return JK_ERR;
    }

    for( level=0; level<JK_LB_LEVELS; level++ ) {
        for(i = 0 ; i < lb->workerCnt[level] ; i++) {
            jk_worker_t *w=lb->workerTables[level][i];

            if( w->route &&
                strcmp( w->route, instanceId ) == 0 &&
                strcmp( w->channel->mbean->name, chName ) == 0 ) {
                /* XXX Create a new channel with the update properties,
                   Then replace it.
                   
                   At this moment we just re-enable the worker.
                */
                if( w->mbean->disabled || w->in_error_state ) {
                    env->l->jkLog(env, env->l, JK_LOG_INFO,
                                  "lb.updateWorkers() re-enabling %s %s\n",
                                  w->channel->mbean->name, instanceId );
                    w->mbean->disabled=JK_FALSE;
                    w->in_error_state=JK_FALSE;
                }
                
                found=JK_TRUE;
                break;
            }
        }
    }

    if( found==JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "lb.updateWorkers() already found %s %s\n",
                      chName, instanceId);
        return JK_OK;
    }

    config=lb->workerEnv->config;

    tmpBuf=(char *)env->tmpPool->calloc( env, env->tmpPool, strlen( chName ) + 10 );
    strcpy( tmpBuf, chName );
    strcat( tmpBuf, ".name" );

    config->setPropertyString( env, config, tmpBuf, chName );
    chBean=env->getBean( env, chName );
    if( chBean==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "lb.updateWorkers() can't create  %s\n",
                      chName );
        return JK_ERR;
    }
        
    for( i=0; i< chProp->size(env, chProp ); i++ ) {
        char *name=chProp->nameAt( env, chProp, i );
        char *value=chProp->valueAt( env, chProp, i );

        config->setProperty( env, config, chBean, name, value );
    }

    config->save( env, config, NULL );

    env->l->jkLog(env, env->l, JK_LOG_ERROR,
                  "lb.updateWorkers() create  %s %s\n",
                  chName, instanceId );
    
    /* XXX Add it to the right groups */
    
    return JK_OK;
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
    int j;
    jk_map_t *groups;

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

            jk2_map_default_create(env, &groups, env->tmpPool);

            msg=jk2_msg_ajp_create2( env, env->tmpPool, slot->data, slot->size);
            msg->checkHeader( env, msg , NULL);

            msg->getByte(env, msg );
            msg->getString(env, msg );

            msg->getMap( env, msg, groups );

            /* The actual data */
            chCnt=msg->getInt(env, msg );

            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "lb.updateWorkers() Reading %s %d channels %d groups %p %p %p\n",
                          slot->name, chCnt, groups->size( env, groups ), slot->data, slot, shm->head);
            
            if( chCnt == 0 ) {
                jk2_worker_lb_disableInstance( env, lb, instanceId );
            } else {
                /* Create all channels we don't have */
                /* XXX Not sure what's the best solution, we can do it in many ways */
                for( j=0; j< chCnt; j++ ) {
                    jk2_worker_lb_registerChannel( env, lb, instanceId, msg, groups );
                }
            }
            
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


    /* Check for configuration updates
     */
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
        if( attempt > MAX_ATTEMPTS ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "lb.service() max attempts exceeded %d\n", attempt);
            return JK_ERR;
        }
        
        rec=jk2_get_most_suitable_worker(env, lb, s, attempt);
        attempt++;
        
        s->is_recoverable_error = JK_FALSE;

        if(rec == NULL) {
            /* NULL record, no more workers left ... */
            env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                          "lb_worker.service() all workers in error or disabled state\n");
            return JK_ERR;
        }

        if( lb->mbean->debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "lb.service() try %s\n", rec->mbean->name );
        
        if( rec->route==NULL ) {
            rec->route=rec->mbean->localName;
        }
        
        s->jvm_route = rec->route;

        /* It may be better to do this on the endpoint */
        rec->reqCnt++;

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
        rec->errCnt++;
        
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

static int JK_METHOD jk2_lb_addWorker(jk_env_t *env, jk_worker_t *lb, 
                                      char *name)
{
    name = lb->mbean->pool->pstrdup(env, lb->mbean->pool, name);
    lb->lbWorkerMap->add(env, lb->lbWorkerMap, name, "");
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "lb_worker.setAttribute(): Adding to %s: %s\n", lb->mbean->localName, name);

}

static char *jk2_worker_lb_multiValueInfo[]={"worker", NULL };
static char *jk2_worker_lb_setAttributeInfo[]={"debug", NULL };

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
                  lb->mbean->name, num_of_workers );
    
    return JK_OK;
}

static int JK_METHOD jk2_lb_destroy(jk_env_t *env, jk_worker_t *w)
{
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
    w->init           = jk2_lb_init;
    w->destroy        = jk2_lb_destroy;
    w->service        = jk2_lb_service;
    
    for( i=0; i<JK_LB_LEVELS; i++ ) {
        w->workerCnt[i]=0;
    }
    
    jk2_map_default_create(env,&w->lbWorkerMap, pool);

    result->setAttribute=jk2_lb_setAttribute;
    result->multiValueInfo=jk2_worker_lb_multiValueInfo;
    result->setAttributeInfo=jk2_worker_lb_setAttributeInfo;
    result->object=w;
    w->mbean=result;

    w->workerEnv=env->getByName( env, "workerEnv" );
    w->workerEnv->addWorker( env, w->workerEnv, w );
    
    return JK_OK;
}

