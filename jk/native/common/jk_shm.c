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

/** jk shm header record structure */
struct jk_shm_h_rec
{
    /* Shared memory magic JK_SHM_MAGIC */
    char            magic[8];
    int             workers;
    int             allocated;
    jk_shm_w_rec_t  *pr[1];
    jk_shm_w_rec_t  rr[1];
};
typedef struct jk_shm_h_rec jk_shm_h_rec_t;

/** jk shm structure */
struct jk_shm
{
    size_t     size;
    const char *filename;
    int        fd;
    int        attached;
    jk_shm_h_rec_t  *hdr;
};

typedef struct jk_shm jk_shm_t;

static const char shm_signature[] = { JK_SHM_MAGIC };
static jk_shm_t jk_shmem = { 0, NULL, -1, 0, NULL};;

#if defined (WIN32) || defined(NETWARE)

/* Use plain memory */
int jk_shm_open(const char *fname)
{
    if (jk_shmem.hdr) {
        return 0;
    }

    jk_shmem.size =  JK_SHM_ALIGN(sizeof(jk_shm_h_rec_t) + JK_SHM_MAX_WORKERS * sizeof(jk_shm_w_rec_t));

    jk_shmem.hdr = (jk_shm_h_rec_t *)calloc(1, jk_shmem.size);
    if (!jk_shmem.hdr)
        return -1;
    jk_shmem.filename = "memory";
    jk_shmem.fd       = 0;
    jk_shmem.attached = 0;

    memcpy(jk_shmem.hdr->magic, shm_signature, 8);
    jk_shmem.hdr->workers = JK_SHM_MAX_WORKERS;
    return 0;
}

int jk_shm_attach(const char *fname)
{
    if (!jk_shm_open(fname)) {
        jk_shmem.attached = 1;
        return 0;
    }
    else
        return -1;
}

void jk_shm_close()
{
    if (jk_shmem.hdr) {
        free(jk_shmem.hdr);
    }
    jk_shmem.hdr = NULL;
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

static int do_shm_open(const char *fname, int attached)
{
    int rc;
    int fd;
    int flags = O_RDWR;
    void *base;

    if (jk_shmem.hdr)
        jk_shm_close();
    jk_shmem.filename = fname;
    jk_shmem.attached = attached;

    /* Use plain memory in case there is no file name */
    if (!fname) {
        jk_shmem.size = JK_SHM_ALIGN(sizeof(jk_shm_h_rec_t) + JK_SHM_MAX_WORKERS * sizeof(jk_shm_w_rec_t *));
        jk_shmem.hdr  = calloc(1, jk_shmem.size);
        if (!jk_shmem.hdr)
            return -1;
        memcpy(jk_shmem.hdr->magic, shm_signature, 8);
        jk_shmem.hdr->workers   = JK_SHM_MAX_WORKERS;
        jk_shmem.hdr->allocated = 0;
        jk_shmem.filename       = "memory";
        return 0;
    }
    
    jk_shmem.size = JK_SHM_ALIGN(sizeof(jk_shm_h_rec_t) + JK_SHM_MAX_WORKERS * sizeof(jk_shm_w_rec_t));
    
    if (!attached)
        flags |= (O_CREAT|O_TRUNC);
    fd = open(fname, flags, 0666);
    if (fd == -1) {
        jk_shmem.size = 0;
        return errno;
    }

    if (!attached) {
        size_t size = lseek(fd, 0, SEEK_END);
        if (size < jk_shmem.size) {
            size = jk_shmem.size;
            if (ftruncate(fd, jk_shmem.size)) {
                rc = errno;
                close(fd);
                jk_shmem.size = 0;
                return rc;
            }
        }
    }
    if (lseek(fd, 0, SEEK_SET) != 0) {
        rc = errno;
        close(fd);
        jk_shmem.size = 0;
        return rc;
    }

    base = mmap(NULL, jk_shmem.size,
                PROT_READ | PROT_WRITE,
                MAP_FILE | MAP_SHARED,
                fd, 0);
    if (base == (void *)MAP_FAILED || base == NULL) {
        rc = errno;
        close(fd);
        jk_shmem.size = 0;
        return rc;
    }
    jk_shmem.hdr = base;
    jk_shmem.fd  = fd;

    /* Clear shared memory */
    if (!attached) {
        memset(jk_shmem.hdr, 0, jk_shmem.size);
        memcpy(jk_shmem.hdr->magic, shm_signature, 8);
        jk_shmem.hdr->workers = JK_SHM_MAX_WORKERS;
    }
    else {
        /* TODO: check header magic */
    }
    return 0;
}

int jk_shm_open(const char *fname)
{
    return do_shm_open(fname, 0);
}

int jk_shm_attach(const char *fname)
{
    return do_shm_open(fname, 1);
}

void jk_shm_close()
{
    if (jk_shmem.hdr) {
        if (jk_shmem.fd >= 0) {
            munmap((void *)jk_shmem.hdr, jk_shmem.size);
            close(jk_shmem.fd);
        }
        else {
            int i;
            for (i = 0; i < jk_shmem.hdr->allocated; i++) {
                if (jk_shmem.hdr->pr[i])
                    free(jk_shmem.hdr->pr[i]);
            }
            free(jk_shmem.hdr);
        }
    }
    jk_shmem.hdr = NULL;
    jk_shmem.fd  = -1;
}


#endif

jk_shm_w_rec_t *jk_shm_worker(int id)
{
    jk_shm_w_rec_t *w = NULL;
    if (jk_shmem.hdr && id >= 0) {
        if (id < jk_shmem.hdr->allocated) {
            if (jk_shmem.fd >= 0)
                w = &(jk_shmem.hdr->rr[id]);
            else
                w = jk_shmem.hdr->pr[id];
            if (id != w->id)
                w = NULL;            
        }
    }
    return w;
}

jk_shm_w_rec_t *jk_shm_worker_alloc()
{
    jk_shm_w_rec_t *w = NULL;
    if (jk_shmem.hdr) {
        if (jk_shmem.hdr->allocated < jk_shmem.hdr->workers) {
            jk_shmem.hdr->allocated++;
            if (jk_shmem.fd >= 0) {
                w = &(jk_shmem.hdr->rr[jk_shmem.hdr->allocated]);
            }
            else {
                jk_shmem.hdr->pr[jk_shmem.hdr->allocated] = calloc(1, sizeof(jk_shm_w_rec_t));
                w = jk_shmem.hdr->pr[jk_shmem.hdr->allocated];
            }
            w->id = jk_shmem.hdr->allocated;
        }
    }
    return w;
}

const char *jk_shm_name()
{
    return jk_shmem.filename;
}
