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

AP_DECLARE(request_rec *) ap_wrap_create_request(conn_rec *conn)
{
    request_rec *r;
    apr_pool_t *p;

    apr_pool_create(&p, conn->pool);
    apr_pool_tag(p, "request");
    r = apr_pcalloc(p, sizeof(request_rec));
    r->pool            = p;
    r->connection      = conn;
    r->server          = conn->base_server;

    r->user            = NULL;
    r->ap_auth_type    = NULL;

    r->headers_in      = apr_table_make(r->pool, 25);
    r->subprocess_env  = apr_table_make(r->pool, 25);
    r->headers_out     = apr_table_make(r->pool, 12);
    r->err_headers_out = apr_table_make(r->pool, 5);
    r->notes           = apr_table_make(r->pool, 5);


    r->status          = HTTP_REQUEST_TIME_OUT;  /* Until we get a request */
    r->the_request     = NULL;

    r->status = HTTP_OK;                         /* Until further notice. */
    return r;
}

AP_DECLARE(process_rec *) ap_wrap_create_process(int argc, const char * const *argv)
{
    process_rec *process;
    apr_pool_t *cntx;
    apr_status_t stat;

    stat = apr_pool_create(&cntx, NULL);
    if (stat != APR_SUCCESS) {
        /* XXX From the time that we took away the NULL pool->malloc mapping
         *     we have been unable to log here without segfaulting.
         */
        ap_log_error(APLOG_MARK, APLOG_ERR, stat, NULL,
                     "apr_pool_create() failed to create "
                     "initial context");
        apr_terminate();
        exit(1);
    }

    apr_pool_tag(cntx, "process");

    process = apr_palloc(cntx, sizeof(process_rec));
    process->pool = cntx;

    apr_pool_create(&process->pconf, process->pool);
    apr_pool_tag(process->pconf, "pconf");
    process->argc = argc;
    process->argv = argv;
    process->short_name = apr_filepath_name_get(argv[0]);
    return process;
}

AP_DECLARE(server_rec *) ap_wrap_create_server(process_rec *process, apr_pool_t *p)
{
    apr_status_t rv;
    server_rec *s = (server_rec *) apr_pcalloc(p, sizeof(server_rec));

    s->process = process;
    s->port = 0;
    s->server_admin = DEFAULT_ADMIN;
    s->server_hostname = NULL;
    s->loglevel = DEFAULT_LOGLEVEL;
    s->limit_req_line = DEFAULT_LIMIT_REQUEST_LINE;
    s->limit_req_fieldsize = DEFAULT_LIMIT_REQUEST_FIELDSIZE;
    s->limit_req_fields = DEFAULT_LIMIT_REQUEST_FIELDS;
    s->timeout = apr_time_from_sec(DEFAULT_TIMEOUT);
    s->keep_alive_timeout = apr_time_from_sec(DEFAULT_KEEPALIVE_TIMEOUT);
    s->keep_alive_max = DEFAULT_KEEPALIVE;
    s->keep_alive = 1;
    s->addrs = apr_pcalloc(p, sizeof(server_addr_rec));

    /* NOT virtual host; don't match any real network interface */
    rv = apr_sockaddr_info_get(&s->addrs->host_addr,
                               NULL, APR_INET, 0, 0, p);

    s->addrs->host_port = 0; /* matches any port */
    s->addrs->virthost = ""; /* must be non-NULL */

    return s;
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

    /* Socket is used only for backend connections
     * Since we don't have client socket skip the 
     * creation of adresses. They will be default
     * to 127.0.0.1:0 both local and remote
     */
    if (csd) {
        if ((rv = apr_socket_addr_get(&c->local_addr, APR_LOCAL, csd))
            != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_INFO, rv, server,
                    "apr_socket_addr_get(APR_LOCAL)");
                apr_socket_close(csd);
                return NULL;
         }
         if ((rv = apr_socket_addr_get(&c->remote_addr, APR_REMOTE, csd))
                != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_INFO, rv, server,
                    "apr_socket_addr_get(APR_REMOTE)");
                apr_socket_close(csd);
            return NULL;
         }
    } 
    else {
        /* localhost should be reachable on all platforms */
        if ((rv = apr_sockaddr_info_get(&c->local_addr, "localhost",
                                        APR_UNSPEC, 0,
                                        APR_IPV4_ADDR_OK, 
                                        c->pool))
            != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_INFO, rv, server,
                    "apr_sockaddr_info_get()");
                return NULL;
         }
         c->remote_addr = c->local_addr;        
    }
    apr_sockaddr_ip_get(&c->local_ip, c->local_addr);
    apr_sockaddr_ip_get(&c->remote_ip, c->remote_addr);
    c->base_server = server;

    c->id = id;
    c->bucket_alloc = alloc;

    return c;
} 

