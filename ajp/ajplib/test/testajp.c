/* Copyright 2000-2004 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_md5.h"
#include "apr_network_io.h"
#include "apr_pools.h"
#include "apr_strings.h"
#include "apr_uri.h"
#include "apr_date.h"
#include "apr_fnmatch.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
 
#include "apr_hooks.h"
#include "apr_optional_hooks.h"
#include "apr_buckets.h"

#include "ajp.h"

#if APR_HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if APR_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#define TEST_POST_DATA "This document is a proposal of evolution of the current " \
                       "Apache JServ Protocol version 1.3, also known as ajp13. " \
                       "I'll not cover here the full protocol but only the add-on from ajp13. " \
                       "This nth pass include comments from the tomcat-dev list and " \
                       "misses discovered during developpment."

#define TEST_CASE_URL "http://localhost/servlets-examples/HelloWorldExample"

/* Main process */
static process_rec *main_process;
/* Default server */
static server_rec *main_server;

/* This function is part of backend.
 * The backend return connected socket and conn_rec
 */
static apr_status_t connect_to_backend(apr_socket_t **socket, conn_rec **con,
                                       const char *host, apr_uint16_t port,
                                       server_rec *server, apr_pool_t *pool)
{
    apr_status_t rv;
    apr_sockaddr_t *remote_sa;
    
    if ((rv = apr_sockaddr_info_get(&remote_sa, host, APR_UNSPEC,
                                    port, 0, pool)) != APR_SUCCESS)
        return rv;
    if ((rv = apr_socket_create(socket, remote_sa->family, SOCK_STREAM,
#if (APR_MAJOR_VERSION > 0)
                                APR_PROTO_TCP,
#endif
                                pool)) != APR_SUCCESS)
        return rv;
    if ((rv = apr_socket_timeout_set(*socket,
                               apr_time_from_sec(3))) != APR_SUCCESS)  
        return rv;
    if ((rv = apr_socket_connect(*socket, remote_sa)) != APR_SUCCESS)
        return rv;

    if (!(*con = ap_run_create_connection(pool, server, *socket,
                                          0, NULL, NULL)))
        return APR_EGENERAL;

    return APR_SUCCESS;
}

#if APR_HAS_THREADS

#define TEST_THREAD_COUNT   10
static apr_thread_t *threads[TEST_THREAD_COUNT];

static void * APR_THREAD_FUNC thread_worker_func(apr_thread_t *thd, void *data)
{
    request_rec *r;
    conn_rec *c = (conn_rec *)data;

    /* Create an empty request */
    if (!(r = ap_wrap_create_request(c)))
        goto cleanup;

    /* TODO: do something usefull */
    apr_sleep(apr_time_from_sec(1));

    /* Clean up the request */
    apr_pool_destroy(r->pool);

    apr_thread_exit(thd, APR_SUCCESS);
    return NULL;
cleanup:
    apr_thread_exit(thd, APR_EGENERAL);
    return NULL;
}

static int create_threads(server_rec *server)
{
    int i;
    apr_status_t rv;

    for (i = 0; i < TEST_THREAD_COUNT; i++) {
        conn_rec *c;
        /* Create a single client connection. The dummy one of course. */
        if (!(c = ap_run_create_connection(server->process->pool, server,
                                       NULL, 0, NULL, NULL)))
            return -1;

        if ((rv = apr_thread_create(&threads[i], NULL,
                    thread_worker_func, (void *)c,
                    server->process->pool)) != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_INFO, rv, NULL, "apr_create_thread failed");
            return -1;
        }
    }
    ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, "Created %d worker threads", i);
    
    return 0;
}

static int join_threads()
{
    int i, rc = 0;
    apr_status_t rv;

    for (i = 0; i < TEST_THREAD_COUNT; i++) {
        apr_thread_join(&rv, threads[i]);
        if (rv != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, "Worker thread %d failed", i);
            rc = -1;
        }
    }

    ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, "All (%d) worker threads joined", i);
    
    return rc;
}

#endif

int main(int argc, const char * const * argv, const char * const *env)
{
    int rv = 0;
    apr_status_t rc;
    conn_rec *c, *con;
    request_rec *r;
    apr_socket_t *sock;
    ajp_msg_t *msg;

    apr_app_initialize(&argc, &argv, &env);

    /* This is done in httpd.conf using LogLevel debug directive.
     * We are setting this directly.
     */
    ap_default_loglevel = APLOG_DEBUG;

    ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, "Testing ajp...");
    ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, "Creating main server...");

    main_process = ap_wrap_create_process(argc, argv);
    /* Create the main server_rec */
    main_server  = ap_wrap_create_server(main_process, main_process->pool);    
    /* Create a single client connection. The dummy one of course. */
    if (!(c = ap_run_create_connection(main_process->pool, main_server,
                                       NULL, 0, NULL, NULL))) {
        rv = -1;
        goto finished;
    }    
    /* ... and a empty request */
    if (!(r = ap_wrap_create_request(c))) {
        rv = -1;
        goto finished;
    }

    
    /* 0. Fill in the request data          */
    if (ap_wrap_make_request(r, TEST_CASE_URL,
                             "POST", NULL, NULL,
                             0,
                             NULL) != APR_SUCCESS) {
        goto finished;
    }
    /*
     * Up to here HTTPD created that for each request.
     * From now on, we have a server_rec, conn_rec, and request_rec
     * Who will ever need something else :)
     */

    /* 1. Obtain a connection to backend    */
    if ((rc = connect_to_backend(&sock, &con, "127.0.0.1", 8009,
            main_server, r->pool)) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rc, NULL, "connect_to_backend");
        rv = -1;
        goto finished;
    }

    /* 2. Create AJP message                */
    if ((rc = ajp_send_header(sock, r)) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rc, NULL, "ajp_send_header");
        rv = -1;
        goto finished;
    }
    /* 3. Send AJP message                  */

    /* 4. Read AJP response                 */
    if ((rc = ajp_read_header(sock, r, &msg)) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rc, NULL, "ajp_read_header");
        rv = -1;
        goto finished;
    }

    /* 5. Display results                   */

    ajp_msg_dump(msg, "");
    /* 6. Release the connection            */


#if APR_HAS_THREADS_remove_this
    /* or make the few requests in paralel 
     * to test the threading.
     * the upper 7 steps will go to thread_worker_func
     */
    if ((rv = create_threads(main_server)))
        goto finished;
    
    if ((rv = join_threads()))
        goto finished;
#endif

finished:
    ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, "%s", rv == 0 ? "OK" : "FAILED");
    apr_terminate();

    return rv;
}
