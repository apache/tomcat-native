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
 * Description: Apache 2 plugin for Jakarta/Tomcat                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 *              Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/

/*
 * mod_jk: keeps all servlet/jakarta related ramblings together.
 */

#include "ap_config.h"
#include "apr_lib.h"
#include "apr_date.h"
#include "apr_file_info.h"
#include "apr_file_io.h"
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#include "util_script.h"

#ifndef AS400
#include "ap_mpm.h"
#endif

#ifdef AS400
#include "ap_charset.h"
#include "util_charset.h"       /* ap_hdrs_from_ascii */
#endif

/* moved to apr since http-2.0.19-dev */
#if (MODULE_MAGIC_NUMBER_MAJOR < 20010523)
#define apr_date_parse_http ap_parseHTTPdate
#include "util_date.h"
#endif

/* deprecated with apr 0.9.3 */

/*
   The latest Apache 2.0.47 for iSeries didn't export apr_filepath_name_get
   but apr_filename_of_pathname, even if includes seems right and the APR
   in use is 0.9.4
*/

#include "apr_version.h"
#if (APR_MAJOR_VERSION == 0) && \
    (APR_MINOR_VERSION <= 9) && \
    (APR_PATCH_VERSION < 3) || defined(AS400)
#define apr_filepath_name_get apr_filename_of_pathname
#endif

#include "apr_strings.h"

#if APR_USE_SYSVSEM_SERIALIZE
#include "unixd.h"      /* for unixd_set_global_mutex_perms */
#endif
/*
 * Jakarta (jk_) include files
 */
#ifdef NETWARE
#define __sys_types_h__
#define __sys_socket_h__
#define __netdb_h__
#define __netinet_in_h__
#define __arpa_inet_h__
#define __sys_timeval_h__
#endif

#include "jk_ajp13.h"
#include "jk_global.h"
#include "jk_logger.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_service.h"
#include "jk_uri_worker_map.h"
#include "jk_util.h"
#include "jk_worker.h"
#include "jk_shm.h"

#define JK_WORKER_ID        ("jakarta.worker")
#define JK_HANDLER          ("jakarta-servlet")
#define JK_DURATION         ("jakarta.worker.duration")
#define JK_MAGIC_TYPE       ("application/x-jakarta-servlet")
#define NULL_FOR_EMPTY(x)   ((x && !strlen(x)) ? NULL : x)

/*
 * If you are not using SSL, comment out the following line. It will make
 * apache run faster.
 *
 * Personally, I (DM), think this may be a lie.
 */
#define ADD_SSL_INFO

/* module MODULE_VAR_EXPORT jk_module; */
AP_MODULE_DECLARE_DATA module jk_module;

typedef struct
{

    /*
     * Log stuff
     */
    char *log_file;
    int log_level;
    jk_logger_t *log;
    apr_file_t *jklogfp;

    /*
     * Worker stuff
     */
    jk_map_t *worker_properties;
    char *worker_file;
    jk_map_t *uri_to_context;

    int mountcopy;
    char *secret_key;
    jk_map_t *automount;

    jk_uri_worker_map_t *uw_map;

    int was_initialized;

    /*
     * Automatic context path apache alias
     */
    char *alias_dir;

    /*
     * Request Logging
     */

    char *format_string;
    apr_array_header_t *format;

    /*
     * SSL Support
     */
    int ssl_enable;
    char *https_indicator;
    char *certs_indicator;
    char *cipher_indicator;
    char *session_indicator;    /* Servlet API 2.3 requirement */
    char *key_size_indicator;   /* Servlet API 2.3 requirement */

    /*
     * Jk Options
     */
    int options;

    /*
     * Environment variables support
     */
    int envvars_in_use;
    apr_table_t *envvars;

    server_rec *s;
} jk_server_conf_t;

struct apache_private_data
{
    jk_pool_t p;

    int response_started;
    int read_body_started;
    request_rec *r;
};
typedef struct apache_private_data apache_private_data_t;

static jk_logger_t *main_log = NULL;
static jk_worker_env_t worker_env;
static apr_global_mutex_t *jk_log_lock = NULL;
static char *jk_shm_file = NULL;

static int JK_METHOD ws_start_response(jk_ws_service_t *s,
                                       int status,
                                       const char *reason,
                                       const char *const *header_names,
                                       const char *const *header_values,
                                       unsigned num_of_headers);

static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b, unsigned len, unsigned *actually_read);

static void init_jk(apr_pool_t * pconf, jk_server_conf_t * conf,
                    server_rec * s);

static int JK_METHOD ws_write(jk_ws_service_t *s, const void *b, unsigned l);


/* ========================================================================= */
/* JK Service step callbacks                                                 */
/* ========================================================================= */

static int JK_METHOD ws_start_response(jk_ws_service_t *s,
                                       int status,
                                       const char *reason,
                                       const char *const *header_names,
                                       const char *const *header_values,
                                       unsigned num_of_headers)
{
    unsigned h;
    apache_private_data_t *p = s->ws_private;
    request_rec *r = p->r;

    if (!reason) {
        reason = "";
    }
    r->status = status;
    r->status_line = apr_psprintf(r->pool, "%d %s", status, reason);

    for (h = 0; h < num_of_headers; h++) {
        if (!strcasecmp(header_names[h], "Content-type")) {
            char *tmp = apr_pstrdup(r->pool, header_values[h]);
            ap_content_type_tolower(tmp);
            /* It should be done like this in Apache 2.0 */
            /* This way, Apache 2.0 will be able to set the output filter */
            /* and it make jk useable with deflate using */
            /* AddOutputFilterByType DEFLATE text/html */
            ap_set_content_type(r, tmp);
        }
        else if (!strcasecmp(header_names[h], "Location")) {
#ifdef AS400
            /* Fix escapes in Location Header URL */
            ap_fixup_escapes((char *)header_values[h],
                             strlen(header_values[h]), ap_hdrs_from_ascii);
#endif
            apr_table_set(r->headers_out, header_names[h], header_values[h]);
        }
        else if (!strcasecmp(header_names[h], "Content-Length")) {
            apr_table_set(r->headers_out, header_names[h], header_values[h]);
        }
        else if (!strcasecmp(header_names[h], "Transfer-Encoding")) {
            apr_table_set(r->headers_out, header_names[h], header_values[h]);
        }
        else if (!strcasecmp(header_names[h], "Last-Modified")) {
            /*
             * If the script gave us a Last-Modified header, we can't just
             * pass it on blindly because of restrictions on future values.
             */
            ap_update_mtime(r, apr_date_parse_http(header_values[h]));
            ap_set_last_modified(r);
        }
        else {
            apr_table_add(r->headers_out, header_names[h], header_values[h]);
        }
    }

    /* this NOP function was removed in apache 2.0 alpha14 */
    /* ap_send_http_header(r); */
    p->response_started = JK_TRUE;

    return JK_TRUE;
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
#ifdef AS400
            int long rv = OK;
            if (rv = ap_change_request_body_xlate(p->r, 65535, 65535)) {        /* turn off request body translation */
                ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0,
                             NULL,
                             "mod_jk: Error on ap_change_request_body_xlate, rc=%d",
                             rv);
                return JK_FALSE;
            }
#else
            long rv;
#endif

            if ((rv = ap_get_client_block(p->r, b, len)) < 0) {
                *actually_read = 0;
            }
            else {
                *actually_read = (unsigned)rv;
            }
            return JK_TRUE;
        }
    }
    return JK_FALSE;
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
/* Works with 4096, fails with 8192 */
#ifndef CHUNK_SIZE
#define CHUNK_SIZE 4096
#endif

