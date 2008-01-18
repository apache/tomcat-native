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
 * Description: Apache 1.3 plugin for Tomcat                               *
 *              See ../common/jk_service.h for general mod_jk info         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 *              Dan Milstein <danmil@shore.net>                            *
 *              Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                        *
 ***************************************************************************/

/*
 * mod_jk: keeps all servlet related ramblings together.
 */

/* #include "ap_config.h" */
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#include "util_script.h"
#include "util_date.h"
#include "http_conf_globals.h"

/*
 * jk_ include files
 */
#ifdef NETWARE
#define _SYS_TYPES_H_
#define _NETDB_H_INCLUDED
#define _IN_
#define _INET_
#define _SYS_TIMEVAL_H_
#define _SYS_SOCKET_H_
#endif
#include "jk_global.h"
#include "jk_util.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_uri_worker_map.h"
#include "jk_ajp13.h"
#include "jk_shm.h"
#include "jk_url.h"

#define JK_LOG_DEF_FILE             ("logs/mod_jk.log")
#define JK_SHM_DEF_FILE             ("logs/jk-runtime-status")
#define JK_ENV_HTTPS                ("HTTPS")
#define JK_ENV_CERTS                ("SSL_CLIENT_CERT")
#define JK_ENV_CIPHER               ("SSL_CIPHER")
#define JK_ENV_SESSION              ("SSL_SESSION_ID")
#define JK_ENV_KEY_SIZE             ("SSL_CIPHER_USEKEYSIZE")
#define JK_ENV_CERTCHAIN_PREFIX     ("SSL_CLIENT_CERT_CHAIN_")
#define JK_ENV_REPLY_TIMEOUT        ("JK_REPLY_TIMEOUT")
#define JK_ENV_WORKER_NAME          ("JK_WORKER_NAME")
#define JK_NOTE_WORKER_NAME         ("JK_WORKER_NAME")
#define JK_NOTE_WORKER_TYPE         ("JK_WORKER_TYPE")
#define JK_NOTE_REQUEST_DURATION    ("JK_REQUEST_DURATION")
#define JK_NOTE_WORKER_ROUTE        ("JK_WORKER_ROUTE")
#define JK_HANDLER          ("jakarta-servlet")
#define JK_MAGIC_TYPE       ("application/x-jakarta-servlet")
#define NULL_FOR_EMPTY(x)   ((x && !strlen(x)) ? NULL : x)
#define STRNULL_FOR_NULL(x) ((x) ? (x) : "(null)")

/*
 * If you are not using SSL, comment out the following line. It will make
 * apache run faster.
 *
 * Personally, I (DM), think this may be a lie.
 */
#define ADD_SSL_INFO

module MODULE_VAR_EXPORT jk_module;
#ifdef WIN32
extern __declspec(dllimport) module dir_module;
#else
extern module dir_module;
#endif

static int xfer_flags = (O_WRONLY | O_APPEND | O_CREAT);
#if defined(OS2) || defined(WIN32) || defined(NETWARE)
/* OS/2 dosen't support users and groups */
static mode_t xfer_mode = (S_IREAD | S_IWRITE);
#else
static mode_t xfer_mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif

/*
 * Environment variable forward object
 */
typedef struct
{
    int has_default;
    char *name;
    char *value;
} envvar_item;

/*
 * Configuration object for the mod_jk module.
 */
typedef struct
{

    /*
     * Log stuff
     */
    char *log_file;
    int log_fd;
    int log_level;
    jk_logger_t *log;

    /*
     * Mount stuff
     */
    char *mount_file;
    int mount_file_reload;
    jk_map_t *uri_to_context;

    int mountcopy;

    jk_uri_worker_map_t *uw_map;

    int was_initialized;

    /*
     * Automatic context path apache alias
     */
    char *alias_dir;

    /*
     * Request Logging
     */

    char *stamp_format_string;
    char *format_string;
    array_header *format;

    /*
     * Setting target worker via environment
     */
    char *worker_indicator;

    /*
     * SSL Support
     */
    int ssl_enable;
    char *https_indicator;
    char *certs_indicator;
    char *cipher_indicator;
    char *session_indicator;
    char *key_size_indicator;
    char *certchain_prefix;     /* Client certificate chain prefix */

    /*
     * Jk Options
     */
    int options;
    int exclude_options;

    int strip_session;
    /*
     * Environment variables support
     */
    int envvars_has_own;
    table *envvars;
    table *envvars_def;
    array_header *envvar_items;

    server_rec *s;
} jk_server_conf_t;


/*
 * The "private", or subclass portion of the web server service class for
 * Apache 1.3.  An instance of this class is created for each request
 * handled.  See jk_service.h for details about the ws_service object in
 * general.
 */
struct apache_private_data
{
    /*
     * For memory management for this request.  Aliased to be identical to
     * the pool in the superclass (jk_ws_service).
     */
    jk_pool_t p;

    /* True iff request body data has been read from Apache */
    int read_body_started;

    /* Apache request structure */
    request_rec *r;
};
typedef struct apache_private_data apache_private_data_t;

typedef struct dir_config_struct
{
    array_header *index_names;
} dir_config_rec;

static server_rec *main_server = NULL;
static jk_logger_t *main_log = NULL;
static table *jk_log_fds = NULL;
static jk_worker_env_t worker_env;
static char *jk_shm_file = NULL;
static size_t jk_shm_size = 0;
/*
 * Worker stuff
*/
static jk_map_t *jk_worker_properties = NULL;
static char *jk_worker_file = NULL;
static int jk_mount_copy_all = JK_FALSE;

static int JK_METHOD ws_start_response(jk_ws_service_t *s,
                                       int status,
                                       const char *reason,
                                       const char *const *header_names,
                                       const char *const *header_values,
                                       unsigned num_of_headers);

static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b, unsigned l, unsigned *a);

static int JK_METHOD ws_write(jk_ws_service_t *s, const void *b, unsigned l);

static void JK_METHOD ws_add_log_items(jk_ws_service_t *s,
                                       const char *const *log_names,
                                       const char *const *log_values,
                                       unsigned num_of_log_items);

static void * JK_METHOD ws_next_vhost(void *d);

static void JK_METHOD ws_vhost_to_text(void *d, char *buf, size_t len);

static jk_uri_worker_map_t * JK_METHOD ws_vhost_to_uw_map(void *d);

/* srevilak - new function prototypes */
static void jk_server_cleanup(void *data);
static void jk_generic_cleanup(server_rec * s);



/* ====================================================================== */
/* JK Service step callbacks                                              */
/* ====================================================================== */


/*
 * Send the HTTP response headers back to the browser.
 *
 * Think of this function as a method of the apache1.3-specific subclass of
 * the jk_ws_service class.  Think of the *s param as a "this" or "self"
 * pointer.
 */
static int JK_METHOD ws_start_response(jk_ws_service_t *s,
                                       int status,
                                       const char *reason,
                                       const char *const *header_names,
                                       const char *const *header_values,
                                       unsigned num_of_headers)
{
    if (s && s->ws_private) {
        unsigned h;

        /* Obtain a subclass-specific "this" pointer */
        apache_private_data_t *p = s->ws_private;
        request_rec *r = p->r;

        if (!reason) {
            reason = "";
        }
        r->status = status;
        r->status_line = ap_psprintf(r->pool, "%d %s", status, reason);

        for (h = 0; h < num_of_headers; h++) {
            if (!strcasecmp(header_names[h], "Content-type")) {
                char *tmp = ap_pstrdup(r->pool, header_values[h]);
                ap_content_type_tolower(tmp);
                r->content_type = tmp;
            }
            else if (!strcasecmp(header_names[h], "Location")) {
                ap_table_set(r->headers_out, header_names[h],
                             header_values[h]);
            }
            else if (!strcasecmp(header_names[h], "Content-Length")) {
                ap_table_set(r->headers_out, header_names[h],
                             header_values[h]);
            }
            else if (!strcasecmp(header_names[h], "Transfer-Encoding")) {
                ap_table_set(r->headers_out, header_names[h],
                             header_values[h]);
            }
            else if (!strcasecmp(header_names[h], "Last-Modified")) {
                /*
                 * If the script gave us a Last-Modified header, we can't just
                 * pass it on blindly because of restrictions on future values.
                 */
                ap_update_mtime(r, ap_parseHTTPdate(header_values[h]));
                ap_set_last_modified(r);
            }
            else {
                ap_table_add(r->headers_out, header_names[h],
                             header_values[h]);
            }
        }

        ap_send_http_header(r);
        s->response_started = JK_TRUE;

        return JK_TRUE;
    }
    return JK_FALSE;
}

/*
 * Read a chunk of the request body into a buffer.  Attempt to read len
 * bytes into the buffer.  Write the number of bytes actually read into
 * actually_read.
 *
 * Think of this function as a method of the apache1.3-specific subclass of
 * the jk_ws_service class.  Think of the *s param as a "this" or "self"
 * pointer.
 */
static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b, unsigned len, unsigned *actually_read)
{
    if (s && s->ws_private && b && actually_read) {
        apache_private_data_t *p = s->ws_private;
        if (!p->read_body_started) {
            if (ap_should_client_block(p->r)) {
                p->read_body_started = JK_TRUE;
            }
        }

        if (p->read_body_started) {
            long rv;
            if ((rv = ap_get_client_block(p->r, b, len)) < 0) {
                *actually_read = 0;
            }
            else {
                *actually_read = (unsigned)rv;
            }
            /* reset timeout after successful read */
            ap_reset_timeout(p->r);
            return JK_TRUE;
        }
    }
    return JK_FALSE;
}

static void JK_METHOD ws_flush(jk_ws_service_t *s)
{
    if (s && s->ws_private) {
        apache_private_data_t *p = s->ws_private;
        BUFF *bf = p->r->connection->client;
        ap_bflush(bf);
    }
}

/*
 * Write a chunk of response data back to the browser.  If the headers
 * haven't yet been sent over, send over default header values (Status =
 * 200, basically).
 *
 * Write len bytes from buffer b.
 *
 * Think of this function as a method of the apache1.3-specific subclass of
 * the jk_ws_service class.  Think of the *s param as a "this" or "self"
 * pointer.
 */
static int JK_METHOD ws_write(jk_ws_service_t *s, const void *b, unsigned len)
{
    if (s && s->ws_private && b) {
        apache_private_data_t *p = s->ws_private;

        if (len) {
            char *buf = (char *)b;
            int w = (int)len;
            int r = 0;

            if (!s->response_started) {
                if (main_log)
                    jk_log(main_log, JK_LOG_INFO,
                           "Write without start, starting with defaults");
                if (!s->start_response(s, 200, NULL, NULL, NULL, 0)) {
                    return JK_FALSE;
                }
            }

            if (p->r->header_only) {
                BUFF *bf = p->r->connection->client;
                ap_bflush(bf);
                return JK_TRUE;
            }

            while (len && !p->r->connection->aborted) {
                w = ap_bwrite(p->r->connection->client, &buf[r], len);
                if (w > 0) {
                    /* reset timeout after successful write */
                    ap_reset_timeout(p->r);
                    r += w;
                    len -= w;
                }
                else if (w < 0) {
                    /* Error writing data -- abort */
                    if (!p->r->connection->aborted) {
                        ap_bsetflag(p->r->connection->client, B_EOUT, 1);
                        p->r->connection->aborted = 1;
                    }
                    return JK_FALSE;
                }

            }
            if (len && p->r->connection->aborted) {
                /* Fail if there is something left to send and
                 * the connection was aborted by the client
                 */
                return JK_FALSE;
            }
        }

        return JK_TRUE;
    }
    return JK_FALSE;
}

