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
 * Author:      Mladen Turk <mturk@apache.org>                             *
 * Based on:                                                               *
 * Version:     $Revision$                                          *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_service.h"
#include "jk_util.h"
#include "jk_worker.h"
#include "jk_lb_worker.h"
#include "jk_mt.h"
#include "jk_shm.h"

/*
 * The load balancing code in this 
 */


/* 
 * Time to wait before retry...
 */
#define WAIT_BEFORE_RECOVER (60*1)
#define WORKER_RECOVER_TIME ("recover_time")

struct lb_endpoint
{
    jk_endpoint_t *e;
    lb_worker_t *worker;

    jk_endpoint_t endpoint;
};
typedef struct lb_endpoint lb_endpoint_t;


/* ========================================================================= */
/* Retrieve the parameter with the given name                                */
static char *get_path_param(jk_ws_service_t *s, const char *name)
{
    char *id_start = NULL;
    for (id_start = strstr(s->req_uri, name);
         id_start; id_start = strstr(id_start + 1, name)) {
        if ('=' == id_start[strlen(name)]) {
            /*
             * Session path-cookie was found, get it's value
             */
            id_start += (1 + strlen(name));
            if (strlen(id_start)) {
                char *id_end;
                id_start = jk_pool_strdup(s->pool, id_start);
                /* 
                 * The query string is not part of req_uri, however
                 * to be on the safe side lets remove the trailing query 
                 * string if appended...
                 */
                if ((id_end = strchr(id_start, '?')) != NULL) {
                    *id_end = '\0';
                }
                /*
                 * Remove any trailing path element.
                 */
                if ((id_end = strchr(id_start, ';')) != NULL) {
                    *id_end = '\0';
                }
                return id_start;
            }
        }
    }

    return NULL;
}

/* ========================================================================= */
/* Retrieve the cookie with the given name                                   */
static char *get_cookie(jk_ws_service_t *s, const char *name)
{
    unsigned i;
    char *result = NULL;

    for (i = 0; i < s->num_headers; i++) {
        if (0 == strcasecmp(s->headers_names[i], "cookie")) {

            char *id_start;
            for (id_start = strstr(s->headers_values[i], name);
                 id_start; id_start = strstr(id_start + 1, name)) {
                if (id_start == s->headers_values[i] ||
                    id_start[-1] == ';' ||
                    id_start[-1] == ',' || isspace(id_start[-1])) {
                    id_start += strlen(name);
                    while (*id_start && isspace(*id_start))
                        ++id_start;
                    if (*id_start == '=' && id_start[1]) {
                        /*
                         * Session cookie was found, get it's value
                         */
                        char *id_end;
                        ++id_start;
                        id_start = jk_pool_strdup(s->pool, id_start);
                        if ((id_end = strchr(id_start, ';')) != NULL) {
                            *id_end = '\0';
                        }
                        if ((id_end = strchr(id_start, ',')) != NULL) {
                            *id_end = '\0';
                        }
                        if (result == NULL) {
                            result = id_start;
                        }
                        else {
                            int osz = strlen(result) + 1;
                            int sz = osz + strlen(id_start) + 1;
                            result =
                                jk_pool_realloc(s->pool, sz, result, osz);
                            strcat(result, ";");
                            strcat(result, id_start);
                        }
                    }
                }
            }
        }
    }

    return result;
}


/* ========================================================================= */
/* Retrieve session id from the cookie or the parameter                      */
/* (parameter first)                                                         */
static char *get_sessionid(jk_ws_service_t *s)
{
    char *val;
    val = get_path_param(s, JK_PATH_SESSION_IDENTIFIER);
    if (!val) {
        val = get_cookie(s, JK_SESSION_IDENTIFIER);
    }
    return val;
}

static void close_workers(lb_worker_t * p, int num_of_workers, jk_logger_t *l)
{
    int i = 0;
    for (i = 0; i < num_of_workers; i++) {
        p->lb_workers[i].w->destroy(&(p->lb_workers[i].w), l);
    }
}

static void retry_worker(worker_record_t *w,
                         int recover_wait_time,
                         jk_logger_t *l)
{
    int elapsed = (int)(time(0) - w->s->error_time);
    JK_TRACE_ENTER(l);

    if (elapsed <= recover_wait_time) {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                    "worker %s will recover in %d seconds",
                    w->s->name, recover_wait_time - elapsed);
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                    "worker %s is marked for recover",
                    w->s->name);
        w->s->in_recovering  = JK_TRUE;
        w->s->in_error_state = JK_FALSE;
    }

    JK_TRACE_EXIT(l);
}