static int JK_METHOD ws_write(jk_ws_service_t *s, const void *b, unsigned l)
{
#ifdef AS400
    int rc;
#endif

    if (s && s->ws_private && b) {
        apache_private_data_t *p = s->ws_private;

        if (l) {
            /* BUFF *bf = p->r->connection->client; */
            size_t r = 0;
            long ll = l;
            char *bb = (char *)b;

            if (!p->response_started) {
                if (JK_IS_DEBUG_LEVEL(main_log))
                    jk_log(main_log, JK_LOG_DEBUG,
                           "Write without start, starting with defaults");
                if (!s->start_response(s, 200, NULL, NULL, NULL, 0)) {
                    return JK_FALSE;
                }
            }
            if (p->r->header_only) {
#ifndef AS400
                ap_rflush(p->r);
#endif
                return JK_TRUE;
            }
#ifdef AS400
            rc = ap_change_response_body_xlate(p->r, 65535, 65535);     /* turn off response body translation */
            if (rc) {
                ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0,
                             NULL,
                             "mod_jk: Error on ap_change_response_body_xlate, rc=%d",
                             rc);
                return JK_FALSE;
            }
#endif

            /* Debug - try to get around rwrite */
            while (ll > 0) {
                size_t toSend = (ll > CHUNK_SIZE) ? CHUNK_SIZE : ll;
                r = ap_rwrite((const char *)bb, toSend, p->r);
                jk_log(main_log, JK_LOG_DEBUG,
                       "writing %ld (%ld) out of %ld", toSend, r, ll);
                ll -= CHUNK_SIZE;
                bb += CHUNK_SIZE;

                if (toSend != r) {
                    return JK_FALSE;
                }

            }

            /*
             * To allow server push. After writing full buffers
             */
#ifndef AS400
            if (ap_rflush(p->r) != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0,
                             NULL, "mod_jk: Error flushing");
                return JK_FALSE;
            }
#endif

        }

        return JK_TRUE;
    }
    return JK_FALSE;
}

/* ========================================================================= */
/* Utility functions                                                         */
/* ========================================================================= */

/* ========================================================================= */
/* Log something to Jk log file then exit */
static void jk_error_exit(const char *file,
                          int line,
                          int level,
                          const server_rec * s,
                          apr_pool_t * p, const char *fmt, ...)
{
    va_list ap;
    char *res;

    va_start(ap, fmt);
    res = apr_pvsprintf(s->process->pool, fmt, ap);
    va_end(ap);

    ap_log_error(file, line, level, 0, s, res);

    /* Exit process */
    exit(1);
}

static int get_content_length(request_rec * r)
{
    if (r->clength > 0) {
        return (int)r->clength;
    }
    else if (r->main == NULL || r->main == r) {
        char *lenp = (char *)apr_table_get(r->headers_in, "Content-Length");

        if (lenp) {
            int rc = atoi(lenp);
            if (rc > 0) {
                return rc;
            }
        }
    }

    return 0;
}

static int init_ws_service(apache_private_data_t * private_data,
                           jk_ws_service_t *s, jk_server_conf_t * conf)
{
    request_rec *r = private_data->r;

    char *ssl_temp = NULL;
    s->jvm_route = NULL;        /* Used for sticky session routing */

    /* Copy in function pointers (which are really methods) */
    s->start_response = ws_start_response;
    s->read = ws_read;
    s->write = ws_write;

    /* Clear RECO status */
    s->reco_status = RECO_NONE;

    s->auth_type = NULL_FOR_EMPTY(r->ap_auth_type);
    s->remote_user = NULL_FOR_EMPTY(r->user);

    s->protocol = r->protocol;
    s->remote_host = (char *)ap_get_remote_host(r->connection,
                                                r->per_dir_config,
                                                REMOTE_HOST, NULL);
    s->remote_host = NULL_FOR_EMPTY(s->remote_host);
    s->remote_addr = NULL_FOR_EMPTY(r->connection->remote_ip);

    /* Dump all connection param so we can trace what's going to
     * the remote tomcat
     */
    if (JK_IS_DEBUG_LEVEL(conf->log))
        jk_log(conf->log, JK_LOG_DEBUG,
               "agsp=%u agsn=%s hostn=%s shostn=%s cbsport=%d sport=%d claport=%d",
               ap_get_server_port(r),
               ap_get_server_name(r) != NULL ? ap_get_server_name(r) : "",
               r->hostname != NULL ? r->hostname : "",
               r->server->server_hostname !=
               NULL ? r->server->server_hostname : "",
               r->connection->base_server->port, r->server->port,
               r->connection->local_addr->port);

    /* get server name */
    s->server_name = (char *)ap_get_server_name(r);

    /* get the real port (otherwise redirect failed) */
    /* XXX: use apache API for getting server port
     *
     * Pre 1.2.7 versions used:
     * s->server_port = r->connection->local_addr->port;
     */
    s->server_port  = ap_get_server_port(r);

    s->server_software = (char *)ap_get_server_version();
    s->method = (char *)r->method;
    s->content_length = get_content_length(r);
    s->is_chunked = r->read_chunked;
    s->no_more_chunks = 0;
#ifdef AS400
    /* Get the query string that is not translated to EBCDIC  */
    s->query_string = ap_get_original_query_string(r);
#else
    s->query_string = r->args;
#endif

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

    case JK_OPT_FWDURIESCAPED:
        s->req_uri = ap_escape_uri(r->pool, r->uri);
        break;

    default:
        return JK_FALSE;
    }

    s->is_ssl = JK_FALSE;
    s->ssl_cert = NULL;
    s->ssl_cert_len = 0;
    s->ssl_cipher = NULL;       /* required by Servlet 2.3 Api,
                                   allready in original ajp13 */
    s->ssl_session = NULL;
    s->ssl_key_size = -1;       /* required by Servlet 2.3 Api, added in jtc */

    if (conf->ssl_enable || conf->envvars_in_use) {
        ap_add_common_vars(r);

        if (conf->ssl_enable) {
            ssl_temp =
                (char *)apr_table_get(r->subprocess_env,
                                      conf->https_indicator);
            if (ssl_temp && !strcasecmp(ssl_temp, "on")) {
                s->is_ssl = JK_TRUE;
                s->ssl_cert =
                    (char *)apr_table_get(r->subprocess_env,
                                          conf->certs_indicator);
                if (s->ssl_cert) {
                    s->ssl_cert_len = strlen(s->ssl_cert);
                }
                /* Servlet 2.3 API */
                s->ssl_cipher =
                    (char *)apr_table_get(r->subprocess_env,
                                          conf->cipher_indicator);
                s->ssl_session =
                    (char *)apr_table_get(r->subprocess_env,
                                          conf->session_indicator);

                if (conf->options & JK_OPT_FWDKEYSIZE) {
                    /* Servlet 2.3 API */
                    ssl_temp = (char *)apr_table_get(r->subprocess_env,
                                                     conf->
                                                     key_size_indicator);
                    if (ssl_temp)
                        s->ssl_key_size = atoi(ssl_temp);
                }
            }
        }

        if (conf->envvars_in_use) {
            const apr_array_header_t *t = apr_table_elts(conf->envvars);
            if (t && t->nelts) {
                int i;
                apr_table_entry_t *elts = (apr_table_entry_t *) t->elts;
                s->attributes_names = apr_palloc(r->pool,
                                                 sizeof(char *) * t->nelts);
                s->attributes_values = apr_palloc(r->pool,
                                                  sizeof(char *) * t->nelts);

                for (i = 0; i < t->nelts; i++) {
                    s->attributes_names[i] = elts[i].key;
                    s->attributes_values[i] =
                        (char *)apr_table_get(r->subprocess_env, elts[i].key);
                    if (!s->attributes_values[i]) {
                        s->attributes_values[i] = elts[i].val;
                    }
                }

                s->num_attributes = t->nelts;
            }
        }
    }

    s->headers_names = NULL;
    s->headers_values = NULL;
    s->num_headers = 0;
    if (r->headers_in && apr_table_elts(r->headers_in)) {
        int need_content_length_header = (!s->is_chunked
                                          && s->content_length ==
                                          0) ? JK_TRUE : JK_FALSE;
        const apr_array_header_t *t = apr_table_elts(r->headers_in);
        if (t && t->nelts) {
            int i;
            apr_table_entry_t *elts = (apr_table_entry_t *) t->elts;
            s->num_headers = t->nelts;
            /* allocate an extra header slot in case we need to add a content-length header */
            s->headers_names =
                apr_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
            s->headers_values =
                apr_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
            if (!s->headers_names || !s->headers_values)
                return JK_FALSE;
            for (i = 0; i < t->nelts; i++) {
                char *hname = apr_pstrdup(r->pool, elts[i].key);
                s->headers_values[i] = apr_pstrdup(r->pool, elts[i].val);
                s->headers_names[i] = hname;
                if (need_content_length_header &&
                    !strcasecmp(s->headers_values[i], "content-length")) {
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
            s->headers_names = apr_palloc(r->pool, sizeof(char *));
            s->headers_values = apr_palloc(r->pool, sizeof(char *));
            if (!s->headers_names || !s->headers_values)
                return JK_FALSE;
            s->headers_names[0] = "content-length";
            s->headers_values[0] = "0";
            s->num_headers++;
        }
    }
    s->uw_map = conf->uw_map;
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
 * JkMountCopy On/Off
 */

static const char *jk_set_mountcopy(cmd_parms * cmd, void *dummy, int flag)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    /* Set up our value */
    conf->mountcopy = flag ? JK_TRUE : JK_FALSE;

    return NULL;
}

/*
 * JkMount directive handling
 *
 * JkMount URI(context) worker
 */

static const char *jk_mount_context(cmd_parms * cmd,
                                    void *dummy,
                                    const char *context,
                                    const char *worker)
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

    uri = apr_pstrcat(cmd->temp_pool, "!", c, NULL);
    /*
     * Add the new worker to the alias map.
     */
    jk_map_put(conf->uri_to_context, uri, w, NULL);
    return NULL;
}


