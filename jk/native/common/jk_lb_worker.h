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
 * Description: load balance worker header file                            *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_LB_WORKER_H
#define JK_LB_WORKER_H

#include "jk_logger.h"
#include "jk_service.h"
#include "jk_mt.h"
#include "jk_shm.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

#define JK_LB_WORKER_NAME     ("lb")
#define JK_LB_WORKER_TYPE     (5)
#define JK_LB_DEF_DOMAIN_NAME ("unknown")

struct worker_record
{
    jk_worker_t     *w;
    /* Shared memory worker data */
    jk_shm_worker_t  *s;
};
typedef struct worker_record worker_record_t;

struct lb_worker
{
    worker_record_t *lb_workers;
    unsigned num_of_workers;
    unsigned num_of_local_workers;

    jk_pool_t p;
    jk_pool_atom_t buf[TINY_POOL_SIZE];

    jk_worker_t worker;
    JK_CRIT_SEC cs; 

    /* Shared memory worker data */
    jk_shm_worker_t  *s;
};
typedef struct lb_worker lb_worker_t;

int JK_METHOD lb_worker_factory(jk_worker_t **w,
                                const char *name, jk_logger_t *l);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* JK_LB_WORKER_H */
