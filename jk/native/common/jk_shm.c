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
#include "jk_mt.h"
#include "jk_shm.h"

/** jk shm header record structure */
struct jk_shm_header
{
    /* Shared memory magic JK_SHM_MAGIC */
    char   magic[8];
    size_t size;
    size_t pos;
    unsigned int childs;
    unsigned int workers;

    unsigned int urimaps;
    time_t modified;
    char   buf[1];
};

typedef struct jk_shm_header jk_shm_header_t;

/** jk shm structure */
struct jk_shm
{
    size_t     size;
    const char *filename;
    int        fd;
    int        fd_lock;
    int        attached;
    jk_shm_header_t  *hdr;
    JK_CRIT_SEC       cs;
};

typedef struct jk_shm jk_shm_t;

static const char shm_signature[] = { JK_SHM_MAGIC };
static jk_shm_t jk_shmem = { 0, NULL, -1, -1, 0, NULL};
static time_t jk_workers_modified_time = 0;

#if defined (WIN32) || defined(NETWARE)

/* Use plain memory */
int jk_shm_open(const char *fname, jk_logger_t *l)
{
    int rc;
    JK_TRACE_ENTER(l);
    if (jk_shmem.hdr) {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG, "Shared memory is already opened");
        JK_TRACE_EXIT(l);
        return 0;
    }

    jk_shmem.size =  JK_SHM_ALIGN(sizeof(jk_shm_header_t) + JK_SHM_SIZE);

    jk_shmem.hdr = (jk_shm_header_t *)calloc(1, jk_shmem.size);
    if (!jk_shmem.hdr) {
        JK_TRACE_EXIT(l);
        return -1;
    }
    jk_shmem.filename = "memory";
    jk_shmem.fd       = 0;
    jk_shmem.attached = 0;
    memcpy(jk_shmem.hdr->magic, shm_signature, 8);
    jk_shmem.hdr->size = JK_SHM_SIZE;
    JK_INIT_CS(&(jk_shmem.cs), rc);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Initialized shared memory size=%u free=%u",
               jk_shmem.size, jk_shmem.hdr->size);
    JK_TRACE_EXIT(l);
    return 0;
}

int jk_shm_attach(const char *fname, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);
    if (!jk_shm_open(fname, l)) {
        jk_shmem.attached = 1;
        jk_shmem.hdr->childs++;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Attached shared memory [%d] size=%u free=%u",
                   jk_shmem.hdr->childs, jk_shmem.hdr->size,
                   jk_shmem.hdr->size - jk_shmem.hdr->pos);
        JK_TRACE_EXIT(l);
        return 0;
    }
    else {
        JK_TRACE_EXIT(l);
        return -1;
    }
}

void jk_shm_close()
{
    if (jk_shmem.hdr) {
        int rc;
        free(jk_shmem.hdr);
        JK_DELETE_CS(&(jk_shmem.cs), rc);
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

static int do_shm_open_lock(const char *fname, int attached, jk_logger_t *l)
{
    int rc;
    int fd;
    int flags = O_RDWR;
    char flkname[256];
    JK_TRACE_ENTER(l);

    jk_shmem.fd_lock = -1;
    strcpy(flkname, fname);
    strcat(flkname, ".lock");
    if (!attached)
        flags |= (O_CREAT|O_TRUNC);
    fd = open(flkname, flags, 0666);
    if (fd == -1) {
        JK_TRACE_EXIT(l);
        return errno;
    }

    if (!attached) {
        if (ftruncate(fd, 1)) {
            rc = errno;
            close(fd);
            JK_TRACE_EXIT(l);
            return rc;
         }
    }
    if (lseek(fd, 0, SEEK_SET) != 0) {
        rc = errno;
        close(fd);
        JK_TRACE_EXIT(l);
        return rc;
    }
    jk_shmem.fd_lock = fd;

    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Opened shared memory lock %s", flkname);
    JK_TRACE_EXIT(l);
    return 0;
}

static int do_shm_open(const char *fname, int attached, jk_logger_t *l)
{
    int rc;
    int fd;
    int flags = O_RDWR;
    void *base;

    JK_TRACE_ENTER(l);
    if (jk_shmem.hdr) {
        /* Probably a call from vhost */
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                    "Shared memory is already open");
        return 0;
    }
    jk_shmem.filename = fname;
    jk_shmem.attached = attached;

    jk_shmem.size = JK_SHM_ALIGN(sizeof(jk_shm_header_t) + JK_SHM_SIZE);

    /* Use plain memory in case there is no file name */
    if (!fname) {
        jk_shmem.filename  = "memory";
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Using process memory as shared memory");
        JK_TRACE_EXIT(l);
        return 0;
    }    
    
    if (!attached)
        flags |= (O_CREAT|O_TRUNC);
    fd = open(fname, flags, 0666);
    if (fd == -1) {
        jk_shmem.size = 0;
        JK_TRACE_EXIT(l);
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
                JK_TRACE_EXIT(l);
                return rc;
            }
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Truncated shared memory to %u", size);
        }
    }
    if (lseek(fd, 0, SEEK_SET) != 0) {
        rc = errno;
        close(fd);
        jk_shmem.size = 0;
        JK_TRACE_EXIT(l);
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
        JK_TRACE_EXIT(l);
        return rc;
    }
    jk_shmem.hdr = base;
    jk_shmem.fd  = fd;

    /* Clear shared memory */
    if (!attached) {
        memset(jk_shmem.hdr, 0, jk_shmem.size);
        memcpy(jk_shmem.hdr->magic, shm_signature, 8);
        jk_shmem.hdr->size = JK_SHM_SIZE;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Initialized shared memory size=%u free=%u",
                   jk_shmem.size, jk_shmem.hdr->size);
    }
    else {
        jk_shmem.hdr->childs++;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_INFO,
                   "Attached shared memory [%d] size=%u free=%u",
                   jk_shmem.hdr->childs, jk_shmem.hdr->size,
                   jk_shmem.hdr->size - jk_shmem.hdr->pos);
        /* TODO: check header magic */
    }
    JK_INIT_CS(&(jk_shmem.cs), rc);
    if ((rc = do_shm_open_lock(fname, attached, l))) {
        munmap((void *)jk_shmem.hdr, jk_shmem.size);
        close(jk_shmem.fd);
        jk_shmem.hdr = NULL;
        jk_shmem.fd  = -1;
        JK_TRACE_EXIT(l);
        return rc;
    }
    JK_TRACE_EXIT(l);
    return 0;
}