/*
 * JkAutoMount directive handling
 *
 * JkAutoMount worker [virtualhost]
 * This is an experimental and undocumented extension made in j-t-c/jk.
 */
static const char *jk_automount_context(cmd_parms * cmd,
                                        void *dummy,
                                        const char *worker,
                                        const char *virtualhost)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    /*
     * Add the new automount to the auto map.
     */
    jk_map_put(conf->automount, worker, virtualhost, NULL);
    return NULL;
}

/*
 * JkWorkersFile Directive Handling
 *
 * JkWorkersFile file
 */

static const char *jk_set_worker_file(cmd_parms * cmd,
                                      void *dummy, const char *worker_file)
{
    server_rec *s = cmd->server;
    struct stat statbuf;

    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    /* we need an absolut path (ap_server_root_relative does the ap_pstrdup) */
    conf->worker_file = ap_server_root_relative(cmd->pool, worker_file);

    if (conf->worker_file == NULL)
        return "JkWorkersFile file_name invalid";

    if (stat(conf->worker_file, &statbuf) == -1)
        return "Can't find the workers file specified";

    return NULL;
}

/*
 * JkLogFile Directive Handling
 *
 * JkLogFile file
 */

static const char *jk_set_log_file(cmd_parms * cmd,
                                   void *dummy, const char *log_file)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    /* we need an absolute path */
    if (*log_file != '|')
        conf->log_file = ap_server_root_relative(cmd->pool, log_file);
    else
        conf->log_file = apr_pstrdup(cmd->pool, log_file);

    if (conf->log_file == NULL)
        return "JkLogFile file_name invalid";

    return NULL;
}

/*
 * JkShmFile Directive Handling
 *
 * JkShmFile file
 */

static const char *jk_set_shm_file(cmd_parms * cmd,
                                   void *dummy, const char *shm_file)
{
    /* we need an absolute path */
    jk_shm_file = ap_server_root_relative(cmd->pool, shm_file);
    if (jk_shm_file == NULL)
        return "JkShmFile file name invalid";

    return NULL;
}

/*
 * JkLogLevel Directive Handling
 *
 * JkLogLevel debug/info/error/emerg
 */

static const char *jk_set_log_level(cmd_parms * cmd,
                                    void *dummy, const char *log_level)
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
                                  void *dummy, const char *log_format)
{
    jk_set_log_format(log_format);
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
    apr_array_header_t *format = conf->format;

    strs = apr_palloc(r->pool, sizeof(char *) * (format->nelts));
    strl = apr_palloc(r->pool, sizeof(int) * (format->nelts));
    items = (request_log_format_item *) format->elts;
    for (i = 0; i < format->nelts; ++i) {
        strs[i] = process_item(r, &items[i]);
    }
    for (i = 0; i < format->nelts; ++i) {
        len += strl[i] = strlen(strs[i]);
    }
    str = apr_palloc(r->pool, len + 1);
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

static char *format_integer(apr_pool_t * p, int i)
{
    return apr_psprintf(p, "%d", i);
}

static char *pfmt(apr_pool_t * p, int i)
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
    return apr_table_get(r->notes, JK_WORKER_ID);
}


static const char *log_request_duration(request_rec * r, char *a)
{
    return apr_table_get(r->notes, JK_DURATION);
}