static worker_record_t *find_by_session(lb_worker_t *p, 
                                        const char *name,
                                        jk_logger_t *l)
{

    worker_record_t *rc = NULL;
    unsigned int i;
    
    for (i = 0; i < p->num_of_workers; i++) {
        if (strcmp(p->lb_workers[i].s->name, name) == 0) {
            rc = &p->lb_workers[i];
            break;
        }
    }
    return rc;
}

static worker_record_t *find_best_bydomain(lb_worker_t *p,
                                           const char *domain,
                                           jk_logger_t *l)
{
    unsigned int i;
    int total_factor = 0;
    size_t mytraffic = 0;
    size_t curmin = 0;

    worker_record_t *candidate = NULL;
    
    /* First try to see if we have available candidate */
    for (i = 0; i < p->num_of_workers; i++) {
        /* Skip all workers that are not member of domain */
        if (strlen(p->lb_workers[i].s->domain) == 0 ||
            strcmp(p->lb_workers[i].s->domain, domain))
            continue;
        /* Take into calculation only the workers that are
         * not in error state or not disabled.
         */
        if (!p->lb_workers[i].s->in_error_state &&
            !p->lb_workers[i].s->is_disabled) {
            if (p->lbmethod == JK_LB_BYREQUESTS) {
                p->lb_workers[i].s->lb_value += p->lb_workers[i].s->lb_factor;
                total_factor += p->lb_workers[i].s->lb_factor;
                if (!candidate || p->lb_workers[i].s->lb_value > candidate->s->lb_value)
                    candidate = &p->lb_workers[i];
            }
            else {
                mytraffic = (p->lb_workers[i].s->transferred/p->lb_workers[i].s->lb_factor) +
                            (p->lb_workers[i].s->readed/p->lb_workers[i].s->lb_factor);
                if (!candidate || mytraffic < curmin) {
                    candidate = &p->lb_workers[i];
                    curmin = mytraffic;
                }
            }
        }
    }

    if (candidate) {
        if (p->lbmethod == JK_LB_BYREQUESTS)
            candidate->s->lb_value -= total_factor;
        candidate->r = &(candidate->s->domain[0]);
    }

    return candidate;
}


static worker_record_t *find_best_byrequests(lb_worker_t *p,
                                             jk_logger_t *l)
{
    unsigned int i;
    int total_factor = 0;
    worker_record_t *candidate = NULL;
    
    /* First try to see if we have available candidate */
    for (i = 0; i < p->num_of_workers; i++) {
        /* If the worker is in error state run
         * retry on that worker. It will be marked as
         * operational if the retry timeout is elapsed.
         * The worker might still be unusable, but we try
         * anyway.
         */
        if (p->lb_workers[i].s->in_error_state &&
            !p->lb_workers[i].s->is_disabled) {
            retry_worker(&p->lb_workers[i], p->s->recover_wait_time, l);
        }
        /* Take into calculation only the workers that are
         * not in error state or not disabled.
         */
        if (!p->lb_workers[i].s->in_error_state &&
            !p->lb_workers[i].s->is_disabled) {
            p->lb_workers[i].s->lb_value += p->lb_workers[i].s->lb_factor;
            total_factor += p->lb_workers[i].s->lb_factor;
            if (!candidate || p->lb_workers[i].s->lb_value > candidate->s->lb_value)
                candidate = &p->lb_workers[i];
        }
    }

    if (candidate) {
        candidate->s->lb_value -= total_factor;
        candidate->r = &(candidate->s->name[0]);
    }

    return candidate;
}

