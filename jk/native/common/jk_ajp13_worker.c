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
 * Description: Bi-directional protocol.                                   *
 * Author:      Costin <costin@costin.dnt.ro>                              *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/

#include "jk_ajp13_worker.h"


/* -------------------- Method -------------------- */
static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t *props,
                              jk_worker_env_t *we, jk_logger_t *l)
{
    int rc;
    JK_TRACE_ENTER(l);
    rc = ajp_validate(pThis, props, we, l, AJP13_PROTO);
    JK_TRACE_EXIT(l);
    return rc;
}


static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t *props,
                          jk_worker_env_t *we, jk_logger_t *l)
{
    int rc;
    ajp_worker_t *aw = ( ajp_worker_t *)pThis->worker_private;
    JK_TRACE_ENTER(l);

    pThis->retries = jk_get_worker_retries(props, aw->name,
                                           JK_RETRIES);

    rc = ajp_init(pThis, props, we, l, AJP13_PROTO);
    JK_TRACE_EXIT(l);
    return rc;
}


static int JK_METHOD destroy(jk_worker_t **pThis, jk_logger_t *l)
{
    int rc;
    JK_TRACE_ENTER(l);
    rc = ajp_destroy(pThis, l, AJP13_PROTO);
    JK_TRACE_EXIT(l);
    return rc;
}


static int JK_METHOD get_endpoint(jk_worker_t *pThis,
                                  jk_endpoint_t **pend, jk_logger_t *l)
{
    int rc;
    JK_TRACE_ENTER(l);
    rc = ajp_get_endpoint(pThis, pend, l, AJP13_PROTO);
    JK_TRACE_EXIT(l);
    return rc;
}

int JK_METHOD ajp13_worker_factory(jk_worker_t **w,
                                   const char *name, jk_logger_t *l)
{
    ajp_worker_t *aw;

    JK_TRACE_ENTER(l);
    if (name == NULL || w == NULL) {
        JK_LOG_NULL_PARAMS(l);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    
    aw = (ajp_worker_t *) calloc(1, sizeof(ajp_worker_t));
    if (!aw) {
        jk_log(l, JK_LOG_ERROR,
               "malloc of private_data failed\n");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    aw->name = strdup(name);

    if (!aw->name) {
        free(aw);
        jk_log(l, JK_LOG_ERROR, "malloc failed\n");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    aw->proto = AJP13_PROTO;
    aw->login = NULL;

    aw->ep_cache_sz = 0;
    aw->ep_cache = NULL;
    aw->connect_retry_attempts = AJP_DEF_RETRY_ATTEMPTS;
    aw->worker.worker_private = aw;

    aw->worker.validate = validate;
    aw->worker.init = init;
    aw->worker.get_endpoint = get_endpoint;
    aw->worker.destroy = destroy;
    aw->worker.retries = JK_RETRIES;

    aw->logon = NULL;           /* No Logon on AJP13 */

    *w = &aw->worker;
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}