static void JK_METHOD ws_add_log_items(jk_ws_service_t *s,
                                       const char *const *log_names,
                                       const char *const *log_values,
                                       unsigned num_of_log_items)
{
    unsigned h;
    apache_private_data_t *p = s->ws_private;
    request_rec *r = p->r;

    for (h = 0; h < num_of_log_items; h++) {
        if (log_names[h] && log_values[h]) {
            ap_table_setn(r->notes, log_names[h], log_values[h]);
        }
    }
}

static void * JK_METHOD ws_next_vhost(void *d)
{
    server_rec *s = (server_rec *)d;
    if (s == NULL)
        return main_server;
    return s->next;
}

static void JK_METHOD ws_vhost_to_text(void *d, char *buf, size_t len)
{
    server_rec *s = (server_rec *)d;
    size_t used = 0;

    if (s->server_hostname)
        used += strlen(s->server_hostname);
    if (!s->is_virtual) {
        if (s->port)
            used += strlen(":XXXXX");
    }
    else if (s->addrs) {
        used += strlen(" [");
        if (s->addrs->virthost)
            used += strlen(s->addrs->virthost);
        if (s->addrs->host_port)
            used += strlen(":XXXXX");
        used += strlen("]");
    }

    if (len < used && len > strlen("XXX")) {
        strcpy(buf, "XXX");
        return;
    }

    used = 0;

    if (s->server_hostname) {
        strcpy(buf + used, s->server_hostname);
        used += strlen(s->server_hostname);
    }
    if (!s->is_virtual) {
        if (s->port) {
            sprintf(buf + used, ":%hu", s->port);
            used = strlen(buf);
        }
    }
    else if (s->addrs) {
        strcpy(buf + used, " [");
        used += strlen(" [");
        if (s->addrs->virthost) {
            strcpy(buf + used, s->addrs->virthost);
            used += strlen(s->addrs->virthost);
        }
        if (s->addrs->host_port) {
            sprintf(buf + used, ":%hu", s->addrs->host_port);
            used = strlen(buf);
        }
        strcpy(buf + used, "]");
        used += strlen("]");
    }
}

static jk_uri_worker_map_t * JK_METHOD ws_vhost_to_uw_map(void *d)
{
    server_rec *s = (server_rec *)d;
    jk_server_conf_t *conf = NULL;
    if (s == NULL)
        return NULL;
    conf = (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                     &jk_module);
    return conf->uw_map;
}

/* ====================================================================== */
/* Utility functions                                                      */
/* ====================================================================== */

/* Log something to JK log file then exit */
static void jk_error_exit(const char *file,
                          int line,
                          int level,
                          const server_rec * s,
                          ap_pool * p, const char *fmt, ...)
{
    va_list ap;
    char *res;
    char *ch;

    va_start(ap, fmt);
    res = ap_pvsprintf(p, fmt, ap);
    va_end(ap);
    /* Replace all format characters in the resulting message */
    /* because we feed the message to ap_log_error(). */
    ch = res;
    while (*ch) {
        if (*ch == '%') {
            *ch = '#';
        }
        ch++;
    }

    ap_log_error(file, line, level, s, res);
    if ( s ) {
        ap_log_error(file, line, level, NULL, res);
    }

    /* Exit process */
    exit(1);
}

/* Return the content length associated with an Apache request structure */
static jk_uint64_t get_content_length(request_rec * r)
{
    if (r->clength > 0) {
        return (jk_uint64_t)r->clength;
    }
    else {
        char *lenp = (char *)ap_table_get(r->headers_in, "Content-Length");

        if (lenp) {
            jk_uint64_t rc = 0;
            if (sscanf(lenp, "%" JK_UINT64_T_FMT, &rc) > 0 && rc > 0) {
                return rc;
            }
        }
    }

    return 0;
}

/*
 * Set up an instance of a ws_service object for a single request.  This
 * particular instance will be of the Apache 1.3-specific subclass.  Copies
 * all of the important request information from the Apache request object
 * into the jk_ws_service_t object.
 *
 * Params
 *
 * private_data: The subclass-specific data structure, already initialized
 * (with a pointer to the Apache request_rec structure, among other things)
 *
 * s: The base class object.
 *
 * conf: Configuration information
 *
 * Called from jk_handler().  See jk_service.h for explanations of what most
 * of these fields mean.
 */
static int init_ws_service(apache_private_data_t * private_data,
                           jk_ws_service_t *s, jk_server_conf_t * conf)
{
    int size;
    request_rec *r = private_data->r;
    char *ssl_temp = NULL;
    const char *reply_timeout = NULL;
    rule_extension_t *e;

    /* Copy in function pointers (which are really methods) */
    s->start_response = ws_start_response;
    s->read = ws_read;
    s->write = ws_write;
    s->flush = ws_flush;
    s->add_log_items = ws_add_log_items;
    s->next_vhost = ws_next_vhost;
    s->vhost_to_text = ws_vhost_to_text;
    s->vhost_to_uw_map = ws_vhost_to_uw_map;

    s->auth_type = NULL_FOR_EMPTY(r->connection->ap_auth_type);
    s->remote_user = NULL_FOR_EMPTY(r->connection->user);

    s->protocol = r->protocol;
    s->remote_host =
        (char *)ap_get_remote_host(r->connection, r->per_dir_config,
                                   REMOTE_HOST);
    s->remote_host = NULL_FOR_EMPTY(s->remote_host);

    if (conf->options & JK_OPT_FWDLOCAL)
        s->remote_addr = NULL_FOR_EMPTY(r->connection->local_ip);
    else
        s->remote_addr = NULL_FOR_EMPTY(r->connection->remote_ip);

    if (conf->options & JK_OPT_FLUSHPACKETS)
        s->flush_packets = 1;
    if (conf->options & JK_OPT_FLUSHEADER)
        s->flush_header = 1;

    e = (rule_extension_t *)ap_get_module_config(r->request_config, &jk_module);
    if (e) {
        s->extension.reply_timeout = e->reply_timeout;
        if (e->activation) {
            s->extension.activation = ap_palloc(r->pool, e->size * sizeof(int));
            memcpy(s->extension.activation, e->activation, e->size * sizeof(int));
        }
    }
    reply_timeout = ap_table_get(r->subprocess_env, "JK_REPLY_TIMEOUT");
    if (reply_timeout) {
        int r = atoi(reply_timeout);
        if (r >= 0)
            s->extension.reply_timeout = r;
    }

    if (conf->options & JK_OPT_DISABLEREUSE)
        s->disable_reuse = 1;

    /* get server name */
    /* s->server_name  = (char *)(r->hostname ? r->hostname : r->server->server_hostname); */
    /* XXX : a la jk2 */
    s->server_name = (char *)ap_get_server_name(r);

    /* get the real port (otherwise redirect failed) */
    /* s->server_port     = htons( r->connection->local_addr.sin_port ); */
    /* XXX : a la jk2 */
    s->server_port = ap_get_server_port(r);

    s->server_software = (char *)ap_get_server_version();

    s->method = (char *)r->method;
    s->content_length = get_content_length(r);
    s->is_chunked = r->read_chunked;
    s->no_more_chunks = 0;
    s->query_string = r->args;

    /*
     * The 2.2 servlet spec errata says the uri from
     * HttpServletRequest.getRequestURI() should remain encoded.
     * [http://java.sun.com/products/servlet/errata_042700.html]
     *
     * We use JkOptions to determine which method to be used
     *
     * ap_escape_uri is the latest recommanded but require
     *               some java decoding (in TC 3.3 rc2)
     *
     * unparsed_uri is used for strict compliance with spec and
     *              old Tomcat (3.2.3 for example)
     *
     * uri is use for compatibilty with mod_rewrite with old Tomcats
     */

    switch (conf->options & JK_OPT_FWDURIMASK) {

    case JK_OPT_FWDURICOMPATUNPARSED:
        s->req_uri = r->unparsed_uri;
        if (s->req_uri != NULL) {
            char *query_str = strchr(s->req_uri, '?');
            if (query_str != NULL) {
                *query_str = 0;
            }
        }

        break;

    case JK_OPT_FWDURICOMPAT:
        s->req_uri = r->uri;
        break;

    case JK_OPT_FWDURIPROXY:
        size = 3 * strlen(r->uri) + 1;
        s->req_uri = ap_palloc(r->pool, size);
        jk_canonenc(r->uri, s->req_uri, size);
        break;

    case JK_OPT_FWDURIESCAPED:
        s->req_uri = ap_escape_uri(r->pool, r->uri);
        break;

    default:
        return JK_FALSE;
    }

    if (conf->ssl_enable || conf->envvars) {
        ap_add_common_vars(r);

        if (conf->ssl_enable) {
            ssl_temp =
                (char *)ap_table_get(r->subprocess_env,
                                     conf->https_indicator);
            if (ssl_temp && !strcasecmp(ssl_temp, "on")) {
                s->is_ssl = JK_TRUE;
                s->ssl_cert =
                    (char *)ap_table_get(r->subprocess_env,
                                         conf->certs_indicator);

                if (conf->options & JK_OPT_FWDCERTCHAIN) {
                    array_header *t = ap_table_elts(r->subprocess_env);
                    if (t && t->nelts) {
                        int i;
                        table_entry *elts = (table_entry *) t->elts;
                        array_header *certs = ap_make_array(r->pool, 1,
                                                            sizeof(char *));
                        *(const char **)ap_push_array(certs) = s->ssl_cert;
                        for (i = 0; i < t->nelts; i++) {
                            if (!elts[i].key)
                                continue;
                            if (!strncasecmp(elts[i].key,
                                             conf->certchain_prefix,
                                             strlen(conf->certchain_prefix)))
                            *(const char **)ap_push_array(certs) = elts[i].val;
                        }
                        s->ssl_cert = ap_array_pstrcat(r->pool, certs, '\0');
                     }
                }

                if (s->ssl_cert) {
                    s->ssl_cert_len = strlen(s->ssl_cert);
                    if (JK_IS_DEBUG_LEVEL(conf->log)) {
                        jk_log(conf->log, JK_LOG_DEBUG,
                               "SSL client certificate (%d bytes): %s",
                               s->ssl_cert_len, s->ssl_cert);
                    }
                }
                /* Servlet 2.3 API */
                s->ssl_cipher =
                    (char *)ap_table_get(r->subprocess_env,
                                         conf->cipher_indicator);
                s->ssl_session =
                    (char *)ap_table_get(r->subprocess_env,
                                         conf->session_indicator);

                if (conf->options & JK_OPT_FWDKEYSIZE) {
                    /* Servlet 2.3 API */
                    ssl_temp =
                        (char *)ap_table_get(r->subprocess_env,
                                             conf->key_size_indicator);
                    if (ssl_temp)
                        s->ssl_key_size = atoi(ssl_temp);
                }
            }
        }

        if (conf->envvars) {
            const array_header *t = conf->envvar_items;
            if (t && t->nelts) {
                int i;
                int j = 0;
                envvar_item *elts = (envvar_item *) t->elts;
                s->attributes_names =
                    ap_palloc(r->pool, sizeof(char *) * t->nelts);
                s->attributes_values =
                    ap_palloc(r->pool, sizeof(char *) * t->nelts);

                for (i = 0; i < t->nelts; i++) {
                    s->attributes_names[i - j] = elts[i].name;
                    s->attributes_values[i - j] =
                        (char *)ap_table_get(r->subprocess_env, elts[i].name);
                    if (!s->attributes_values[i - j]) {
                        if (elts[i].has_default) {
                            s->attributes_values[i - j] = elts[i].value;
                        }
                        else {
                            s->attributes_values[i - j] = "";
                            s->attributes_names[i - j] = "";
                            j++;
                        }
                    }
                }

                s->num_attributes = t->nelts - j;
            }
        }
    }

    if (r->headers_in && ap_table_elts(r->headers_in)) {
        int need_content_length_header = (!s->is_chunked
                                          && s->content_length ==
                                          0) ? JK_TRUE : JK_FALSE;
        array_header *t = ap_table_elts(r->headers_in);
        if (t && t->nelts) {
            int i;
            table_entry *elts = (table_entry *) t->elts;
            s->num_headers = t->nelts;
            /* allocate an extra header slot in case we need to add a content-length header */
            s->headers_names =
                ap_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
            s->headers_values =
                ap_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
            if (!s->headers_names || !s->headers_values)
                return JK_FALSE;
            for (i = 0; i < t->nelts; i++) {
                char *hname = ap_pstrdup(r->pool, elts[i].key);
                s->headers_values[i] = ap_pstrdup(r->pool, elts[i].val);
                s->headers_names[i] = hname;
                if (need_content_length_header &&
                    !strcasecmp(s->headers_names[i], "content-length")) {
                    need_content_length_header = JK_FALSE;
                }
            }
            /* Add a content-length = 0 header if needed.
             * Ajp13 assumes an absent content-length header means an unknown,
             * but non-zero length body.
             */
            if (need_content_length_header) {
                s->headers_names[s->num_headers] = "content-length";
                s->headers_values[s->num_headers] = "0";
                s->num_headers++;
            }
        }
        /* Add a content-length = 0 header if needed. */
        else if (need_content_length_header) {
            s->headers_names = ap_palloc(r->pool, sizeof(char *));
            s->headers_values = ap_palloc(r->pool, sizeof(char *));
            if (!s->headers_names || !s->headers_values)
                return JK_FALSE;
            s->headers_names[0] = "content-length";
            s->headers_values[0] = "0";
            s->num_headers++;
        }
    }
    s->uw_map = conf->uw_map;

    /* Dump all connection param so we can trace what's going to
     * the remote tomcat
     */
    if (JK_IS_DEBUG_LEVEL(conf->log)) {
        jk_log(conf->log, JK_LOG_DEBUG,
               "Service protocol=%s method=%s host=%s addr=%s name=%s port=%d auth=%s user=%s laddr=%s raddr=%s uri=%s",
               STRNULL_FOR_NULL(s->protocol),
               STRNULL_FOR_NULL(s->method),
               STRNULL_FOR_NULL(s->remote_host),
               STRNULL_FOR_NULL(s->remote_addr),
               STRNULL_FOR_NULL(s->server_name),
               s->server_port,
               STRNULL_FOR_NULL(s->auth_type),
               STRNULL_FOR_NULL(s->remote_user),
               STRNULL_FOR_NULL(r->connection->local_ip),
               STRNULL_FOR_NULL(r->connection->remote_ip),
               STRNULL_FOR_NULL(s->req_uri));
    }

    return JK_TRUE;
}