static worker_record_t *find_best_bytraffic(lb_worker_t *p, 
                                             jk_logger_t *l)
{
    unsigned int i;
    size_t mytraffic = 0;
    size_t curmin = 0;
    worker_record_t *candidate = NULL;
    
    /* First try to see if we have available candidate */
    for (i = 0; i < p->num_of_workers; i++) {
        /* If the worker is in error state run
         * retry on that worker. It will be marked as
         * operational if the retry timeout is elapsed.
         * The worker might still be unusable, but we try
         * anyway.
         */
        if (p->lb_workers[i].s->in_error_state &&
            !p->lb_workers[i].s->is_disabled) {
            retry_worker(&p->lb_workers[i], p->s->recover_wait_time, l);
        }
        /* Take into calculation only the workers that are
         * not in error state or not disabled.
         */
        if (!p->lb_workers[i].s->in_error_state &&
            !p->lb_workers[i].s->is_disabled) {
            mytraffic = (p->lb_workers[i].s->transferred/p->lb_workers[i].s->lb_factor) +
                        (p->lb_workers[i].s->readed/p->lb_workers[i].s->lb_factor);
            if (!candidate || mytraffic < curmin) {
                candidate = &p->lb_workers[i];
                curmin = mytraffic;
            }
        }
    }
    
    return candidate;
}

static worker_record_t *find_best_worker(lb_worker_t * p,
                                         jk_logger_t *l)
{
    worker_record_t *rc = NULL;

    if (p->lbmethod == JK_LB_BYREQUESTS)
        rc = find_best_byrequests(p, l);
    else if (p->lbmethod == JK_LB_BYTRAFFIC)
        rc = find_best_bytraffic(p, l);
    /* By default use worker name as session route */
    if (rc)
        rc->r = &(rc->s->name[0]);

    return rc;
}

static worker_record_t *find_session_route(lb_worker_t *p, 
                                           const char *name,
                                           jk_logger_t *l)
{
    unsigned int i;
    int total_factor = 0;
    int uses_domain  = 0;
    worker_record_t *candidate = NULL;

    candidate = find_by_session(p, name, l);
    if (!candidate) {
        uses_domain = 1;
        candidate = find_best_bydomain(p, name, l);
    }
    if (candidate) {
        if (candidate->s->in_error_state && !candidate->s->is_disabled) {
            retry_worker(candidate, p->s->recover_wait_time, l);
        }
        if (candidate->s->in_error_state) {
            /* We have a worker that is unusable.
             * It can be in error or disabled, but in case
             * it has a redirection set use that redirection worker.
             * This enables to safely remove the member from the
             * balancer. Of course you will need a some kind of
             * session replication between those two remote.
             */
            if (*candidate->s->redirect)
                candidate = find_by_session(p, candidate->s->redirect, l);
            else if (*candidate->s->domain && !uses_domain) {
                uses_domain = 1;
                candidate = find_best_bydomain(p, candidate->s->domain, l);
            }
            if (candidate && candidate->s->in_error_state)
                candidate = NULL;
        }
    }
    if (candidate && !uses_domain) {
        for (i = 0; i < p->num_of_workers; i++) {
            if (!p->lb_workers[i].s->in_error_state &&
                !p->lb_workers[i].s->is_disabled) {
                /* Skip all workers that are not member of candidate domain */
                if (*candidate->s->domain &&
                    strcmp(p->lb_workers[i].s->domain, candidate->s->domain))
                    continue;
                p->lb_workers[i].s->lb_value += p->lb_workers[i].s->lb_factor;
                total_factor += p->lb_workers[i].s->lb_factor;
            }
        }
        candidate->s->lb_value -= total_factor;
    }
    return candidate;
}