AP_DECLARE(const char *) ap_get_remote_host(conn_rec *conn, void *dir_config,
                                            int type, int *str_is_ip)
{
    int ignored_str_is_ip;

    if (!str_is_ip) { /* caller doesn't want to know */
        str_is_ip = &ignored_str_is_ip;
    }
    *str_is_ip = 0;

    /*
     * Return the desired information; either the remote DNS name, if found,
     * or either NULL (if the hostname was requested) or the IP address
     * (if any identifier was requested).
     */
    if (conn->remote_host != NULL && conn->remote_host[0] != '\0') {
        return conn->remote_host;
    }
    else {
        if (type == REMOTE_HOST || type == REMOTE_DOUBLE_REV) {
            return NULL;
        }
        else {
            *str_is_ip = 1;
            return conn->remote_ip;
        }
    }
} 

AP_DECLARE(const char *) ap_get_server_name(request_rec *r)
{
    /* default */
    return r->server->server_hostname;
} 

#define UNKNOWN_METHOD (-1)

static int lookup_builtin_method(const char *method, apr_size_t len)
{
    /* Note: the following code was generated by the "shilka" tool from
       the "cocom" parsing/compilation toolkit. It is an optimized lookup
       based on analysis of the input keywords. Postprocessing was done
       on the shilka output, but the basic structure and analysis is
       from there. Should new HTTP methods be added, then manual insertion
       into this code is fine, or simply re-running the shilka tool on
       the appropriate input. */

    /* Note: it is also quite reasonable to just use our method_registry,
       but I'm assuming (probably incorrectly) we want more speed here
       (based on the optimizations the previous code was doing). */

    switch (len)
    {
    case 3:
        switch (method[0])
        {
        case 'P':
            return (method[1] == 'U'
                    && method[2] == 'T'
                    ? M_PUT : UNKNOWN_METHOD);
        case 'G':
            return (method[1] == 'E'
                    && method[2] == 'T'
                    ? M_GET : UNKNOWN_METHOD);
        default:
            return UNKNOWN_METHOD;
        }

    case 4:
        switch (method[0])
        {
        case 'H':
            return (method[1] == 'E'
                    && method[2] == 'A'
                    && method[3] == 'D'
                    ? M_GET : UNKNOWN_METHOD);
        case 'P':
            return (method[1] == 'O'
                    && method[2] == 'S'
                    && method[3] == 'T'
                    ? M_POST : UNKNOWN_METHOD);
        case 'M':
            return (method[1] == 'O'
                    && method[2] == 'V'
                    && method[3] == 'E'
                    ? M_MOVE : UNKNOWN_METHOD);
        case 'L':
            return (method[1] == 'O'
                    && method[2] == 'C'
                    && method[3] == 'K'
                    ? M_LOCK : UNKNOWN_METHOD);
        case 'C':
            return (method[1] == 'O'
                    && method[2] == 'P'
                    && method[3] == 'Y'
                    ? M_COPY : UNKNOWN_METHOD);
        default:
            return UNKNOWN_METHOD;
        }

    case 5:
        switch (method[2])
        {
        case 'T':
            return (memcmp(method, "PATCH", 5) == 0
                    ? M_PATCH : UNKNOWN_METHOD);
        case 'R':
            return (memcmp(method, "MERGE", 5) == 0
                    ? M_MERGE : UNKNOWN_METHOD);
        case 'C':
            return (memcmp(method, "MKCOL", 5) == 0
                    ? M_MKCOL : UNKNOWN_METHOD);
        case 'B':
            return (memcmp(method, "LABEL", 5) == 0
                    ? M_LABEL : UNKNOWN_METHOD);
        case 'A':
            return (memcmp(method, "TRACE", 5) == 0
                    ? M_TRACE : UNKNOWN_METHOD);
        default:
            return UNKNOWN_METHOD;
        }

    case 6:
        switch (method[0])
        {
        case 'U':
            switch (method[5])
            {
            case 'K':
                return (memcmp(method, "UNLOCK", 6) == 0
                        ? M_UNLOCK : UNKNOWN_METHOD);
            case 'E':
                return (memcmp(method, "UPDATE", 6) == 0
                        ? M_UPDATE : UNKNOWN_METHOD);
            default:
                return UNKNOWN_METHOD;
            }
        case 'R':
            return (memcmp(method, "REPORT", 6) == 0
                    ? M_REPORT : UNKNOWN_METHOD);
        case 'D':
            return (memcmp(method, "DELETE", 6) == 0
                    ? M_DELETE : UNKNOWN_METHOD);
        default:
            return UNKNOWN_METHOD;
        }

    case 7:
        switch (method[1])
        {
        case 'P':
            return (memcmp(method, "OPTIONS", 7) == 0
                    ? M_OPTIONS : UNKNOWN_METHOD);
        case 'O':
            return (memcmp(method, "CONNECT", 7) == 0
                    ? M_CONNECT : UNKNOWN_METHOD);
        case 'H':
            return (memcmp(method, "CHECKIN", 7) == 0
                    ? M_CHECKIN : UNKNOWN_METHOD);
        default:
            return UNKNOWN_METHOD;
        }

    case 8:
        switch (method[0])
        {
        case 'P':
            return (memcmp(method, "PROPFIND", 8) == 0
                    ? M_PROPFIND : UNKNOWN_METHOD);
        case 'C':
            return (memcmp(method, "CHECKOUT", 8) == 0
                    ? M_CHECKOUT : UNKNOWN_METHOD);
        default:
            return UNKNOWN_METHOD;
        }

    case 9:
        return (memcmp(method, "PROPPATCH", 9) == 0
                ? M_PROPPATCH : UNKNOWN_METHOD);

    case 10:
        switch (method[0])
        {
        case 'U':
            return (memcmp(method, "UNCHECKOUT", 10) == 0
                    ? M_UNCHECKOUT : UNKNOWN_METHOD);
        case 'M':
            return (memcmp(method, "MKACTIVITY", 10) == 0
                    ? M_MKACTIVITY : UNKNOWN_METHOD);
        default:
            return UNKNOWN_METHOD;
        }

    case 11:
        return (memcmp(method, "MKWORKSPACE", 11) == 0
                ? M_MKWORKSPACE : UNKNOWN_METHOD);

    case 15:
        return (memcmp(method, "VERSION-CONTROL", 15) == 0
                ? M_VERSION_CONTROL : UNKNOWN_METHOD);

    case 16:
        return (memcmp(method, "BASELINE-CONTROL", 16) == 0
                ? M_BASELINE_CONTROL : UNKNOWN_METHOD);

    default:
        return UNKNOWN_METHOD;
    }

    /* NOTREACHED */
}

