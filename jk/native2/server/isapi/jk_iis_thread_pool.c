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
 * Description: IIS Jk2 Thread Pool                                        *
 * Author:      Mladen Turk <mturk@apache.org>                             *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#define _WIN32_WINNT 0x0400

#include <httpext.h>
#include <httpfilt.h>
#include <wininet.h>

#include "jk_global.h"
#include "jk_requtil.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_logger.h"
#include "jk_env.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "apr_general.h"
#include "jk_iis.h"

extern int use_thread_pool;
extern apr_pool_t *jk_globalPool;

typedef struct iis_thread_pool_t iis_thread_pool_t;
typedef struct iis_thread_t iis_thread_t;

struct iis_thread_pool_t
{
    HANDLE manager_port;
    HANDLE manager_thread;
    DWORD manager_id;
    HANDLE worker_port;
    int worker_threads;
    int thread_count;
    iis_thread_t **threads;
    HANDLE *handles;
    CRITICAL_SECTION cs;
    apr_pool_t *pool;
};

struct iis_thread_t
{
    DWORD thread_id;
    jk_ws_service_t service;
    int busy;
};

static iis_thread_pool_t global_thread_pool = { 0 };

#define THREAD_POOL_SHUTDOWN ((OVERLAPPED *)0xFFFFFFFF)
#define THREAD_POOL_RECYCLE  ((OVERLAPPED *)0xFFFFFFFE)
/* Manager service timeout 10 seconds */
#define MANAGER_TIMEOUT      10000
/* Timeout for threads shutdown 2 minutes */
#define SHUTDOWN_TIMEOUT     120000

VOID WINAPI thread_pool_manager(void *p)
{
    ULONG n1, n2;
    OVERLAPPED *pOverLapped;
    BOOL work = TRUE;

    while (work) {
        work = FALSE;
        while (GetQueuedCompletionStatus(global_thread_pool.manager_port,
                                         &n1, &n2, &pOverLapped,
                                         MANAGER_TIMEOUT)) {
            if (pOverLapped == THREAD_POOL_SHUTDOWN) {

                break;
            }
            else {
                /* XXX If something else received should be error */
            }
        }
        if (GetLastError() == WAIT_TIMEOUT) {
            /* XXX Dynamic management */

            work = TRUE;
        }
    }
    /* Clean up and die. */
    ExitThread(0);
}

VOID WINAPI thread_worker(void *p)
{
    ULONG n1, n2;
    OVERLAPPED *pOverLapped;
    iis_thread_t *thread = (iis_thread_t *) p;

    while (GetQueuedCompletionStatus(global_thread_pool.worker_port,
                                     &n1, &n2, &pOverLapped, INFINITE)) {
        if (pOverLapped == THREAD_POOL_SHUTDOWN) {
            jk_ws_service_t *s = &thread->service;
            if (s->workerEnv && s->realWorker) {
                struct jk_worker *w = s->realWorker;
                jk_env_t *env =
                    s->workerEnv->globalEnv->getEnv(s->workerEnv->globalEnv);
                if (w != NULL && w->channel != NULL
                    && w->channel->afterRequest != NULL) {
                    w->channel->afterRequest(env, w->channel, w, NULL, s);
                }
            }
            break;
        }
        else if (pOverLapped == THREAD_POOL_RECYCLE) {
            /* XXX Kill ourself, will be used for dynamic thread pool */

            break;
        }
        else {
            /* do the job */
            LPEXTENSION_CONTROL_BLOCK lpEcb = (LPEXTENSION_CONTROL_BLOCK) n1;

            InterlockedIncrement(&thread->busy);
            HttpExtensionProcWorker(lpEcb, &thread->service);
            InterlockedDecrement(&thread->busy);
        }
    }
    /* Clean up and die. */
    ExitThread(0);
}

