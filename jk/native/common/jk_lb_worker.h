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

#define JK_LB_METHOD_REQUESTS          (1)
#define JK_LB_METHOD_TRAFFIC           (2)
#define JK_LB_METHOD_BUSYNESS          (3)
#define JK_LB_METHOD_DEF               (JK_LB_METHOD_REQUESTS)
#define JK_LB_METHOD_TEXT_REQUESTS     ("Request")
#define JK_LB_METHOD_TEXT_TRAFFIC      ("Traffic")
#define JK_LB_METHOD_TEXT_BUSYNESS     ("Busyness")
#define JK_LB_METHOD_TEXT_DEF          (JK_LB_METHOD_TEXT_REQUESTS)
#define JK_LB_LOCK_OPTIMISTIC          (1)
#define JK_LB_LOCK_PESSIMISTIC         (2)
#define JK_LB_LOCK_DEF                 (JK_LB_LOCK_OPTIMISTIC)
#define JK_LB_LOCK_TEXT_OPTIMISTIC     ("Optimistic")
#define JK_LB_LOCK_TEXT_PESSIMISTIC    ("Pessimistic")
#define JK_LB_LOCK_TEXT_DEF            (JK_LB_LOCK_TEXT_OPTIMISTIC)
#define JK_LB_STATE_NA                 (1)
#define JK_LB_STATE_OK                 (2)
#define JK_LB_STATE_RECOVER            (3)
#define JK_LB_STATE_BUSY               (4)
#define JK_LB_STATE_ERROR              (5)
#define JK_LB_STATE_TEXT_NA            ("N/A")
#define JK_LB_STATE_TEXT_OK            ("OK")
#define JK_LB_STATE_TEXT_RECOVER       ("REC")
#define JK_LB_STATE_TEXT_BUSY          ("BSY")
#define JK_LB_STATE_TEXT_ERROR         ("ERR")
#define JK_LB_ACTIVATION_ACTIVE        (1)
#define JK_LB_ACTIVATION_DISABLED      (2)
#define JK_LB_ACTIVATION_STOPPED       (3)
#define JK_LB_ACTIVATION_TEXT_ACTIVE   ("ACT")
#define JK_LB_ACTIVATION_TEXT_DISABLED ("DIS")
#define JK_LB_ACTIVATION_TEXT_STOPPED  ("STP")

#define JK_LB_UINT64_STR_SZ          (21)
#define JK_LB_NOTES_COUNT            (9)
#define JK_NOTE_LB_FIRST_NAME        ("JK_LB_FIRST_NAME")
#define JK_NOTE_LB_FIRST_VALUE       ("JK_LB_FIRST_VALUE")
#define JK_NOTE_LB_FIRST_ACCESSED    ("JK_LB_FIRST_ACCESSED")
#define JK_NOTE_LB_FIRST_READ        ("JK_LB_FIRST_READ")
#define JK_NOTE_LB_FIRST_TRANSFERRED ("JK_LB_FIRST_TRANSFERRED")
#define JK_NOTE_LB_FIRST_ERRORS      ("JK_LB_FIRST_ERRORS")
#define JK_NOTE_LB_FIRST_BUSY        ("JK_LB_FIRST_BUSY")
#define JK_NOTE_LB_FIRST_ACTIVATION  ("JK_LB_FIRST_ACTIVATION")
#define JK_NOTE_LB_FIRST_STATE       ("JK_LB_FIRST_STATE")
#define JK_NOTE_LB_LAST_NAME         ("JK_LB_LAST_NAME")
#define JK_NOTE_LB_LAST_VALUE        ("JK_LB_LAST_VALUE")
#define JK_NOTE_LB_LAST_ACCESSED     ("JK_LB_LAST_ACCESSED")
#define JK_NOTE_LB_LAST_READ         ("JK_LB_LAST_READ")
#define JK_NOTE_LB_LAST_TRANSFERRED  ("JK_LB_LAST_TRANSFERRED")
#define JK_NOTE_LB_LAST_ERRORS       ("JK_LB_LAST_ERRORS")
#define JK_NOTE_LB_LAST_BUSY         ("JK_LB_LAST_BUSY")
#define JK_NOTE_LB_LAST_ACTIVATION   ("JK_LB_LAST_ACTIVATION")
#define JK_NOTE_LB_LAST_STATE        ("JK_LB_LAST_STATE")

/* Minimal time in ms to wait between get_endpoint retries for balanced workers */
#define JK_LB_MIN_RETRY_WAIT  (25)
/* Maximal time in ms to wait between get_endpoint retries for balanced workers */
#define JK_LB_MAX_RETRY_WAIT  (100)
/* Time to wait before retry. */
#define WAIT_BEFORE_RECOVER   (60)
/* We accept doing global maintenance if we are */
/* JK_LB_MAINTAIN_TOLERANCE seconds early. */
#define JK_LB_MAINTAIN_TOLERANCE (2)
/* We divide load values by 2^x during global maintenance. */
/* The exponent x is JK_LB_DECAY_MULT*#MAINT_INTV_SINCE_LAST_MAINT */
#define JK_LB_DECAY_MULT         (1)

static const char *lb_locking_type[] = {
    "unknown",
    JK_LB_LOCK_TEXT_OPTIMISTIC,
    JK_LB_LOCK_TEXT_PESSIMISTIC,
    NULL
};

static const char *lb_method_type[] = {
    "unknown",
    JK_LB_METHOD_TEXT_REQUESTS,
    JK_LB_METHOD_TEXT_TRAFFIC,
    JK_LB_METHOD_TEXT_BUSYNESS,
    NULL
};

static const char *lb_state_type[] = {
    "unknown",
    JK_LB_STATE_TEXT_NA,
    JK_LB_STATE_TEXT_OK,
    JK_LB_STATE_TEXT_RECOVER,
    JK_LB_STATE_TEXT_BUSY,
    JK_LB_STATE_TEXT_ERROR,
    NULL
};

static const char *lb_activation_type[] = {
    "unknown",
    JK_LB_ACTIVATION_TEXT_ACTIVE,
    JK_LB_ACTIVATION_TEXT_DISABLED,
    JK_LB_ACTIVATION_TEXT_STOPPED,
    NULL
};

struct worker_record
{
    jk_worker_t      *w;
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