static const char *log_request_line(request_rec * r, char *a)
{
    /* NOTE: If the original request contained a password, we
     * re-write the request line here to contain XXXXXX instead:
     * (note the truncation before the protocol string for HTTP/0.9 requests)
     * (note also that r->the_request contains the unmodified request)
     */
    return (r->parsed_uri.password) ? apr_pstrcat(r->pool, r->method, " ",
                                                  apr_uri_unparse(r->pool,
                                                                  &r->
                                                                  parsed_uri,
                                                                  0),
                                                  r->
                                                  assbackwards ? NULL : " ",
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
    return apr_psprintf(r->pool, "%u",
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
    return (r->args != NULL) ? apr_pstrcat(r->pool, "?", r->args, NULL)
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
        return apr_off_t_toa(r->pool, r->bytes_sent);
    }
}

static const char *log_bytes_sent(request_rec * r, char *a)
{
    if (!r->sent_bodyct) {
        return "0";
    }
    else {
        return apr_psprintf(r->pool, "%" APR_OFF_T_FMT, r->bytes_sent);
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

static char *parse_request_log_misc_string(apr_pool_t * p,
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
    it->arg = apr_palloc(p, s - *sa + 1);

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

static char *parse_request_log_item(apr_pool_t * p,
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
        return apr_pstrcat(p, "Unrecognized JkRequestLogFormat directive %",
                           dummy, NULL);
    }
    it->func = l->func;
    *sa = s;
    return NULL;
}

static apr_array_header_t *parse_request_log_string(apr_pool_t * p,
                                                    const char *s,
                                                    const char **err)
{
    apr_array_header_t *a =
        apr_array_make(p, 15, sizeof(request_log_format_item));
    char *res;

    while (*s) {
        if ((res =
             parse_request_log_item(p,
                                    (request_log_format_item *)
                                    apr_array_push(a), &s))) {
            *err = res;
            return NULL;
        }
    }

    s = "\n";
    parse_request_log_item(p, (request_log_format_item *) apr_array_push(a),
                           &s);
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
    const char *err_string = NULL;
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->format_string = apr_pstrdup(cmd->pool, format);
    if (format != NULL) {
        conf->format =
            parse_request_log_string(cmd->pool, format, &err_string);
    }
    if (conf->format == NULL)
        return "JkRequestLogFormat format array NULL";

    return err_string;
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
                                          void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->https_indicator = apr_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * JkCERTSIndicator Directive Handling
 *
 * JkCERTSIndicator SSL_CLIENT_CERT
 */

static const char *jk_set_certs_indicator(cmd_parms * cmd,
                                          void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->certs_indicator = apr_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * JkCIPHERIndicator Directive Handling
 *
 * JkCIPHERIndicator SSL_CIPHER
 */

static const char *jk_set_cipher_indicator(cmd_parms * cmd,
                                           void *dummy, const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->cipher_indicator = apr_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * JkSESSIONIndicator Directive Handling
 *
 * JkSESSIONIndicator SSL_SESSION_ID
 */

static const char *jk_set_session_indicator(cmd_parms * cmd,
                                            void *dummy,
                                            const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->session_indicator = apr_pstrdup(cmd->pool, indicator);

    return NULL;
}

/*
 * JkKEYSIZEIndicator Directive Handling
 *
 * JkKEYSIZEIndicator SSL_CIPHER_USEKEYSIZE
 */

static const char *jk_set_key_size_indicator(cmd_parms * cmd,
                                             void *dummy,
                                             const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->key_size_indicator = apr_pstrdup(cmd->pool, indicator);

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

        if (!strcasecmp(w, "ForwardKeySize")) {
            opt = JK_OPT_FWDKEYSIZE;
        }
        else if (!strcasecmp(w, "ForwardURICompat")) {
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
        else if (!strcasecmp(w, "ForwardDirectories")) {
            opt = JK_OPT_FWDDIRS;
        }
        else
            return apr_pstrcat(cmd->pool, "JkOptions: Illegal option '", w,
                               "'", NULL);

        conf->options &= ~mask;

        if (action == '-') {
            conf->options &= ~opt;
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
                                  const char *env_name,
                                  const char *default_value)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                  &jk_module);

    conf->envvars_in_use = JK_TRUE;

    apr_table_add(conf->envvars, env_name, default_value);

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

    if (jk_map_read_property(conf->worker_properties, line) == JK_FALSE)
        return apr_pstrcat(cmd->temp_pool, "Invalid JkWorkerProperty ", line);

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
    AP_INIT_TAKE1("JkWorkersFile", jk_set_worker_file, NULL, RSRC_CONF,
                  "the name of a worker file for the Jakarta servlet containers"),

    /*
     * JkAutoMount specifies that the list of handled URLs must be
     * asked to the servlet engine (autoconf feature)
     */
    AP_INIT_TAKE12("JkAutoMount", jk_automount_context, NULL, RSRC_CONF,
                   "automatic mount points to a Tomcat worker"),

    /*
     * JkMount mounts a url prefix to a worker (the worker need to be
     * defined in the worker properties file.
     */
    AP_INIT_TAKE12("JkMount", jk_mount_context, NULL, RSRC_CONF|ACCESS_CONF,
                   "A mount point from a context to a Tomcat worker"),

    /*
     * JkUnMount unmounts a url prefix to a worker (the worker need to be
     * defined in the worker properties file.
     */
    AP_INIT_TAKE12("JkUnMount", jk_unmount_context, NULL, RSRC_CONF|ACCESS_CONF,
                   "A no mount point from a context to a Tomcat worker"),

    /*
     * JkMountCopy specifies if mod_jk should copy the mount points
     * from the main server to the virtual servers.
     */
    AP_INIT_FLAG("JkMountCopy", jk_set_mountcopy, NULL, RSRC_CONF,
                 "Should the base server mounts be copied to the virtual server"),

    /*
     * JkLogFile & JkLogLevel specifies to where should the plugin log
     * its information and how much.
     * JkLogStampFormat specify the time-stamp to be used on log
     */
    AP_INIT_TAKE1("JkLogFile", jk_set_log_file, NULL, RSRC_CONF,
                  "Full path to the Jakarta Tomcat module log file"),

    AP_INIT_TAKE1("JkShmFile", jk_set_shm_file, NULL, RSRC_CONF,
                  "Full path to the Jakarta Tomcat module shared memory file"),

    AP_INIT_TAKE1("JkLogLevel", jk_set_log_level, NULL, RSRC_CONF,
                  "The Jakarta Tomcat module log level, can be debug, "
                  "info, error or emerg"),
    AP_INIT_TAKE1("JkLogStampFormat", jk_set_log_fmt, NULL, RSRC_CONF,
                  "The Jakarta Tomcat module log format, follow strftime synthax"),
    AP_INIT_TAKE1("JkRequestLogFormat", jk_set_request_log_format, NULL,
                  RSRC_CONF,
                  "The Jakarta mod_jk module request log format string"),

    /*
     * Automatically Alias webapp context directories into the Apache
     * document space.
     */
    AP_INIT_TAKE1("JkAutoAlias", jk_set_auto_alias, NULL, RSRC_CONF,
                  "The Jakarta mod_jk module automatic context apache alias directory"),

    /*
     * Apache has multiple SSL modules (for example apache_ssl, stronghold
     * IHS ...). Each of these can have a different SSL environment names
     * The following properties let the administrator specify the envoiroment
     * variables names.
     *
     * HTTPS - indication for SSL
     * CERTS - Base64-Der-encoded client certificates.
     * CIPHER - A string specifing the ciphers suite in use.
     * KEYSIZE - Size of Key used in dialogue (#bits are secure)
     * SESSION - A string specifing the current SSL session.
     */
    AP_INIT_TAKE1("JkHTTPSIndicator", jk_set_https_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains SSL indication"),
    AP_INIT_TAKE1("JkCERTSIndicator", jk_set_certs_indicator, NULL, RSRC_CONF,
                  "Name of the Apache environment that contains SSL client certificates"),
    AP_INIT_TAKE1("JkCIPHERIndicator", jk_set_cipher_indicator, NULL,
                  RSRC_CONF,
                  "Name of the Apache environment that contains SSL client cipher"),
    AP_INIT_TAKE1("JkSESSIONIndicator", jk_set_session_indicator, NULL,
                  RSRC_CONF,
                  "Name of the Apache environment that contains SSL session"),
    AP_INIT_TAKE1("JkKEYSIZEIndicator", jk_set_key_size_indicator, NULL,
                  RSRC_CONF,
                  "Name of the Apache environment that contains SSL key size in use"),
    AP_INIT_FLAG("JkExtractSSL", jk_set_enable_ssl, NULL, RSRC_CONF,
                 "Turns on SSL processing and information gathering by mod_jk"),

    /*
     * Options to tune mod_jk configuration
     * for now we understand :
     * +ForwardSSLKeySize        => Forward SSL Key Size, to follow 2.3 specs but may broke old TC 3.2
     * -ForwardSSLKeySize        => Don't Forward SSL Key Size, will make mod_jk works with all TC release
     *  ForwardURICompat         => Forward URI normally, less spec compliant but mod_rewrite compatible (old TC)
     *  ForwardURICompatUnparsed => Forward URI as unparsed, spec compliant but broke mod_rewrite (old TC)
     *  ForwardURIEscaped        => Forward URI escaped and Tomcat (3.3 rc2) stuff will do the decoding part
     */
    AP_INIT_RAW_ARGS("JkOptions", jk_set_options, NULL, RSRC_CONF,
                     "Set one of more options to configure the mod_jk module"),

    /*
     * JkEnvVar let user defines envs var passed from WebServer to
     * Servlet Engine
     */
    AP_INIT_TAKE2("JkEnvVar", jk_add_env_var, NULL, RSRC_CONF,
                  "Adds a name of environment variable that should be sent "
                  "to servlet-engine"),

    AP_INIT_RAW_ARGS("JkWorkerProperty", jk_set_worker_property,
                     NULL, RSRC_CONF,
                     "Set workers.properties formated directive"),

    {NULL}
};

/* ========================================================================= */
/* The JK module handlers                                                    */
/* ========================================================================= */

/** Util - cleanup endpoint.
 */
apr_status_t jk_cleanup_endpoint(void *data)
{
    jk_endpoint_t *end = (jk_endpoint_t *)data;
    /*     printf("XXX jk_cleanup1 %ld\n", data); */
    end->done(&end, NULL);
    return 0;
}

/** Util - cleanup shmem.
 */
apr_status_t jk_cleanup_shmem(void *data)
{
    jk_logger_t *l = data;
    jk_shm_close();
    if (l && JK_IS_DEBUG_LEVEL(l))
       jk_log(l, JK_LOG_DEBUG, "Shmem cleanup");

    return 0;
}

/** Main service method, called to forward a request to tomcat
 */
static int jk_handler(request_rec * r)
{
    const char *worker_name;
    jk_server_conf_t *xconf;
    jk_server_conf_t *conf;
    int rc, dmt = 1;

    /* We do DIR_MAGIC_TYPE here to make sure TC gets all requests, even
     * if they are directory requests, in case there are no static files
     * visible to Apache and/or DirectoryIndex was not used. This is only
     * used when JkOptions has ForwardDirectories set. */
    /* Not for me, try next handler */
    if (strcmp(r->handler, JK_HANDLER)
        && (dmt = strcmp(r->handler, DIR_MAGIC_TYPE)))
        return DECLINED;

    xconf =
        (jk_server_conf_t *) ap_get_module_config(r->server->module_config,
                                                  &jk_module);
    JK_TRACE_ENTER(xconf->log);
    if (apr_table_get(r->subprocess_env, "no-jk")) {
        jk_log(xconf->log, JK_LOG_DEBUG,
               "Into handler no-jk env var detected for uri=%s, declined",
               r->uri);

        JK_TRACE_EXIT(xconf->log);
        return DECLINED;
    }

    /* Was the option to forward directories to Tomcat set? */
    if (!dmt && !(xconf->options & JK_OPT_FWDDIRS)) {
        JK_TRACE_EXIT(xconf->log);
        return DECLINED;
    }
    worker_name = apr_table_get(r->notes, JK_WORKER_ID);

    /* Set up r->read_chunked flags for chunked encoding, if present */
    if ((rc = ap_setup_client_block(r, REQUEST_CHUNKED_DECHUNK)) != APR_SUCCESS) {
        JK_TRACE_EXIT(xconf->log);
        return rc;
    }

    if (worker_name == NULL) {
        /* we may be here because of a manual directive ( that overrides
           translate and
           sets the handler directly ). We still need to know the worker.
         */
        if (worker_env.num_of_workers == 1) {
          /** We have a single worker ( the common case ).
              ( lb is a bit special, it should count as a single worker but
              I'm not sure how ). We also have a manual config directive that
              explicitely give control to us. */
            worker_name = worker_env.worker_list[0];
            if (JK_IS_DEBUG_LEVEL(xconf->log))
                jk_log(xconf->log, JK_LOG_DEBUG,
                       "Manual configuration for %s %s %d",
                       r->uri, worker_env.worker_list[0],
                       worker_env.num_of_workers);
        }
        else {
            worker_name = map_uri_to_worker(xconf->uw_map, r->uri, xconf->log);
            if (worker_name == NULL && worker_env.num_of_workers) {
                worker_name = worker_env.worker_list[0];
                if (JK_IS_DEBUG_LEVEL(xconf->log))
                    jk_log(xconf->log, JK_LOG_DEBUG,
                           "Manual configuration for %s %d",
                           r->uri, worker_env.worker_list[0]);
            }
        }
    }

    if (JK_IS_DEBUG_LEVEL(xconf->log))
       jk_log(xconf->log, JK_LOG_DEBUG, "Into handler %s worker=%s"
              " r->proxyreq=%d",
              r->handler, worker_name, r->proxyreq);

    conf = (jk_server_conf_t *) ap_get_module_config(r->server->module_config,
                                                     &jk_module);

    /* If this is a proxy request, we'll notify an error */
    if (r->proxyreq) {
        jk_log(xconf->log, JK_LOG_INFO, "Proxy request for worker=%s"
              " is not allowed",
              worker_name);
        JK_TRACE_EXIT(xconf->log);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    if (conf && !worker_name) {
        /* Direct mapping ( via setHandler ). Try overrides */
        char *uri = apr_pstrdup(r->pool, r->uri);
        worker_name = map_uri_to_worker(conf->uw_map, uri, conf->log);
        if (!worker_name) {
            /* Since we are here, an explicit (native) mapping has been used */
            /* Use default worker */
            worker_name = "ajp14";      /* XXX add a directive for default */
        }
        if (worker_name) {
            apr_table_setn(r->notes, JK_WORKER_ID, worker_name);
        }
    }

    if (worker_name) {
        jk_worker_t *worker = wc_get_worker_for_name(worker_name, xconf->log);

        /* If the remote client has aborted, just ignore the request */
        if (r->connection->aborted) {
            jk_log(xconf->log, JK_LOG_INFO, "Client connection aborted for"
                   " worker=%s",
                   worker_name);
            JK_TRACE_EXIT(xconf->log);
            return OK;
        }

        if (worker) {
#ifndef NO_GETTIMEOFDAY
            struct timeval tv_begin, tv_end;
#endif
            int rc = JK_FALSE;
            apache_private_data_t private_data;
            jk_ws_service_t s;
            jk_pool_atom_t buf[SMALL_POOL_SIZE];
            jk_open_pool(&private_data.p, buf, sizeof(buf));

            private_data.response_started = JK_FALSE;
            private_data.read_body_started = JK_FALSE;
            private_data.r = r;

            jk_init_ws_service(&s);
            /* Update retries for this worker */
            s.retries = worker->retries;
            s.ws_private = &private_data;
            s.pool = &private_data.p;
#ifndef NO_GETTIMEOFDAY
            if (conf->format != NULL) {
                gettimeofday(&tv_begin, NULL);
            }
#endif

            if (init_ws_service(&private_data, &s, conf)) {
                jk_endpoint_t *end = NULL;

                /* Use per/thread pool ( or "context" ) to reuse the
                   endpoint. It's a bit faster, but I don't know
                   how to deal with load balancing - but it's usefull for JNI
                 */

                /* worker->get_endpoint might fail if we are out of memory so check */
                /* and handle it */
                if (worker->get_endpoint(worker, &end, xconf->log)) {
                    int is_recoverable_error = JK_FALSE;
                    rc = end->service(end, &s, xconf->log,
                                      &is_recoverable_error);

                    if (s.content_read < s.content_length ||
                        (s.is_chunked && !s.no_more_chunks)) {

                        /*
                         * If the servlet engine didn't consume all of the
                         * request data, consume and discard all further
                         * characters left to read from client
                         */
                        char *buff = apr_palloc(r->pool, 2048);
                        if (buff != NULL) {
                            int rd;
                            while ((rd =
                                    ap_get_client_block(r, buff, 2048)) > 0) {
                                s.content_read += rd;
                            }
                        }
                    }
                    end->done(&end, xconf->log);
                }
                else {            /* this means we couldn't get an endpoint */
                    jk_log(xconf->log, JK_LOG_ERROR, "Could not get endpoint"
                           " for worker=%s",
                           worker_name);
                    rc = 0;       /* just to make sure that we know we've failed */
                }
            }
            else {
                jk_log(xconf->log, JK_LOG_ERROR, "Could not init service"
                       " for worker=%s",
                       worker_name);
                JK_TRACE_EXIT(xconf->log);
                return HTTP_INTERNAL_SERVER_ERROR;
            }
#ifndef NO_GETTIMEOFDAY
            if (conf->format != NULL) {
                char *duration = NULL;
                long micro, seconds;
                gettimeofday(&tv_end, NULL);
                if (tv_end.tv_usec < tv_begin.tv_usec) {
                    tv_end.tv_usec += 1000000;
                    tv_end.tv_sec--;
                }
                micro = tv_end.tv_usec - tv_begin.tv_usec;
                seconds = tv_end.tv_sec - tv_begin.tv_sec;
                duration = apr_psprintf(r->pool, "%.1ld.%.6ld", seconds, micro);
                apr_table_setn(r->notes, JK_DURATION, duration);
                request_log_transaction(r, conf);
            }
#endif

            jk_close_pool(&private_data.p);

            if (rc > 0) {
                /* If tomcat returned no body and the status is not OK,
                   let apache handle the error code */
                if (!r->sent_bodyct && r->status >= HTTP_BAD_REQUEST) {
                    jk_log(xconf->log, JK_LOG_INFO, "No body with status=%d"
                           " for worker=%s",
                           r->status, worker_name);
                    JK_TRACE_EXIT(xconf->log);
                    return r->status;
                }
                if (JK_IS_DEBUG_LEVEL(xconf->log))
                    jk_log(xconf->log, JK_LOG_DEBUG, "Service finished"
                           " with status=%d for worker=%s",
                           r->status, worker_name);
                JK_TRACE_EXIT(xconf->log);
                return OK;      /* NOT r->status, even if it has changed. */
            }
            else if (rc == JK_CLIENT_ERROR) {
                r->connection->aborted = 1;
                jk_log(xconf->log, JK_LOG_INFO, "Aborting connection"
                       " for worker=%s",
                       worker_name);
                JK_TRACE_EXIT(xconf->log);
                return OK;
            }
            else {
                jk_log(xconf->log, JK_LOG_INFO, "Service error=%d"
                       " for worker=%s",
                       rc, worker_name);
                JK_TRACE_EXIT(xconf->log);
                return HTTP_INTERNAL_SERVER_ERROR;
            }
        }
        else {
            jk_log(xconf->log, JK_LOG_INFO, "Could not find a worker"
                   " for worker name=%s",
                   worker_name);
            JK_TRACE_EXIT(xconf->log);
            return HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    JK_TRACE_EXIT(xconf->log);
    return DECLINED;
}

/** Standard apache hook, cleanup jk
 */
static apr_status_t jk_apr_pool_cleanup(void *data)
{
    server_rec *s = data;

    while (NULL != s) {
        jk_server_conf_t *conf =
            (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                      &jk_module);

        if (conf) {
            /* On pool cleanup pass NULL for the jk_logger to
               prevent segmentation faults on Windows because
               we can't guarantee what order pools get cleaned
               up between APR implementations. */
            wc_close(NULL);
            if (conf->worker_properties)
                jk_map_free(&conf->worker_properties);
            if (conf->uri_to_context)
                jk_map_free(&conf->uri_to_context);
            if (conf->automount)
                jk_map_free(&conf->automount);
            if (conf->uw_map)
                uri_worker_map_free(&conf->uw_map, NULL);
        }
        s = s->next;
    }
    return APR_SUCCESS;
}

/** Create default jk_config. XXX This is mostly server-independent,
    all servers are using something similar - should go to common.
 */
static void *create_jk_config(apr_pool_t * p, server_rec * s)
{
    jk_server_conf_t *c =
        (jk_server_conf_t *) apr_pcalloc(p, sizeof(jk_server_conf_t));

    c->worker_properties = NULL;
    jk_map_alloc(&c->worker_properties);
    c->worker_file = NULL;
    c->log_file = NULL;
    c->log_level = -1;
    c->log = NULL;
    c->alias_dir = NULL;
    c->format_string = NULL;
    c->format = NULL;
    c->mountcopy = JK_FALSE;
    c->was_initialized = JK_FALSE;
    c->options = JK_OPT_FWDURIDEFAULT;

    /*
     * By default we will try to gather SSL info.
     * Disable this functionality through JkExtractSSL
     */
    c->ssl_enable = JK_TRUE;
    /*
     * The defaults ssl indicators match those in mod_ssl (seems
     * to be in more use).
     */
    c->https_indicator = "HTTPS";
    c->certs_indicator = "SSL_CLIENT_CERT";

    /*
     * The following (comented out) environment variables match apache_ssl!
     * If you are using apache_sslapache_ssl uncomment them (or use the
     * configuration directives to set them.
     *
     c->cipher_indicator = "HTTPS_CIPHER";
     c->session_indicator = NULL;
     */

    /*
     * The following environment variables match mod_ssl! If you
     * are using another module (say apache_ssl) comment them out.
     */
    c->cipher_indicator = "SSL_CIPHER";
    c->session_indicator = "SSL_SESSION_ID";
    c->key_size_indicator = "SSL_CIPHER_USEKEYSIZE";

    if (!jk_map_alloc(&(c->uri_to_context))) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG, s, p, "Memory error");
    }
    if (!jk_map_alloc(&(c->automount))) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG, s, p, "Memory error");
    }

    c->uw_map = NULL;
    c->secret_key = NULL;

    c->envvars_in_use = JK_FALSE;
    c->envvars = apr_table_make(p, 0);

    c->s = s;

    apr_pool_cleanup_register(p, s, jk_apr_pool_cleanup, jk_apr_pool_cleanup);
    return c;
}


/** Utility - copy a map . XXX Should move to jk_map, it's generic code.
 */
static void copy_jk_map(apr_pool_t * p, server_rec * s, jk_map_t *src,
                        jk_map_t *dst)
{
    int sz = jk_map_size(src);
    int i;
    for (i = 0; i < sz; i++) {
        const char *name = jk_map_name_at(src, i);
        if (jk_map_get(src, name, NULL) == NULL) {
            if (!jk_map_put(dst, name,
                            apr_pstrdup(p, jk_map_get_string(src, name, NULL)),
                            NULL)) {
                jk_error_exit(APLOG_MARK, APLOG_EMERG, s, p, "Memory error");
            }
        }
    }
}

/** Standard apache callback, merge jk options specified in <Directory>
    context or <Host>.
 */
static void *merge_jk_config(apr_pool_t * p, void *basev, void *overridesv)
{
    jk_server_conf_t *base = (jk_server_conf_t *) basev;
    jk_server_conf_t *overrides = (jk_server_conf_t *) overridesv;

    if (base->ssl_enable) {
        overrides->ssl_enable = base->ssl_enable;
        overrides->https_indicator = base->https_indicator;
        overrides->certs_indicator = base->certs_indicator;
        overrides->cipher_indicator = base->cipher_indicator;
        overrides->session_indicator = base->session_indicator;
    }

    overrides->options = base->options;

    if (overrides->mountcopy) {
        copy_jk_map(p, overrides->s, base->uri_to_context,
                    overrides->uri_to_context);
        copy_jk_map(p, overrides->s, base->automount, overrides->automount);
    }

    if (base->envvars_in_use) {
        overrides->envvars_in_use = JK_TRUE;
        overrides->envvars = apr_table_overlay(p, overrides->envvars,
                                               base->envvars);
    }

    if (!uri_worker_map_alloc(&(overrides->uw_map),
                              overrides->uri_to_context, overrides->log)) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG, overrides->s,
                      overrides->s->process->pool, "Memory error");
    }

    if (base->secret_key)
        overrides->secret_key = base->secret_key;

    return overrides;
}

static int JK_METHOD jk_log_to_file(jk_logger_t *l,
                                    int level, const char *what)
{
    if (l &&
        (l->level <= level || level == JK_LOG_REQUEST_LEVEL) &&
        l->logger_private && what) {
        unsigned sz = strlen(what);
        apr_size_t wrote = sz;
        char error[256];
        if (sz) {
            apr_status_t status;
            file_logger_t *p = l->logger_private;
            if (p->jklogfp) {
                apr_status_t rv;
                rv = apr_global_mutex_lock(jk_log_lock);
                if (rv != APR_SUCCESS) {
                    ap_log_error(APLOG_MARK, APLOG_ERR, rv, NULL,
                                 "apr_global_mutex_lock(jk_log_lock) failed");
                    /* XXX: Maybe this should be fatal? */
                }
                status = apr_file_write(p->jklogfp, what, &wrote);
                if (status != APR_SUCCESS) {
                    apr_strerror(status, error, 254);
                    ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0,
                                 NULL,
                                 "mod_jk: jk_log_to_file %s failed: %s",
                                 what, error);
                }
                else
                    apr_file_putc('\n', p->jklogfp);
                rv = apr_global_mutex_unlock(jk_log_lock);
                if (rv != APR_SUCCESS) {
                    ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, NULL,
                                  "apr_global_mutex_unlock(jk_log_lock) failed");
                    /* XXX: Maybe this should be fatal? */
                }
            }
        }

        return JK_TRUE;
    }

    return JK_FALSE;
}

/*
** +-------------------------------------------------------+
** |                                                       |
** |              jk logfile support                       |
** |                                                       |
** +-------------------------------------------------------+
*/

static apr_status_t jklog_cleanup(void *d)
{
    /* set the main_log to NULL */
    main_log = NULL;
    return APR_SUCCESS;
}

static int open_jklog(server_rec * s, apr_pool_t * p)
{
    jk_server_conf_t *conf;
    const char *fname;
    apr_status_t rc;
    piped_log *pl;
    jk_logger_t *jkl;
    file_logger_t *flp;
    int jklog_flags = (APR_WRITE | APR_APPEND | APR_CREATE);
    apr_fileperms_t jklog_mode =
        (APR_UREAD | APR_UWRITE | APR_GREAD | APR_WREAD);

    conf = ap_get_module_config(s->module_config, &jk_module);

    if (main_log != NULL) {
        conf->log = main_log;
        return 0;
    }
    if (conf->log_file == NULL) {
        return 0;
    }
    if (*(conf->log_file) == '\0') {
        return 0;
    }

    if (*conf->log_file == '|') {
        if ((pl = ap_open_piped_log(p, conf->log_file + 1)) == NULL) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                         "mod_jk: could not open reliable pipe "
                         "to jk log %s", conf->log_file + 1);
            return -1;
        }
        conf->jklogfp = (void *)ap_piped_log_write_fd(pl);
    }
    else {
        fname = ap_server_root_relative(p, conf->log_file);
        if (!fname) {
            ap_log_error(APLOG_MARK, APLOG_ERR, APR_EBADPATH, s,
                         "mod_jk: Invalid JkLog " "path %s", conf->log_file);
            return -1;
        }
        if ((rc = apr_file_open(&conf->jklogfp, fname,
                                jklog_flags, jklog_mode, p))
            != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_ERR, rc, s,
                         "mod_jk: could not open JkLog " "file %s", fname);
            return -1;
        }
        apr_file_inherit_set(conf->jklogfp);
    }
    jkl = (jk_logger_t *)apr_palloc(p, sizeof(jk_logger_t));
    flp = (file_logger_t *) apr_palloc(p, sizeof(file_logger_t));
    if (jkl && flp) {
        jkl->log = jk_log_to_file;
        jkl->level = conf->log_level;
        jkl->logger_private = flp;
        flp->jklogfp = conf->jklogfp;
        conf->log = jkl;
        if (main_log == NULL)
            main_log = conf->log;
        apr_pool_cleanup_register(p, main_log, jklog_cleanup, jklog_cleanup);
        return 0;
    }

    return -1;
}


