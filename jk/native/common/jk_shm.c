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
jk_shm_t *jk_shm_open(const char *fname, int workers, int dynamic, jk_pool_t *p)
{
    jk_shm_t *shm;
    jk_shm_h_rec_t *hdr;

    if (jk_global_shm)
        return jk_global_shm;
    shm = jk_pool_alloc(p, sizeof(jk_shm_t));

    shm->size = sizeof(jk_shm_h_rec_t) + (workers + dynamic) * sizeof(jk_shm_w_rec_t);

    shm->base = calloc(1, shm->size);
    if (!shm->base)
        return NULL;
    shm->filename = fname;
    shm->fd = -1;
    shm->attached = 0;

    hdr = (jk_shm_h_rec_t *)shm->base;

    memcpy(hdr->magic, shm_signature, 8);
    hdr->workers = workers;
    hdr->dynamic = dynamic;

    jk_global_shm = shm;
    return shm;
}

jk_shm_t *jk_shm_attach(const char *fname, int workers, int dynamic, jk_pool_t *p)
{
    jk_shm_t *shm = jk_shm_open(fname, workers, dynamic, p);

    if (shm) {
        shm->attached = 1;
    }
    return shm;
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

static jk_shm_t *do_shm_open(const char *fname, int workers, int dynamic,
                             jk_pool_t *p, int attached)
{
    jk_shm_t *shm;
    int fd;
    int flags = O_RDWR;

    if (!attached)
        flags |= (O_CREAT|O_TRUNC);
    fd = open(fname, flags, 0666);
    if (fd == -1)
        return NULL;
    shm = jk_pool_alloc(p, sizeof(jk_shm_t));
    shm->size = sizeof(jk_shm_h_rec_t) + (workers + dynamic) * sizeof(jk_shm_w_rec_t);

    if (!attached) {
        size_t size = lseek(fd, 0, SEEK_END);
        if (size < shm->size) {
            size = shm->size;
            if (ftruncate(fd, shm->size)) {
                close(fd);
                return NULL;
            }
        }
    }
    if (lseek(fd, 0, SEEK_SET) != 0) {
        close(fd);
        return NULL;
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
        return NULL;
    }
    shm->filename = fname;
    shm->fd = fd;
    shm->attached = attached;

    /* Clear shared memory */
    if (!attached) {
        jk_shm_h_rec *hdr;

        memset(shm->base, 0, size);
        hdr = (jk_shm_h_rec_t *)shm->base;

        memcpy(hdr->magic, shm_signature, 8);
        hdr->workers = workers;
        hdr->dynamic = dynamic;
    }
    else {
        /* TODO: check header magic */
    }

    return shm;
}

jk_shm_t *jk_shm_open(const char *fname, int workers, int dynamic, jk_pool_t *p)
{
    return do_shm_open(fname, workers, dynamic, p, 0);
}

jk_shm_t *jk_shm_attach(const char *fname, int workers, int dynamic, jk_pool_t *p)
{
    return do_shm_open(fname, workers, dynamic, p, 1);
}

void jk_shm_close(jk_shm_t *shm)
{
    if (shm) {
        if (shm->base)
            munmap(shm->base, shm->size);
            close(shm->fd);
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
