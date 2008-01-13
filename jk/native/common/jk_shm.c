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
 * Description: Shared Memory support                                      *
 * Author:      Mladen Turk <mturk@jboss.com>                              *
 * Author:      Rainer Jung <rjung@apache.org>                             *
 * Version:     $Revision$                                        *
 ***************************************************************************/

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_util.h"
#include "jk_mt.h"
#include "jk_lb_worker.h"
#include "jk_ajp13_worker.h"
#include "jk_shm.h"

/** jk shm header core data structure */
struct jk_shm_header_data
{
    /* Shared memory magic JK_SHM_MAGIC */
    char   magic[JK_SHM_MAGIC_SIZ];
    size_t size;
    size_t pos;
    unsigned int childs;
    unsigned int workers;
    time_t modified;
};

typedef struct jk_shm_header_data jk_shm_header_data_t;

/** jk shm header record structure */
struct jk_shm_header
{
    union {
        jk_shm_header_data_t data;
        char alignbuf[JK_SHM_ALIGN(sizeof(jk_shm_header_data_t))];
    } h;
    char   buf[1];
};

typedef struct jk_shm_header jk_shm_header_t;

/** jk shm structure */
struct jk_shm
{
    size_t     size;
    unsigned   ajp13_workers;
    unsigned   lb_workers;
    char       *filename;
    char       *lockname;
    int        fd;
    int        fd_lock;
    int        attached;
    jk_shm_header_t  *hdr;
    JK_CRIT_SEC       cs;
};

typedef struct jk_shm jk_shm_t;

static const char shm_signature[] = { JK_SHM_MAGIC };
static jk_shm_t jk_shmem = { 0, 0, 0, NULL, NULL, -1, -1, 0, NULL};
static time_t jk_workers_modified_time = 0;
static time_t jk_workers_access_time = 0;
#if defined (WIN32)
static HANDLE jk_shm_map = NULL;
#endif

/* Calculate needed shm size */
size_t jk_shm_calculate_size(jk_map_t *init_data, jk_logger_t *l)
{
    char **worker_list;
    unsigned i;
    unsigned num_of_workers;
    int num_of_lb_workers = 0;
    int num_of_ajp13_workers = 0;

    JK_TRACE_ENTER(l);

    if (jk_get_worker_list(init_data, &worker_list,
                           &num_of_workers) == JK_FALSE) {
        jk_log(l, JK_LOG_ERROR,
               "Could not get worker list from map");
        JK_TRACE_EXIT(l);
        return 0;
    }

    for (i = 0; i < num_of_workers; i++) {
        const char *type = jk_get_worker_type(init_data, worker_list[i]);

        if (!strcmp(type, JK_LB_WORKER_NAME)) {
            char **member_list;
            unsigned num_of_members;
            num_of_lb_workers++;
            if (jk_get_lb_worker_list(init_data, worker_list[i],
                                      &member_list, &num_of_members) == JK_FALSE) {
                jk_log(l, JK_LOG_ERROR,
                       "Could not get member list for lb worker from map");
            }
            else {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG, "worker %s of type %s has %u members",
                           worker_list[i], JK_LB_WORKER_NAME, num_of_members);
                num_of_ajp13_workers += num_of_members;
            }
        }
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG, "shared memory will contain %d lb workers with %d ajp13 members",
               num_of_lb_workers, num_of_ajp13_workers);
    jk_shmem.lb_workers = num_of_lb_workers;
    jk_shmem.ajp13_workers = num_of_ajp13_workers;
    JK_TRACE_EXIT(l);
    return JK_SHM_LB_SIZE(jk_shmem.lb_workers) + JK_SHM_AJP13_SIZE(jk_shmem.ajp13_workers);
}


#if defined (WIN32) || defined(NETWARE)

/* Use plain memory */
int jk_shm_open(const char *fname, size_t sz, jk_logger_t *l)
{
    int rc;
    int attached = 0;
    JK_TRACE_ENTER(l);
    if (jk_shmem.hdr) {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG, "Shared memory is already opened");
        JK_TRACE_EXIT(l);
        return 0;
    }

    jk_shmem.size = JK_SHM_ALIGN(sizeof(jk_shm_header_t) + sz);