/** Standard apache callback, initialize jk.
 */
static void jk_child_init(apr_pool_t * pconf, server_rec * s)
{
    jk_server_conf_t *conf;
    apr_status_t rv;
    int rc;

    conf = ap_get_module_config(s->module_config, &jk_module);

    rv = apr_global_mutex_child_init(&jk_log_lock, NULL, pconf);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, rv, s,
                     "mod_jk: could not init JK log lock in child");
    }

    JK_TRACE_ENTER(conf->log);

    if ((rc = jk_shm_attach(jk_shm_file, conf->log)) == 0) {
        if (JK_IS_DEBUG_LEVEL(conf->log))
            jk_log(conf->log, JK_LOG_DEBUG, "Attached shm:%s",
                   jk_shm_name());
            apr_pool_cleanup_register(pconf, conf->log, jk_cleanup_shmem,
                                     jk_cleanup_shmem);
    }
    else
        jk_log(conf->log, JK_LOG_ERROR, "Attachning shm:%s errno=%d",
               jk_shm_name(), rc);

    if (JK_IS_DEBUG_LEVEL(conf->log))
        jk_log(conf->log, JK_LOG_DEBUG, "Initialized %s", JK_EXPOSED_VERSION);
    JK_TRACE_EXIT(conf->log);
}

