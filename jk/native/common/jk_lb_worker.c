/*
 * Copyright (c) 1997-1999 The Java Apache Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * 4. The names "Apache JServ", "Apache JServ Servlet Engine" and 
 *    "Java Apache Project" must not be used to endorse or promote products 
 *    derived from this software without prior written permission.
 *
 * 5. Products derived from this software may not be called "Apache JServ"
 *    nor may "Apache" nor "Apache JServ" appear in their names without 
 *    prior written permission of the Java Apache Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *    
 * THIS SOFTWARE IS PROVIDED BY THE JAVA APACHE PROJECT "AS IS" AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE JAVA APACHE PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Java Apache Group. For more information
 * on the Java Apache Project and the Apache JServ Servlet Engine project,
 * please see <http://java.apache.org/>.
 *
 */

/***************************************************************************
 * Description: Load balancer worker, knows how to load balance among      *
 *              several workers.                                           *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Based on:                                                               *
 * Version:     $Revision$                                               *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_service.h"
#include "jk_util.h"
#include "jk_worker.h"
#include "jk_lb_worker.h"

/*
 * The load balancing code in this 
 */


/* 
 * Time to wait before retry...
 */
#define WAIT_BEFORE_RECOVER (60*1) 
#define ADDITINAL_WAIT_LOAD (20)

struct worker_record {
    char    *name;
    double  lb_factor;
    double  lb_value;
    int     in_error_state;
    int     in_recovering;
    time_t  error_time;
    jk_worker_t *w;
};
typedef struct worker_record worker_record_t;

struct lb_worker {
    worker_record_t *lb_workers;
    unsigned num_of_workers;

    jk_pool_t p;
    jk_pool_atom_t buf[TINY_POOL_SIZE];

    char *name; 
    jk_worker_t worker;
};
typedef struct lb_worker lb_worker_t;

struct lb_endpoint {    
    jk_endpoint_t *e;
    lb_worker_t *worker;
    
    jk_endpoint_t endpoint;
};
typedef struct lb_endpoint lb_endpoint_t;