AP_DECLARE(apr_status_t) ap_wrap_make_request(request_rec *r, const char *url,
                                              const char *method,
                                              const char *content_type,
                                              const char *content_encoding,
                                              apr_size_t content_length, char *content)
{
    apr_status_t rc;

    if (!url) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Missing url");
        return APR_EINVAL;
    }
    if (!method)
        method = "GET";
    if ((r->method_number = lookup_builtin_method(method, strlen(method))) ==
                                UNKNOWN_METHOD) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Unknown HTTP metdod %s",
                      method);
        return APR_EINVAL;
    }
    r->method = method;
    r->protocol = AP_SERVER_PROTOCOL;
    r->proto_num = HTTP_VERSION(1, 1);

    if ((rc = apr_uri_parse(r->pool, url, &r->parsed_uri)) != APR_SUCCESS) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, rc, r, "error parsing uri");
        return APR_EINVAL;
    }
    r->user = r->parsed_uri.user;
    r->content_type = content_type;
    r->content_encoding = content_encoding;
    r->uri = r->parsed_uri.path;
    if (r->parsed_uri.query)
        r->unparsed_uri = apr_pstrcat(r->pool, r->parsed_uri.path, "?",
                                      r->parsed_uri.query, NULL);
    else
        r->unparsed_uri = r->uri;
    if (!r->parsed_uri.port)
        r->parsed_uri.port = r->server->port;
    
    if (r->parsed_uri.hostname) {
        if (r->parsed_uri.port)
        apr_table_addn(r->headers_in, "Host",
            apr_psprintf(r->pool, "%s:%d", r->parsed_uri.hostname, r->parsed_uri.port));
        else
            apr_table_addn(r->headers_in, "Host", r->parsed_uri.hostname);
    }
    if (r->content_type)
        apr_table_addn(r->headers_in, "Content-Type", r->content_type);
    if (r->content_encoding)
        apr_table_addn(r->headers_in, "Transfer-Encoding", r->content_encoding);
    if (content_length)
        apr_table_addn(r->headers_in, "Content-Length", apr_itoa(r->pool,
                                                       (int)content_length));
    apr_table_addn(r->headers_in, "Accept", "*/*");
    apr_table_addn(r->headers_in, "Pragma", "no-cache");
    apr_table_addn(r->headers_in, "User-Agent", "httpd-wrap/1.0");
    apr_table_addn(r->headers_in, "Accept-Charset", "iso-8859-2");
    apr_table_addn(r->headers_in, "Accept-Language", "hr");

    /* Create a simple bucket brigade for post data inside connection */
    if (content) {
        apr_bucket *e;
        if (!r->connection->bucket_alloc)
            r->connection->bucket_alloc = apr_bucket_alloc_create(r->connection->pool);
        r->connection->bb = apr_brigade_create(r->connection->pool,
                                               r->connection->bucket_alloc);
        e = apr_bucket_transient_create(content, content_length, r->connection->bucket_alloc);
        APR_BRIGADE_INSERT_TAIL(r->connection->bb, e);
 
    }
    return APR_SUCCESS;
}