/** Initialize jk, using worker.properties.
    We also use apache commands ( JkWorker, etc), but this use is
    deprecated, as we'll try to concentrate all config in
    workers.properties, urimap.properties, and ajp14 autoconf.

    Apache config will only be used for manual override, using
    SetHandler and normal apache directives ( but minimal jk-specific
    stuff )
*/
static void init_jk(apr_pool_t * pconf, jk_server_conf_t * conf,
                    server_rec * s)
{
    int rc;
    int mpm_threads = 1;

    /*     jk_map_t *init_map = NULL; */
    jk_map_t *init_map = conf->worker_properties;

    if ((rc = jk_shm_open(jk_shm_file, conf->log)) == 0) {
        if (JK_IS_DEBUG_LEVEL(conf->log))
            jk_log(conf->log, JK_LOG_DEBUG, "Initialized shm:%s",
                   jk_shm_name(), rc);
            apr_pool_cleanup_register(pconf, conf->log, jk_cleanup_shmem,
                                      jk_cleanup_shmem);
    }
    else
        jk_log(conf->log, JK_LOG_ERROR, "Initializing shm:%s errno=%d",
               jk_shm_name(), rc);

    /* Set default connection cache size for worker mpm */
#if APR_HAS_THREADS
#ifndef AS400
    if (ap_mpm_query(AP_MPMQ_MAX_THREADS, &mpm_threads) != APR_SUCCESS)
        mpm_threads = 1;
#endif
#endif
     jk_set_worker_def_cache_size(mpm_threads);

    if (!uri_worker_map_alloc(&(conf->uw_map),
                              conf->uri_to_context,
                              conf->log)) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG, s, pconf, "Memory error");
    }

    /*     if(map_alloc(&init_map)) { */
    if (!jk_map_read_properties(init_map, conf->worker_file)) {
        if (jk_map_size(init_map) == 0) {
            ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO,
                         APLOG_EMERG, NULL,
                         "No worker file and no worker options in httpd.conf"
                         "use JkWorkerFile to set workers");
            return;
        }
    }

    /* we add the URI->WORKER MAP since workers using AJP14
       will feed it */
    worker_env.uri_to_worker = conf->uw_map;
    worker_env.virtual = "*";   /* for now */
    worker_env.server_name = (char *)ap_get_server_version();
    if (wc_open(init_map, &worker_env, conf->log)) {
        ap_add_version_component(pconf, JK_EXPOSED_VERSION);
    }
}

