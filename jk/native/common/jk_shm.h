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

/* Really huge numbers, but 1024 workers should be enough */
#define JK_SHM_MAX_WORKERS  768
#define JK_SHM_MAX_DYNAMIC  256

/** jk shm structure */
struct jk_shm
{
    size_t     size;
    const char *filename;
    int        fd;
    void       *base;
    int        attached;
};

typedef struct jk_shm jk_shm_t;

/** jk shm header record structure */
struct jk_shm_h_rec
{
    /* Shared memory magic JK_SHM_MAGIC */
    char    magic[8];
    int     workers;
    int     dynamic;

    /* Align to 128 bytes */

    /* XXX:  the following should be #if sizeof(int) == 4 */
#if 1
    char    reserved[112];
#else
    char    reserved[104];
#endif
};

typedef struct jk_shm_h_rec jk_shm_h_rec_t;

/** jk shm worker record structure */
struct jk_shm_w_rec
{
    int     id;
    /* Number of currently busy channels */
    int     busy;
    /* Number of currently idle channels */
    int     idle;
    /* Maximum number of channels */
    int     max_conn;
    /* worker name */
    char    route[JK_SHM_STR_SIZ+1];
    /* worker domain */
    char    domain[JK_SHM_STR_SIZ+1];
    /* worker redirect route */
    char    redirect_route[JK_SHM_STR_SIZ+1];
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
    int     error_time;
    /* Number of bytes read from remote */
    int     readed;
    /* Number of bytes transferred to remote */
    int     transferred;
    /* Number of times the worker was elected */
    int     elected;
    /* Number of non 200 responses */
    int     errors;
    /* Align to 512 bytes */

    /* XXX:  the following should be #if sizeof(int) == 4 */
#if 1
    char    reserved[244];
#else
    char    reserved[168];
#endif

};

typedef struct jk_shm_w_rec jk_shm_w_rec_t;

/* Open the shared memory creating file if needed
 */
int jk_shm_open(const char *fname, int workers, int dynamic, jk_shm_t *shm);

/* Close the shared memory
 */
void jk_shm_close(jk_shm_t *shm);

/* Attach the shared memory in child process.
 * File has to be opened in parent.
 */
int jk_shm_attach(const char *fname, int workers, int dynamic, jk_shm_t *shm);


/* Return shm header record
 */
jk_shm_h_rec_t *jk_shm_header(jk_shm_t *shm);

/* Return shm worker record
 */
jk_shm_w_rec_t *jk_shm_worker(jk_shm_t *shm, int id);


#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* _JK_SHM_H */
