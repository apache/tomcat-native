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
 * Author:      Rainer Jung <rjung@apache.org>                             *
 * Based on:                                                               *
 * Version:     $Revision$                                          *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_service.h"
#include "jk_util.h"
#include "jk_worker.h"
#include "jk_lb_worker.h"
#include "jk_ajp13.h"
#include "jk_ajp13_worker.h"
#include "jk_ajp14_worker.h"
#include "jk_mt.h"
#include "jk_shm.h"

/*
 * The load balancing code in this
 */

#define JK_WORKER_IN_ERROR(w) ((w)->in_error_state  && !(w)->is_busy)
#define JK_WORKER_USABLE(w)   (!(w)->in_error_state && !(w)->is_stopped && !(w)->is_disabled && !(w)->is_busy)

struct lb_endpoint
{
    jk_endpoint_t *e;
    lb_worker_t *worker;

    jk_endpoint_t endpoint;
};
typedef struct lb_endpoint lb_endpoint_t;


/* Calculate the greatest common divisor of two positive integers */
static jk_uint64_t gcd(jk_uint64_t a, jk_uint64_t b)
{
    jk_uint64_t r;
    if (b > a) {
        r = a;
        a = b;
        b = r;
    }
    while (b > 0) {
        r = a % b;
        a = b;
        b = r;
    }
    return a;
}

/* Calculate the smallest common multiple of two positive integers */
static jk_uint64_t scm(jk_uint64_t a, jk_uint64_t b)
{
    return a * b / gcd(a, b);
}

/* Update the load multipliers wrt. lb_factor */
void update_mult(lb_worker_t *p, jk_logger_t *l)
{
    unsigned int i = 0;
    jk_uint64_t s = 1;
    JK_TRACE_ENTER(l);
    for (i = 0; i < p->num_of_workers; i++) {
        s = scm(s, p->lb_workers[i].s->lb_factor);
    }
    for (i = 0; i < p->num_of_workers; i++) {
        p->lb_workers[i].s->lb_mult = s / p->lb_workers[i].s->lb_factor;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "worker %s gets multiplicity %"
                   JK_UINT64_T_FMT,
                   p->lb_workers[i].s->name,
                   p->lb_workers[i].s->lb_mult);
    }
    JK_TRACE_EXIT(l);
}

/* Reset all lb values.
 */
void reset_lb_values(lb_worker_t *p, jk_logger_t *l)
{
    unsigned int i = 0;
    JK_TRACE_ENTER(l);
    if (p->lbmethod != JK_LB_METHOD_BUSYNESS) {
        for (i = 0; i < p->num_of_workers; i++) {
            p->lb_workers[i].s->lb_value = 0;
        }
    }
}