static int jk_post_config(apr_pool_t * pconf,
                          apr_pool_t * plog,
                          apr_pool_t * ptemp, server_rec * s)
{
    apr_status_t rv;

    /* create the jk log lockfiles in the parent */
    if ((rv = apr_global_mutex_create(&jk_log_lock, NULL,
                                      APR_LOCK_DEFAULT,
                                      pconf)) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, rv, s,
                     "mod_jk: could not create jk_log_lock");
        return HTTP_INTERNAL_SERVER_ERROR;
    }

#if APR_USE_SYSVSEM_SERIALIZE
    rv = unixd_set_global_mutex_perms(jk_log_lock);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, rv, s,
                     "mod_jk: Could not set permissions on "
                     "jk_log_lock; check User and Group directives");
        return HTTP_INTERNAL_SERVER_ERROR;
    }
#endif

    if (!s->is_virtual) {
        jk_server_conf_t *conf =
            (jk_server_conf_t *) ap_get_module_config(s->module_config,
                                                      &jk_module);
        if (!conf->was_initialized) {
            server_rec *srv = s;
            conf->was_initialized = JK_TRUE;
            /* step through the servers and open each jk logfile.
             */
            for (; srv; srv = srv->next) {
                if (open_jklog(srv, pconf))
                    return HTTP_INTERNAL_SERVER_ERROR;
            }
            init_jk(pconf, conf, s);
        }
    }

    return OK;
}

/** Use the internal mod_jk mappings to find if this is a request for
 *    tomcat and what worker to use.
 */
