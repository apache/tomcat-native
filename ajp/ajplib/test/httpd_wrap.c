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

#include "httpd_wrap.h"

int AP_DECLARE_DATA ap_default_loglevel = DEFAULT_LOGLEVEL;

static const char *levels[] = {
    "emerg",
    "alert",
    "crit",
    "error",
    "warn",
    "notice",
    "info",
    "debug",
    NULL
};

static void log_error_core(const char *file, int line, int level,
                           apr_status_t status,
                           const char *fmt, va_list args)
{
    FILE *stream;
    char timstr[32];
    char errstr[MAX_STRING_LEN];
    
    /* Skip the loging for lower levels */
    if (level < 0 || level > ap_default_loglevel)
        return;
    if (level < APLOG_WARNING)
        stream = stderr;
    else
        stream = stdout;
    apr_ctime(&timstr[0], apr_time_now());
    fprintf(stream, "[%s] [%s] ", timstr, levels[level]);
    if (file && level == APLOG_DEBUG) {
#ifndef WIN32
        char *e = strrchr(file, '/');
#else
        char *e = strrchr(file, '\\');
#endif
        if (e)
            fprintf(stream, "%s (%d) ", e + 1, line);
    }

    if (status != 0) {
        if (status < APR_OS_START_EAIERR) {
            fprintf(stream, "(%d)", status);
        }
        else if (status < APR_OS_START_SYSERR) {
            fprintf(stream, "(EAI %d)", status - APR_OS_START_EAIERR);
        }
        else if (status < 100000 + APR_OS_START_SYSERR) {
            fprintf(stream, "(OS %d)", status - APR_OS_START_SYSERR);
        }
        else {
            fprintf(stream, "(os 0x%08x)", status - APR_OS_START_SYSERR);
        }
        apr_strerror(status, errstr, MAX_STRING_LEN);
        fprintf(stream, " %s ", errstr);
    }

    apr_vsnprintf(errstr, MAX_STRING_LEN, fmt, args);
    fputs(errstr, stream);
    fputs("\n", stream);    
    if (level < APLOG_WARNING)
        fflush(stream);

}

AP_DECLARE(void) ap_log_error(const char *file, int line, int level,
                              apr_status_t status, const server_rec *s,
                              const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_error_core(file, line, level, status, fmt, args);
    va_end(args);
}

AP_DECLARE(void) ap_log_perror(const char *file, int line, int level,
                               apr_status_t status, apr_pool_t *p,
                               const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_error_core(file, line, level, status, fmt, args);
    va_end(args);
}
 

AP_DECLARE(void) ap_log_rerror(const char *file, int line, int level,
                               apr_status_t status, const request_rec *r,
                               const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_error_core(file, line, level, status, fmt, args);
    va_end(args);
}


AP_DECLARE(conn_rec *) ap_run_create_connection(apr_pool_t *ptrans,
                                  server_rec *server,
                                  apr_socket_t *csd, long id, void *sbh,
                                  apr_bucket_alloc_t *alloc)
{
    apr_status_t rv;
    conn_rec *c = (conn_rec *) apr_pcalloc(ptrans, sizeof(conn_rec));

    c->sbh = sbh;

    /* Got a connection structure, so initialize what fields we can
     * (the rest are zeroed out by pcalloc).
     */
    c->notes = apr_table_make(ptrans, 5);

    c->pool = ptrans;
    if ((rv = apr_socket_addr_get(&c->local_addr, APR_LOCAL, csd))
        != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_INFO, rv, server,
                     "apr_socket_addr_get(APR_LOCAL)");
        apr_socket_close(csd);
        return NULL;
    }

    apr_sockaddr_ip_get(&c->local_ip, c->local_addr);
    if ((rv = apr_socket_addr_get(&c->remote_addr, APR_REMOTE, csd))
        != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_INFO, rv, server,
                     "apr_socket_addr_get(APR_REMOTE)");
        apr_socket_close(csd);
        return NULL;
    }

    apr_sockaddr_ip_get(&c->remote_ip, c->remote_addr);
    c->base_server = server;

    c->id = id;
    c->bucket_alloc = alloc;

    return c;
} 