/*
 * The JK module command processors
 *
 * The below are all installed so that Apache calls them while it is
 * processing its config files.  This allows configuration info to be
 * copied into a jk_server_conf_t object, which is then used for request
 * filtering/processing.
 *
 * See jk_cmds definition below for explanations of these options.
 */

/*
 * JkMountCopy directive handling
 *
 * JkMountCopy On/Off/All
 */

static const char *jk_set_mountcopy(cmd_parms * cmd,
                                    void *dummy, char *mount_copy)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);
    if (! strcasecmp(mount_copy, "all")) {
        jk_mount_copy_all = JK_TRUE;
    }
    else if (strcasecmp(mount_copy, "on") && strcasecmp(mount_copy, "off")) {
        return "JkMountCopy must be All, On or Off";
    }
    else {
        conf->mountcopy = strcasecmp(mount_copy, "off") ? JK_TRUE : JK_FALSE;
    }

    return NULL;
}

/*
 * JkMount directive handling
 *
 * JkMount URI(context) worker
 */

static const char *jk_mount_context(cmd_parms * cmd,
                                    void *dummy,
                                    char *context,
                                    char *worker)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);
    const char *c, *w;

    if (worker != NULL && cmd->path == NULL ) {
        c = context;
        w = worker;
    }
    else if (worker == NULL && cmd->path != NULL) {
        c = cmd->path;
        w = context;
    }
    else {
        if (worker == NULL)
            return "JkMount needs a path when not defined in a location";
        else
            return "JkMount can not have a path when defined in a location";
    }

    if (c[0] != '/')
        return "JkMount context should start with /";

    if (!conf->uri_to_context) {
        if (!jk_map_alloc(&(conf->uri_to_context))) {
            return "JkMount Memory error";
        }
    }
    /*
     * Add the new worker to the alias map.
     */
    jk_map_put(conf->uri_to_context, c, w, NULL);
    return NULL;
}

/*
 * JkUnMount directive handling
 *
 * JkUnMount URI(context) worker
 */

static const char *jk_unmount_context(cmd_parms * cmd,
                                      void *dummy,
                                      const char *context,
                                      const char *worker)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);
    char *uri;
    const char *c, *w;

    if (worker != NULL && cmd->path == NULL ) {
        c = context;
        w = worker;
    }
    else if (worker == NULL && cmd->path != NULL) {
        c = cmd->path;
        w = context;
    }
    else {
        if (worker == NULL)
            return "JkUnMount needs a path when not defined in a location";
        else
            return "JkUnMount can not have a path when defined in a location";
    }
    if (c[0] != '/')
        return "JkUnMount context should start with /";
    uri = ap_pstrcat(cmd->temp_pool, "!", c, NULL);

    if (!conf->uri_to_context) {
        if (!jk_map_alloc(&(conf->uri_to_context))) {
            return "JkUnMount Memory error";
        }
    }
    /*
     * Add the new worker to the alias map.
     */
    jk_map_put(conf->uri_to_context, uri, w, NULL);
    return NULL;
}

/*
 * JkWorkersFile Directive Handling
 *
 * JkWorkersFile file
 */

static const char *jk_set_worker_file(cmd_parms * cmd,
                                      void *dummy, char *worker_file)
{
    struct stat statbuf;

    if (jk_worker_file != NULL)
        return "JkWorkersFile only allowed once";

    /* we need an absolute path */
    jk_worker_file = ap_server_root_relative(cmd->pool, worker_file);

#ifdef CHROOTED_APACHE
    ap_server_strip_chroot(jk_worker_file, 0);
#endif

    if (jk_worker_file == worker_file)
        jk_worker_file = ap_pstrdup(cmd->pool, worker_file);

    if (jk_worker_file == NULL)
        return "JkWorkersFile file name invalid";

    if (stat(jk_worker_file, &statbuf) == -1)
        return "Can't find the workers file specified";

    return NULL;
}

/*
 * JkMountFile Directive Handling
 *
 * JkMountFile file
 */

static const char *jk_set_mount_file(cmd_parms * cmd,
                                     void *dummy, char *mount_file)
{
    server_rec *s = cmd->server;
    struct stat statbuf;

    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    /* we need an absolute path (ap_server_root_relative does the ap_pstrdup) */
    conf->mount_file = ap_server_root_relative(cmd->pool, mount_file);

#ifdef CHROOTED_APACHE
    ap_server_strip_chroot(conf->mount_file, 0);
#endif

    if (conf->mount_file == NULL)
        return "JkMountFile file name invalid";

    if (stat(conf->mount_file, &statbuf) == -1)
        return "Can't find the mount file specified";

    if (!conf->uri_to_context) {
        if (!jk_map_alloc(&(conf->uri_to_context))) {
            return "JkMountFile Memory error";
        }
    }

    return NULL;
}

/*
 * JkMountFileReload Directive Handling
 *
 * JkMountFileReload seconds
 */

static const char *jk_set_mount_file_reload(cmd_parms * cmd,
                                            void *dummy, char *mount_file_reload)
{
    server_rec *s = cmd->server;
    int interval;

    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    interval = atoi(mount_file_reload);
    if (interval < 0) {
        interval = 0;
    }

    conf->mount_file_reload = interval;

    return NULL;
}

/*
 * JkLogFile Directive Handling
 *
 * JkLogFile file
 */

static const char *jk_set_log_file(cmd_parms * cmd,
                                   void *dummy, char *log_file)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    /* we need an absolute path */
    if (*log_file != '|') {
        conf->log_file = ap_server_root_relative(cmd->pool, log_file);

#ifdef CHROOTED_APACHE
        ap_server_strip_chroot(conf->log_file, 0);
#endif

    }
    else
        conf->log_file = ap_pstrdup(cmd->pool, log_file);

    if (conf->log_file == NULL)
        return "JkLogFile file name invalid";

    return NULL;
}

/*
 * JkShmFile Directive Handling
 *
 * JkShmFile file
 */

static const char *jk_set_shm_file(cmd_parms * cmd,
                                   void *dummy, char *shm_file)
{

    /* we need an absolute path */
    jk_shm_file = ap_server_root_relative(cmd->pool, shm_file);

#ifdef CHROOTED_APACHE
    ap_server_strip_chroot(jk_shm_file, 0);
#endif

    if (jk_shm_file == shm_file)
        jk_shm_file = ap_pstrdup(cmd->pool, shm_file);

    if (jk_shm_file == NULL)
        return "JkShmFile file name invalid";

    return NULL;
}

/*
 * JkShmSize Directive Handling
 *
 * JkShmSize size in kilobytes
 */

static const char *jk_set_shm_size(cmd_parms * cmd,
                                   void *dummy, const char *shm_size)
{
    int sz = 0;
    /* we need an absolute path */
    sz = atoi(shm_size) * 1024;
    if (sz < JK_SHM_DEF_SIZE)
        sz = JK_SHM_DEF_SIZE;
    else
        sz = JK_SHM_ALIGN(sz);
    jk_shm_size = (size_t)sz;
    return NULL;
}

/*
 * JkLogLevel Directive Handling
 *
 * JkLogLevel debug/info/request/error/emerg
 */

static const char *jk_set_log_level(cmd_parms * cmd,
                                    void *dummy, char *log_level)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->log_level = jk_parse_log_level(log_level);

    return NULL;
}

/*
 * JkLogStampFormat Directive Handling
 *
 * JkLogStampFormat "[%a %b %d %H:%M:%S %Y] "
 */

static const char *jk_set_log_fmt(cmd_parms * cmd,
                                  void *dummy, char *log_format)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->stamp_format_string = ap_pstrdup(cmd->pool, log_format);

    return NULL;
}

/*
 * JkAutoAlias Directive Handling
 *
 * JkAutoAlias application directory
 */

static const char *jk_set_auto_alias(cmd_parms * cmd,
                                     void *dummy, char *directory)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->alias_dir = directory;

    if (conf->alias_dir == NULL)
        return "JkAutoAlias directory invalid";

    return NULL;
}

/*
 * JkStripSession directive handling
 *
 * JkStripSession On/Off
 */

static const char *jk_set_strip_session(cmd_parms * cmd, void *dummy, int flag)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    /* Set up our value */
    conf->strip_session = flag ? JK_TRUE : JK_FALSE;

    return NULL;
}

/*****************************************************************
 *
 * Actually logging.
 */

typedef const char *(*item_key_func) (request_rec *, char *);

typedef struct
{
    item_key_func func;
    char *arg;
} request_log_format_item;

static const char *process_item(request_rec * r,
                                request_log_format_item * item)
{
    const char *cp;

    cp = (*item->func) (r, item->arg);
    return cp ? cp : "-";
}

