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

static const char *search_types[] = {
    "any",
    "sticky",
    "redirect",
    "sticky domain",
    "local",
    "local domain",
    NULL
};

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


static int is_worker_candidate(worker_record_t *wr,
                               int search_id,
                               const char *search_string,
                               jk_logger_t *l)
{
    switch (search_id) {
        case 0:
            return JK_TRUE;
        case 1:
            if (strcmp(search_string, wr->s->name) == 0) {
                return JK_TRUE;
            }
        break;
        case 2:
            if (strcmp(search_string, wr->s->name) == 0) {
                return JK_TRUE;
            }
        break;
        case 3:
            if (strcmp(search_string, wr->s->domain) == 0) {
                return JK_TRUE;
            }
        break;
        case 4:
            if (wr->s->is_local_worker) {
                return JK_TRUE;
            }
        break;
        case 5:
            if (wr->s->is_local_domain) {
                return JK_TRUE;
            }
        break;
    }
    return JK_FALSE;
}

static worker_record_t *get_suitable_worker(lb_worker_t *p, 
                                            int search_id,
                                            const char *search_string,
                                            int start,
                                            int stop,
                                            int use_lb_factor,
                                            int *domain_id,
                                            jk_logger_t *l)
{

    worker_record_t *rc = NULL;
    int lb_max = 0;
    int total_factor = 0;
    const char *search_type = search_types[search_id];
    int i;
    
    *domain_id = -1;

    JK_ENTER_CS(&(p->cs), i);
    if (!i) {
        jk_log(l, JK_LOG_ERROR,
               "could not lock load balancer = %s",
               p->s->name);
        return NULL;
    }
    if (JK_IS_DEBUG_LEVEL(l))
       jk_log(l, JK_LOG_DEBUG,
              "searching for %s worker (%s)",
              search_type, search_string);

    for (i = start; i < stop; i++) {
        if (search_id < 3 && p->lb_workers[i].s->is_disabled) {

            continue;
        }
        if (is_worker_candidate(&(p->lb_workers[i]), search_id, search_string, l)) {
            if (JK_IS_DEBUG_LEVEL(l))
               jk_log(l, JK_LOG_DEBUG,
                      "found candidate worker %s (%d) for match with %s (%s)",
                      p->lb_workers[i].s->name, i, search_type, search_string);
            if (search_id == 1 && strlen(p->lb_workers[i].s->redirect)) {
                *domain_id = i;
            }
            else if (search_id == 2) {
                *domain_id = i;
            }
            if (!p->lb_workers[i].s->in_error_state || !p->lb_workers[i].s->in_recovering) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "found candidate worker %s (%d) with previous load %d in search with %s (%s)",
                           p->lb_workers[i].s->name, i, p->lb_workers[i].s->lb_value,
                           search_type, search_string);

                if (p->lb_workers[i].s->in_error_state) {

                    time_t now = time(0);
                    int elapsed = now - p->lb_workers[i].s->error_time;
                    if (elapsed <= p->s->recover_wait_time) {
                        if (JK_IS_DEBUG_LEVEL(l))
                            jk_log(l, JK_LOG_DEBUG,
                                   "worker candidate %s (%d) is in error state - will not yet recover (%d < %d)",
                                   p->lb_workers[i].s->name, i, elapsed, p->s->recover_wait_time);
                        continue;
                    }
                }

                if (use_lb_factor) {
                    p->lb_workers[i].s->lb_value += p->lb_workers[i].s->lb_factor;
                    total_factor += p->lb_workers[i].s->lb_factor;
                    if (p->lb_workers[i].s->lb_value > lb_max || !rc) {
                        lb_max = p->lb_workers[i].s->lb_value;
                        rc = &(p->lb_workers[i]);
                        if (JK_IS_DEBUG_LEVEL(l))
                            jk_log(l, JK_LOG_DEBUG,
                                   "new maximal worker %s (%d) with previous load %d in search with %s (%s)",
                                   rc->s->name, i, rc->s->lb_value, search_type, search_string);
                    }
                } else {
                    rc = &(p->lb_workers[i]);
                    break;
                }
            }
            else if (JK_IS_DEBUG_LEVEL(l)) {
                jk_log(l, JK_LOG_TRACE,
                    "worker candidate %s (%d) is in error state - already recovers",
                    p->lb_workers[i].s->name, i);
            }
        }
    }

    if (rc) {
        if (rc->s->in_error_state) {
            time_t now = time(0);
            rc->s->in_recovering  = JK_TRUE;
            rc->s->error_time     = now;
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "found worker %s is in error state - will recover",
                       rc->s->name);
        }
        rc->s->lb_value -= total_factor;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "found worker %s with new load %d in search with %s (%s)",
                   rc->s->name, rc->s->lb_value, search_type, search_string);
        JK_LEAVE_CS(&(p->cs), i);
        return rc;
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "found no %s (%s) worker",
               search_type, search_string);
    JK_LEAVE_CS(&(p->cs), i);
    return rc;
}