AP_DECLARE(int) ap_setup_client_block(request_rec *r, int read_policy)
{
    const char *lenp = apr_table_get(r->headers_in, "Content-Length");
    
    r->remaining = 0;
    r->read_length = 0;
    if (lenp)
        r->remaining = atoi(lenp);
    return OK;
}

AP_DECLARE(int) ap_should_client_block(request_rec *r)
{
    /* First check if we have already read the request body */

    if (r->read_length || r->remaining <= 0)
        return 0;
    else
        return 1;
}

AP_DECLARE(long) ap_get_client_block(request_rec *r, char *buffer,
                                     apr_size_t bufsiz)
{
    apr_status_t rv;
 
    if (r->remaining < 0 || r->remaining == 0)
        return 0;
    
    rv = apr_brigade_flatten(r->connection->bb, buffer, &bufsiz); 

    if (rv != APR_SUCCESS) {
        return -1;
    }

    r->read_length += bufsiz;
    r->remaining   -= bufsiz;

    /* Remove the readed part */
    if (bufsiz) {
        apr_bucket *e;
        e = APR_BRIGADE_FIRST(r->connection->bb);
        apr_bucket_split(e, bufsiz);
        e = APR_BRIGADE_FIRST(r->connection->bb);
        APR_BUCKET_REMOVE(e);
    }

    return (long)bufsiz; 

}

/* No op */
AP_DECLARE(int) ap_discard_request_body(request_rec *r)
{
    return OK;
}