static worker_record_t *get_most_suitable_worker(lb_worker_t * p,
                                                 jk_ws_service_t *s,
                                                 int attempt,
                                                 jk_logger_t *l)
{
    worker_record_t *rc = NULL;
    char *sessionid = NULL;
    int r;

    JK_TRACE_ENTER(l);
    if (p->num_of_workers == 1) {
        /* No need to find the best worker
         * if there is a single one
         */
        if (p->lb_workers[0].s->in_error_state &&
            !p->lb_workers[0].s->is_disabled) {
            retry_worker(&p->lb_workers[0], p->s->recover_wait_time, l);
        }
        if (!p->lb_workers[0].s->in_error_state) {
            p->lb_workers[0].r = &(p->lb_workers[0].s->name[0]);
            JK_TRACE_EXIT(l);
            return &p->lb_workers[0];
        }
        else {
            JK_TRACE_EXIT(l);
            return NULL;
        }
    }
    else if (p->s->sticky_session) {
        sessionid = get_sessionid(s);
    }
    JK_ENTER_CS(&(p->cs), r);
    if (!r) {
       jk_log(l, JK_LOG_ERROR,
              "locking thread with errno=%d",
              errno);
        JK_TRACE_EXIT(l);
        return NULL;
    }
    if (sessionid) {
        char *session = sessionid;
        if (JK_IS_DEBUG_LEVEL(l)) {
            jk_log(l, JK_LOG_DEBUG,
                   "total sessionid is %s",
                    sessionid ? sessionid : "empty");
        }
        while (sessionid) {
            char *next = strchr(sessionid, ';');
            char *session_route = NULL;
            if (next)
               *next++ = '\0';
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "searching worker for partial sessionid %s",
                       sessionid);
            session_route = strchr(sessionid, '.');
            if (session_route) {
                ++session_route;

                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "searching worker for session route %s",
                            session_route);

                /* We have a session route. Whow! */
                rc = find_session_route(p, session_route, l);
                if (rc) {
                    JK_LEAVE_CS(&(p->cs), r);
                    if (JK_IS_DEBUG_LEVEL(l))
                        jk_log(l, JK_LOG_DEBUG,
                               "found worker %s for partial sessionid %s",
                               rc->s->name, sessionid);
                        JK_TRACE_EXIT(l);
                    return rc;
                }
            }
            sessionid = next;
        }
        if (!rc && p->s->sticky_session_force) {
           JK_LEAVE_CS(&(p->cs), r);
            jk_log(l, JK_LOG_INFO,
                   "all workers are in error state for session %s",
                    session);
            JK_TRACE_EXIT(l);
            return NULL;
        }
    }
    rc = find_best_worker(p, l);
    JK_LEAVE_CS(&(p->cs), r);
    if (rc && JK_IS_DEBUG_LEVEL(l)) {
        jk_log(l, JK_LOG_DEBUG,
               "found best worker (%s) using %s method", rc->s->name,
               p->lbmethod == JK_LB_BYREQUESTS ? "by request" : "by traffic");
    }
    JK_TRACE_EXIT(l);
    return rc;
}

