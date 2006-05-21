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
 * Author:      Rainer Jung <rjung@apache.org>                             *
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

#define JK_LB_BYREQUESTS      (0)
#define JK_LB_BYTRAFFIC       (1)
#define JK_LB_BYBUSYNESS      (2)
#define JK_LB_METHOD_REQUESTS ("Request")
#define JK_LB_METHOD_TRAFFIC  ("Traffic")
#define JK_LB_METHOD_BUSYNESS ("Busyness")
#define JK_LB_LOCK_DEFAULT     (0)
#define JK_LB_LOCK_PESSIMISTIC (1)
#define JK_LB_LM_DEFAULT       ("Optimistic")
#define JK_LB_LM_PESSIMISTIC   ("Pessimistic")

/* Time to wait before retry. */
#define WAIT_BEFORE_RECOVER   (60)
/* We accept doing global maintenance if we are */
/* JK_LB_MAINTAIN_TOLERANCE seconds early. */
#define JK_LB_MAINTAIN_TOLERANCE (2)
/* We divide load values by 2^x during global maintenance. */
/* The exponent x is JK_LB_DECAY_MULT*#MAINT_INTV_SINCE_LAST_MAINT */
#define JK_LB_DECAY_MULT         (1)

static const char *lb_locking_type[] = {
    JK_LB_LM_DEFAULT,
    JK_LB_LM_PESSIMISTIC,
    "unknown",
    NULL
};

static const char *lb_method_type[] = {
    JK_LB_METHOD_REQUESTS,
    JK_LB_METHOD_TRAFFIC,
    JK_LB_METHOD_BUSYNESS,
    "unknown",
    NULL
};

struct worker_record
{
    jk_worker_t     *w;
    /* Shared memory worker data */
    jk_shm_worker_t  *s;
    /* Current jvmRoute. Can be name or domain */
    const char       *r;
};
typedef struct worker_record worker_record_t;

struct lb_worker
{
    worker_record_t *lb_workers;
    unsigned int num_of_workers;
    int          lbmethod;
    int          lblock;
    time_t       maintain_time;

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

void reset_lb_values(lb_worker_t *p, jk_logger_t *l);
void update_mult(lb_worker_t * p, jk_logger_t *l);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* JK_LB_WORKER_H */