int jk_shm_open(const char *fname, jk_logger_t *l)
{
    return do_shm_open(fname, 0, l);
}

int jk_shm_attach(const char *fname, jk_logger_t *l)
{
    return do_shm_open(fname, 1, l);
}

void jk_shm_close()
{
    int rc;
    if (jk_shmem.hdr) {
        if (jk_shmem.fd_lock >= 0) {
            close(jk_shmem.fd_lock);
        }
        if (jk_shmem.fd >= 0) {
            munmap((void *)jk_shmem.hdr, jk_shmem.size);
            close(jk_shmem.fd);
        }
        jk_shmem.fd_lock = -1;
    }
    if (jk_shmem.size) {
        JK_DELETE_CS(&(jk_shmem.cs), rc);
    }
    jk_shmem.size = 0;
    jk_shmem.hdr  = NULL;
    jk_shmem.fd   = -1;
}


#endif

void *jk_shm_alloc(jk_pool_t *p, size_t size)
{
    void *rc = NULL;

    if (jk_shmem.hdr) {
        size = JK_ALIGN_DEFAULT(size);
        if ((jk_shmem.hdr->size - jk_shmem.hdr->pos) >= size) {
            rc = &(jk_shmem.hdr->buf[jk_shmem.hdr->pos]);
            jk_shmem.hdr->pos += size;
        }
    }
    else if (p)
        rc = jk_pool_alloc(p, size);

    return rc;
}

const char *jk_shm_name()
{
    return jk_shmem.filename;
}


time_t jk_shm_get_workers_time()
{
    if (jk_shmem.hdr)
        return jk_shmem.hdr->modified;
    else
        return jk_workers_modified_time;
}

void jk_shm_set_workers_time(time_t t)
{
    if (jk_shmem.hdr)
        jk_shmem.hdr->modified = t;
    else
        jk_workers_modified_time = t;
}

int jk_shm_lock()
{
    int rc;
    JK_ENTER_CS(&(jk_shmem.cs), rc);
    if (rc == JK_TRUE && jk_shmem.fd_lock != -1) {
        JK_ENTER_LOCK(jk_shmem.fd_lock, rc);
    }
    return rc;
}

int jk_shm_unlock()
{
    int rc;
    JK_LEAVE_CS(&(jk_shmem.cs), rc);
    if (rc == JK_TRUE && jk_shmem.fd_lock != -1) {
        JK_LEAVE_LOCK(jk_shmem.fd_lock, rc);
    }
    return rc;
}

jk_shm_worker_t *jk_shm_alloc_worker(jk_pool_t *p)
{
    jk_shm_worker_t *w = (jk_shm_worker_t *)jk_shm_alloc(p, sizeof(jk_shm_worker_t));
    if (w) {
        memset(w, 0, sizeof(jk_shm_worker_t));
        if (jk_shmem.hdr) {
            jk_shmem.hdr->workers++;
            w->id = jk_shmem.hdr->workers;
        }
        else
            w->id = -1;
    }
    return w;
}

jk_shm_urimap_t *jk_shm_alloc_urimap(jk_pool_t *p)
{
    jk_shm_urimap_t *u = (jk_shm_urimap_t *)jk_shm_alloc(p, sizeof(jk_shm_urimap_t));
    if (u) {
        memset(u, 0, sizeof(jk_shm_urimap_t));
        if (jk_shmem.hdr) {
            jk_shmem.hdr->urimaps++;
            u->id = jk_shmem.hdr->urimaps;
        }
        else
            u->id = -1;
    }
    return u;
}