static worker_record_t *get_most_suitable_worker(lb_worker_t * p,
                                                 jk_ws_service_t *s,
                                                 int attempt,
                                                 jk_logger_t *l)
{
    worker_record_t *rc = NULL;
    char *sessionid = NULL;
    int domain_id = -1;

    JK_TRACE_ENTER(l);
    if (p->s->sticky_session) {
        sessionid = get_sessionid(s);
    }

    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "total sessionid is %s.",
                sessionid ? sessionid : "empty");
    while (sessionid) {
        char *next = strchr(sessionid, ';');
        char *session_route;
        const char *session_domain;
        if (next) {
            *next++ = '\0';
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "searching worker for partial sessionid %s.",
                   sessionid);
        session_route = strchr(sessionid, '.');
        if (session_route) {
            ++session_route;

            rc = get_suitable_worker(p, 1, session_route, 0, p->num_of_workers, 0, &domain_id, l);
            if (rc) {
                JK_TRACE_EXIT(l);
                return rc;
            }
            if (domain_id >= 0 && domain_id < (int)p->num_of_workers) {
                session_domain = p->lb_workers[domain_id].s->domain;
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "found redirect %s in route %s",
                           session_domain, session_route);

                rc = get_suitable_worker(p, 2, session_domain, 0, p->num_of_workers,
                                         0, &domain_id, l);
                if (rc) {
                    JK_TRACE_EXIT(l);
                    return rc;
                }
            }

            if (domain_id >= 0 && domain_id < (int)p->num_of_workers) {
                session_domain = p->lb_workers[domain_id].s->domain;
            }
            else {
                session_domain = JK_LB_DEF_DOMAIN_NAME;
            }
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "found domain %s in route %s",
                       session_domain, session_route);

            rc = get_suitable_worker(p, 3, session_domain, 0, p->num_of_workers,
                                     1, &domain_id, l);
            if (rc) {
                JK_TRACE_EXIT(l);
                return rc;
            }

        }
        sessionid = next;
    }


    if (p->num_of_local_workers) {
        rc = get_suitable_worker(p, 4, "any", 0, p->num_of_local_workers,
                                 1, &domain_id, l);
        if (rc) {
            JK_TRACE_EXIT(l);
            return rc;
        }

        if (p->s->local_worker_only) {
            JK_TRACE_EXIT(l);
            return NULL;
        }

        rc = get_suitable_worker(p, 5, "any", p->num_of_local_workers,
                                 p->num_of_workers, 1, &domain_id, l);
        if (rc) {
            JK_TRACE_EXIT(l);
            return rc;
        }
    }
    rc = get_suitable_worker(p, 0, "any", p->num_of_local_workers, p->num_of_workers,
                             1, &domain_id, l);
    JK_TRACE_EXIT(l);
    return rc;
}

