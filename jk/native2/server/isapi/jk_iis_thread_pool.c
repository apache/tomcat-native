/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2002 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */

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

struct iis_thread_pool_t {
    HANDLE          manager_port;
    HANDLE          manager_thread;
    DWORD           manager_id;
    HANDLE          worker_port;
    int             worker_threads;
    int             thread_count;
    iis_thread_t    **threads;
    HANDLE          *handles;
    CRITICAL_SECTION    cs;
    apr_pool_t      *pool;
};

struct iis_thread_t {
    DWORD           thread_id;
    jk_ws_service_t service;
    int             busy;
};

static iis_thread_pool_t global_thread_pool = {0};

#define THREAD_POOL_SHUTDOWN ((OVERLAPPED *)0xFFFFFFFF)
#define THREAD_POOL_RECYCLE  ((OVERLAPPED *)0xFFFFFFFE)
/* Manager service timeout 10 seconds */
#define MANAGER_TIMEOUT      10000
/* Timeout for threads shutdown 2 minutes */
#define SHUTDOWN_TIMEOUT     120000

DWORD WINAPI thread_pool_manager(void *p)
{
    ULONG       n1, n2; 
    OVERLAPPED  *pOverLapped;
    BOOL        work = TRUE;

    while (work) {
        work = FALSE;
        while (GetQueuedCompletionStatus(global_thread_pool.manager_port,
                &n1, &n2, &pOverLapped,
                MANAGER_TIMEOUT)) {
            if  (pOverLapped == THREAD_POOL_SHUTDOWN) {

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
    return 0;
}

DWORD WINAPI thread_worker(void *p)
{
    ULONG       n1, n2; 
    OVERLAPPED  *pOverLapped;    
    iis_thread_t *thread = (iis_thread_t *)p;

    while (GetQueuedCompletionStatus(global_thread_pool.worker_port,
            &n1, &n2, &pOverLapped, INFINITE)) {    
        if  (pOverLapped == THREAD_POOL_SHUTDOWN) {
            break;
        }
        else if (pOverLapped == THREAD_POOL_RECYCLE) {
            /* XXX Kill ourself, will be used for dynamic thread pool */

            break;
        }
        else {
            /* do the job */
            LPEXTENSION_CONTROL_BLOCK lpEcb = (LPEXTENSION_CONTROL_BLOCK)n1;

            InterlockedIncrement(&thread->busy);            
            HttpExtensionProcWorker(lpEcb, &thread->service);                                                
            InterlockedDecrement(&thread->busy);
        }
    }
    return 0;
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
    global_thread_pool.threads = (iis_thread_t **)apr_palloc(
                                    global_thread_pool.pool,
                                    workers * sizeof(iis_thread_t *));
    for (i = 0; i < workers; i++) {
        global_thread_pool.threads[i] = (iis_thread_t *)apr_pcalloc(
                                            global_thread_pool.pool,
                                            workers * sizeof(iis_thread_t));

    }
    
    InitializeCriticalSection(&global_thread_pool.cs);
    global_thread_pool.manager_port = CreateIoCompletionPort(
                                            (HANDLE)INVALID_HANDLE_VALUE,
                                            NULL,
                                            0,
                                            0);
    /* Create the ThreadPool manager thread */
    global_thread_pool.manager_thread = CreateThread(NULL,
                                        0,
                                        thread_pool_manager,
                                        NULL,
                                        0,
                                        &global_thread_pool.manager_id);

    /* Create the ThreadPool worker port */
    global_thread_pool.worker_port =  CreateIoCompletionPort(
                                            (HANDLE)INVALID_HANDLE_VALUE,
                                            NULL,
                                            0,
                                            0);

    global_thread_pool.handles = (HANDLE *)apr_palloc(
                                    global_thread_pool.pool,
                                    workers * sizeof(HANDLE));
    /* Start the worker threads */
    for (i = 0; i < workers; i++) {
        DWORD id;
        global_thread_pool.handles[i] = CreateThread(NULL,
                                        0,
                                        thread_worker,
                                        global_thread_pool.threads[i],
                                        0,
                                        &id);
        global_thread_pool.threads[i]->thread_id = id;
    }

    return JK_OK;
}

int jk2_iis_close_pool(jk_env_t *env)
{
    int i, workers ,rc;
    /* Do nothing if the thread pool is not used */
    if (use_thread_pool <= 0)
        return JK_OK;
    else
        workers = use_thread_pool;
    EnterCriticalSection(&global_thread_pool.cs);
    PostQueuedCompletionStatus(global_thread_pool.manager_port,
                               (DWORD)0,
                               (DWORD)0,
                               THREAD_POOL_SHUTDOWN);
    WaitForSingleObject(global_thread_pool.manager_port, INFINITE);
    CloseHandle(global_thread_pool.manager_port);
    CloseHandle(global_thread_pool.manager_thread);

    /* Send shutdown event to each worker thread */
    for (i = 0; i < workers; i++) {
       PostQueuedCompletionStatus(global_thread_pool.worker_port,
                                   (DWORD)0,
                                   (DWORD)0,
                                   THREAD_POOL_SHUTDOWN);
    }
    /* Wait for threads to die */
    rc = WaitForMultipleObjects(workers,
                                global_thread_pool.handles,
                                TRUE,
                                SHUTDOWN_TIMEOUT);

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

int jk2_iis_thread_pool(LPEXTENSION_CONTROL_BLOCK lpEcb) {
    
    return PostQueuedCompletionStatus(global_thread_pool.worker_port,
                                      (DWORD)lpEcb,
                                      (DWORD)0,
                                      NULL);

}