static void request_log_transaction(request_rec * r, jk_server_conf_t * conf)
{
    request_log_format_item *items;
    char *str, *s;
    int i;
    int len = 0;
    const char **strs;
    int *strl;
    array_header *format = conf->format;

    strs = ap_palloc(r->pool, sizeof(char *) * (format->nelts));
    strl = ap_palloc(r->pool, sizeof(int) * (format->nelts));
    items = (request_log_format_item *) format->elts;
    for (i = 0; i < format->nelts; ++i) {
        strs[i] = process_item(r, &items[i]);
    }
    for (i = 0; i < format->nelts; ++i) {
        len += strl[i] = strlen(strs[i]);
    }
    str = ap_palloc(r->pool, len + 1);
    for (i = 0, s = str; i < format->nelts; ++i) {
        memcpy(s, strs[i], strl[i]);
        s += strl[i];
    }
    *s = 0;
    jk_log(conf->log, JK_LOG_REQUEST, "%s", str);
}

/*****************************************************************
 *
 * Parsing the log format string
 */

static char *format_integer(pool * p, int i)
{
    return ap_psprintf(p, "%d", i);
}

static char *pfmt(pool * p, int i)
{
    if (i <= 0) {
        return "-";
    }
    else {
        return format_integer(p, i);
    }
}

static const char *constant_item(request_rec * dummy, char *stuff)
{
    return stuff;
}

static const char *log_worker_name(request_rec * r, char *a)
{
    return ap_table_get(r->notes, JK_NOTE_WORKER_NAME);
}

static const char *log_worker_route(request_rec * r, char *a)
{
    return ap_table_get(r->notes, JK_NOTE_WORKER_ROUTE);
}

static const char *log_request_duration(request_rec * r, char *a)
{
    return ap_table_get(r->notes, JK_NOTE_REQUEST_DURATION);
}

static const char *log_request_line(request_rec * r, char *a)
{
    /* NOTE: If the original request contained a password, we
     * re-write the request line here to contain XXXXXX instead:
     * (note the truncation before the protocol string for HTTP/0.9 requests)
     * (note also that r->the_request contains the unmodified request)
     */
    return (r->parsed_uri.password) ? ap_pstrcat(r->pool, r->method, " ",
                                                 ap_unparse_uri_components(r->
                                                                           pool,
                                                                           &r->
                                                                           parsed_uri,
                                                                           0),
                                                 r->assbackwards ? NULL : " ",
                                                 r->protocol, NULL)
        : r->the_request;
}

/* These next two routines use the canonical name:port so that log
 * parsers don't need to duplicate all the vhost parsing crud.
 */
static const char *log_virtual_host(request_rec * r, char *a)
{
    return r->server->server_hostname;
}

static const char *log_server_port(request_rec * r, char *a)
{
    return ap_psprintf(r->pool, "%u",
                       r->server->port ? r->server->
                       port : ap_default_port(r));
}

/* This respects the setting of UseCanonicalName so that
 * the dynamic mass virtual hosting trick works better.
 */
static const char *log_server_name(request_rec * r, char *a)
{
    return ap_get_server_name(r);
}

static const char *log_request_uri(request_rec * r, char *a)
{
    return r->uri;
}
static const char *log_request_method(request_rec * r, char *a)
{
    return r->method;
}

static const char *log_request_protocol(request_rec * r, char *a)
{
    return r->protocol;
}
static const char *log_request_query(request_rec * r, char *a)
{
    return (r->args != NULL) ? ap_pstrcat(r->pool, "?", r->args, NULL)
        : "";
}
static const char *log_status(request_rec * r, char *a)
{
    return pfmt(r->pool, r->status);
}

static const char *clf_log_bytes_sent(request_rec * r, char *a)
{
    if (!r->sent_bodyct) {
        return "-";
    }
    else {
        long int bs;
        ap_bgetopt(r->connection->client, BO_BYTECT, &bs);
        return ap_psprintf(r->pool, "%ld", bs);
    }
}

static const char *log_bytes_sent(request_rec * r, char *a)
{
    if (!r->sent_bodyct) {
        return "0";
    }
    else {
        long int bs;
        ap_bgetopt(r->connection->client, BO_BYTECT, &bs);
        return ap_psprintf(r->pool, "%ld", bs);
    }
}

static struct log_item_list
{
    char ch;
    item_key_func func;
} log_item_keys[] = {

    {
    'T', log_request_duration}, {
    'r', log_request_line}, {
    'U', log_request_uri}, {
    's', log_status}, {
    'b', clf_log_bytes_sent}, {
    'B', log_bytes_sent}, {
    'V', log_server_name}, {
    'v', log_virtual_host}, {
    'p', log_server_port}, {
    'H', log_request_protocol}, {
    'm', log_request_method}, {
    'q', log_request_query}, {
    'w', log_worker_name}, {
    'R', log_worker_route}, {
    '\0'}
};

static struct log_item_list *find_log_func(char k)
{
    int i;

    for (i = 0; log_item_keys[i].ch; ++i)
        if (k == log_item_keys[i].ch) {
            return &log_item_keys[i];
        }

    return NULL;
}

static char *parse_request_log_misc_string(pool * p,
                                           request_log_format_item * it,
                                           const char **sa)
{
    const char *s;
    char *d;

    it->func = constant_item;

    s = *sa;
    while (*s && *s != '%') {
        s++;
    }
    /*
     * This might allocate a few chars extra if there's a backslash
     * escape in the format string.
     */
    it->arg = ap_palloc(p, s - *sa + 1);

    d = it->arg;
    s = *sa;
    while (*s && *s != '%') {
        if (*s != '\\') {
            *d++ = *s++;
        }
        else {
            s++;
            switch (*s) {
            case '\\':
                *d++ = '\\';
                s++;
                break;
            case 'n':
                *d++ = '\n';
                s++;
                break;
            case 't':
                *d++ = '\t';
                s++;
                break;
            default:
                /* copy verbatim */
                *d++ = '\\';
                /*
                 * Allow the loop to deal with this *s in the normal
                 * fashion so that it handles end of string etc.
                 * properly.
                 */
                break;
            }
        }
    }
    *d = '\0';

    *sa = s;
    return NULL;
}

static char *parse_request_log_item(pool * p,
                                    request_log_format_item * it,
                                    const char **sa)
{
    const char *s = *sa;
    struct log_item_list *l;

    if (*s != '%') {
        return parse_request_log_misc_string(p, it, sa);
    }

    ++s;
    it->arg = "";               /* For safety's sake... */

    l = find_log_func(*s++);
    if (!l) {
        char dummy[2];

        dummy[0] = s[-1];
        dummy[1] = '\0';
        return ap_pstrcat(p, "Unrecognized JkRequestLogFormat directive %",
                          dummy, NULL);
    }
    it->func = l->func;
    *sa = s;
    return NULL;
}

static array_header *parse_request_log_string(pool * p, const char *s,
                                              const char **err)
{
    array_header *a = ap_make_array(p, 15, sizeof(request_log_format_item));
    char *res;

    while (*s) {
        if ((res =
             parse_request_log_item(p,
                                    (request_log_format_item *)
                                    ap_push_array(a), &s))) {
            *err = res;
            return NULL;
        }
    }

    return a;
}

/*
 * JkRequestLogFormat Directive Handling
 *
 * JkRequestLogFormat format string
 *
 * %b - Bytes sent, excluding HTTP headers. In CLF format
 * %B - Bytes sent, excluding HTTP headers.
 * %H - The request protocol
 * %m - The request method
 * %p - The canonical Port of the server serving the request
 * %q - The query string (prepended with a ? if a query string exists,
 *      otherwise an empty string)
 * %r - First line of request
 * %s - request HTTP status code
 * %T - Requset duration, elapsed time to handle request in seconds '.' micro seconds
 * %U - The URL path requested, not including any query string.
 * %v - The canonical ServerName of the server serving the request.
 * %V - The server name according to the UseCanonicalName setting.
 * %w - Tomcat worker name
 */

static const char *jk_set_request_log_format(cmd_parms * cmd,
                                             void *dummy, char *format)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->format_string = ap_pstrdup(cmd->pool, format);

    return NULL;
}

/*
 * JkWorkerIndicator Directive Handling
 *
 * JkWorkerIndicator JkWorker
 */

static const char *jk_set_worker_indicator(cmd_parms * cmd,
                                           void *dummy, char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->worker_indicator = ap_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * JkExtractSSL Directive Handling
 *
 * JkExtractSSL On/Off
 */

static const char *jk_set_enable_ssl(cmd_parms * cmd, void *dummy, int flag)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    /* Set up our value */
    conf->ssl_enable = flag ? JK_TRUE : JK_FALSE;
    return NULL;
}

/*
 * JkHTTPSIndicator Directive Handling
 *
 * JkHTTPSIndicator HTTPS
 */

static const char *jk_set_https_indicator(cmd_parms * cmd,
                                          void *dummy, char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->https_indicator = ap_pstrdup(cmd->pool, indicator);
    return NULL;
}

/*
 * JkCERTSIndicator Directive Handling
 *
 * JkCERTSIndicator SSL_CLIENT_CERT
 */

static const char *jk_set_certs_indicator(cmd_parms * cmd,
                                          void *dummy, char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->certs_indicator = ap_pstrdup(cmd->pool, indicator);
    return NULL;
}

/*
 * JkCIPHERIndicator Directive Handling
 *
 * JkCIPHERIndicator SSL_CIPHER
 */

static const char *jk_set_cipher_indicator(cmd_parms * cmd,
                                           void *dummy, char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->cipher_indicator = ap_pstrdup(cmd->pool, indicator);
    return NULL;
}

/*
 * JkCERTCHAINPrefix Directive Handling
 *
 * JkCERTCHAINPrefix SSL_CLIENT_CERT_CHAIN_
 */

static const char *jk_set_certchain_prefix(cmd_parms * cmd,
                                           void *dummy, const char *prefix)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->certchain_prefix = ap_pstrdup(cmd->pool, prefix);

    return NULL;
}

/*
 * JkSESSIONIndicator Directive Handling
 *
 * JkSESSIONIndicator SSL_SESSION_ID
 */

static const char *jk_set_session_indicator(cmd_parms * cmd,
                                            void *dummy, char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->session_indicator = ap_pstrdup(cmd->pool, indicator);
    return NULL;
}

/*
 * JkKEYSIZEIndicator Directive Handling
 *
 * JkKEYSIZEIndicator SSL_CIPHER_USEKEYSIZE
 */

static const char *jk_set_key_size_indicator(cmd_parms * cmd,
                                             void *dummy, char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->key_size_indicator = ap_pstrdup(cmd->pool, indicator);
    return NULL;
}

/*
 * JkOptions Directive Handling
 *
 *
 * +ForwardSSLKeySize        => Forward SSL Key Size, to follow 2.3 specs but may broke old TC 3.2
 * -ForwardSSLKeySize        => Don't Forward SSL Key Size, will make mod_jk works with all TC release
 *  ForwardURICompat         => Forward URI normally, less spec compliant but mod_rewrite compatible (old TC)
 *  ForwardURICompatUnparsed => Forward URI as unparsed, spec compliant but broke mod_rewrite (old TC)
 *  ForwardURIEscaped        => Forward URI escaped and Tomcat (3.3 rc2) stuff will do the decoding part
 *  ForwardDirectories       => Forward all directory requests with no index files to Tomcat
 * +ForwardSSLCertChain      => Forward SSL certificate chain
 * -ForwardSSLCertChain      => Don't forward SSL certificate chain
 */