static int jk_translate(request_rec * r)
{
    if (!r->proxyreq) {
        jk_server_conf_t *conf =
            (jk_server_conf_t *) ap_get_module_config(r->server->
                                                      module_config,
                                                      &jk_module);

        if (conf) {
            const char *worker;
            char *uri;
            if ((r->handler != NULL) && (!strcmp(r->handler, JK_HANDLER))) {
                /* Somebody already set the handler, probably manual config
                 * or "native" configuration, no need for extra overhead
                 */
                if (JK_IS_DEBUG_LEVEL(conf->log))
                    jk_log(conf->log, JK_LOG_DEBUG,
                           "Manually mapped, no need to call uri_to_worker");
                return DECLINED;
            }

            if (apr_table_get(r->subprocess_env, "no-jk")) {
                if (JK_IS_DEBUG_LEVEL(conf->log))
                    jk_log(conf->log, JK_LOG_DEBUG,
                           "Into translate no-jk env var detected for uri=%s, declined",
                           r->uri);

                return DECLINED;
            }

            /* Special case to make sure that apache can serve a directory
               listing if there are no matches for the DirectoryIndex and
               Tomcat webapps are mapped into apache using JkAutoAlias. */
            if (r->main != NULL && (conf->alias_dir != NULL) &&
                !strcmp(r->main->handler, DIR_MAGIC_TYPE)) {

                /* Append the request uri to the JkAutoAlias directory and
                   determine if the file exists. */
                char *clean_uri;
                apr_finfo_t finfo;
                finfo.filetype = APR_NOFILE;
                clean_uri = apr_pstrdup(r->pool, r->uri);
                ap_no2slash(clean_uri);
                /* Map uri to a context static file */
                if (strlen(clean_uri) > 1) {
                    char *context_path = NULL;

                    context_path = apr_pstrcat(r->pool, conf->alias_dir,
                                               ap_os_escape_path(r->pool,
                                                                 clean_uri,
                                                                 1), NULL);
                    if (context_path != NULL) {
                        apr_stat(&finfo, context_path, APR_FINFO_TYPE,
                                 r->pool);
                    }
                }
                if (finfo.filetype != APR_REG) {
                    if (JK_IS_DEBUG_LEVEL(conf->log))
                        jk_log(conf->log, JK_LOG_DEBUG,
                               "JkAutoAlias, no DirectoryIndex file for URI %s",
                               r->uri);
                    return DECLINED;
                }
            }

            uri = apr_pstrdup(r->pool, r->uri);
            worker = map_uri_to_worker(conf->uw_map, uri, conf->log);

            if (worker) {
                r->handler = apr_pstrdup(r->pool, JK_HANDLER);
                apr_table_setn(r->notes, JK_WORKER_ID, worker);

                /* This could be a sub-request, possibly from mod_dir */
                /* Also set the HANDLER and uri for subrequest */
                if (r->main) {
                    r->main->handler = apr_pstrdup(r->main->pool, JK_HANDLER);
                    r->main->uri = apr_pstrdup(r->main->pool, r->uri);
                    apr_table_setn(r->main->notes, JK_WORKER_ID, worker);
                }

                return OK;
            }
            else if (conf->alias_dir != NULL) {
                char *clean_uri = apr_pstrdup(r->pool, r->uri);
                ap_no2slash(clean_uri);
                /* Automatically map uri to a context static file */
                if (JK_IS_DEBUG_LEVEL(conf->log))
                    jk_log(conf->log, JK_LOG_DEBUG,
                           "mod_jk::jk_translate, check alias_dir: %s",
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
                        context_dir = apr_pstrndup(r->pool, index, size);
                        /* Get the context child directory name */
                        index = index + size + 1;
                        suffix = strchr(index, '/');
                        if (suffix != NULL) {
                            size = suffix - index;
                            child_dir = apr_pstrndup(r->pool, index, size);
                        }
                        else {
                            child_dir = index;
                        }
                        /* Deny access to WEB-INF and META-INF directories */
                        if (child_dir != NULL) {
                            if (JK_IS_DEBUG_LEVEL(conf->log))
                                jk_log(conf->log, JK_LOG_DEBUG,
                                       "mod_jk::jk_translate, AutoAlias child_dir: %s",
                                       child_dir);
                            if (!strcasecmp(child_dir, "WEB-INF")
                                || !strcasecmp(child_dir, "META-INF")) {
                                if (JK_IS_DEBUG_LEVEL(conf->log))
                                    jk_log(conf->log, JK_LOG_DEBUG,
                                           "mod_jk::jk_translate, AutoAlias HTTP_FORBIDDEN for URI: %s",
                                           r->uri);
                                return HTTP_FORBIDDEN;
                            }
                        }
                    }
                    else {
                        context_dir = apr_pstrdup(r->pool, index);
                    }

                    context_path = apr_pstrcat(r->pool, conf->alias_dir,
                                               ap_os_escape_path(r->pool,
                                                                 context_dir,
                                                                 1), NULL);
                    if (context_path != NULL) {
                        apr_finfo_t finfo;
                        finfo.filetype = APR_NOFILE;
                        apr_stat(&finfo, context_path, APR_FINFO_TYPE,
                                 r->pool);
                        if (finfo.filetype == APR_DIR) {
                            char *escurl =
                                ap_os_escape_path(r->pool, clean_uri, 1);
                            char *ret =
                                apr_pstrcat(r->pool, conf->alias_dir, escurl,
                                            NULL);
                            /* Add code to verify real path ap_os_canonical_name */
                            if (ret != NULL) {
                                if (JK_IS_DEBUG_LEVEL(conf->log))
                                    jk_log(conf->log, JK_LOG_DEBUG,
                                           "mod_jk::jk_translate, AutoAlias OK for file: %s",
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
                                           "mod_jk::jk_translate, AutoAlias HTTP_FORBIDDEN for URI: %s",
                                           r->uri);
                                return HTTP_FORBIDDEN;
                            }
                        }
                    }
                }
            }
        }
    }

    return DECLINED;
}

#if (MODULE_MAGIC_NUMBER_MAJOR > 20010808)
/* bypass the directory_walk and file_walk for non-file requests */
static int jk_map_to_storage(request_rec * r)
{

    if (!r->proxyreq && !apr_table_get(r->notes, JK_WORKER_ID)) {
        jk_server_conf_t *conf =
            (jk_server_conf_t *) ap_get_module_config(r->server->
                                                      module_config,
                                                      &jk_module);

        if (conf) {
            const char *worker;
            char *uri;
            if ((r->handler != NULL) && (!strcmp(r->handler, JK_HANDLER))) {
                /* Somebody already set the handler, probably manual config
                 * or "native" configuration, no need for extra overhead
                 */
                if (JK_IS_DEBUG_LEVEL(conf->log))
                    jk_log(conf->log, JK_LOG_DEBUG,
                           "Manually mapped, no need to call uri_to_worker");
                return DECLINED;
            }

            if (apr_table_get(r->subprocess_env, "no-jk")) {
                if (JK_IS_DEBUG_LEVEL(conf->log))
                    jk_log(conf->log, JK_LOG_DEBUG,
                           "Into map_to_storage no-jk env var detected for uri=%s, declined",
                           r->uri);

                return DECLINED;
            }

            uri = apr_pstrdup(r->pool, r->uri);
            worker = map_uri_to_worker(conf->uw_map, uri, conf->log);

            if (worker) {
                r->handler = apr_pstrdup(r->pool, JK_HANDLER);
                apr_table_setn(r->notes, JK_WORKER_ID, worker);

                /* This could be a sub-request, possibly from mod_dir */
                if (r->main)
                    apr_table_setn(r->main->notes, JK_WORKER_ID, worker);

            }
        }
    }

    if (apr_table_get(r->notes, JK_WORKER_ID)) {

        /* First find just the name of the file, no directory */
        r->filename = (char *)apr_filepath_name_get(r->uri);

        /* Only if sub-request for a directory, most likely from mod_dir */
        if (r->main && r->main->filename &&
            (!apr_filepath_name_get(r->main->filename) ||
             !strlen(apr_filepath_name_get(r->main->filename)))) {

            /* The filename from the main request will be set to what should
             * be picked up, aliases included. Tomcat will need to know about
             * those aliases or things won't work for them. Normal files should
             * be fine. */

            /* Need absolute path to stat */
            if (apr_filepath_merge(&r->filename,
                                   r->main->filename, r->filename,
                                   APR_FILEPATH_SECUREROOT |
                                   APR_FILEPATH_TRUENAME, r->pool)
                != APR_SUCCESS) {
                return DECLINED;        /* We should never get here, very bad */
            }

            /* Stat the file so that mod_dir knows it's there */
            apr_stat(&r->finfo, r->filename, APR_FINFO_TYPE, r->pool);
        }

        return OK;
    }
    return DECLINED;
}
#endif

static void jk_register_hooks(apr_pool_t * p)
{
    ap_hook_handler(jk_handler, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_post_config(jk_post_config, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_child_init(jk_child_init, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_translate_name(jk_translate, NULL, NULL, APR_HOOK_MIDDLE);
#if (MODULE_MAGIC_NUMBER_MAJOR > 20010808)
    ap_hook_map_to_storage(jk_map_to_storage, NULL, NULL, APR_HOOK_MIDDLE);
#endif
}

module AP_MODULE_DECLARE_DATA jk_module = {
    STANDARD20_MODULE_STUFF,
    NULL,                       /* dir config creater */
    NULL,                       /* dir merger --- default is to override */
    create_jk_config,           /* server config */
    merge_jk_config,            /* merge server config */
    jk_cmds,                    /* command ap_table_t */
    jk_register_hooks           /* register hooks */
};
