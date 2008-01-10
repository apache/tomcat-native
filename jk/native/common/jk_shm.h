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
 * Description: Shared Memory object header file                           *
 * Author:      Mladen Turk <mturk@jboss.com>                              *
 * Author:      Rainer Jung <rjung@apache.org>                             *
 * Version:     $Revision$                                           *
 ***************************************************************************/
#ifndef _JK_SHM_H
#define _JK_SHM_H

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_util.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

/**
 * @file jk_shm.h
 * @brief Jk shared memory management
 *
 *
 */

#define JK_SHM_MAJOR    '1'
#define JK_SHM_MINOR    '3'
#define JK_SHM_STR_SIZ  63
#define JK_SHM_URI_SIZ  127
#define JK_SHM_DYNAMIC  16
#define JK_SHM_MAGIC    '!', 'J', 'K', 'S', 'H', 'M', JK_SHM_MAJOR, JK_SHM_MINOR
#define JK_SHM_MAGIC_SIZ  8

/* Really huge numbers, but 64 workers should be enough */
#define JK_SHM_MAX_WORKERS       64
#define JK_SHM_ALIGNMENT         64
#define JK_SHM_ALIGN(x)          JK_ALIGN((x), JK_SHM_ALIGNMENT)
#define JK_SHM_AJP13_WORKER_SIZE JK_SHM_ALIGN(sizeof(jk_shm_ajp13_worker_t))
#define JK_SHM_LB_WORKER_SIZE    JK_SHM_ALIGN(sizeof(jk_shm_lb_worker_t))
#define JK_SHM_AJP13_SIZE(x)     ((x) * JK_SHM_AJP13_WORKER_SIZE)
#define JK_SHM_LB_SIZE(x)        ((x) * JK_SHM_LB_WORKER_SIZE)
#define JK_SHM_DEF_SIZE          JK_SHM_AJP13_SIZE(JK_SHM_MAX_WORKERS)

/** jk shm ajp13 worker record structure */
struct jk_shm_ajp13_worker
{
    int     id;
    int     type;
    /* worker name */
    char    name[JK_SHM_STR_SIZ+1];
    /* Sequence counter starting at 0 and increasing
     * every time we change the config
     */
    volatile unsigned int sequence;
    /* Number of currently busy channels */
    volatile int busy;
    /* Maximum number of busy channels */
    volatile int max_busy;
    /* route */
    char    route[JK_SHM_STR_SIZ+1];
    /* worker domain */
    char    domain[JK_SHM_STR_SIZ+1];
    /* worker redirect route */
    char    redirect[JK_SHM_STR_SIZ+1];
    /* worker distance */
    volatile int distance;
    /* current activation state (config) of the worker */
    volatile int activation;
    /* current error state (runtime) of the worker */
    volatile int state;
    /* Current lb factor */
    volatile int lb_factor;
    /* Current lb reciprocal factor */
    volatile jk_uint64_t lb_mult;
    /* Current lb value  */
    volatile jk_uint64_t lb_value;
    /* Statistical data */
    volatile time_t  error_time;
    /* Number of bytes read from remote */
    volatile jk_uint64_t readed;
    /* Number of bytes transferred to remote */
    volatile jk_uint64_t transferred;
    /* Number of times the worker was elected */
    volatile jk_uint64_t  elected;
    /* Number of times the worker was elected - snapshot during maintenance */
    volatile jk_uint64_t  elected_snapshot;
    /* Number of non 200 responses */
    volatile jk_uint32_t  errors;
    /* Decayed number of reply_timeout errors */
    volatile jk_uint32_t  reply_timeouts;
    /* Number of client errors */
    volatile jk_uint32_t  client_errors;
};
typedef struct jk_shm_ajp13_worker jk_shm_ajp13_worker_t;

/** jk shm lb worker record structure */
struct jk_shm_lb_worker
{
    int     id;
    int     type;
    /* worker name */
    char    name[JK_SHM_STR_SIZ+1];
    /* Sequence counter starting at 0 and increasing
     * every time we change the config
     */
    volatile unsigned int sequence;
    /* Number of currently busy channels */
    volatile int busy;
    /* Maximum number of busy channels */
    volatile int max_busy;
    int     sticky_session;
    int     sticky_session_force;
    int     recover_wait_time;
    int     max_reply_timeouts;
    int     retries;
    int     lbmethod;
    int     lblock;
    /* Service transfer rate time */
    volatile time_t  last_maintain_time;
};
typedef struct jk_shm_lb_worker jk_shm_lb_worker_t;

const char *jk_shm_name(void);

/* Calculate needed shm size */
size_t jk_shm_calculate_size(jk_map_t *init_data, jk_logger_t *l);

/* Open the shared memory creating file if needed
 */
int jk_shm_open(const char *fname, size_t sz, jk_logger_t *l);

/* Close the shared memory
 */
void jk_shm_close(void);

/* Attach the shared memory in child process.
 * File has to be opened in parent.
 */
int jk_shm_attach(const char *fname, size_t sz, jk_logger_t *l);

/* allocate shm memory
 * If there is no shm present the pool will be used instead
 */
void *jk_shm_alloc(jk_pool_t *p, size_t size);

/* allocate shm ajp13 worker record
 * If there is no shm present the pool will be used instead
 */
jk_shm_ajp13_worker_t *jk_shm_alloc_ajp13_worker(jk_pool_t *p);

/* allocate shm lb worker record
 * If there is no shm present the pool will be used instead
 */
jk_shm_lb_worker_t *jk_shm_alloc_lb_worker(jk_pool_t *p);

/* Return workers.properties last modified time
 */
time_t jk_shm_get_workers_time(void);

/* Set workers.properties last modified time
 */
void jk_shm_set_workers_time(time_t t);

/* Check if the shared memory has been modified
 * by some other process.
 */
int jk_shm_is_modified(void);

/* Synchronize access and modification time.
 * This function should be called when the shared memory
 * is modified and after we update the config acording
 * to the current shared memory data.
 */
void jk_shm_sync_access_time(void);


/* Lock shared memory for thread safe access */
int jk_shm_lock(void);

/* Unlock shared memory for thread safe access */
int jk_shm_unlock(void);


#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* _JK_SHM_H */
