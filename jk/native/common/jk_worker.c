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
 * Description: Workers controller                                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/

#define _PLACE_WORKER_LIST_HERE
#include "jk_worker_list.h"
#include "jk_worker.h"
#include "jk_util.h"

static void close_workers(jk_logger_t *l);

static worker_factory get_factory_for(char *type);

static int build_worker_map(jk_map_t *init_data,
                            char **worker_list,
                            unsigned num_of_workers,
                            jk_worker_env_t *we, jk_logger_t *l);

int wc_open(jk_map_t *init_data, jk_worker_env_t *we, jk_logger_t *l)
{
    char **worker_list = NULL;
    unsigned num_of_workers = 0;

    JK_TRACE_ENTER(l);

    if (!jk_map_alloc(&worker_map)) {
        return JK_FALSE;
    }

    if (!jk_get_worker_list(init_data, &worker_list, &num_of_workers)) {
        return JK_FALSE;
    }

    if (!build_worker_map(init_data, worker_list, num_of_workers, we, l)) {
        close_workers(l);
        return JK_FALSE;
    }

    we->num_of_workers = num_of_workers;
    we->first_worker = worker_list[0];
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}


void wc_close(jk_logger_t *l)
{
    JK_TRACE_ENTER(l);
    close_workers(l);
    JK_TRACE_EXIT(l);
}

jk_worker_t *wc_get_worker_for_name(const char *name, jk_logger_t *l)
{
    jk_worker_t *rc;

    JK_TRACE_ENTER(l);
    if (!name) {
        jk_log(l, JK_LOG_ERROR, "wc_get_worker_for_name NULL name\n");
    }

    jk_log(l, JK_LOG_DEBUG, "Into wc_get_worker_for_name %s\n", name);

    rc = jk_map_get(worker_map, name, NULL);

    jk_log(l, JK_LOG_DEBUG, "wc_get_worker_for_name, done %s a worker\n",
           rc ? "found" : "did not find");
    JK_TRACE_EXIT(l);
    return rc;
}


int wc_create_worker(const char *name,
                     jk_map_t *init_data,
                     jk_worker_t **rc, jk_worker_env_t *we, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (rc) {
        char *type = jk_get_worker_type(init_data, name);
        worker_factory fac = get_factory_for(type);
        jk_worker_t *w = NULL;

        *rc = NULL;

        if (!fac) {
            jk_log(l, JK_LOG_ERROR, "NULL factory for %s\n",
                   type);
            return JK_FALSE;
        }

        jk_log(l, JK_LOG_DEBUG,
               "about to create instance %s of %s\n", name,
               type);

        if (!fac(&w, name, l) || !w) {
            jk_log(l, JK_LOG_ERROR,
                   "factory for %s failed for %s\n", type,
                   name);
            return JK_FALSE;
        }

        jk_log(l, JK_LOG_DEBUG,
               "about to validate and init %s\n", name);
        if (!w->validate(w, init_data, we, l)) {
            w->destroy(&w, l);
            jk_log(l, JK_LOG_ERROR,
                   "validate failed for %s\n", name);
            return JK_FALSE;
        }

        if (!w->init(w, init_data, we, l)) {
            w->destroy(&w, l);
            jk_log(l, JK_LOG_ERROR, "init failed for %s\n",
                   name);
            return JK_FALSE;
        }

        *rc = w;
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    return JK_FALSE;
}

static void close_workers(jk_logger_t *l)
{
    int sz = jk_map_size(worker_map);

    JK_TRACE_ENTER(l);

    if (sz > 0) {
        int i;
        for (i = 0; i < sz; i++) {
            jk_worker_t *w = jk_map_value_at(worker_map, i);
            if (w) {
                jk_log(l, JK_LOG_DEBUG,
                       "close_workers will destroy worker %s\n",
                       jk_map_name_at(worker_map, i));
                w->destroy(&w, l);
            }
        }
    }
    JK_TRACE_EXIT(l);
    jk_map_free(&worker_map);
}

static int build_worker_map(jk_map_t *init_data,
                            char **worker_list,
                            unsigned num_of_workers,
                            jk_worker_env_t *we, jk_logger_t *l)
{
    unsigned i;

    JK_TRACE_ENTER(l);

    for (i = 0; i < num_of_workers; i++) {
        jk_worker_t *w = NULL;

        jk_log(l, JK_LOG_DEBUG,
               "creating worker %s\n", worker_list[i]);

        if (wc_create_worker(worker_list[i], init_data, &w, we, l)) {
            jk_worker_t *oldw = NULL;
            if (!jk_map_put(worker_map, worker_list[i], w, (void *)&oldw)) {
                w->destroy(&w, l);
                return JK_FALSE;
            }

            jk_log(l, JK_LOG_DEBUG,
                   "removing old %s worker \n",
                   worker_list[i]);
            if (oldw) {
                oldw->destroy(&oldw, l);
            }
        }
        else {
            jk_log(l, JK_LOG_ERROR,
                   "failed to create worker%s\n",
                   worker_list[i]);
            return JK_FALSE;
        }
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static worker_factory get_factory_for(char *type)
{
    worker_factory_record_t *factory = &worker_factories[0];
    while (factory->name) {
        if (0 == strcmp(factory->name, type)) {
            return factory->fac;
        }

        factory++;
    }

    return NULL;
}