static int JK_METHOD service(jk_endpoint_t *e,
                             jk_ws_service_t *s,
                             jk_logger_t *l, int *is_recoverable_error)
{
    JK_TRACE_ENTER(l);

    if (e && e->endpoint_private && s && is_recoverable_error) {
        lb_endpoint_t *p = e->endpoint_private;
        jk_endpoint_t *end = NULL;
        int attempt = 0;

        /* you can not recover on another load balancer */
        *is_recoverable_error = JK_FALSE;

        /* set the recovery post, for LB mode */
        s->reco_buf = jk_b_new(s->pool);
        jk_b_set_buffer_size(s->reco_buf, DEF_BUFFER_SZ);
        jk_b_reset(s->reco_buf);
        s->reco_status = RECO_INITED;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "service sticky_session=%d",
                   p->worker->s->sticky_session);

        while (1) {
            worker_record_t *rec =
                get_most_suitable_worker(p->worker, s, attempt++, l);
            int rc;

            if (rec) {
                int is_recoverable = JK_FALSE;

                s->jvm_route = jk_pool_strdup(s->pool, rec->s->name);

                rc = rec->w->get_endpoint(rec->w, &end, l);

                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "service worker=%s jvm_route=%s rc=%d",
                           rec->s->name, s->jvm_route, rc);
                rec->s->elected++;
                if (rc && end) {
                    int src;
                    end->rd = end->wr = 0;
                    src = end->service(end, s, l, &is_recoverable);
                    rec->s->readed += end->rd;
                    rec->s->transferred += end->wr;
                    end->done(&end, l);
                    if (src) {
                        rec->s->in_error_state = JK_FALSE;
                        rec->s->in_recovering = JK_FALSE;
                        rec->s->error_time = 0;
                        JK_TRACE_EXIT(l);
                        return JK_TRUE;
                    }
                }
                rec->s->errors++;
                /*
                 * Service failed !!!
                 *
                 * Time for fault tolerance (if possible)...
                 */

                rec->s->in_error_state = JK_TRUE;
                rec->s->in_recovering = JK_FALSE;
                rec->s->error_time = time(0);

                if (!is_recoverable) {
                    /*
                     * Error is not recoverable - break with an error.
                     */
                    jk_log(l, JK_LOG_ERROR,
                           "lb: unrecoverable error, request failed. Tomcat failed in the middle of request, we can't recover to another instance.");
                    JK_TRACE_EXIT(l);
                    return JK_FALSE;
                }

                /* 
                 * Error is recoverable by submitting the request to
                 * another worker... Lets try to do that.
                 */
                jk_log(l, JK_LOG_DEBUG,
                       "recoverable error... will try to recover on other host");
            }
            else {
                /* NULL record, no more workers left ... */
                jk_log(l, JK_LOG_ERROR,
                       "lb: All tomcat instances failed, no more workers left.");
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }
        }
    }

    jk_log(l, JK_LOG_ERROR, "lb: end of service with error");
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
        unsigned int num_of_local_workers;

        p->s->in_local_worker_mode = JK_FALSE;
        p->s->local_worker_only = jk_get_local_worker_only_flag(props, p->s->name);
        p->s->sticky_session = jk_get_is_sticky_session(props, p->s->name);
        p->num_of_local_workers = 0;

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
                strncpy(p->lb_workers[i].s->name, worker_names[i],
                        JK_SHM_STR_SIZ);
                p->lb_workers[i].s->lb_factor =
                    jk_get_lb_factor(props, worker_names[i]);
                if (p->lb_workers[i].s->lb_factor < 1) {
                    p->lb_workers[i].s->lb_factor = 1;
                }
                strncpy(p->lb_workers[i].s->domain,
                        jk_get_worker_domain(props, worker_names[i],
                                             JK_LB_DEF_DOMAIN_NAME),
                        JK_SHM_STR_SIZ);
                p->lb_workers[i].s->is_local_worker =
                    jk_get_is_local_worker(props, worker_names[i]);
                if (p->lb_workers[i].s->is_local_worker)
                    p->s->in_local_worker_mode = JK_TRUE;
                /* 
                 * Allow using lb in fault-tolerant mode.
                 * A value of 0 means the worker will be used for all requests without
                 * sessions
                 */
                p->lb_workers[i].s->lb_value = p->lb_workers[i].s->lb_factor;
                p->lb_workers[i].s->in_error_state = JK_FALSE;
                p->lb_workers[i].s->in_recovering = JK_FALSE;
                if (!wc_create_worker(p->lb_workers[i].s->name,
                                      props,
                                      &(p->lb_workers[i].w),
                                      we, l) || !p->lb_workers[i].w) {
                    break;
                }
                else if (p->lb_workers[i].s->is_local_worker) {
                    /*
                     * If lb_value is 0 than move it at the beginning of the list
                     */
                    if (i != j) {
                        worker_record_t tmp_worker;
                        tmp_worker = p->lb_workers[j];
                        p->lb_workers[j] = p->lb_workers[i];
                        p->lb_workers[i] = tmp_worker;
                    }
                    j++;
                }
            }
            num_of_local_workers = j;

            if (!p->s->in_local_worker_mode) {
                p->s->local_worker_only = JK_FALSE;
            }

            if (i != num_of_workers) {
                close_workers(p, i, l);
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Failed to create worker %s",
                           p->lb_workers[i].s->name);

            }
            else {
                for (i = 0; i < num_of_local_workers; i++) {
                    p->lb_workers[i].s->is_local_domain=1;
                }
                for (i = num_of_local_workers; i < num_of_workers; i++) {
                    p->lb_workers[i].s->is_local_domain=0;
                    for (j = 0; j < num_of_local_workers; j++) {
                        if(!strcmp(p->lb_workers[i].s->domain, p->lb_workers[j].s->domain)) {
                            p->lb_workers[i].s->is_local_domain = 1;
                            break;
                        }
                    }
                }

                if (JK_IS_DEBUG_LEVEL(l)) {
                    for (i = 0; i < num_of_workers; i++) {
                        jk_log(l, JK_LOG_DEBUG,
                               "Balanced worker %i has name %s in domain %s and has local=%d and local_domain=%d",
                               i, p->lb_workers[i].s->name, p->lb_workers[i].s->domain,
                               p->lb_workers[i].s->is_local_worker, p->lb_workers[i].s->is_local_domain);
                    }
                }
                if (JK_IS_DEBUG_LEVEL(l)) {
                    jk_log(l, JK_LOG_DEBUG,
                           "in_local_worker_mode: %s",
                           (p->s->in_local_worker_mode ? "true" : "false"));
                    jk_log(l, JK_LOG_DEBUG,
                           "local_worker_only: %s",
                           (p->s->local_worker_only ? "true" : "false"));
                }
                p->num_of_workers = num_of_workers;
                p->num_of_local_workers = num_of_local_workers;
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
        private_data->num_of_local_workers = 0;
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