/* ========================================================================= */
/* Retrieve the parameter with the given name                                */
static char *get_path_param(jk_ws_service_t *s,
                            const char *name)
{
    char *id_start = NULL;
    for(id_start = strstr(s->req_uri, name) ; 
        id_start ; 
        id_start = strstr(id_start + 1, name)) {
        if('=' == id_start[strlen(name)]) {
            /*
             * Session path-cookie was found, get it's value
             */
            id_start += (1 + strlen(name));
            if(strlen(id_start)) {
                char *id_end;
                id_start = jk_pool_strdup(s->pool, id_start);
                /* 
                 * The query string is not part of req_uri, however
                 * to be on the safe side lets remove the trailing query 
                 * string if appended...
                 */
                if(id_end = strchr(id_start, '?')) { 
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
static char *get_cookie(jk_ws_service_t *s,
                        const char *name)
{
    unsigned i;

    for(i = 0 ; i < s->num_headers ; i++) {
        if(0 == strcasecmp(s->headers_names[i], "cookie")) {

            char *id_start;
            for(id_start = strstr(s->headers_values[i], name) ; 
                id_start ; 
                id_start = strstr(id_start + 1, name)) {
                if('=' == id_start[strlen(name)]) {
                    /*
                     * Session cookie was found, get it's value
                     */
                    id_start += (1 + strlen(name));
                    if(strlen(id_start)) {
                        char *id_end;
                        id_start = jk_pool_strdup(s->pool, id_start);
                        if(id_end = strchr(id_start, ';')) {
                            *id_end = '\0';
                        }
                        return id_start;
                    }
                }
            }
        }
    }

    return NULL;
}


/* ========================================================================= */
/* Retrieve session id from the cookie or the parameter                      */
/* (parameter first)                                                         */
static char *get_sessionid(jk_ws_service_t *s)
{
    char *val;
    val = get_path_param(s, JK_PATH_SESSION_IDENTIFIER);
    if(!val) {
        val = get_cookie(s, JK_SESSION_IDENTIFIER);
    }
    return val;
}

static char *get_session_route(jk_ws_service_t *s)
{
    char *sessionid = get_sessionid(s);
    char *ch;

    if(!sessionid) {
        return NULL;
    }

    /*
     * Balance parameter is appended to the end
     */  
    ch = strrchr(sessionid, '.');
    if(!ch) {
        return 0;
    }
    ch++;
    if(*ch == '\0') {
        return NULL;
    }
    return ch;
}

static void close_workers(lb_worker_t *p, 
                          int num_of_workers,
                          jk_logger_t *l)
{
    int i = 0;
    for(i = 0 ; i < num_of_workers ; i++) {
        p->lb_workers[i].w->destroy(&(p->lb_workers[i].w),
                                    l);
    }
}

static double get_max_lb(lb_worker_t *p) 
{
    unsigned i;
    double rc = 0.0;    

    for(i = 0 ; i < p->num_of_workers ; i++) {
        if(!p->lb_workers[i].in_error_state) {
            if(p->lb_workers[i].lb_value > rc) {
                rc = p->lb_workers[i].lb_value;
            }
        }            
    }

    return rc;

}

static worker_record_t *get_most_suitable_worker(lb_worker_t *p, 
                                                 jk_ws_service_t *s)
{
    worker_record_t *rc = NULL;
    double lb_min = 0.0;    
    unsigned i;
    char *session_route = get_session_route(s);
       
    if(session_route) {
        for(i = 0 ; i < p->num_of_workers ; i++) {
            if(0 == strcmp(session_route, p->lb_workers[i].name)) {
                if(p->lb_workers[i].in_error_state) {
                   break;
                } else {
                    return &(p->lb_workers[i]);
                }
            }
        }
    }

    for(i = 0 ; i < p->num_of_workers ; i++) {
        if(p->lb_workers[i].in_error_state) {
            if(!p->lb_workers[i].in_recovering) {
                time_t now = time(0);
                
                if((now - p->lb_workers[i].error_time) > WAIT_BEFORE_RECOVER) {
                    
                    p->lb_workers[i].in_recovering  = JK_TRUE;
                    p->lb_workers[i].error_time     = now;
                    rc = &(p->lb_workers[i]);

                    break;
                }
            }
        } else {
            if(p->lb_workers[i].lb_value < lb_min || !rc) {
                lb_min = p->lb_workers[i].lb_value;
                rc = &(p->lb_workers[i]);
            }
        }            
    }

    if(rc) {
        rc->lb_value += rc->lb_factor;                
    }

    return rc;
}
    
static int JK_METHOD service(jk_endpoint_t *e, 
                             jk_ws_service_t *s,
                             jk_logger_t *l,
                             int *is_recoverable_error)
{
    jk_log(l, JK_LOG_DEBUG, 
           "Into jk_endpoint_t::service\n");

    if(e && e->endpoint_private && s && is_recoverable_error) {
        lb_endpoint_t *p = e->endpoint_private;
        jk_endpoint_t *end = NULL;

        /* you can not recover on another load balancer */
        *is_recoverable_error = JK_FALSE;


        while(1) {
            worker_record_t *rec = get_most_suitable_worker(p->worker, s);
            int rc;

            if(rec) {
                int is_recoverable = JK_FALSE;
                
                s->jvm_route = jk_pool_strdup(s->pool,  rec->name);

                rc = rec->w->get_endpoint(rec->w, &end, l);
                if(rc && end) {
                    int src = end->service(end, s, l, &is_recoverable);
                    end->done(&end, l);
                    if(src) {                        
                        if(rec->in_recovering) {
                            rec->lb_value = get_max_lb(p->worker) + ADDITINAL_WAIT_LOAD;
                        }
                        rec->in_error_state = JK_FALSE;
                        rec->in_recovering  = JK_FALSE;
                        rec->error_time     = 0;                        
                        return JK_TRUE;
                    } 
                }

                /*
                 * Service failed !!!
                 *
                 * Time for fault tolerance (if possible)...
                 */

                rec->in_error_state = JK_TRUE;
                rec->in_recovering  = JK_FALSE;
                rec->error_time     = time(0);

                if(!is_recoverable) {
                    /*
                     * Error is not recoverable - break with an error.
                     */
                    jk_log(l, JK_LOG_ERROR, 
                           "In jk_endpoint_t::service, none recoverable error...\n");
                    break;
                }

                /* 
                 * Error is recoverable by submitting the request to
                 * another worker... Lets try to do that.
                 */
                 jk_log(l, JK_LOG_DEBUG, 
                        "In jk_endpoint_t::service, recoverable error... will try to recover on other host\n");
            } else {
                /* NULL record, no more workers left ... */
                 jk_log(l, JK_LOG_ERROR, 
                        "In jk_endpoint_t::service, No more workers left, can not submit the request\n");
                break;
            }
        }
    }

    jk_log(l, JK_LOG_ERROR, 
           "In jk_endpoint_t::service: NULL Parameters\n");

    return JK_FALSE;
}

static int JK_METHOD done(jk_endpoint_t **e,
                          jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, 
           "Into jk_endpoint_t::done\n");

    if(e && *e && (*e)->endpoint_private) {
        lb_endpoint_t *p = (*e)->endpoint_private;

        if(p->e) {
            p->e->done(&p->e, l);
        }

        free(p);
        *e = NULL;
        return JK_TRUE;
    }

    jk_log(l, JK_LOG_ERROR, 
           "In jk_endpoint_t::done: NULL Parameters\n");

    return JK_FALSE;
}

static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t *props,                            
                              jk_worker_env_t *we,
                              jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, 
           "Into jk_worker_t::validate\n");

    if(pThis && pThis->worker_private) {        
        lb_worker_t *p = pThis->worker_private;
        char **worker_names;
        unsigned num_of_workers;
        
        if(jk_get_lb_worker_list(props,
                                 p->name,
                                 &worker_names, 
                                 &num_of_workers) && num_of_workers) {
            unsigned i = 0;

            p->lb_workers = jk_pool_alloc(&p->p, 
                                          num_of_workers * sizeof(worker_record_t));

            if(!p->lb_workers) {
                return JK_FALSE;
            }

            for(i = 0 ; i < num_of_workers ; i++) {
                p->lb_workers[i].name = jk_pool_strdup(&p->p, worker_names[i]);
                p->lb_workers[i].lb_factor = jk_get_lb_factor(props, 
                                                               worker_names[i]);
                p->lb_workers[i].lb_factor = 1/p->lb_workers[i].lb_factor;
                p->lb_workers[i].lb_value = 0.0;
                p->lb_workers[i].in_error_state = JK_FALSE;
                p->lb_workers[i].in_recovering  = JK_FALSE;
                if(!wc_create_worker(p->lb_workers[i].name, 
                                     props, 
                                     &(p->lb_workers[i].w), 
                                     we,
                                     l) || !p->lb_workers[i].w) {
                    break;
                }
            }

            if(i != num_of_workers) {
                close_workers(p, i, l);
                jk_log(l, JK_LOG_ERROR, 
                       "In jk_worker_t::validate: Failed to create worker %s\n",
                       p->lb_workers[i].name);

            } else {
                p->num_of_workers = num_of_workers;
                return JK_TRUE;
            }
        }        
    }

    jk_log(l, JK_LOG_ERROR, 
           "In jk_worker_t::validate: NULL Parameters\n");

    return JK_FALSE;
}

static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t *props, 
                          jk_worker_env_t *we,
                          jk_logger_t *log)
{
    /* Nothing to do for now */
    return JK_TRUE;
}

static int JK_METHOD get_endpoint(jk_worker_t *pThis,
                                  jk_endpoint_t **pend,
                                  jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, 
           "Into jk_worker_t::get_endpoint\n");

    if(pThis && pThis->worker_private && pend) {        
        lb_endpoint_t *p = (lb_endpoint_t *)malloc(sizeof(lb_endpoint_t));
        if(p) {
            p->e = NULL;
            p->worker = pThis->worker_private;
            p->endpoint.endpoint_private = p;
            p->endpoint.service = service;
            p->endpoint.done = done;
            *pend = &p->endpoint;

            return JK_TRUE;
        }
        jk_log(l, JK_LOG_ERROR, 
               "In jk_worker_t::get_endpoint, malloc failed\n");
    } else {
        jk_log(l, JK_LOG_ERROR, 
               "In jk_worker_t::get_endpoint, NULL parameters\n");
    }

    return JK_FALSE;
}