int jk2_iis_init_pool(jk_env_t *env)
{
    int i, workers;
    /* Do nothing if the thread pool is not used */
    if (use_thread_pool <= 0)
        return JK_OK;
    else
        workers = use_thread_pool;

    global_thread_pool.worker_threads = workers;
    apr_pool_create(&global_thread_pool.pool, jk_globalPool);
    if (!global_thread_pool.pool) {

        return JK_ERR;
    }
    global_thread_pool.threads =
        (iis_thread_t **) apr_palloc(global_thread_pool.pool,
                                     workers * sizeof(iis_thread_t *));
    for (i = 0; i < workers; i++) {
        global_thread_pool.threads[i] =
            (iis_thread_t *) apr_pcalloc(global_thread_pool.pool,
                                         workers * sizeof(iis_thread_t));

    }

    InitializeCriticalSection(&global_thread_pool.cs);
    global_thread_pool.manager_port = CreateIoCompletionPort((HANDLE)
                                                             INVALID_HANDLE_VALUE,
                                                             NULL, 0, 0);
    /* Create the ThreadPool manager thread */
    global_thread_pool.manager_thread = CreateThread(NULL,
                                                     0,
                                                     (LPTHREAD_START_ROUTINE)
                                                     thread_pool_manager,
                                                     NULL, 0,
                                                     &global_thread_pool.
                                                     manager_id);

    /* Create the ThreadPool worker port */
    global_thread_pool.worker_port = CreateIoCompletionPort((HANDLE)
                                                            INVALID_HANDLE_VALUE,
                                                            NULL, 0, 0);

    global_thread_pool.handles =
        (HANDLE *) apr_palloc(global_thread_pool.pool,
                              workers * sizeof(HANDLE));
    /* Start the worker threads */
    for (i = 0; i < workers; i++) {
        DWORD id;
        global_thread_pool.handles[i] = CreateThread(NULL,
                                                     0,
                                                     (LPTHREAD_START_ROUTINE)
                                                     thread_worker,
                                                     global_thread_pool.
                                                     threads[i], 0, &id);
        global_thread_pool.threads[i]->thread_id = id;
    }

    return JK_OK;
}

int jk2_iis_close_pool(jk_env_t *env)
{
    int i, workers, rc;
    /* Do nothing if the thread pool is not used */
    if (use_thread_pool <= 0)
        return JK_OK;
    else
        workers = use_thread_pool;
    EnterCriticalSection(&global_thread_pool.cs);
    PostQueuedCompletionStatus(global_thread_pool.manager_port,
                               (DWORD) 0, (DWORD) 0, THREAD_POOL_SHUTDOWN);
    WaitForSingleObject(global_thread_pool.manager_port, INFINITE);
    CloseHandle(global_thread_pool.manager_port);
    CloseHandle(global_thread_pool.manager_thread);

    /* Send shutdown event to each worker thread */
    for (i = 0; i < workers; i++) {
        PostQueuedCompletionStatus(global_thread_pool.worker_port,
                                   (DWORD) 0,
                                   (DWORD) 0, THREAD_POOL_SHUTDOWN);
    }
    /* Wait for threads to die */
    rc = WaitForMultipleObjects(workers,
                                global_thread_pool.handles,
                                TRUE, SHUTDOWN_TIMEOUT);

    CloseHandle(global_thread_pool.worker_port);

    if (rc == WAIT_TIMEOUT) {
        DWORD exitCode;
        /* Terminate the threads that didn't respont to the shutdown event */
        for (i = 0; i < workers; i++)
            if (GetExitCodeThread(global_thread_pool.handles[i],
                                  &exitCode) == STILL_ACTIVE)
                TerminateThread(global_thread_pool.handles[i], -1);
    }

    LeaveCriticalSection(&global_thread_pool.cs);
    DeleteCriticalSection(&global_thread_pool.cs);
    return JK_OK;
}

int jk2_iis_thread_pool(LPEXTENSION_CONTROL_BLOCK lpEcb)
{

    return PostQueuedCompletionStatus(global_thread_pool.worker_port,
                                      (DWORD) lpEcb, (DWORD) 0, NULL);

}