static int JK_METHOD service(jk_endpoint_t *e,
                             jk_ws_service_t *s,
                             jk_logger_t *l, int *is_error)
{
    JK_TRACE_ENTER(l);

    if (e && e->endpoint_private && s && is_error) {
        lb_endpoint_t *p = e->endpoint_private;
        int attempt = 0;
        int num_of_workers = p->worker->num_of_workers;
        /* Set returned error to OK */
        *is_error = JK_HTTP_OK;

        /* set the recovery post, for LB mode */
        s->reco_buf = jk_b_new(s->pool);
        jk_b_set_buffer_size(s->reco_buf, DEF_BUFFER_SZ);
        jk_b_reset(s->reco_buf);
        s->reco_status = RECO_INITED;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "service sticky_session=%d",
                   p->worker->s->sticky_session);

        while (num_of_workers) {
            worker_record_t *rec =
                get_most_suitable_worker(p->worker, s, attempt++, l);
            int rc;

            if (rec) {
                int is_service_error = JK_HTTP_OK;
                int service_ok = JK_FALSE;
                jk_endpoint_t *end = NULL;
                
                /* XXX: No need to strdup here ? */
#if 0
                s->jvm_route = jk_pool_strdup(s->pool, rec->r);
#else
                s->jvm_route = rec->r;
#endif
                rc = rec->w->get_endpoint(rec->w, &end, l);

                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "service worker=%s jvm_route=%s rc=%d",
                           rec->s->name, s->jvm_route, rc);
                rec->s->elected++;
                if (rc && end) {
                    /* Reset endpoint read and write sizes for
                     * this request.
                     */
                    end->rd = end->wr = 0;
                    /* Increment the number of workers serving request */
                    p->worker->s->busy++;
                    rec->s->busy++;
                    service_ok = end->service(end, s, l, &is_service_error);
                    /* Update partial reads and writes if any */
                    rec->s->readed += end->rd;
                    rec->s->transferred += end->wr;
                    end->done(&end, l);
                    /* Decrement the busy worker count */
                    rec->s->busy--;
                    p->worker->s->busy--;
                    if (service_ok) {
                        rec->s->in_error_state = JK_FALSE;
                        rec->s->in_recovering = JK_FALSE;
                        rec->s->error_time = 0;
                        JK_TRACE_EXIT(l);
                        return JK_TRUE;
                    }
                }
                if (!service_ok) {
                    /*
                    * Service failed !!!
                    *
                    * Time for fault tolerance (if possible)...
                    */

                    rec->s->errors++;
                    rec->s->in_error_state = JK_TRUE;
                    rec->s->in_recovering = JK_FALSE;
                    rec->s->error_time = time(0);

                    if (is_service_error > JK_HTTP_OK) {
                        /*
                        * Error is not recoverable - break with an error.
                        */
                        jk_log(l, JK_LOG_ERROR,
                            "unrecoverable error %d, request failed."
                            " Tomcat failed in the middle of request,"
                            " we can't recover to another instance.",
                            is_service_error);
                        *is_error = is_service_error;
                        JK_TRACE_EXIT(l);
                        return JK_FALSE;
                    }
                    jk_log(l, JK_LOG_INFO,
                           "service failed, worker %s is in error state",
                           rec->s->name);
                }
                else {
                    /* If we can not get the endpoint from the worker
                     * that does not mean that the worker is in error
                     * state. It means that the worker is busy.
                     * We will try another worker.
                     * To prevent infinite loop decrement worker count;
                     */
                    --num_of_workers;
                }
                /* 
                 * Error is recoverable by submitting the request to
                 * another worker... Lets try to do that.
                 */
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "recoverable error... will try to recover on other host");
            }
            else {
                /* NULL record, no more workers left ... */
                jk_log(l, JK_LOG_ERROR,
                       "All tomcat instances failed, no more workers left");
                JK_TRACE_EXIT(l);
                *is_error = JK_HTTP_SERVER_ERROR;
                return JK_FALSE;
            }
        }
        jk_log(l, JK_LOG_INFO,
               "All tomcat instances are busy or in error state");
        JK_TRACE_EXIT(l);
        /* Set error to Server busy */
        *is_error = JK_HTTP_SERVER_BUSY;
        return JK_FALSE;
    }
    if (is_error)
        *is_error = JK_HTTP_SERVER_ERROR;
    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD done(jk_endpoint_t **e, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (e && *e && (*e)->endpoint_private) {
        lb_endpoint_t *p = (*e)->endpoint_private;

        if (p->e) {
            p->e->done(&p->e, l);
        }

        free(p);
        *e = NULL;
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t *props,
                              jk_worker_env_t *we, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (pThis && pThis->worker_private) {
        lb_worker_t *p = pThis->worker_private;
        char **worker_names;
        unsigned int num_of_workers;

        p->s->sticky_session = jk_get_is_sticky_session(props, p->s->name);
        p->s->sticky_session_force = jk_get_is_sticky_session_force(props, p->s->name);

        if (jk_get_lb_worker_list(props,
                                  p->s->name,
                                  &worker_names,
                                  &num_of_workers) && num_of_workers) {
            unsigned int i = 0;

            p->lb_workers = jk_pool_alloc(&p->p,
                                          num_of_workers *
                                          sizeof(worker_record_t));
            if (!p->lb_workers) {
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }

            for (i = 0; i < num_of_workers; i++) {
                p->lb_workers[i].s = jk_shm_alloc_worker(&p->p);
                if (p->lb_workers[i].s == NULL) {
                    jk_log(l, JK_LOG_ERROR, 
                           "allocating worker record from shared memory");
                    JK_TRACE_EXIT(l);
                    return JK_FALSE;
                }
            }
            for (i = 0; i < num_of_workers; i++) {
                const char *s;
                strncpy(p->lb_workers[i].s->name, worker_names[i],
                        JK_SHM_STR_SIZ);
                p->lb_workers[i].s->lb_factor =
                    jk_get_lb_factor(props, worker_names[i]);
                if (p->lb_workers[i].s->lb_factor < 1) {
                    p->lb_workers[i].s->lb_factor = 1;
                }
                if ((s = jk_get_worker_domain(props, worker_names[i], NULL)))
                    strncpy(p->lb_workers[i].s->domain, s, JK_SHM_STR_SIZ);
                if ((s = jk_get_worker_redirect(props, worker_names[i], NULL)))
                    strncpy(p->lb_workers[i].s->redirect, s, JK_SHM_STR_SIZ);

                p->lb_workers[i].s->lb_value = p->lb_workers[i].s->lb_factor;
                p->lb_workers[i].s->in_error_state = JK_FALSE;
                p->lb_workers[i].s->in_recovering = JK_FALSE;
                p->lb_workers[i].s->error_time = 0;
                /* Worker can be initaly disabled as hot standby */
                p->lb_workers[i].s->is_disabled = jk_get_is_worker_disabled(props, worker_names[i]);
                if (!wc_create_worker(p->lb_workers[i].s->name,
                                      props,
                                      &(p->lb_workers[i].w),
                                      we, l) || !p->lb_workers[i].w) {
                    break;
                }
            }

            if (i != num_of_workers) {
                close_workers(p, i, l);
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Failed to create worker %s",
                           p->lb_workers[i].s->name);

            }
            else {
                if (JK_IS_DEBUG_LEVEL(l)) {
                    for (i = 0; i < num_of_workers; i++) {
                        jk_log(l, JK_LOG_DEBUG,
                               "Balanced worker %i has name %s in domain %s",
                               i, p->lb_workers[i].s->name, p->lb_workers[i].s->domain);
                    }
                }
                p->num_of_workers = num_of_workers;
                JK_TRACE_EXIT(l);
                return JK_TRUE;
            }
        }
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t *props,
                          jk_worker_env_t *we, jk_logger_t *log)
{
    int i;

    lb_worker_t *p = (lb_worker_t *)pThis->worker_private;
    JK_TRACE_ENTER(log);

    pThis->retries = jk_get_worker_retries(props, p->s->name,
                                           JK_RETRIES);
    p->s->retries = pThis->retries;
    if (jk_get_worker_int_prop(props, p->s->name,
                               WORKER_RECOVER_TIME,
                               &i))
        p->s->recover_wait_time = i;
    if (p->s->recover_wait_time < WAIT_BEFORE_RECOVER)
        p->s->recover_wait_time = WAIT_BEFORE_RECOVER;

    JK_INIT_CS(&(p->cs), i);
    if (i == JK_FALSE) {
        jk_log(log, JK_LOG_ERROR,
               "creating thread lock errno=%d",
               errno);
        JK_TRACE_EXIT(log);
        return JK_FALSE;
    }

    JK_TRACE_EXIT(log);
    return JK_TRUE;
}

static int JK_METHOD get_endpoint(jk_worker_t *pThis,
                                  jk_endpoint_t **pend, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (pThis && pThis->worker_private && pend) {
        lb_endpoint_t *p = (lb_endpoint_t *) malloc(sizeof(lb_endpoint_t));
        p->e = NULL;
        p->worker = pThis->worker_private;
        p->endpoint.endpoint_private = p;
        p->endpoint.service = service;
        p->endpoint.done = done;
        *pend = &p->endpoint;

        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }
    else {
        JK_LOG_NULL_PARAMS(l);
    }

    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD destroy(jk_worker_t **pThis, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (pThis && *pThis && (*pThis)->worker_private) {
        unsigned int i;
        lb_worker_t *private_data = (*pThis)->worker_private;

        close_workers(private_data, private_data->num_of_workers, l);
        JK_DELETE_CS(&(private_data->cs), i);
        jk_close_pool(&private_data->p);
        free(private_data);

        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

int JK_METHOD lb_worker_factory(jk_worker_t **w,
                                const char *name, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (NULL != name && NULL != w) {
        lb_worker_t *private_data =
            (lb_worker_t *) calloc(1, sizeof(lb_worker_t));


        jk_open_pool(&private_data->p,
                        private_data->buf,
                        sizeof(jk_pool_atom_t) * TINY_POOL_SIZE);

        private_data->s = jk_shm_alloc_worker(&private_data->p);
        if (!private_data->s) {
            free(private_data);
            JK_TRACE_EXIT(l);
            return 0;
        }
        strncpy(private_data->s->name, name, JK_SHM_STR_SIZ);
        private_data->lb_workers = NULL;
        private_data->num_of_workers = 0;
        private_data->worker.worker_private = private_data;
        private_data->worker.validate = validate;
        private_data->worker.init = init;
        private_data->worker.get_endpoint = get_endpoint;
        private_data->worker.destroy = destroy;
        private_data->worker.retries = JK_RETRIES;
        private_data->s->recover_wait_time = WAIT_BEFORE_RECOVER;
        *w = &private_data->worker;
        JK_TRACE_EXIT(l);
        return JK_LB_WORKER_TYPE;
    }
    else {
        JK_LOG_NULL_PARAMS(l);
    }

    JK_TRACE_EXIT(l);
    return 0;
}