const char *jk_set_options(cmd_parms * cmd, void *dummy, const char *line)
{
    int opt = 0;
    int mask = 0;
    char action;
    char *w;

    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    while (line[0] != 0) {
        w = ap_getword_conf(cmd->pool, &line);
        action = 0;

        if (*w == '+' || *w == '-') {
            action = *(w++);
        }

        mask = 0;

        if (action == '-' && !strncasecmp(w, "ForwardURI", strlen("ForwardURI")))
            return ap_pstrcat(cmd->pool, "JkOptions: Illegal option '-", w,
                               "': ForwardURI* options can not be disabled", NULL);

        if (!strcasecmp(w, "ForwardURICompat")) {
            opt = JK_OPT_FWDURICOMPAT;
            mask = JK_OPT_FWDURIMASK;
        }
        else if (!strcasecmp(w, "ForwardURICompatUnparsed")) {
            opt = JK_OPT_FWDURICOMPATUNPARSED;
            mask = JK_OPT_FWDURIMASK;
        }
        else if (!strcasecmp(w, "ForwardURIEscaped")) {
            opt = JK_OPT_FWDURIESCAPED;
            mask = JK_OPT_FWDURIMASK;
        }
        else if (!strcasecmp(w, "ForwardURIProxy")) {
            opt = JK_OPT_FWDURIPROXY;
            mask = JK_OPT_FWDURIMASK;
        }
        else if (!strcasecmp(w, "ForwardDirectories")) {
            opt = JK_OPT_FWDDIRS;
        }
        else if (!strcasecmp(w, "ForwardLocalAddress")) {
            opt = JK_OPT_FWDLOCAL;
        }
        else if (!strcasecmp(w, "FlushPackets")) {
            opt = JK_OPT_FLUSHPACKETS;
        }
        else if (!strcasecmp(w, "FlushHeader")) {
            opt = JK_OPT_FLUSHEADER;
        }
        else if (!strcasecmp(w, "DisableReuse")) {
            opt = JK_OPT_DISABLEREUSE;
        }
        else if (!strcasecmp(w, "ForwardSSLCertChain")) {
            opt = JK_OPT_FWDCERTCHAIN;
        }
        else if (!strcasecmp(w, "ForwardKeySize")) {
            opt = JK_OPT_FWDKEYSIZE;
        }
        else if (!strcasecmp(w, "RejectUnsafeURI")) {
            opt = JK_OPT_REJECTUNSAFE;
        }
        else
            return ap_pstrcat(cmd->pool, "JkOptions: Illegal option '", w,
                              "'", NULL);

        conf->options &= ~mask;

        if (action == '-') {
            conf->exclude_options |= opt;
        }
        else if (action == '+') {
            conf->options |= opt;
        }
        else {                  /* for now +Opt == Opt */
            conf->options |= opt;
        }
    }
    return NULL;
}

/*
 * JkEnvVar Directive Handling
 *
 * JkEnvVar MYOWNDIR
 */

static const char *jk_add_env_var(cmd_parms * cmd,
                                  void *dummy,
                                  char *env_name, char *default_value)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->envvars_has_own = JK_TRUE;
    if (!conf->envvars) {
        conf->envvars      = ap_make_table(cmd->pool, 0);
        conf->envvars_def  = ap_make_table(cmd->pool, 0);
        conf->envvar_items = ap_make_array(cmd->pool, 0,
                                           sizeof(envvar_item));
    }

    /* env_name is mandatory, default_value is optional.
     * No value means send the attribute only, if the env var is set during runtime.
     */
    ap_table_setn(conf->envvars, env_name, default_value ? default_value : "");
    ap_table_setn(conf->envvars_def, env_name, default_value ? "1" : "0");

    return NULL;
}

/*
 * JkWorkerProperty Directive Handling
 *
 * JkWorkerProperty name=value
 */

static const char *jk_set_worker_property(cmd_parms * cmd,
                                          void *dummy,
                                          const char *line)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    if (!jk_worker_properties)
        jk_map_alloc(&jk_worker_properties);
    if (jk_map_read_property(jk_worker_properties, line,
                             JK_MAP_HANDLE_DUPLICATES, conf->log) == JK_FALSE)
        return ap_pstrcat(cmd->temp_pool, "Invalid JkWorkerProperty ", line, NULL);

    return NULL;
}

static const command_rec jk_cmds[] = {
    /*
     * JkWorkersFile specifies a full path to the location of the worker
     * properties file.
     *
     * This file defines the different workers used by apache to redirect
     * servlet requests.
     */
    {"JkWorkersFile", jk_set_worker_file, NULL, RSRC_CONF, TAKE1,
     "the name of a worker file for the Tomcat servlet containers"},

    /*
     * JkMountFile specifies a full path to the location of the
     * uriworker properties file.
     *
     * This file defines the different mapping for workers used by apache
     * to redirect servlet requests.
     */
    {"JkMountFile", jk_set_mount_file, NULL, RSRC_CONF, TAKE1,
     "the name of a mount file for the Tomcat servlet uri mappings"},

    /*
     * JkMountFileReload specifies the reload check interval for the
     * uriworker properties file.
     *
     * Default value is: JK_URIMAP_DEF_RELOAD
     */
    {"JkMountFileReload", jk_set_mount_file_reload, NULL, RSRC_CONF, TAKE1,
     "the reload check interval of the mount file"},

    /*
     * JkMount mounts a url prefix to a worker (the worker need to be
     * defined in the worker properties file.
     */
    {"JkMount", jk_mount_context, NULL, RSRC_CONF|ACCESS_CONF, TAKE12,
     "A mount point from a context to a servlet-engine worker"},

    /*
     * JkUnMount unmounts a url prefix to a worker (the worker need to be
     * defined in the worker properties file.
     */
    {"JkUnMount", jk_unmount_context, NULL, RSRC_CONF|ACCESS_CONF, TAKE12,
     "A no mount point from a context to a servlet-engine worker"},

     /*
     * JkMountCopy specifies if mod_jk should copy the mount points
     * from the main server to the virtual servers.
     */
    {"JkMountCopy", jk_set_mountcopy, NULL, RSRC_CONF, TAKE1,
     "Should the base server mounts be copied to the virtual server"},

    /*
     * JkStripSession specifies if mod_jk should strip the ;jsessionid
     * from the unmapped urls
     */
    {"JkStripSession", jk_set_strip_session, NULL, RSRC_CONF, FLAG,
     "Should the server strip the jsessionid from unmapped URLs"},

    /*
     * JkLogFile & JkLogLevel specifies to where should the plugin log
     * its information and how much.
     * JkLogStampFormat specify the time-stamp to be used on log
     */
    {"JkLogFile", jk_set_log_file, NULL, RSRC_CONF, TAKE1,
     "Full path to the mod_jk module log file"},
    {"JkShmFile", jk_set_shm_file, NULL, RSRC_CONF, TAKE1,
     "Full path to the mod_jk module shared memory file"},
    {"JkShmSize", jk_set_shm_size, NULL, RSRC_CONF, TAKE1,
     "Size of the shared memory file in KBytes"},
    {"JkLogLevel", jk_set_log_level, NULL, RSRC_CONF, TAKE1,
     "The mod_jk module log level, can be debug, info, request, error, or emerg"},
    {"JkLogStampFormat", jk_set_log_fmt, NULL, RSRC_CONF, TAKE1,
     "The mod_jk module log format, follow strftime syntax"},
    {"JkRequestLogFormat", jk_set_request_log_format, NULL, RSRC_CONF, TAKE1,
     "The mod_jk module request log format string"},

    /*
     * Automatically Alias webapp context directories into the Apache
     * document space.
     */
    {"JkAutoAlias", jk_set_auto_alias, NULL, RSRC_CONF, TAKE1,
     "The mod_jk module automatic context apache alias directory"},

    /*
     * Enable worker name to be set in an environment variable.
     * this way one can use LocationMatch together with mod_end,
     * mod_setenvif and mod_rewrite to set the target worker.
     * Use this in combination with SetHandler jakarta-servlet to
     * make mod_jk the handler for the request.
     *
     */
    {"JkWorkerIndicator", jk_set_worker_indicator, NULL, RSRC_CONF, TAKE1,
     "Name of the Apache environment that contains the worker name"},

    /*
     * Apache has multiple SSL modules (for example apache_ssl, stronghold
     * IHS ...). Each of these can have a different SSL environment names
     * The following properties let the administrator specify the envoiroment
     * variables names.
     *
     * HTTPS - indication for SSL
     * CERTS - Base64-Der-encoded client certificates.
     * CIPHER - A string specifing the ciphers suite in use.
     * SESSION - A string specifing the current SSL session.
     * KEYSIZE - Size of Key used in dialogue (#bits are secure)
     */
    {"JkHTTPSIndicator", jk_set_https_indicator, NULL, RSRC_CONF, TAKE1,
     "Name of the Apache environment that contains SSL indication"},
    {"JkCERTSIndicator", jk_set_certs_indicator, NULL, RSRC_CONF, TAKE1,
     "Name of the Apache environment that contains SSL client certificates"},
    {"JkCIPHERIndicator", jk_set_cipher_indicator, NULL, RSRC_CONF, TAKE1,
     "Name of the Apache environment that contains SSL client cipher"},
    {"JkCERTCHAINPrefix", jk_set_certchain_prefix, NULL, RSRC_CONF, TAKE1,
     "Name of the Apache environment (prefix) that contains SSL client chain certificates"},
    {"JkSESSIONIndicator", jk_set_session_indicator, NULL, RSRC_CONF, TAKE1,
     "Name of the Apache environment that contains SSL session"},
    {"JkKEYSIZEIndicator", jk_set_key_size_indicator, NULL, RSRC_CONF, TAKE1,
     "Name of the Apache environment that contains SSL key size in use"},
    {"JkExtractSSL", jk_set_enable_ssl, NULL, RSRC_CONF, FLAG,
     "Turns on SSL processing and information gathering by mod_jk"},

    /*
     * Options to tune mod_jk configuration
     * for now we understand :
     * +ForwardSSLKeySize        => Forward SSL Key Size, to follow 2.3 specs but may broke old TC 3.2
     * -ForwardSSLKeySize        => Don't Forward SSL Key Size, will make mod_jk works with all TC release
     *  ForwardURICompat         => Forward URI normally, less spec compliant but mod_rewrite compatible (old TC)
     *  ForwardURICompatUnparsed => Forward URI as unparsed, spec compliant but broke mod_rewrite (old TC)
     *  ForwardURIEscaped        => Forward URI escaped and Tomcat (3.3 rc2) stuff will do the decoding part
     * +ForwardSSLCertChain      => Forward SSL certificate chain
     * -ForwardSSLCertChain      => Don't forward SSL certificate chain
     */
    {"JkOptions", jk_set_options, NULL, RSRC_CONF, RAW_ARGS,
     "Set one of more options to configure the mod_jk module"},

    /*
     * JkEnvVar let user defines envs var passed from WebServer to
     * Servlet Engine
     */
    {"JkEnvVar", jk_add_env_var, NULL, RSRC_CONF, TAKE12,
     "Adds a name of environment variable and an optional value "
     "that should be sent to servlet-engine"},

    {"JkWorkerProperty", jk_set_worker_property, NULL, RSRC_CONF, RAW_ARGS,
     "Set workers.properties formated directive"},

    {NULL}
};

/* ====================================================================== */
/* The JK module handlers                                                 */
/* ====================================================================== */

/*
 * Called to handle a single request.
 */