/* Retrieve the parameter with the given name                                */
static char *get_path_param(jk_ws_service_t *s, const char *name)
{
    char *id_start = NULL;
    for (id_start = strstr(s->req_uri, name);
         id_start; id_start = strstr(id_start + 1, name)) {
        if (id_start[strlen(name)] == '=') {
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

/* Retrieve the cookie with the given name                                   */
static char *get_cookie(jk_ws_service_t *s, const char *name)
{
    unsigned i;
    char *result = NULL;

    for (i = 0; i < s->num_headers; i++) {
        if (strcasecmp(s->headers_names[i], "cookie") == 0) {

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
                            size_t osz = strlen(result) + 1;
                            size_t sz = osz + strlen(id_start) + 1;
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


/* Retrieve session id from the cookie or the parameter
 * (parameter first)
 */
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

/* If the worker is in error state run
 * retry on that worker. It will be marked as
 * operational if the retry timeout is elapsed.
 * The worker might still be unusable, but we try
 * anyway.
 */
static void recover_workers(lb_worker_t *p,
                            jk_uint64_t curmax,
                            time_t now,
                            jk_logger_t *l)
{
    unsigned int i;
    int elapsed;
    worker_record_t *w = NULL;
    JK_TRACE_ENTER(l);

    for (i = 0; i < p->num_of_workers; i++) {
        w = &p->lb_workers[i];
        if (JK_WORKER_IN_ERROR(w->s)) {
            elapsed = (int)difftime(now, w->s->error_time);
            if (elapsed <= p->s->recover_wait_time) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "worker %s will recover in %d seconds",
                           w->s->name, p->s->recover_wait_time - elapsed);
            }
            else {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "worker %s is marked for recovery",
                           w->s->name);
                w->s->lb_value = curmax;
                w->s->in_recovering = JK_TRUE;
                w->s->in_error_state = JK_FALSE;
                w->s->is_busy = JK_FALSE;
            }
        }
    }

    JK_TRACE_EXIT(l);
}

/* Divide old load values by the decay factor,
 * such that older values get less important
 * for the routing decisions.
 */
static jk_uint64_t decay_load(lb_worker_t *p,
                              time_t exponent,
                              jk_logger_t *l)
{
    unsigned int i;
    jk_uint64_t curmax = 0;

    JK_TRACE_ENTER(l);
    if (p->lbmethod != JK_LB_METHOD_BUSYNESS) {
        for (i = 0; i < p->num_of_workers; i++) {
            p->lb_workers[i].s->lb_value >>= exponent;
            if (p->lb_workers[i].s->lb_value > curmax) {
                curmax = p->lb_workers[i].s->lb_value;
            }
        }
    }
    JK_TRACE_EXIT(l);
    return curmax;
}

static int JK_METHOD maintain_workers(jk_worker_t *p, time_t now, jk_logger_t *l)
{
    unsigned int i = 0;
    jk_uint64_t curmax = 0;
    long delta;

    JK_TRACE_ENTER(l);
    if (p && p->worker_private) {
        lb_worker_t *lb = (lb_worker_t *)p->worker_private;

        for (i = 0; i < lb->num_of_workers; i++) {
            if (lb->lb_workers[i].w->maintain) {
                lb->lb_workers[i].w->maintain(lb->lb_workers[i].w, now, l);
            }
        }

        jk_shm_lock();

        /* Now we check for global maintenance (once for all processes).
         * Checking workers for recovery and applying decay to the
         * load values should not be done by each process individually.
         * Therefore we globally sync and we use a global timestamp.
         * Since it's possible that we come here a few milliseconds
         * before the interval has passed, we allow a little tolerance.
         */
        delta = (long)difftime(now, lb->s->last_maintain_time) + JK_LB_MAINTAIN_TOLERANCE;
        if (delta >= lb->maintain_time) {
            lb->s->last_maintain_time = now;
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "decay with 2^%d",
                       JK_LB_DECAY_MULT * delta / lb->maintain_time);
            curmax = decay_load(lb, JK_LB_DECAY_MULT * delta / lb->maintain_time, l);
            recover_workers(lb, curmax, now, l);
        }

        jk_shm_unlock();

    }
    else {
        JK_LOG_NULL_PARAMS(l);
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static worker_record_t *find_by_session(lb_worker_t *p,
                                        const char *name,
                                        jk_logger_t *l)
{

    worker_record_t *rc = NULL;
    unsigned int i;

    for (i = 0; i < p->num_of_workers; i++) {
        if (strcmp(p->lb_workers[i].s->jvm_route, name) == 0) {
            rc = &p->lb_workers[i];
            rc->r = &(rc->s->jvm_route[0]);
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
    int d = 0;
    jk_uint64_t curmin = 0;

    worker_record_t *candidate = NULL;

    /* First try to see if we have available candidate */
    for (i = 0; i < p->num_of_workers; i++) {
        /* Skip all workers that are not member of domain */
        if (strlen(p->lb_workers[i].s->domain) == 0 ||
            strcmp(p->lb_workers[i].s->domain, domain))
            continue;
        /* Take into calculation only the workers that are
         * not in error state, stopped, disabled or busy.
         */
        if (JK_WORKER_USABLE(p->lb_workers[i].s)) {
            if (!candidate || p->lb_workers[i].s->distance < d ||
                (p->lb_workers[i].s->lb_value < curmin &&
                p->lb_workers[i].s->distance == d)) {
                candidate = &p->lb_workers[i];
                curmin = p->lb_workers[i].s->lb_value;
                d = p->lb_workers[i].s->distance;
            }
        }
    }

    if (candidate) {
        candidate->r = &(candidate->s->domain[0]);
    }

    return candidate;
}


static worker_record_t *find_best_byvalue(lb_worker_t *p,
                                          jk_logger_t *l)
{
    static unsigned int next_offset = 0;
    unsigned int i;
    unsigned int j;
    unsigned int offset;
    int d = 0;
    jk_uint64_t curmin = 0;

    /* find the least busy worker */
    worker_record_t *candidate = NULL;

    offset = next_offset;

    /* First try to see if we have available candidate */
    for (j = offset; j < p->num_of_workers + offset; j++) {
        i = j % p->num_of_workers;

        /* Take into calculation only the workers that are
         * not in error state, stopped, disabled or busy.
         */
        if (JK_WORKER_USABLE(p->lb_workers[i].s)) {
            if (!candidate || p->lb_workers[i].s->distance < d ||
                (p->lb_workers[i].s->lb_value < curmin &&
                p->lb_workers[i].s->distance == d)) {
                candidate = &p->lb_workers[i];
                curmin = p->lb_workers[i].s->lb_value;
                d = p->lb_workers[i].s->distance;
                next_offset = i + 1;
            }
        }
    }
    return candidate;
}

static worker_record_t *find_bysession_route(lb_worker_t *p,
                                             const char *name,
                                             jk_logger_t *l)
{
    int uses_domain  = 0;
    worker_record_t *candidate = NULL;

    candidate = find_by_session(p, name, l);
    if (!candidate) {
        uses_domain = 1;
        candidate = find_best_bydomain(p, name, l);
    }
    if (candidate) {
        if (candidate->s->in_error_state || candidate->s->is_stopped ) {
            /* We have a worker that is error state or stopped.
             * If it has a redirection set use that redirection worker.
             * This enables to safely remove the member from the
             * balancer. Of course you will need a some kind of
             * session replication between those two remote.
             */
            if (p->s->sticky_session_force)
                candidate = NULL;
            else if (*candidate->s->redirect)
                candidate = find_by_session(p, candidate->s->redirect, l);
            else if (*candidate->s->domain && !uses_domain) {
                uses_domain = 1;
                candidate = find_best_bydomain(p, candidate->s->domain, l);
            }
            if (candidate && (candidate->s->in_error_state || candidate->s->is_stopped))
                candidate = NULL;
        }
    }
    return candidate;
}

static worker_record_t *find_failover_worker(lb_worker_t * p,
                                             jk_logger_t *l)
{
    worker_record_t *rc = NULL;
    unsigned int i;
    const char *redirect = NULL;

    for (i = 0; i < p->num_of_workers; i++) {
        if (strlen(p->lb_workers[i].s->redirect)) {
            redirect = &(p->lb_workers[i].s->redirect[0]);
            break;
        }
    }
    if (redirect)
        rc = find_bysession_route(p, redirect, l);
    return rc;
}

static worker_record_t *find_best_worker(lb_worker_t * p,
                                         jk_logger_t *l)
{
    worker_record_t *rc = NULL;

    rc = find_best_byvalue(p, l);
    /* By default use worker jvm route as session route */
    if (rc)
        rc->r = &(rc->s->jvm_route[0]);
    else
        rc = find_failover_worker(p, l);
    return rc;
}

static worker_record_t *get_most_suitable_worker(lb_worker_t * p,
                                                 jk_ws_service_t *s,
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
        if(!p->lb_workers[0].s->in_error_state && !p->lb_workers[0].s->is_stopped) {
            p->lb_workers[0].r = &(p->lb_workers[0].s->jvm_route[0]);
            JK_TRACE_EXIT(l);
            return &p->lb_workers[0];
        }
        else {
            JK_TRACE_EXIT(l);
            return NULL;
        }
    }
    else if (p->s->sticky_session) {
        /* Use sessionid only if sticky_session is
         * defined for this load balancer
         */
        sessionid = get_sessionid(s);
    }
    if (p->lblock == JK_LB_LOCK_PESSIMISTIC)
        r = jk_shm_lock();
    else {
        JK_ENTER_CS(&(p->cs), r);
    }
    if (!r) {
       jk_log(l, JK_LOG_ERROR,
              "locking failed with errno=%d",
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
                rc = find_bysession_route(p, session_route, l);
                if (rc) {
                    if (p->lblock == JK_LB_LOCK_PESSIMISTIC)
                        jk_shm_unlock();
                    else {
                        JK_LEAVE_CS(&(p->cs), r);
                    }
                    if (JK_IS_DEBUG_LEVEL(l))
                        jk_log(l, JK_LOG_DEBUG,
                               "found worker %s (%s) for route %s and partial sessionid %s",
                               rc->s->name, rc->s->jvm_route, session_route, sessionid);
                        JK_TRACE_EXIT(l);
                    return rc;
                }
            }
            /* Try next partial sessionid if present */
            sessionid = next;
            rc = NULL;
        }
        if (!rc && p->s->sticky_session_force) {
            if (p->lblock == JK_LB_LOCK_PESSIMISTIC)
                jk_shm_unlock();
            else {
                JK_LEAVE_CS(&(p->cs), r);
            }
            jk_log(l, JK_LOG_INFO,
                   "all workers are in error state for session %s",
                   session);
            JK_TRACE_EXIT(l);
            return NULL;
        }
    }
    rc = find_best_worker(p, l);
    if (p->lblock == JK_LB_LOCK_PESSIMISTIC)
        jk_shm_unlock();
    else {
        JK_LEAVE_CS(&(p->cs), r);
    }
    if (rc && JK_IS_DEBUG_LEVEL(l)) {
        jk_log(l, JK_LOG_DEBUG,
               "found best worker %s (%s) using method '%s'",
               rc->s->name, rc->s->jvm_route, lb_method_type[p->lbmethod]);
    }
    JK_TRACE_EXIT(l);
    return rc;
}

static int JK_METHOD service(jk_endpoint_t *e,
                             jk_ws_service_t *s,
                             jk_logger_t *l, int *is_error)
{
    lb_endpoint_t *p;
    int attempt = 0;
    int num_of_workers;
    worker_record_t *prec = NULL;

    JK_TRACE_ENTER(l);

    if (is_error)
        *is_error = JK_HTTP_SERVER_ERROR;
    if (!e || !e->endpoint_private || !s || !is_error) {
        JK_LOG_NULL_PARAMS(l);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    p = e->endpoint_private;
    num_of_workers = p->worker->num_of_workers;

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
            get_most_suitable_worker(p->worker, s, l);
        int rc;
        /* Do not reuse previous worker, because
         * that worker already failed.
         */
        if (rec && rec != prec) {
            int is_service_error = JK_HTTP_OK;
            int service_stat = JK_FALSE;
            jk_endpoint_t *end = NULL;

            s->jvm_route = rec->r;
            rc = rec->w->get_endpoint(rec->w, &end, l);

            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "service worker=%s jvm_route=%s",
                       rec->s->name, s->jvm_route);
            if (rc && end) {
                size_t rd = 0;
                size_t wr = 0;
                /* Reset endpoint read and write sizes for
                 * this request.
                 */
                end->rd = end->wr = 0;
                if (p->worker->lblock == JK_LB_LOCK_PESSIMISTIC)
                    jk_shm_lock();

                rec->s->elected++;
                /* Increment the number of workers serving request */
                p->worker->s->busy++;
                if (p->worker->s->busy > p->worker->s->max_busy)
                    p->worker->s->max_busy = p->worker->s->busy;
                rec->s->busy++;
                if (p->worker->lbmethod == JK_LB_METHOD_REQUESTS)
                    rec->s->lb_value += rec->s->lb_mult;
                else if (p->worker->lbmethod == JK_LB_METHOD_BUSYNESS)
                    rec->s->lb_value += rec->s->lb_mult;
                if (rec->s->busy > rec->s->max_busy)
                    rec->s->max_busy = rec->s->busy;
                if (p->worker->lblock == JK_LB_LOCK_PESSIMISTIC)
                    jk_shm_unlock();

                service_stat = end->service(end, s, l, &is_service_error);
                rd = end->rd;
                wr = end->wr;
                end->done(&end, l);

                if (p->worker->lblock == JK_LB_LOCK_PESSIMISTIC)
                    jk_shm_lock();

                /* Update partial reads and writes if any */
                rec->s->readed += rd;
                rec->s->transferred += wr;
                if (p->worker->lbmethod == JK_LB_METHOD_TRAFFIC)
                    rec->s->lb_value += (rd+wr)*rec->s->lb_mult;
                else if (p->worker->lbmethod == JK_LB_METHOD_BUSYNESS)
                    if (rec->s->lb_value >= rec->s->lb_mult)
                        rec->s->lb_value -= rec->s->lb_mult;
                    else {
                        rec->s->lb_value = 0;
                        if (JK_IS_DEBUG_LEVEL(l)) {
                            jk_log(l, JK_LOG_DEBUG,
                                   "worker %s has load value to low (%"
                                   JK_UINT64_T_FMT
                                   " < %"
                                   JK_UINT64_T_FMT
                                   ") ",
                                   "- correcting to 0",
                                   rec->s->name,
                                   rec->s->lb_value,
                                   rec->s->lb_mult);
                        }
                    }

                /* When returning the endpoint mark the worker as not busy.
                 * We have at least one endpoint free
                 */
                rec->s->is_busy = JK_FALSE;
                /* Decrement the busy worker count.
                 * Check if the busy was reset to zero by graceful
                 * restart of the server.
                 */
                if (rec->s->busy)
                    rec->s->busy--;
                if (p->worker->s->busy)
                    p->worker->s->busy--;
                if (service_stat == JK_TRUE) {
                    rec->s->in_error_state = JK_FALSE;
                    rec->s->in_recovering = JK_FALSE;
                    rec->s->error_time = 0;
                    if (p->worker->lblock == JK_LB_LOCK_PESSIMISTIC)
                        jk_shm_unlock();
                    JK_TRACE_EXIT(l);
                    return JK_TRUE;
                }
                if (p->worker->lblock == JK_LB_LOCK_PESSIMISTIC)
                    jk_shm_unlock();
            }
            else {
                /* If we can not get the endpoint
                 * mark the worker as busy rather then
                 * as in error if the attemp number is
                 * greater then the number of retries.
                 */
                attempt++;
                if (attempt > p->worker->s->retries) {
                    rec->s->is_busy = JK_TRUE;
                    num_of_workers = 0;
                }
                jk_log(l, JK_LOG_INFO,
                       "could not get free endpoint for worker %s (attempt %d)",
                       rec->s->name, attempt);
                /* In case of attempt > num of workers sleep for 100 ms
                 * on each consequtive attempt.
                 */
                if (attempt > (int)p->worker->num_of_workers)
                    jk_sleep_def();
                continue;
            }
            if (service_stat == JK_FALSE) {
                /*
                * Service failed !!!
                *
                * Time for fault tolerance (if possible)...
                */

                rec->s->errors++;
                rec->s->in_error_state = JK_TRUE;
                rec->s->in_recovering = JK_FALSE;
                rec->s->error_time = time(NULL);

                if (is_service_error != JK_HTTP_SERVER_BUSY) {
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
            else if (service_stat == JK_CLIENT_ERROR) {
                /*
                * Client error !!!
                * Since this is bad request do not fail over.
                */
                rec->s->errors++;
                rec->s->in_error_state = JK_FALSE;
                rec->s->in_recovering = JK_FALSE;
                rec->s->error_time = 0;
                *is_error = is_service_error;

                jk_log(l, JK_LOG_INFO,
                       "unrecoverable error %d, request failed."
                       " Client failed in the middle of request,"
                       " we can't recover to another instance.",
                       is_service_error);
                JK_TRACE_EXIT(l);
                return JK_CLIENT_ERROR;
            }
            else {
                /* If we can not get the endpoint from the worker
                 * that does not mean that the worker is in error
                 * state. It means that the worker is busy.
                 * We will try another worker.
                 * To prevent infinite loop decrement worker count;
                 */
            }
            /*
             * Error is recoverable by submitting the request to
             * another worker... Lets try to do that.
             */
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "recoverable error... will try to recover on other host");
        }
#if 0
        else {
            /* NULL record, no more workers left ... */
            jk_log(l, JK_LOG_ERROR,
                   "All tomcat instances failed, no more workers left");
            JK_TRACE_EXIT(l);
            *is_error = JK_HTTP_SERVER_BUSY;
            return JK_FALSE;
        }
#endif
        --num_of_workers;
        prec = rec;
    }
    jk_log(l, JK_LOG_INFO,
           "All tomcat instances are busy or in error state");
    /* Set error to Timeout */
    *is_error = JK_HTTP_SERVER_BUSY;
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
        const char *secret;

        p->s->sticky_session = jk_get_is_sticky_session(props, p->s->name);
        p->s->sticky_session_force = jk_get_is_sticky_session_force(props, p->s->name);
        secret = jk_get_worker_secret(props, p->s->name);

        if (jk_get_lb_worker_list(props,
                                  p->s->name,
                                  &worker_names,
                                  &num_of_workers) && num_of_workers) {
            unsigned int i = 0;
            unsigned int j = 0;

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
                p->lb_workers[i].s->distance =
                    jk_get_distance(props, worker_names[i]);
                if ((s = jk_get_worker_jvm_route(props, worker_names[i], NULL)))
                    strncpy(p->lb_workers[i].s->jvm_route, s, JK_SHM_STR_SIZ);
                else
                    strncpy(p->lb_workers[i].s->jvm_route, worker_names[i], JK_SHM_STR_SIZ);
                if ((s = jk_get_worker_domain(props, worker_names[i], NULL)))
                    strncpy(p->lb_workers[i].s->domain, s, JK_SHM_STR_SIZ);
                if ((s = jk_get_worker_redirect(props, worker_names[i], NULL)))
                    strncpy(p->lb_workers[i].s->redirect, s, JK_SHM_STR_SIZ);

                p->lb_workers[i].s->lb_value = 0;
                p->lb_workers[i].s->in_error_state = JK_FALSE;
                p->lb_workers[i].s->in_recovering = JK_FALSE;
                p->lb_workers[i].s->is_busy = JK_FALSE;
                p->lb_workers[i].s->error_time = 0;
                /* Worker can be initaly disabled as hot standby */
                p->lb_workers[i].s->is_disabled = jk_get_is_worker_disabled(props, worker_names[i]);
                /* Worker can be initaly deactive as cold standby */
                p->lb_workers[i].s->is_stopped = jk_get_is_worker_stopped(props, worker_names[i]);
                if (!wc_create_worker(p->lb_workers[i].s->name, 0,
                                      props,
                                      &(p->lb_workers[i].w),
                                      we, l) || !p->lb_workers[i].w) {
                    break;
                }
                if (secret && (p->lb_workers[i].w->type == JK_AJP13_WORKER_TYPE ||
                    p->lb_workers[i].w->type == JK_AJP14_WORKER_TYPE)) {
                    ajp_worker_t *aw = (ajp_worker_t *)p->lb_workers[i].w->worker_private;
                    if (!aw->secret)
                        aw->secret = secret;
                }
            }

            if (i != num_of_workers) {
                jk_log(l, JK_LOG_ERROR,
                       "Failed creating worker %s",
                       p->lb_workers[i].s->name);
                close_workers(p, i, l);
            }
            else {
                for (i = 0; i < num_of_workers; i++) {
                    if (JK_IS_DEBUG_LEVEL(l)) {
                        jk_log(l, JK_LOG_DEBUG,
                               "Balanced worker %i has name %s and jvm_route %s in domain %s",
                               i,
                               p->lb_workers[i].s->name,
                               p->lb_workers[i].s->jvm_route,
                               p->lb_workers[i].s->domain);
                    }
                }
                p->num_of_workers = num_of_workers;
                update_mult(p, l);
                for (i = 0; i < num_of_workers; i++) {
                    for (j = 0; j < i; j++) {
                        if (strcmp(p->lb_workers[i].s->jvm_route, p->lb_workers[j].s->jvm_route) == 0) {
                            jk_log(l, JK_LOG_ERROR,
                                   "Balanced workers number %i (%s) and %i (%s) share the same jvm_route %s - aborting configuration!",
                                   i,
                                   p->lb_workers[i].s->name,
                                   j,
                                   p->lb_workers[j].s->name,
                                   p->lb_workers[i].s->jvm_route);
                            JK_TRACE_EXIT(l);
                            return JK_FALSE;
                        }
                    }
                }
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
    p->s->recover_wait_time = jk_get_worker_recover_timeout(props, p->s->name,
                                                            WAIT_BEFORE_RECOVER);
    if (p->s->recover_wait_time < 1)
        p->s->recover_wait_time = 1;
    p->maintain_time = jk_get_worker_maintain_time(props);
    p->s->last_maintain_time = time(NULL);

    p->lbmethod = jk_get_lb_method(props, p->s->name);
    p->lblock   = jk_get_lb_lock(props, p->s->name);

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
        private_data->worker.maintain = maintain_workers;
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
