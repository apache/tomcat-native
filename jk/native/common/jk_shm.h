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
#define JK_SHM_DYNAMIC  16
#define JK_SHM_MAGIC    '!', 'J', 'K', 'S', 'H', 'M', JK_SHM_MAJOR, JK_SHM_MINOR

/* Really huge numbers, but 512 workers should be enough */
#define JK_SHM_MAX_WORKERS  512
#define JK_SHM_SIZE         (1024 * 1024)
#define JK_SHM_ALIGN(x)  JK_ALIGN(x, 1024)

/** jk shm worker record structure */
struct jk_shm_worker
{
    int     id;
    /* Number of currently busy channels */
    int     busy;
    /* Number of currently idle channels */
    int     idle;
    /* Maximum number of channels */
    int     max_conn;
    /* worker name */
    char    name[JK_SHM_STR_SIZ+1];
    /* worker domain */
    char    domain[JK_SHM_STR_SIZ+1];
    /* worker redirect route */
    char    redirect[JK_SHM_STR_SIZ+1];
    /* current status of the worker */
    int     status;
    /* Current lb factor */
    int     lb_factor;
    /* Current lb value  */
    int     lb_value;
    int     is_local_worker;
    int     is_local_domain;
    int     in_error_state;
    int     in_recovering;
    int     in_local_worker_mode;
    int     local_worker_only;
    int     sticky_session;
    int     recover_wait_time;
    time_t  error_time;
    /* Number of bytes read from remote */
    size_t   readed;
    /* Number of bytes transferred to remote */
    size_t   transferred;
    /* Number of times the worker was elected */
    size_t   elected;
    /* Number of non 200 responses */
    size_t   errors;
};
typedef struct jk_shm_worker jk_shm_worker_t;

const char *jk_shm_name();

/* Open the shared memory creating file if needed
 */
int jk_shm_open(const char *fname);

/* Close the shared memory
 */
void jk_shm_close();

/* Attach the shared memory in child process.
 * File has to be opened in parent.
 */
int jk_shm_attach(const char *fname);

/* allocate shm memory
 * If there is no shm present the pool will be used instead
 */
void *jk_shm_alloc(jk_pool_t *p, size_t size);


#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* _JK_SHM_H */