static int jk_handler(request_rec * r)
{
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(r->server->
                                                  module_config,
                                                  &jk_module);
    /* Retrieve the worker name stored by jk_translate() */
    const char *worker_name = ap_table_get(r->notes, JK_NOTE_WORKER_NAME);
    int rc;

    JK_TRACE_ENTER(conf->log);

    if (ap_table_get(r->subprocess_env, "no-jk")) {
        if (JK_IS_DEBUG_LEVEL(conf->log))
            jk_log(conf->log, JK_LOG_DEBUG,
                   "Into handler no-jk env var detected for uri=%s, declined",
                   r->uri);
        JK_TRACE_EXIT(conf->log);
        return DECLINED;
    }

    if (r->proxyreq) {
        jk_log(conf->log, JK_LOG_ERROR,
               "Request has proxyreq flag set in mod_jk handler - aborting.");
        JK_TRACE_EXIT(conf->log);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    /* Set up r->read_chunked flags for chunked encoding, if present */
    if ((rc = ap_setup_client_block(r, REQUEST_CHUNKED_DECHUNK))) {
        jk_log(conf->log, JK_LOG_ERROR,
               "Could not setup client_block for chunked encoding - aborting");
        JK_TRACE_EXIT(conf->log);
        return rc;
    }

    if (worker_name == NULL && r->handler && !strcmp(r->handler, JK_HANDLER)) {
        /* we may be here because of a manual directive ( that overrides
         * translate and
         * sets the handler directly ). We still need to know the worker.
         */
            if (JK_IS_DEBUG_LEVEL(conf->log))
                jk_log(conf->log, JK_LOG_DEBUG,
                       "Retrieving environment %s", conf->worker_indicator);
        worker_name = (char *)ap_table_get(r->subprocess_env, conf->worker_indicator);
        if (worker_name) {
          /* The JkWorkerIndicator environment variable has
           * been used to explicitely set the worker without JkMount.
           * This is useful in combination with LocationMatch or mod_rewrite.
           */
            if (JK_IS_DEBUG_LEVEL(conf->log))
                jk_log(conf->log, JK_LOG_DEBUG,
                       "Retrieved worker (%s) from env %s for %s",
                       worker_name, conf->worker_indicator, r->uri);
        }
        else if (worker_env.num_of_workers == 1) {
          /* We have a single worker ( the common case ).
           * ( lb is a bit special, it should count as a single worker but
           * I'm not sure how ). We also have a manual config directive that
           * explicitely give control to us.
           */
            worker_name = worker_env.worker_list[0];
            if (JK_IS_DEBUG_LEVEL(conf->log))
                jk_log(conf->log, JK_LOG_DEBUG,
                       "Single worker (%s) configuration for %s",
                       worker_name, r->uri);
        }
        else if (worker_env.num_of_workers) {
            worker_name = worker_env.worker_list[0];
            if (JK_IS_DEBUG_LEVEL(conf->log))
                jk_log(conf->log, JK_LOG_DEBUG,
                       "Using first worker (%s) from %d workers for %s",
                       worker_name, worker_env.num_of_workers, r->uri);
        }
    }

    if (worker_name) {
        jk_worker_t *worker;

        worker = wc_get_worker_for_name(worker_name, conf->log);

        if (worker) {
#ifndef NO_GETTIMEOFDAY
            struct timeval tv_begin, tv_end;
            long micro, seconds;
            char *duration = NULL;
#endif
            int rc = JK_FALSE;
            int is_error = JK_HTTP_SERVER_ERROR;
            apache_private_data_t private_data;
            jk_ws_service_t s;
            jk_pool_atom_t buf[SMALL_POOL_SIZE];
            jk_open_pool(&private_data.p, buf, sizeof(buf));

            private_data.read_body_started = JK_FALSE;
            private_data.r = r;

            wc_maintain(conf->log);
            jk_init_ws_service(&s);

            s.ws_private = &private_data;
            s.pool = &private_data.p;
            ap_table_setn(r->notes, JK_NOTE_WORKER_TYPE,
                          wc_get_name_for_type(worker->type, conf->log));
#ifndef NO_GETTIMEOFDAY
            gettimeofday(&tv_begin, NULL);
#endif

            if (init_ws_service(&private_data, &s, conf)) {
                jk_endpoint_t *end = NULL;
                if (worker->get_endpoint(worker, &end, conf->log)) {
                    rc = end->service(end, &s, conf->log, &is_error);
                    end->done(&end, conf->log);

                    if (s.content_read < s.content_length ||
                        (s.is_chunked && !s.no_more_chunks)) {
                        /*
                         * If the servlet engine didn't consume all of the
                         * request data, consume and discard all further
                         * characters left to read from client
                         */
                        char *buff = ap_palloc(r->pool, 2048);
                        if (buff != NULL) {
                            int rd;
                            while ((rd =
                                    ap_get_client_block(r, buff, 2048)) > 0) {
                                s.content_read += rd;
                            }
                        }
                    }
                }
#ifndef NO_GETTIMEOFDAY
                gettimeofday(&tv_end, NULL);
                if (tv_end.tv_usec < tv_begin.tv_usec) {
                    tv_end.tv_usec += 1000000;
                    tv_end.tv_sec--;
                }
                micro = tv_end.tv_usec - tv_begin.tv_usec;
                seconds = tv_end.tv_sec - tv_begin.tv_sec;
                duration =
                    ap_psprintf(r->pool, "%.1ld.%.6ld", seconds, micro);
                ap_table_setn(r->notes, JK_NOTE_REQUEST_DURATION, duration);
#endif
                if (s.route && *s.route)
                    ap_table_setn(r->notes, JK_NOTE_WORKER_ROUTE, s.route);
                if (conf->format != NULL) {
                    request_log_transaction(r, conf);
                }
            }
            else {
                jk_log(conf->log, JK_LOG_ERROR, "Could not init service"
                       " for worker=%s",
                       worker_name);
                jk_close_pool(&private_data.p);
                JK_TRACE_EXIT(conf->log);
                return is_error;
            }
            jk_close_pool(&private_data.p);

            if (rc > 0) {
                /* If tomcat returned no body and the status is not OK,
                   let apache handle the error code */
                if (!r->sent_bodyct && r->status >= HTTP_BAD_REQUEST) {
                    jk_log(conf->log, JK_LOG_INFO, "No body with status=%d"
                           " for worker=%s",
                           r->status, worker_name);
                    JK_TRACE_EXIT(conf->log);
                    return r->status;
                }
                if (JK_IS_DEBUG_LEVEL(conf->log))
                    jk_log(conf->log, JK_LOG_DEBUG, "Service finished"
                           " with status=%d for worker=%s",
                           r->status, worker_name);
                JK_TRACE_EXIT(conf->log);
                return OK;      /* NOT r->status, even if it has changed. */
            }
            else if (rc == JK_CLIENT_ERROR) {
                if (is_error != HTTP_REQUEST_ENTITY_TOO_LARGE)
                    r->connection->aborted = 1;
                jk_log(conf->log, JK_LOG_INFO, "Aborting connection"
                       " for worker=%s",
                       worker_name);
                JK_TRACE_EXIT(conf->log);
                return is_error;
            }
            else {
                jk_log(conf->log, JK_LOG_INFO, "Service error=%d"
                       " for worker=%s",
                       rc, worker_name);
                JK_TRACE_EXIT(conf->log);
                return is_error;
            }
        }
        else {
            jk_log(conf->log, JK_LOG_ERROR, "Could not init service"
                   " for worker=%s",
                   worker_name);
            JK_TRACE_EXIT(conf->log);
            return HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    JK_TRACE_EXIT(conf->log);
    return HTTP_INTERNAL_SERVER_ERROR;
}

/*
 * Create a default config object.
 */
static void *create_jk_config(ap_pool * p, server_rec * s)
{
    jk_server_conf_t *c =
        (jk_server_conf_t *) ap_pcalloc(p, sizeof(jk_server_conf_t));

    c->log_fd = -1;
    c->mountcopy = JK_FALSE;
    c->was_initialized = JK_FALSE;

    if (s->is_virtual) {
        c->mountcopy = JK_UNSET;
        c->mount_file_reload = JK_UNSET;
        c->log_level = JK_UNSET;
        c->ssl_enable = JK_UNSET;
        c->strip_session = JK_UNSET;
    } else {
        if (!jk_map_alloc(&(c->uri_to_context))) {
            ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, NULL, "Memory error");
        }
        c->mountcopy = JK_FALSE;
        c->mount_file_reload = JK_URIMAP_DEF_RELOAD;
        c->log_level = JK_LOG_DEF_LEVEL;
        c->options = JK_OPT_FWDURIDEFAULT;
        c->worker_indicator = JK_ENV_WORKER_NAME;
        /*
         * By default we will try to gather SSL info.
         * Disable this functionality through JkExtractSSL
         */
        c->ssl_enable = JK_TRUE;
        /*
         * The defaults ssl indicators match those in mod_ssl (seems
         * to be in more use).
         */
        c->https_indicator = JK_ENV_HTTPS;
        c->certs_indicator = JK_ENV_CERTS;
        c->cipher_indicator = JK_ENV_CIPHER;
        c->certchain_prefix = JK_ENV_CERTCHAIN_PREFIX;
        c->session_indicator = JK_ENV_SESSION;
        c->key_size_indicator = JK_ENV_KEY_SIZE;
        c->strip_session = JK_FALSE;
    }
    c->envvars_has_own = JK_FALSE;

    c->s = s;

    return c;
}


/*
 * Utility - copy items from apr table src to dst,
 * for keys that exist in src but not in dst.
 */
static void merge_apr_table(table *src, table *dst)
{
    int i;
    const array_header *arr;
    const table_entry *elts;

    arr = ap_table_elts(src);
    elts = (const table_entry *)arr->elts;
    for (i = 0; i < arr->nelts; ++i) {
        if (!ap_table_get(dst, elts[i].key)) {
            ap_table_setn(dst, elts[i].key, elts[i].val);
        }
    }
}


static void *merge_jk_config(ap_pool * p, void *basev, void *overridesv)
{
    jk_server_conf_t *base = (jk_server_conf_t *) basev;
    jk_server_conf_t *overrides = (jk_server_conf_t *) overridesv;

    if (!overrides->log_file)
        overrides->log_file = base->log_file;
    if (overrides->log_level == JK_UNSET)
        overrides->log_level = base->log_level;

    if (!overrides->stamp_format_string)
        overrides->stamp_format_string = base->stamp_format_string;
    if (!overrides->format_string)
        overrides->format_string = base->format_string;

    if (!overrides->worker_indicator)
        overrides->worker_indicator = base->worker_indicator;

    if (overrides->ssl_enable == JK_UNSET)
        overrides->ssl_enable = base->ssl_enable;
    if (!overrides->https_indicator)
        overrides->https_indicator = base->https_indicator;
    if (!overrides->certs_indicator)
        overrides->certs_indicator = base->certs_indicator;
    if (!overrides->cipher_indicator)
        overrides->cipher_indicator = base->cipher_indicator;
    if (!overrides->certchain_prefix)
        overrides->certchain_prefix = base->certchain_prefix;
    if (!overrides->session_indicator)
        overrides->session_indicator = base->session_indicator;
    if (!overrides->key_size_indicator)
        overrides->key_size_indicator = base->key_size_indicator;

    overrides->options |= (base->options & ~base->exclude_options);

    if (base->envvars) {
        if (overrides->envvars && overrides->envvars_has_own) {
/* merge_apr_table() preserves existing entries in overrides table */
            merge_apr_table(base->envvars, overrides->envvars);
            merge_apr_table(base->envvars_def, overrides->envvars_def);
        }
        else {
            overrides->envvars = base->envvars;
            overrides->envvars_def = base->envvars_def;
            overrides->envvar_items = base->envvar_items;
        }
    }

    if (overrides->mountcopy == JK_UNSET && jk_mount_copy_all == JK_TRUE) {
        overrides->mountcopy = JK_TRUE;
    }
    if (overrides->uri_to_context && overrides->mountcopy == JK_TRUE) {
/* jk_map_copy() preserves existing entries in overrides map */
        if (jk_map_copy(base->uri_to_context, overrides->uri_to_context) == JK_FALSE) {
                jk_error_exit(APLOG_MARK, APLOG_EMERG, overrides->s, p, "Memory error");
        }
        if (!overrides->mount_file)
            overrides->mount_file = base->mount_file;
    }
    if (overrides->mountcopy == JK_TRUE) {
        if (!overrides->alias_dir)
            overrides->alias_dir = base->alias_dir;
    }
    if (overrides->mount_file_reload == JK_UNSET)
        overrides->mount_file_reload = base->mount_file_reload;
    if (overrides->strip_session == JK_UNSET)
        overrides->strip_session = base->strip_session;

    return overrides;
}

static int JK_METHOD jk_log_to_file(jk_logger_t *l, int level,
                                    int used, char *what)
{
    if (l &&
        (l->level <= level || level == JK_LOG_REQUEST_LEVEL) &&
         l->logger_private && what && used > 0) {
        jk_file_logger_t *flp = l->logger_private;
        int log_fd = flp->log_fd;
        if (log_fd >= 0) {
#if defined(WIN32)
            what[used++] = '\r';
#endif
            what[used++] = '\n';
            if (write(log_fd, what, used) < 0 ) {
                ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, NULL,
                             "mod_jk: jk_log_to_file %s failed",
                             what);
            }
        }

        return JK_TRUE;
    }

    return JK_FALSE;
}

static int log_fd_get(char *key)
{
    const char *buf=ap_table_get(jk_log_fds, key);
    if (buf)
        return atoi(buf);
    return 0;
}

static void log_fd_set(pool *p, char *key, int v)
{
    char *buf=(char *)ap_pcalloc(p, 8*sizeof(char));
    ap_snprintf(buf, 8, "%d", v);
    ap_table_setn(jk_log_fds, key, buf);
}

static void open_jk_log(server_rec *s, pool *p)
{
    const char *fname;
    int jklogfd;
    piped_log *pl;
    jk_logger_t *jkl;
    jk_file_logger_t *flp;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    if (!s->is_virtual && !conf->log_file) {
        conf->log_file = ap_server_root_relative(p, JK_LOG_DEF_FILE);
        if (conf->log_file)
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_NOERRNO, s,
                         "No JkLogFile defined in httpd.conf. "
                         "Using default %s", conf->log_file);
    }

    if (s->is_virtual && conf->log_file == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR, s,
                     "mod_jk: Invalid JkLogFile NULL");
        conf->log = main_log;
        return;
    }
    if (s->is_virtual && *(conf->log_file) == '\0') {
        ap_log_error(APLOG_MARK, APLOG_ERR, s,
                     "mod_jk: Invalid JkLogFile EMPTY");
        conf->log = main_log;
        return;
    }