#if defined (WIN32)
    if (fname) {
        jk_shm_map = CreateFileMapping(INVALID_HANDLE_VALUE,
                                       NULL,
                                       PAGE_READWRITE,
                                       0,
                                       (DWORD)(sizeof(jk_shm_header_t) + sz),
                                       fname);
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            attached = 1;
            if (jk_shm_map == NULL || jk_shm_map == INVALID_HANDLE_VALUE) {
                jk_shm_map = OpenFileMapping(PAGE_READWRITE, FALSE, fname);
            }
        }
        if (jk_shm_map == NULL || jk_shm_map == INVALID_HANDLE_VALUE) {
            JK_TRACE_EXIT(l);
            return -1;
        }
        jk_shmem.hdr = (jk_shm_header_t *)MapViewOfFile(jk_shm_map,
                                                        FILE_MAP_ALL_ACCESS,
                                                        0,
                                                        0,
                                                        0);
    }
    else
#endif
    jk_shmem.hdr = (jk_shm_header_t *)calloc(1, jk_shmem.size);
    if (!jk_shmem.hdr) {
#if defined (WIN32)
        if (jk_shm_map) {
            CloseHandle(jk_shm_map);
            jk_shm_map = NULL;
        }
#endif
        JK_TRACE_EXIT(l);
        return -1;
    }
    if (!jk_shmem.filename) {
        if (fname)
            jk_shmem.filename = strdup(fname);
        else
            jk_shmem.filename = strdup("memory");
    }
    jk_shmem.fd       = 0;
    jk_shmem.attached = attached;
    if (!attached) {
        memcpy(jk_shmem.hdr->h.data.magic, shm_signature,
               JK_SHM_MAGIC_SIZ);
        jk_shmem.hdr->h.data.size = sz;
        jk_shmem.hdr->h.data.childs = 1;
    }
    else {
        jk_shmem.hdr->h.data.childs++;
        /*
         * Reset the shared memory so that
         * alloc works even for attached memory.
         * XXX: This might break already used memory
         * if the number of workers change between
         * open and attach or between two attach operations.
         */
        if (jk_shmem.hdr->h.data.childs > 1) {
            if (JK_IS_DEBUG_LEVEL(l)) {
                jk_log(l, JK_LOG_DEBUG,
                       "Reseting the shared memory for child %d",
                       jk_shmem.hdr->h.data.childs);
            }
        }
        jk_shmem.hdr->h.data.pos     = 0;
        jk_shmem.hdr->h.data.workers = 0;
    }
    JK_INIT_CS(&(jk_shmem.cs), rc);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "%s shared memory %s size=%u free=%u addr=%#lx",
               attached ? "Attached" : "Initialized",
               jk_shm_name(), jk_shmem.size,
               jk_shmem.hdr->h.data.size - jk_shmem.hdr->h.data.pos,
               jk_shmem.hdr);
    JK_TRACE_EXIT(l);
    return 0;
}

