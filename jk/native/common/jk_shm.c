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
 * Description: Shared Memory support                                      *
 * Author:      Mladen Turk <mturk@jboss.com>                              *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_shm.h"

const char shm_signature[] = { JK_SHM_MAGIC };

#if defined (WIN32) || defined(NETWARE)

/* Simulate file */
static jk_shm_t *jk_global_shm = NULL;

/* Use plain memory */
int jk_shm_open(const char *fname, int workers, int dynamic, jk_shm_t *shm)
{
    jk_shm_h_rec_t *hdr;

    if (jk_global_shm) {
        shm->attached = jk_global_shm->attached;
        shm->size = jk_global_shm->size;
        shm->base = jk_global_shm->base;
        shm->filename = jk_global_shm->filename;
        return 0;
    }
    if (workers < 1 || workers > JK_SHM_MAX_WORKERS)
        workers = JK_SHM_MAX_WORKERS;
    if (dynamic < 1 || dynamic > JK_SHM_MAX_WORKERS)
        dynamic = JK_SHM_MAX_WORKERS;

    shm->size = sizeof(jk_shm_h_rec_t) + (workers + dynamic) * sizeof(jk_shm_w_rec_t);

    shm->base = calloc(1, shm->size);
    if (!shm->base)
        return -1;
    shm->filename = "memory";
    shm->fd = -1;
    shm->attached = 0;

    hdr = (jk_shm_h_rec_t *)shm->base;

    memcpy(hdr->magic, shm_signature, 8);
    hdr->workers = workers;
    hdr->dynamic = dynamic;

    jk_global_shm = shm;
    return 0;
}

int jk_shm_attach(const char *fname, int workers, int dynamic, jk_shm_t *shm)
{
    if (!jk_shm_open(fname, workers, dynamic, shm)) {
        shm->attached = 1;
        return 0;
    }
    else
        return -1;
}

void jk_shm_close(jk_shm_t *shm)
{
    if (shm) {
        if (shm->base)
            free(shm->base);
        shm->base = NULL;
    }
}

#else

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>

#ifndef MAP_FAILED
#define MAP_FAILED  (-1)
#endif

#ifndef MAP_FILE
#define MAP_FILE    (0)
#endif

static int do_shm_open(const char *fname, int workers, int dynamic,
                       jk_shm_t *shm, int attached)
{
    int fd;
    int flags = O_RDWR;

    shm->size = 0;
    shm->base = NULL;

    if (workers < 1 || workers > JK_SHM_MAX_WORKERS)
        workers = JK_SHM_MAX_WORKERS;
    if (dynamic < 1 || dynamic > JK_SHM_MAX_WORKERS)
        dynamic = JK_SHM_MAX_WORKERS;

    shm->filename = fname;
    shm->attached = attached;
    shm->size = sizeof(jk_shm_h_rec_t) + (workers + dynamic) * sizeof(jk_shm_w_rec_t);

    /* Use plain memory in case there is no file name */
    if (!fname) {
        jk_shm_h_rec_t *hdr;
        shm->base = calloc(1, shm->size);
        if (!shm->base)
            return -1;
        hdr = (jk_shm_h_rec_t *)shm->base;

        memcpy(hdr->magic, shm_signature, 8);
        hdr->workers = workers;
        hdr->dynamic = dynamic;
        shm->filename = "memory";
        return 0;
    }
    if (!attached)
        flags |= (O_CREAT|O_TRUNC);
    fd = open(fname, flags, 0666);
    if (fd == -1) {
        shm->size = 0;
        return -1;
    }

    if (!attached) {
        size_t size = lseek(fd, 0, SEEK_END);
        if (size < shm->size) {
            size = shm->size;
            if (ftruncate(fd, shm->size)) {
                close(fd);
                shm->size = 0;
                return -1;
            }
        }
    }
    if (lseek(fd, 0, SEEK_SET) != 0) {
        close(fd);
        shm->size = 0;
        return -1;
    }

    shm->base = mmap(NULL, shm->size,
                     PROT_READ | PROT_WRITE,
                     MAP_FILE | MAP_SHARED,
                     fd, 0);
    if (shm->base == (void *)MAP_FAILED) {
        shm->base = NULL;
    }
    if (!shm->base) {
        close(fd);
        shm->size = 0;
        return -1;
    }
    shm->fd = fd;

    /* Clear shared memory */
    if (!attached) {
        jk_shm_h_rec_t *hdr;

        memset(shm->base, 0, shm->size);
        hdr = (jk_shm_h_rec_t *)shm->base;

        memcpy(hdr->magic, shm_signature, 8);
        hdr->workers = workers;
        hdr->dynamic = dynamic;
    }
    else {
        /* TODO: check header magic */
    }

    return 0;
}

int jk_shm_open(const char *fname, int workers, int dynamic, jk_shm_t *shm)
{
    return do_shm_open(fname, workers, dynamic, shm, 0);
}

int jk_shm_attach(const char *fname, int workers, int dynamic, jk_shm_t *shm)
{
    return do_shm_open(fname, workers, dynamic, shm, 1);
}

void jk_shm_close(jk_shm_t *shm)
{
    if (shm) {
        if (shm->base) {
            if (shm->fd >= 0) {
                munmap(shm->base, shm->size);
                close(shm->fd);
            }
            else
                free(shm->base);
        }
        shm->base = NULL;
        shm->fd = -1;
    }
}


#endif

jk_shm_h_rec_t *jk_shm_header(jk_shm_t *shm)
{
    if (shm && shm->base) {
        return (jk_shm_h_rec_t *)shm->base;
    }
    else
        return NULL;
}

jk_shm_w_rec_t *jk_shm_worker(jk_shm_t *shm, int id)
{
    jk_shm_w_rec_t *w = NULL;
    if (shm && shm->base && id >= 0) {
        jk_shm_h_rec_t *hdr = (jk_shm_h_rec_t *)shm->base;
        if (id < (hdr->workers + hdr->dynamic)) {
            unsigned char *base = (unsigned char *)shm->base;
            base += (sizeof(jk_shm_h_rec_t) + id * sizeof(jk_shm_w_rec_t));
            w = (jk_shm_w_rec_t *)base;
            /* Check worker id */
            if (id < hdr->workers && id != w->id)
                w = NULL;
        }

    }
    return w;
}