#ifdef CHROOTED_APACHE
    ap_server_strip_chroot(conf->log_file, 0);
#endif

    jklogfd = log_fd_get(conf->log_file);
    if (!jklogfd) {
        if (*conf->log_file == '|') {
            if ((pl = ap_open_piped_log(p, conf->log_file + 1)) == NULL) {
                ap_log_error(APLOG_MARK, APLOG_ERR, s,
                             "mod_jk: could not open reliable pipe "
                             "to jk log %s", conf->log_file + 1);
                exit(1);
            }
            jklogfd = ap_piped_log_write_fd(pl);
        }
        else {
            fname = ap_server_root_relative(p, conf->log_file);
            if (!fname) {
                ap_log_error(APLOG_MARK, APLOG_ERR, s,
                             "mod_jk: Invalid JkLog " "path %s", conf->log_file);
                exit(1);
            }
#if AP_MODULE_MAGIC_AT_LEAST(19990320,14)
            if ((jklogfd = ap_popenf_ex(p, fname, xfer_flags, xfer_mode, 1))
                 < 0) {
#else
            if ((jklogfd = ap_popenf(p, fname, xfer_flags, xfer_mode))
                 < 0) {
#endif
                ap_log_error(APLOG_MARK, APLOG_ERR, s,
                             "mod_jk: could not open JkLog " "file %s", fname);
                exit(1);
            }
        }
        log_fd_set(p, conf->log_file, jklogfd);
    }
    conf->log_fd = jklogfd;
    jkl = (jk_logger_t *)ap_palloc(p, sizeof(jk_logger_t));
    flp = (jk_file_logger_t *)ap_palloc(p, sizeof(jk_file_logger_t));
    if (jkl && flp) {
        jkl->log = jk_log_to_file;
        jkl->level = conf->log_level;
        jkl->logger_private = flp;
        flp->log_fd = conf->log_fd;
        conf->log = jkl;
        jk_set_time_fmt(conf->log, conf->stamp_format_string);
        if (main_log == NULL)
            main_log = conf->log;
        return;
    }

    return;
}

static void jk_init(server_rec * s, ap_pool * p)
{
    int rc;
    server_rec *srv = s;
    const char *err_string = NULL;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);
    if (!jk_worker_properties)
        jk_map_alloc(&jk_worker_properties);
    jk_map_put(jk_worker_properties, "ServerRoot", ap_server_root, NULL);

    main_server = s;
    jk_log_fds = ap_make_table(p, 0);

    /* step through the servers and open each jk logfile
     * and do additional post config initialization.
     */
    for (; srv; srv = srv->next) {
        jk_server_conf_t *sconf = (jk_server_conf_t *)ap_get_module_config(srv->module_config,
                                                                           &jk_module);

/*
 * If a virtual server contains no JK directive, httpd shares
 * the config structure. But we don't want to share some settings
 * by default, especially the JkMount rules.
 * Therefore we check, if this config structure really belongs to this
 * vhost, otherwise we create a new one and merge.
 */
        if (sconf && sconf->s != srv) {
            jk_server_conf_t *srvconf = (jk_server_conf_t *)create_jk_config(p, srv);
            sconf = (jk_server_conf_t *)merge_jk_config(p, sconf, srvconf);
            ap_set_module_config(srv->module_config, &jk_module, sconf);

        }

        if (sconf && sconf->was_initialized == JK_FALSE) {
            sconf->was_initialized = JK_TRUE;
            open_jk_log(srv, p);
            sconf->options &= ~sconf->exclude_options;
            if (sconf->uri_to_context) {
                if (!uri_worker_map_alloc(&(sconf->uw_map),
                                          sconf->uri_to_context, sconf->log))
                    jk_error_exit(APLOG_MARK, APLOG_EMERG, srv,
                                  p, "Memory error");
                if (sconf->options & JK_OPT_REJECTUNSAFE)
                    sconf->uw_map->reject_unsafe = 1;
                else
                    sconf->uw_map->reject_unsafe = 0;
                if (sconf->mount_file) {
                    sconf->uw_map->fname = sconf->mount_file;
                    sconf->uw_map->reload = sconf->mount_file_reload;
                    uri_worker_map_load(sconf->uw_map, sconf->log);
                }
            }
            else {
                if (sconf->mountcopy == JK_TRUE) {
                    sconf->uw_map = conf->uw_map;
                }
            }
            if (sconf->format_string) {
                sconf->format =
                    parse_request_log_string(p, sconf->format_string, &err_string);
                if (sconf->format == NULL)
                    ap_log_error(APLOG_MARK, APLOG_ERR, srv,
                                 "JkRequestLogFormat format array NULL");
            }
            if (sconf->envvars && sconf->envvars_has_own) {
                int i;
                const array_header *arr;
                const table_entry *elts;
                envvar_item *item;
                const char *envvar_def;

                arr = ap_table_elts(sconf->envvars);
                if (arr) {
                    elts = (const table_entry *)arr->elts;
                    for (i = 0; i < arr->nelts; ++i) {
                        item = (envvar_item *)ap_push_array(sconf->envvar_items);
                        if (!item)
                            jk_error_exit(APLOG_MARK, APLOG_EMERG, srv,
                                          p, "Memory error");
                        item->name = elts[i].key;
                        envvar_def = ap_table_get(sconf->envvars_def, elts[i].key);
                        if (envvar_def && !strcmp("1", envvar_def) ) {
                            item->value = elts[i].val;
                            item->has_default = 1;
                        }
                        else {
                            item->value = "";
                            item->has_default = 0;
                        }
                    }
                }
            }
        }
    }

    if ((jk_worker_file != NULL) &&
        !jk_map_read_properties(jk_worker_properties, jk_worker_file, NULL,
                                JK_MAP_HANDLE_DUPLICATES, conf->log)) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG | APLOG_NOERRNO, s, p,
                      "Error in reading worker properties from '%s'",
                      jk_worker_file);
    }

    if (jk_map_resolve_references(jk_worker_properties, "worker.",
                                  1, 1, conf->log) == JK_FALSE) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG | APLOG_NOERRNO, s, p,
                      "Error in resolving configuration references");
    }

#if !defined(WIN32) && !defined(NETWARE)
    if (!jk_shm_file) {
        jk_shm_file = ap_server_root_relative(p, JK_SHM_DEF_FILE);

#ifdef CHROOTED_APACHE
        ap_server_strip_chroot(jk_shm_file, 0);
#endif

        if (jk_shm_file)
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_NOERRNO, s,
                         "No JkShmFile defined in httpd.conf. "
                         "Using default %s", jk_shm_file);
    }
#endif

    if (jk_shm_size == 0)
        jk_shm_size = jk_shm_calculate_size(jk_worker_properties, conf->log);
    else {
        jk_log(conf->log, JK_LOG_WARNING,
               "The optimal shared memory size can now be determined automatically.");
        jk_log(conf->log, JK_LOG_WARNING,
               "You can remove the JkShmSize directive if you want to use the optimal size.");
    }
    if ((rc = jk_shm_open(jk_shm_file, jk_shm_size, conf->log)) != 0)
        jk_log(conf->log, JK_LOG_ERROR,
               "Initializing shm:%s errno=%d. Load balancing workers will not function properly.",
               jk_shm_name(), rc);

    /* SREVILAK -- register cleanup handler to clear resources on restart,
     * to make sure log file gets closed in the parent process  */
    ap_register_cleanup(p, s, jk_server_cleanup, ap_null_cleanup);

    /* we add the URI->WORKER MAP since workers using AJP14 will feed it */
    worker_env.uri_to_worker = conf->uw_map;
    worker_env.virtual = "*";       /* for now */
    worker_env.server_name = (char *)ap_get_server_version();

    if (wc_open(jk_worker_properties, &worker_env, conf->log)) {
#if MODULE_MAGIC_NUMBER >= 19980527
        /* Tell apache we're here */
        ap_add_version_component(JK_EXPOSED_VERSION);
#endif
        jk_log(conf->log, JK_LOG_INFO,
               "%s initialized",
               JK_EXPOSED_VERSION);
        return;
    }
    else {
        jk_error_exit(APLOG_MARK, APLOG_EMERG | APLOG_NOERRNO, s, p,
                      "Error in creating the workers."
                      " Please consult your mod_jk log file '%s'.", conf->log_file);
    }
    uri_worker_map_ext(conf->uw_map, conf->log);
    for (srv = s; srv; srv = srv->next) {
        jk_server_conf_t *sconf = (jk_server_conf_t *)ap_get_module_config(srv->module_config,
                                                                           &jk_module);
        if (conf->uw_map != sconf->uw_map)
            uri_worker_map_ext(sconf->uw_map, sconf->log);
    }

}

/*
 * Perform uri to worker mapping, and store the name of the relevant worker
 * in the notes fields of the request_rec object passed in.  This will then
 * get picked up in jk_handler().
 */
