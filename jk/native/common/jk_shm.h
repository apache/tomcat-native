/*
 *  Copyright 1999-2005 The Apache Software Foundation
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
 * Description: Shared Memory object header file                           *
 * Author:      Mladen Turk <mturk@jboss.com>                              *
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
#define JK_SHM_MINOR    '0'
#define JK_SHM_STR_SIZ  63
#define JK_SHM_URI_SIZ  127
#define JK_SHM_DYNAMIC  16
#define JK_SHM_MAGIC    '!', 'J', 'K', 'S', 'H', 'M', JK_SHM_MAJOR, JK_SHM_MINOR

/* Really huge numbers, but 512 workers should be enough */
#define JK_SHM_MAX_WORKERS  512
#define JK_SHM_DEF_SIZE     (1024 * 1024)
#define JK_SHM_ALIGN(x)     JK_ALIGN(x, 1024)

/** jk shm worker record structure */
struct jk_shm_worker
{
    int     id;
    /* Number of currently busy channels */
    int     busy;
    /* worker name */
    char    name[JK_SHM_STR_SIZ+1];
    /* worker domain */
    char    domain[JK_SHM_STR_SIZ+1];
    /* worker redirect route */
    char    redirect[JK_SHM_STR_SIZ+1];
    /* current status of the worker */
    int     is_disabled;
    int     is_busy;
    /* Current lb factor */
    int     lb_factor;
    /* Current lb value  */
    int     lb_value;
    int     in_error_state;
    int     in_recovering;
    int     sticky_session;
    int     sticky_session_force;
    int     recover_wait_time;
    int     retries;
    /* Statistical data */
    time_t  error_time;
    /* Number of bytes read from remote */
    size_t  readed;
    /* Number of bytes transferred to remote */
    size_t  transferred;
    /* Number of times the worker was elected */
    size_t  elected;
    /* Number of non 200 responses */
    size_t  errors;
};
typedef struct jk_shm_worker jk_shm_worker_t;

/** jk shm uri worker map record structure */
struct jk_shm_urimap
{
    int          id;
    int          worker_id;
    unsigned int match_type;
};
typedef struct jk_shm_urimap jk_shm_urimap_t;

const char *jk_shm_name();

/* Open the shared memory creating file if needed
 */
int jk_shm_open(const char *fname, size_t sz, jk_logger_t *l);

/* Close the shared memory
 */
void jk_shm_close();

/* Attach the shared memory in child process.
 * File has to be opened in parent.
 */
int jk_shm_attach(const char *fname, size_t sz, jk_logger_t *l);

/* allocate shm memory
 * If there is no shm present the pool will be used instead
 */
void *jk_shm_alloc(jk_pool_t *p, size_t size);

/* allocate shm worker record
 * If there is no shm present the pool will be used instead
 */
jk_shm_worker_t *jk_shm_alloc_worker(jk_pool_t *p);

/* allocate shm uri worker map record
 * If there is no shm present the pool will be used instead
 */
jk_shm_urimap_t *jk_shm_alloc_urimap(jk_pool_t *p);

/* Return workers.properties last modified time
 */
time_t jk_shm_get_workers_time();

/* Set workers.properties last modified time
 */
void jk_shm_set_workers_time(time_t t);

/* Check if the shared memory has been modified
 * by some other process.
 */
int jk_shm_is_modified();

/* Synchronize access and modification time.
 * This function should be called when the shared memory
 * is modified and after we update the config acording
 * to the current shared memory data.
 */
void jk_shm_sync_access_time();


/* Lock shared memory for thread safe access */
int jk_shm_lock();

/* Unlock shared memory for thread safe access */
int jk_shm_unlock();


#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* _JK_SHM_H */
