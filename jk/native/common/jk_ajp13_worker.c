/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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
    JK_TRACE_ENTER(l);

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
    if (ajp_worker_factory(w, name, l) == JK_FALSE)
        return 0;

    aw = (*w)->worker_private;
    aw->proto = AJP13_PROTO;

    aw->worker.validate = validate;
    aw->worker.init = init;
    aw->worker.get_endpoint = get_endpoint;
    aw->worker.destroy = destroy;

    JK_TRACE_EXIT(l);
    return JK_AJP13_WORKER_TYPE;
}