static int jk_translate(request_rec * r)
{
    rule_extension_t **ext = ap_palloc(r->pool, sizeof(rule_extension_t *));
    *ext = NULL;
    ap_set_module_config(r->request_config, &jk_module, ext);

    if (!r->proxyreq) {
        jk_server_conf_t *conf =
            (jk_server_conf_t *) ap_get_module_config(r->server->
                                                      module_config,
                                                      &jk_module);

        if (conf) {
            char *clean_uri = ap_pstrdup(r->pool, r->uri);
            const char *worker;

            if (ap_table_get(r->subprocess_env, "no-jk")) {
                if (JK_IS_DEBUG_LEVEL(conf->log))
                    jk_log(conf->log, JK_LOG_DEBUG,
                           "Into translate no-jk env var detected for uri=%s, declined",
                           r->uri);
                return DECLINED;
            }

            ap_no2slash(clean_uri);
            if (!conf->uw_map) {
                if (JK_IS_DEBUG_LEVEL(conf->log))
                    jk_log(conf->log, JK_LOG_DEBUG,
                           "missing uri map for %s:%s",
                           conf->s->server_hostname ? conf->s->server_hostname : "_default_",
                           r->uri);
                return DECLINED;
            }
            else {
                rule_extension_t *e;
                worker = map_uri_to_worker_ext(conf->uw_map, clean_uri,
                                               NULL, &e, conf->log);
                ap_set_module_config(r->request_config, &jk_module, e);
            }

            /* Don't know the worker, ForwardDirectories is set, there is a
             * previous request for which the handler is JK_HANDLER (as set by
             * jk_fixups) and the request is for a directory:
             * --> forward to Tomcat, via default worker */
            if (!worker && (conf->options & JK_OPT_FWDDIRS) &&
                r->prev && r->prev->handler &&
                !strcmp(r->prev->handler, JK_HANDLER) && clean_uri &&
                strlen(clean_uri) && clean_uri[strlen(clean_uri) - 1] == '/') {

                if (worker_env.num_of_workers) {
                    /* Nothing here to do but assign the first worker since we
                     * already tried mapping and it didn't work out */
                    worker = worker_env.worker_list[0];

                    if (JK_IS_DEBUG_LEVEL(conf->log))
                        jk_log(conf->log, JK_LOG_DEBUG, "Manual configuration for %s %s",
                               clean_uri, worker_env.worker_list[0]);
                }
            }

            if (worker) {
                r->handler = ap_pstrdup(r->pool, JK_HANDLER);
                ap_table_setn(r->notes, JK_NOTE_WORKER_NAME, worker);
            }
            else if (conf->alias_dir != NULL) {
                /* Automatically map uri to a context static file */
                if (JK_IS_DEBUG_LEVEL(conf->log))
                    jk_log(conf->log, JK_LOG_DEBUG,
                           "check alias_dir: %s",
                           conf->alias_dir);
                if (strlen(clean_uri) > 1) {
                    /* Get the context directory name */
                    char *context_dir = NULL;
                    char *context_path = NULL;
                    char *child_dir = NULL;
                    char *index = clean_uri;
                    char *suffix = strchr(index + 1, '/');
                    if (suffix != NULL) {
                        int size = suffix - index;
                        context_dir = ap_pstrndup(r->pool, index, size);
                        /* Get the context child directory name */
                        index = index + size + 1;
                        suffix = strchr(index, '/');
                        if (suffix != NULL) {
                            size = suffix - index;
                            child_dir = ap_pstrndup(r->pool, index, size);
                        }
                        else {
                            child_dir = index;
                        }
                        /* Deny access to WEB-INF and META-INF directories */
                        if (child_dir != NULL) {
                            if (JK_IS_DEBUG_LEVEL(conf->log))
                                jk_log(conf->log, JK_LOG_DEBUG,
                                       "AutoAlias child_dir: %s",
                                       child_dir);
                            if (!strcasecmp(child_dir, "WEB-INF") ||
                                !strcasecmp(child_dir, "META-INF")) {
                                if (JK_IS_DEBUG_LEVEL(conf->log))
                                    jk_log(conf->log, JK_LOG_DEBUG,
                                           "AutoAlias HTTP_NOT_FOUND for URI: %s",
                                           r->uri);
                                return HTTP_NOT_FOUND;
                            }
                        }
                    }
                    else {
                        context_dir = ap_pstrdup(r->pool, index);
                    }

                    context_path = ap_pstrcat(r->pool, conf->alias_dir,
                                              ap_os_escape_path(r->pool,
                                                                context_dir,
                                                                1), NULL);
                    if (context_path != NULL) {
                        DIR *dir = ap_popendir(r->pool, context_path);
                        if (dir != NULL) {
                            char *escurl =
                                ap_os_escape_path(r->pool, clean_uri, 1);
                            char *ret =
                                ap_pstrcat(r->pool, conf->alias_dir, escurl,
                                           NULL);
                            ap_pclosedir(r->pool, dir);
                            /* Add code to verify real path ap_os_canonical_name */
                            if (ret != NULL) {
                                if (JK_IS_DEBUG_LEVEL(conf->log))
                                    jk_log(conf->log, JK_LOG_DEBUG,
                                           "AutoAlias OK for file: %s",
                                           ret);
                                r->filename = ret;
                                return OK;
                            }
                        }
                        else {
                            /* Deny access to war files in web app directory */
                            int size = strlen(context_dir);
                            if (size > 4
                                && !strcasecmp(context_dir + (size - 4),
                                               ".war")) {
                                if (JK_IS_DEBUG_LEVEL(conf->log))
                                    jk_log(conf->log, JK_LOG_DEBUG,
                                           "AutoAlias FORBIDDEN for URI: %s",
                                           r->uri);
                                return FORBIDDEN;
                            }
                        }
                    }
                }
            }
            else {
                if (JK_IS_DEBUG_LEVEL(conf->log))
                    jk_log(conf->log, JK_LOG_DEBUG,
                           "no match for %s found",
                           r->uri);
                if (conf->strip_session == JK_TRUE) {
                    char *jsessionid;
                    if (r->uri) {
                        jsessionid = strstr(r->uri, JK_PATH_SESSION_IDENTIFIER);
                        if (jsessionid) {
                            if (JK_IS_DEBUG_LEVEL(conf->log))
                                jk_log(conf->log, JK_LOG_DEBUG,
                                       "removing session identifier [%s] for non servlet url [%s]",
                                       jsessionid, r->uri);
                            *jsessionid = '\0';
                        }
                    }
                    if (r->filename) {
                        jsessionid = strstr(r->filename, JK_PATH_SESSION_IDENTIFIER);
                        if (jsessionid)
                            *jsessionid = '\0';
                    }
                }
            }
        }
    }

    return DECLINED;
}

/* In case ForwardDirectories option is set, we need to know if all files
 * mentioned in DirectoryIndex have been exhausted without success. If yes, we
 * need to let mod_dir know that we want Tomcat to handle the directory
 */
static int jk_fixups(request_rec * r)
{
    /* This is a sub-request, probably from mod_dir */
    if (r->main) {
        jk_server_conf_t *conf = (jk_server_conf_t *)
            ap_get_module_config(r->server->module_config, &jk_module);
        char *worker = (char *)ap_table_get(r->notes, JK_NOTE_WORKER_NAME);

        if (ap_table_get(r->subprocess_env, "no-jk")) {
            if (JK_IS_DEBUG_LEVEL(conf->log))
                jk_log(conf->log, JK_LOG_DEBUG,
                       "Into fixup no-jk env var detected for uri=%s, declined",
                       r->uri);
            return DECLINED;
        }

        /* Only if we have no worker and ForwardDirectories is set */
        if (!worker && (conf->options & JK_OPT_FWDDIRS)) {
            char *dummy_ptr[1], **names_ptr, *idx;
            int num_names;
            dir_config_rec *d = (dir_config_rec *)
                ap_get_module_config(r->per_dir_config, &dir_module);

            /* Direct lift from mod_dir */
            if (d->index_names) {
                names_ptr = (char **)d->index_names->elts;
                num_names = d->index_names->nelts;
            }
            else {
                dummy_ptr[0] = DEFAULT_INDEX;
                names_ptr = dummy_ptr;
                num_names = 1;
            }

            /* Where the index file would start within the filename */
            idx = r->filename + strlen(r->filename) -
                strlen(names_ptr[num_names - 1]);

            /* The requested filename has the last index file at the end */
            if (idx >= r->filename && !strcmp(idx, names_ptr[num_names - 1])) {
                r->uri = r->main->uri;  /* Trick mod_dir with URI */
                r->finfo.st_mode = S_IFREG;     /* Trick mod_dir with file stat */

                /* We'll be checking for handler in r->prev later on */
                r->main->handler = ap_pstrdup(r->pool, JK_HANDLER);

                if (JK_IS_DEBUG_LEVEL(conf->log))
                    jk_log(conf->log, JK_LOG_DEBUG, "ForwardDirectories on: %s",
                           r->uri);
            }
        }
    }

    return DECLINED;
}

static void child_exit_handler(server_rec * s, ap_pool * p)
{
    /* srevilak - refactor cleanup body to jk_generic_cleanup() */
    jk_generic_cleanup(s);
    jk_shm_close();
}

static void child_init_handler(server_rec * s, ap_pool * p)
{
    int rc;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    JK_TRACE_ENTER(conf->log);

    if ((rc = jk_shm_attach(jk_shm_file, jk_shm_size, conf->log)) != 0)
        jk_log(conf->log, JK_LOG_ERROR, "Attaching shm:%s errno=%d",
               jk_shm_name(), rc);

    JK_TRACE_EXIT(conf->log);

}


/** srevilak -- registered as a cleanup handler in jk_init */
static void jk_server_cleanup(void *data)
{
    jk_generic_cleanup((server_rec *) data);
    jk_shm_close();
}


/** BEGIN SREVILAK
 * body taken from exit_handler()
 */
static void jk_generic_cleanup(server_rec *s)
{

    if (jk_worker_properties) {
        jk_map_free(&jk_worker_properties);
        jk_worker_properties = NULL;
        jk_worker_file = NULL;
        jk_mount_copy_all = JK_FALSE;
    }

    /* loop through all available servers to clean up all configuration
     * records we've created
     */
    while (NULL != s) {
        jk_server_conf_t *conf =
            (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                      &jk_module);

        if (conf && conf->was_initialized == JK_TRUE) {
            /* On pool cleanup pass NULL for the jk_logger to
               prevent segmentation faults on Windows because
               we can't guarantee what order pools get cleaned
               up between APR implementations. */
            wc_close(NULL);
            if (conf->uri_to_context) {
                jk_map_free(&conf->uri_to_context);
                /* We cannot have allocated uw_map
                 * unless we've allocated uri_to_context
                 */
                if (conf->uw_map)
                    uri_worker_map_free(&conf->uw_map, NULL);
            }
            conf->was_initialized = JK_FALSE;
        }
        s = s->next;
    }
}

/** END SREVILAK **/


static const handler_rec jk_handlers[] = {
    {JK_MAGIC_TYPE, jk_handler},
    {JK_HANDLER, jk_handler},
    {NULL}
};

module MODULE_VAR_EXPORT jk_module = {
    STANDARD_MODULE_STUFF,
    jk_init,                    /* module initializer */
    NULL,                       /* per-directory config creator */
    NULL,                       /* dir config merger */
    create_jk_config,           /* server config creator */
    merge_jk_config,            /* server config merger */
    jk_cmds,                    /* command table */
    jk_handlers,                /* [7] list of handlers */
    jk_translate,               /* [2] filename-to-URI translation */
    NULL,                       /* [5] check/validate user_id */
    NULL,                       /* [6] check user_id is valid *here* */
    NULL,                       /* [4] check access by host address */
    NULL,                       /* XXX [7] MIME type checker/setter */
    jk_fixups,                  /* [8] fixups */
    NULL,                       /* [10] logger */
    NULL,                       /* [3] header parser */
    child_init_handler,         /* apache child process initializer */
    child_exit_handler,         /* apache child process exit/cleanup */
    NULL                        /* [1] post read_request handling */
#ifdef EAPI
        /*
         * Extended module APIs, needed when using SSL.
         * STDC say that we do not have to have them as NULL but
         * why take a chance
         */
        , NULL,                 /* add_module */
    NULL,                       /* remove_module */
    NULL,                       /* rewrite_command */
    NULL,                       /* new_connection */
    NULL                        /* close_connection */
#endif /* EAPI */
};