int jk_shm_attach(const char *fname, size_t sz, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);
    if (!jk_shm_open(fname, sz, l)) {
        if (!jk_shmem.attached) {
            jk_shmem.attached = 1;
            if (JK_IS_DEBUG_LEVEL(l)) {
                jk_log(l, JK_LOG_DEBUG,
                   "Attached shared memory %s [%d] size=%u free=%u addr=%#lx",
                   jk_shm_name(), jk_shmem.hdr->h.data.childs, jk_shmem.size,
                   jk_shmem.hdr->h.data.size - jk_shmem.hdr->h.data.pos,
                   jk_shmem.hdr);
            }
        }
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
#if defined (WIN32)
        if (jk_shm_map) {
            --jk_shmem.hdr->h.data.childs;
            UnmapViewOfFile(jk_shmem.hdr);
            CloseHandle(jk_shm_map);
            jk_shm_map = NULL;
        }
        else
#endif
        free(jk_shmem.hdr);
        JK_DELETE_CS(&(jk_shmem.cs), rc);
    }
    jk_shmem.hdr = NULL;
    if (jk_shmem.filename) {
        free(jk_shmem.filename);
        jk_shmem.filename = NULL;
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

static int do_shm_open_lock(const char *fname, int attached, jk_logger_t *l)
{
    int rc;
    char flkname[256];
#ifdef AS400_UTF8
    char *wptr;
#endif

    JK_TRACE_ENTER(l);

    if (attached && jk_shmem.lockname) {
#ifdef JK_SHM_LOCK_REOPEN
        jk_shmem.fd_lock = open(jk_shmem.lockname, O_RDWR, 0666);
#else
        errno = EINVAL;
#endif
        if (jk_shmem.fd_lock == -1) {
            rc = errno;
            JK_TRACE_EXIT(l);
            return rc;
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Duplicated shared memory lock %s", jk_shmem.lockname);
        JK_TRACE_EXIT(l);
        return 0;
    }

    if (!jk_shmem.lockname) {
#ifdef JK_SHM_LOCK_REOPEN
        int i;
        jk_shmem.fd_lock = -1;
        mode_t mask = umask(0);
        for (i = 0; i < 8; i++) {
            strcpy(flkname, "/tmp/jkshmlock.XXXXXX");
            if (mktemp(flkname)) {
                jk_shmem.fd_lock = open(flkname, O_RDWR|O_CREAT|O_TRUNC, 0666);
                if (jk_shmem.fd_lock >= 0)
                    break;
            }
        }
        umask(mask);
#else
        strcpy(flkname, fname);
        strcat(flkname, ".lock");
#ifdef AS400_UTF8
        wptr = (char *)malloc(strlen(flkname) + 1);
        jk_ascii2ebcdic((char *)flkname, wptr);
        jk_shmem.fd_lock = open(wptr, O_RDWR|O_CREAT|O_TRUNC, 0666);
        free(wptr);
#else
        jk_shmem.fd_lock = open(flkname, O_RDWR|O_CREAT|O_TRUNC, 0666);
#endif
#endif
        if (jk_shmem.fd_lock == -1) {
            rc = errno;
            JK_TRACE_EXIT(l);
            return rc;
        }
        jk_shmem.lockname = strdup(flkname);
    }
    else {
        /* Nothing to do */
        JK_TRACE_EXIT(l);
        return 0;
    }

    if (ftruncate(jk_shmem.fd_lock, 1)) {
        rc = errno;
        close(jk_shmem.fd_lock);
        jk_shmem.fd_lock = -1;
        JK_TRACE_EXIT(l);
        return rc;
    }
    if (lseek(jk_shmem.fd_lock, 0, SEEK_SET) != 0) {
        rc = errno;
        close(jk_shmem.fd_lock);
        jk_shmem.fd_lock = -1;
        return rc;
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Opened shared memory lock %s", jk_shmem.lockname);
    JK_TRACE_EXIT(l);
    return 0;
}

static int do_shm_open(const char *fname, int attached,
                       size_t sz, jk_logger_t *l)
{
    int rc;
    int fd;
    void *base;
#ifdef AS400_UTF8
    char *wptr;
#endif

    JK_TRACE_ENTER(l);
    if (jk_shmem.hdr) {
        /* Probably a call from vhost */
        if (!attached)
            attached = 1;
    }
    else if (attached) {
        /* We should already have a header
         * Use memory if we don't
         */
        JK_TRACE_EXIT(l);
        return 0;
    }
    jk_shmem.size = JK_SHM_ALIGN(sizeof(jk_shm_header_t) + sz);

    if (!fname) {
        /* Use plain memory in case there is no file name */
        if (!jk_shmem.filename)
            jk_shmem.filename  = strdup("memory");
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Using process memory as shared memory");
        JK_TRACE_EXIT(l);
        return 0;
    }

    if (!jk_shmem.filename) {
        jk_shmem.filename = (char *)malloc(strlen(fname) + 32);
        sprintf(jk_shmem.filename, "%s.%" JK_PID_T_FMT, fname, getpid());
    }
    if (!attached) {
        size_t size;
        jk_shmem.attached = 0;
#ifdef AS400_UTF8
        wptr = (char *)malloc(strlen(jk_shmem.filename) + 1);
        jk_ascii2ebcdic((char *)jk_shmem.filename, wptr);
        fd = open(wptr, O_RDWR|O_CREAT|O_TRUNC, 0666);
        free(wptr);
#else
        fd = open(jk_shmem.filename, O_RDWR|O_CREAT|O_TRUNC, 0666);
#endif
        if (fd == -1) {
            jk_shmem.size = 0;
            JK_TRACE_EXIT(l);
            return errno;
        }
        size = lseek(fd, 0, SEEK_END);
        if (size < jk_shmem.size) {
            size = jk_shmem.size;
            if (ftruncate(fd, jk_shmem.size)) {
                rc = errno;
                close(fd);
#ifdef  AS400_UTF8
                wptr = (char *)malloc(strlen(jk_shmem.filename) + 1);
                jk_ascii2ebcdic((char *)jk_shmem.filename, wptr);
                unlink(wptr);
                free(wptr);
#else
                unlink(jk_shmem.filename);
#endif
                jk_shmem.size = 0;
                JK_TRACE_EXIT(l);
                return rc;
            }
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Truncated shared memory to %u", size);
        }
        if (lseek(fd, 0, SEEK_SET) != 0) {
            rc = errno;
            close(fd);
#ifdef  AS400_UTF8
            wptr = (char *)malloc(strlen(jk_shmem.filename) + 1);
            jk_ascii2ebcdic((char *)jk_shmem.filename, wptr);
            unlink(wptr);
            free(wptr);
#else
            unlink(jk_shmem.filename);
#endif
            jk_shmem.size = 0;
            JK_TRACE_EXIT(l);
            return rc;
        }

        base = mmap((caddr_t)0, jk_shmem.size,
                    PROT_READ | PROT_WRITE,
                    MAP_FILE | MAP_SHARED,
                    fd, 0);
        if (base == (caddr_t)MAP_FAILED || base == (caddr_t)0) {
            rc = errno;
            close(fd);
#ifdef  AS400_UTF8
            wptr = (char *)malloc(strlen(jk_shmem.filename) + 1);
            jk_ascii2ebcdic((char *)jk_shmem.filename, wptr);
            unlink(wptr);
            free(wptr);
#else
            unlink(jk_shmem.filename);
#endif
            jk_shmem.size = 0;
            JK_TRACE_EXIT(l);
            return rc;
        }
        jk_shmem.hdr = base;
        jk_shmem.fd  = fd;
        memset(jk_shmem.hdr, 0, jk_shmem.size);
        memcpy(jk_shmem.hdr->h.data.magic, shm_signature, JK_SHM_MAGIC_SIZ);
        jk_shmem.hdr->h.data.size = sz;
        jk_shmem.hdr->h.data.childs = 1;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Initialized shared memory %s size=%u free=%u addr=%#lx",
                   jk_shm_name(), jk_shmem.size,
                   jk_shmem.hdr->h.data.size - jk_shmem.hdr->h.data.pos,
                   jk_shmem.hdr);
    }
    else {
        unsigned int nchild;
        jk_shmem.hdr->h.data.childs++;
        jk_shmem.attached = (int)getpid();
        nchild = jk_shmem.hdr->h.data.childs;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Attached shared memory %s [%d] size=%u free=%u addr=%#lx",
                   jk_shm_name(), nchild, jk_shmem.size,
                   jk_shmem.hdr->h.data.size - jk_shmem.hdr->h.data.pos,
                   jk_shmem.hdr);
        /*
         * Reset the shared memory so that
         * alloc works even for attached memory.
         * XXX: This might break already used memory
         * if the number of workers change between
         * open and attach or between two attach operations.
         */
        if (nchild > 1) {
            if (JK_IS_DEBUG_LEVEL(l)) {
                jk_log(l, JK_LOG_DEBUG,
                       "Reseting the shared memory for child %d",
                       nchild);
            }
        }
        jk_shmem.hdr->h.data.pos     = 0;
        jk_shmem.hdr->h.data.workers = 0;
    }
    JK_INIT_CS(&(jk_shmem.cs), rc);
    if ((rc = do_shm_open_lock(jk_shmem.filename, attached, l))) {
        if (!attached) {
            munmap((void *)jk_shmem.hdr, jk_shmem.size);
            close(jk_shmem.fd);
#ifdef  AS400_UTF8
            wptr = (char *)malloc(strlen(jk_shmem.filename) + 1);
            jk_ascii2ebcdic((char *)jk_shmem.filename, wptr);
            unlink(wptr);
            free(wptr);
#else
            unlink(jk_shmem.filename);
#endif
        }
        jk_shmem.hdr = NULL;
        jk_shmem.fd  = -1;
        JK_TRACE_EXIT(l);
        return rc;
    }
    JK_TRACE_EXIT(l);
    return 0;
}

int jk_shm_open(const char *fname, size_t sz, jk_logger_t *l)
{
    return do_shm_open(fname, 0, sz, l);
}

int jk_shm_attach(const char *fname, size_t sz, jk_logger_t *l)
{
    return do_shm_open(fname, 1, sz, l);
}

void jk_shm_close()
{
    int rc;
#ifdef AS400_UTF8
    char *wptr;
#endif

    if (jk_shmem.hdr) {
        --jk_shmem.hdr->h.data.childs;

#ifdef JK_SHM_LOCK_REOPEN
        if (jk_shmem.fd_lock >= 0) {
            close(jk_shmem.fd_lock);
            jk_shmem.fd_lock = -1;
        }
#endif
        JK_DELETE_CS(&(jk_shmem.cs), rc);
        if (jk_shmem.attached) {
            int p = (int)getpid();
            if (p == jk_shmem.attached) {
                /* In case this is a forked child
                 * do not close the shared memory.
                 * It will be closed by the parent.
                 */
                jk_shmem.size = 0;
                jk_shmem.hdr  = NULL;
                jk_shmem.fd   = -1;
                return;
            }
        }
        if (jk_shmem.fd >= 0) {
            munmap((void *)jk_shmem.hdr, jk_shmem.size);
            close(jk_shmem.fd);
        }
        if (jk_shmem.fd_lock >= 0)
            close(jk_shmem.fd_lock);
        if (jk_shmem.lockname) {
#ifdef  AS400_UTF8
            wptr = (char *)malloc(strlen(jk_shmem.lockname) + 1);
            jk_ascii2ebcdic((char *)jk_shmem.lockname, wptr);
            unlink(wptr);
            free(wptr);
#else
            unlink(jk_shmem.lockname);
#endif
            free(jk_shmem.lockname);
            jk_shmem.lockname = NULL;
        }
        if (jk_shmem.filename) {
#ifdef  AS400_UTF8
            wptr = (char *)malloc(strlen(jk_shmem.filename) + 1);
            jk_ascii2ebcdic((char *)jk_shmem.filename, wptr);
            unlink(wptr);
            free(wptr);
#else
            unlink(jk_shmem.filename);
#endif
            free(jk_shmem.filename);
            jk_shmem.filename = NULL;
        }
    }
    jk_shmem.size    = 0;
    jk_shmem.hdr     = NULL;
    jk_shmem.fd      = -1;
    jk_shmem.fd_lock = -1;
}


#endif

void *jk_shm_alloc(jk_pool_t *p, size_t size)
{
    void *rc = NULL;

    if (jk_shmem.hdr) {
        size = JK_SHM_ALIGN(size);
        if ((jk_shmem.hdr->h.data.size - jk_shmem.hdr->h.data.pos) >= size) {
            rc = &(jk_shmem.hdr->buf[jk_shmem.hdr->h.data.pos]);
            jk_shmem.hdr->h.data.pos += size;
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
        return jk_shmem.hdr->h.data.modified;
    else
        return jk_workers_modified_time;
}

void jk_shm_set_workers_time(time_t t)
{
    if (jk_shmem.hdr)
        jk_shmem.hdr->h.data.modified = t;
    else
        jk_workers_modified_time = t;
    jk_workers_access_time = t;
}

int jk_shm_is_modified()
{
    time_t m = jk_shm_get_workers_time();
    if (m != jk_workers_access_time)
        return 1;
    else
        return 0;
}

void jk_shm_sync_access_time()
{
    jk_workers_access_time = jk_shm_get_workers_time();
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

jk_shm_ajp13_worker_t *jk_shm_alloc_ajp13_worker(jk_pool_t *p)
{
    jk_shm_ajp13_worker_t *w = (jk_shm_ajp13_worker_t *)jk_shm_alloc(p, JK_SHM_AJP13_WORKER_SIZE);
    if (w) {
        memset(w, 0, JK_SHM_AJP13_WORKER_SIZE);
        if (jk_shmem.hdr) {
            jk_shmem.hdr->h.data.workers++;
            w->h.id = jk_shmem.hdr->h.data.workers;
            w->h.type = JK_AJP13_WORKER_TYPE;
        }
        else
            w->h.id = -1;
    }
    return w;
}

jk_shm_lb_worker_t *jk_shm_alloc_lb_worker(jk_pool_t *p)
{
    jk_shm_lb_worker_t *w = (jk_shm_lb_worker_t *)jk_shm_alloc(p, JK_SHM_LB_WORKER_SIZE);
    if (w) {
        memset(w, 0, JK_SHM_LB_WORKER_SIZE);
        if (jk_shmem.hdr) {
            jk_shmem.hdr->h.data.workers++;
            w->h.id = jk_shmem.hdr->h.data.workers;
            w->h.type = JK_LB_WORKER_TYPE;
        }
        else
            w->h.id = -1;
    }
    return w;
}