static int JK_METHOD destroy(jk_worker_t **pThis,
                             jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, 
           "Into jk_worker_t::destroy\n");

    if(pThis && *pThis && (*pThis)->worker_private) {
        lb_worker_t *private_data = (*pThis)->worker_private;

        close_workers(private_data, 
                      private_data->num_of_workers,
                      l);

        jk_close_pool(&private_data->p);
        free(private_data);

        return JK_TRUE;
    }

    jk_log(l, JK_LOG_ERROR, 
           "In jk_worker_t::destroy, NULL parameters\n");
    return JK_FALSE;
}

int JK_METHOD lb_worker_factory(jk_worker_t **w,
                                const char *name,
                                jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, 
           "Into lb_worker_factory\n");

    if(NULL != name && NULL != w) {
        lb_worker_t *private_data = 
            (lb_worker_t *)malloc(sizeof(lb_worker_t));

        if(private_data) {

            jk_open_pool(&private_data->p, 
                         private_data->buf, 
                         sizeof(jk_pool_atom_t) * TINY_POOL_SIZE);

            private_data->name = jk_pool_strdup(&private_data->p, name);          

            if(private_data->name) {
                private_data->lb_workers = NULL;
                private_data->num_of_workers = 0;
                private_data->worker.worker_private = private_data;
                private_data->worker.validate       = validate;
                private_data->worker.init           = init;
                private_data->worker.get_endpoint   = get_endpoint;
                private_data->worker.destroy        = destroy;

                *w = &private_data->worker;
                return JK_TRUE;
            }

            jk_close_pool(&private_data->p);
            free(private_data);
        }
        jk_log(l, JK_LOG_ERROR, 
               "In lb_worker_factory, malloc failed\n");
    } else {
        jk_log(l, JK_LOG_ERROR, 
               "In lb_worker_factory, NULL parameters\n");
    }

    return JK_FALSE;
}

