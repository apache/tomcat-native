/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil-*- */
/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
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
 * Description: Apache 2 plugin for Jakarta/Tomcat                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 *                 Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

/*
 * mod_jk: keeps all servlet/jakarta related ramblings together.
 */

#include "apu_compat.h"
#include "ap_config.h"
#include "apr_lib.h"
#include "apr_date.h"
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#include "util_script.h"
#ifdef AS400
#include "ap_charset.h"
#include "util_charset.h"              /* ap_hdrs_from_ascii */
#endif

/* moved to apr since http-2.0.19-dev */
#if (MODULE_MAGIC_NUMBER_MAJOR < 20010523)
#include "util_date.h"
#endif

#include "apr_strings.h"
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
#include "jk_global.h"
#include "jk_util.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_uri_worker_map.h"

#define JK_WORKER_ID        ("jakarta.worker")
#define JK_HANDLER          ("jakarta-servlet")
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


typedef struct {

    /*
     * Log stuff
     */
    char *log_file;
    int  log_level;
    jk_logger_t *log;

    /*
     * Worker stuff
     */
    jk_map_t *worker_properties;
    char     *worker_file;
    jk_map_t *uri_to_context;

    int      mountcopy;
    char     *secret_key;
    jk_map_t *automount;

    jk_uri_worker_map_t *uw_map;

    int was_initialized;

    /*
     * SSL Support
     */
    int  ssl_enable;
    char *https_indicator;
    char *certs_indicator;
    char *cipher_indicator;
    char *session_indicator;  /* Servlet API 2.3 requirement */
    char *key_size_indicator; /* Servlet API 2.3 requirement */

    /*
     * Jk Options
     */
    int options;

    /*
     * Environment variables support
     */
    int envvars_in_use;
    apr_table_t * envvars;

    server_rec *s;
} jk_server_conf_t;

struct apache_private_data {
    jk_pool_t p;
    
    int response_started;
    int read_body_started;
    request_rec *r;
};
typedef struct apache_private_data apache_private_data_t;

static jk_logger_t *     main_log = NULL;
static jk_worker_env_t   worker_env;


static int JK_METHOD ws_start_response(jk_ws_service_t *s,
                                       int status,
                                       const char *reason,
                                       const char * const *header_names,
                                       const char * const *header_values,
                                       unsigned num_of_headers);

static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b,
                             unsigned len,
                             unsigned *actually_read);

static void init_jk( apr_pool_t *pconf,jk_server_conf_t *conf, server_rec *s );

static int JK_METHOD ws_write(jk_ws_service_t *s,
                              const void *b,
                              unsigned l);


/* ========================================================================= */
/* JK Service step callbacks                                                 */
/* ========================================================================= */

static int JK_METHOD ws_start_response(jk_ws_service_t *s,
                                       int status,
                                       const char *reason,
                                       const char * const *header_names,
                                       const char * const *header_values,
                                       unsigned num_of_headers)
{
    if(s && s->ws_private) {
        unsigned h;
        apache_private_data_t *p = s->ws_private;
        request_rec *r = p->r;
        
        if(!reason) {
            reason = "";
        }
        r->status = status;
        r->status_line = apr_psprintf(r->pool, "%d %s", status, reason);

        for(h = 0 ; h < num_of_headers ; h++) {
            if(!strcasecmp(header_names[h], "Content-type")) {
                char *tmp = apr_pstrdup(r->pool, header_values[h]);
                ap_content_type_tolower(tmp);
                r->content_type = tmp;
            } else if(!strcasecmp(header_names[h], "Location")) {
#ifdef AS400 
                /* Fix escapes in Location Header URL*/
                ap_fixup_escapes((char *)header_values[h], 
                strlen(header_values[h]), ap_hdrs_from_ascii);
#endif 
                apr_table_set(r->headers_out, 
                              header_names[h], header_values[h]);
            } else if(!strcasecmp(header_names[h], "Content-Length")) {
                apr_table_set(r->headers_out, 
                              header_names[h], header_values[h]);
            } else if(!strcasecmp(header_names[h], "Transfer-Encoding")) {
                apr_table_set(r->headers_out, 
                              header_names[h], header_values[h]);
            } else if(!strcasecmp(header_names[h], "Last-Modified")) {
                /*
                 * If the script gave us a Last-Modified header, we can't just
                 * pass it on blindly because of restrictions on future values.
                 */
                ap_update_mtime(r, ap_parseHTTPdate(header_values[h]));
                ap_set_last_modified(r);
            } else {                
                apr_table_add(r->headers_out, 
                              header_names[h], header_values[h]);
            }
        }

        /* this NOP function was removed in apache 2.0 alpha14 */
        /* ap_send_http_header(r); */
        p->response_started = JK_TRUE;
        
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
                             void *b,
                             unsigned len,
                             unsigned *actually_read)
{
    if(s && s->ws_private && b && actually_read) {
        apache_private_data_t *p = s->ws_private;
        if(!p->read_body_started) {
           if(ap_should_client_block(p->r)) {
                p->read_body_started = JK_TRUE;
            }
        }

        if(p->read_body_started) {
#ifdef AS400
    int long rv = OK;
    if (rv = ap_change_request_body_xlate(p->r, 65535, 65535)) /* turn off request body translation*/
    {		 
        ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, 
                     NULL, "mod_jk: Error on ap_change_request_body_xlate, rc=%d \n", rv);
        return JK_FALSE;
    }
#else
    long rv;
#endif

            if ((rv = ap_get_client_block(p->r, b, len)) < 0) {
                *actually_read = 0;
            } else {
                *actually_read = (unsigned) rv;
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

static int JK_METHOD ws_write(jk_ws_service_t *s,
                              const void *b,
                              unsigned l)
{
#ifdef AS400
    int rc;
#endif

    if(s && s->ws_private && b) {
        apache_private_data_t *p = s->ws_private;

        if(l) {
            /* BUFF *bf = p->r->connection->client; */
            size_t w = (size_t)l;
            size_t r = 0;
            long ll=l;
            char *bb=(char *)b;
            
            if(!p->response_started) {
                jk_log(main_log, JK_LOG_DEBUG, 
                       "Write without start, starting with defaults\n");
                if(!s->start_response(s, 200, NULL, NULL, NULL, 0)) {
                    return JK_FALSE;
                }
            }
#ifdef AS400
            rc = ap_change_response_body_xlate(p->r, 65535, 65535); /* turn off response body translation*/
	    if(rc){
                ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, 
                             NULL, "mod_jk: Error on ap_change_response_body_xlate, rc=%d \n", rc);
                 return JK_FALSE;	     
            }
#endif
            
            /* Debug - try to get around rwrite */
            while( ll > 0 ) {
                size_t toSend=(ll>CHUNK_SIZE) ? CHUNK_SIZE : ll;
                r = ap_rwrite((const char *)bb, toSend, p->r );
                jk_log(main_log, JK_LOG_DEBUG, 
                       "writing %ld (%ld) out of %ld \n",toSend, r, ll );
                ll-=CHUNK_SIZE;
                bb+=CHUNK_SIZE;
                
                if(toSend != r) { 
                    return JK_FALSE; 
                } 
                
            }

            /*
             * To allow server push. After writing full buffers
             */
#ifndef AS400
            if(ap_rflush(p->r) != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, 
                             NULL, "mod_jk: Error flushing \n"  );
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
                          const server_rec *s, 
                          apr_pool_t *p, 
                          const char *fmt, ...)
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

static int get_content_length(request_rec *r)
{
    if(r->clength > 0) {
        return r->clength;
    } else {
        char *lenp = (char *)apr_table_get(r->headers_in, "Content-Length");

        if(lenp) {
            int rc = atoi(lenp);
            if(rc > 0) {
                return rc;
            }
        }
    }

    return 0;
}

static int init_ws_service(apache_private_data_t *private_data,
                           jk_ws_service_t *s,
                           jk_server_conf_t *conf)
{
    request_rec *r      = private_data->r;

    apr_port_t port;

    char *ssl_temp      = NULL;
    s->jvm_route        = NULL;    /* Used for sticky session routing */

    /* Copy in function pointers (which are really methods) */
    s->start_response   = ws_start_response;
    s->read             = ws_read;
    s->write            = ws_write;

    s->auth_type    = NULL_FOR_EMPTY(r->ap_auth_type);
    s->remote_user  = NULL_FOR_EMPTY(r->user);

    s->protocol     = r->protocol;
    s->remote_host  = (char *)ap_get_remote_host(r->connection, 
                                                 r->per_dir_config, 
                                                 REMOTE_HOST, NULL);
    s->remote_host  = NULL_FOR_EMPTY(s->remote_host);
    s->remote_addr  = NULL_FOR_EMPTY(r->connection->remote_ip);

    jk_log(main_log, JK_LOG_DEBUG, 
                 "agsp=%u agsn=%s hostn=%s shostn=%s cbsport=%d sport=%d \n",
                ap_get_server_port( r ),
                ap_get_server_name( r ),
                r->hostname,
                r->server->server_hostname,
                r->connection->base_server->port,
                r->server->port
                );

    /* get server name */
    /* s->server_name= (char *)(r->hostname ? r->hostname : r->server->server_hostname); */
    /* XXX : à la jk2 */
	s->server_name  = ap_get_server_name(r);

    /* get the real port (otherwise redirect failed) */
    /* apr_sockaddr_port_get(&port,r->connection->local_addr); */
    /* s->server_port = port; */
    /* XXX : à la jk2 */
	s->server_port  = ap_get_server_port(r);

    s->server_software = (char *)ap_get_server_version();

    s->method         = (char *)r->method;
    s->content_length = get_content_length(r);
    s->is_chunked     = r->read_chunked;
    s->no_more_chunks = 0;
#ifdef AS400
    /* Get the query string that is not translated to EBCDIC  */
    s->query_string   = ap_get_original_query_string(r); 
#else
    s->query_string   = r->args;
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

        case JK_OPT_FWDURICOMPATUNPARSED :
            s->req_uri      = r->unparsed_uri;
            if (s->req_uri != NULL) {
                char *query_str = strchr(s->req_uri, '?');
                if (query_str != NULL) {
                    *query_str = 0;
                }
            }

        break;

        case JK_OPT_FWDURICOMPAT :
            s->req_uri = r->uri;
        break;

        case JK_OPT_FWDURIESCAPED :
            s->req_uri      = ap_escape_uri(r->pool, r->uri);
        break;

        default :
            return JK_FALSE;
    }

    s->is_ssl       = JK_FALSE;
    s->ssl_cert     = NULL;
    s->ssl_cert_len = 0;
    s->ssl_cipher   = NULL;        /* required by Servlet 2.3 Api, 
                                   allready in original ajp13 */
    s->ssl_session  = NULL;
    s->ssl_key_size = -1;        /* required by Servlet 2.3 Api, added in jtc */

    if(conf->ssl_enable || conf->envvars_in_use) {
        ap_add_common_vars(r);

        if(conf->ssl_enable) {
            ssl_temp = 
                (char *)apr_table_get(r->subprocess_env, 
                                      conf->https_indicator);
            if(ssl_temp && !strcasecmp(ssl_temp, "on")) {
                s->is_ssl       = JK_TRUE;
                s->ssl_cert     = 
                    (char *)apr_table_get(r->subprocess_env, 
                                          conf->certs_indicator);
                if(s->ssl_cert) {
                    s->ssl_cert_len = strlen(s->ssl_cert);
                }
                /* Servlet 2.3 API */
                s->ssl_cipher   = 
                    (char *)apr_table_get(r->subprocess_env, 
                                          conf->cipher_indicator);
                s->ssl_session  = 
                    (char *)apr_table_get(r->subprocess_env, 
                                          conf->session_indicator);

                if (conf->options & JK_OPT_FWDKEYSIZE) {
                    /* Servlet 2.3 API */
                    ssl_temp = (char *)apr_table_get(r->subprocess_env, 
                                                 conf->key_size_indicator);
                    if (ssl_temp)
                        s->ssl_key_size = atoi(ssl_temp);
                }
            }
        }

        if(conf->envvars_in_use) {
            const apr_array_header_t *t = apr_table_elts(conf->envvars);
            if(t && t->nelts) {
                int i;
                apr_table_entry_t *elts = (apr_table_entry_t *)t->elts;
                s->attributes_names = apr_palloc(r->pool, 
                                                 sizeof(char *) * t->nelts);
                s->attributes_values = apr_palloc(r->pool, 
                                                  sizeof(char *) * t->nelts);

                for(i = 0 ; i < t->nelts ; i++) {
                    s->attributes_names[i] = elts[i].key;
                    s->attributes_values[i] = 
                        (char *)apr_table_get(r->subprocess_env, elts[i].key);
                    if(!s->attributes_values[i]) {
                        s->attributes_values[i] = elts[i].val;
                    }
                }

                s->num_attributes = t->nelts;
            }
        }
    }

    s->headers_names    = NULL;
    s->headers_values   = NULL;
    s->num_headers      = 0;
    if(r->headers_in && apr_table_elts(r->headers_in)) {
        int need_content_length_header = (!s->is_chunked && s->content_length == 0) ? JK_TRUE : JK_FALSE;
        const apr_array_header_t *t = apr_table_elts(r->headers_in);
        if(t && t->nelts) {
            int i;
            apr_table_entry_t *elts = (apr_table_entry_t *)t->elts;
            s->num_headers = t->nelts;
            /* allocate an extra header slot in case we need to add a content-length header */
            s->headers_names  = apr_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
            s->headers_values = apr_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
            if(!s->headers_names || !s->headers_values)
                return JK_FALSE;
            for(i = 0 ; i < t->nelts ; i++) {
                char *hname = apr_pstrdup(r->pool, elts[i].key);
                s->headers_values[i] = apr_pstrdup(r->pool, elts[i].val);
                s->headers_names[i] = hname;
                while(*hname) {
                    *hname = tolower(*hname);
                    hname++;
                }
                if(need_content_length_header &&
                        !strncmp(s->headers_values[i],"content-length",14)) {
                    need_content_length_header = JK_FALSE;
                }
            }
            /* Add a content-length = 0 header if needed.
             * Ajp13 assumes an absent content-length header means an unknown,
             * but non-zero length body.
             */
            if(need_content_length_header) {
                s->headers_names[s->num_headers] = "content-length";
                s->headers_values[s->num_headers] = "0";
                s->num_headers++;
            }
        }
        /* Add a content-length = 0 header if needed.*/
        else if (need_content_length_header) {
            s->headers_names  = apr_palloc(r->pool, sizeof(char *));
            s->headers_values = apr_palloc(r->pool, sizeof(char *));
            if(!s->headers_names || !s->headers_values)
                return JK_FALSE;
            s->headers_names[0] = "content-length";
            s->headers_values[0] = "0";
            s->num_headers++;
        }
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
 * JkMountCopy On/Off
 */

static const char *jk_set_mountcopy(cmd_parms *cmd, 
                                    void *dummy, 
                                    int flag) 
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);
    
    /* Set up our value */
    conf->mountcopy = flag ? JK_TRUE : JK_FALSE;

    return NULL;
}

/*
 * JkMount directive handling
 *
 * JkMount URI(context) worker
 */

static const char *jk_mount_context(cmd_parms *cmd, 
                                    void *dummy, 
                                    const char *context,
                                    const char *worker,
                                    const char *maybe_cookie)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);
    char *old;
    if (context[0]!='/')
        return "Context should start with /";

    /*
     * Add the new worker to the alias map.
     */
    map_put(conf->uri_to_context, context, worker, (void **)&old);
    return NULL;
}


/*
 * JkAutoMount directive handling
 *
 * JkAutoMount worker [virtualhost]
 * This is an experimental and undocumented extension made in j-t-c/jk.
 */
static const char *jk_automount_context(cmd_parms *cmd,
                                        void *dummy,
                                        const char *worker,
                                        const char *virtualhost)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    /*
     * Add the new automount to the auto map.
     */
    char * old;
    map_put(conf->automount, worker, virtualhost, (void **)&old);
    return NULL;
}

/*
 * JkWorkersFile Directive Handling
 *
 * JkWorkersFile file
 */

static const char *jk_set_worker_file(cmd_parms *cmd, 
                                      void *dummy, 
                                      const char *worker_file)
{
    server_rec *s = cmd->server;
    struct stat statbuf;

    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    /* we need an absolut path (ap_server_root_relative does the ap_pstrdup) */
    conf->worker_file = ap_server_root_relative(cmd->pool,worker_file);
 
    if (conf->worker_file == NULL)
        return "JkWorkersFile file_name invalid";

    if (stat(conf->worker_file, &statbuf) == -1)
        return "Can't find the workers file specified";

    return NULL;
}

/*
 * JkWorker name value
 * This is an experimental and undocumented extension made in j-t-c/jk.
 */
static const char *jk_worker_property(cmd_parms *cmd,
                                      void *dummy,
                                      const char *name,
                                      const char *value)
{
    server_rec *s = cmd->server;
    struct stat statbuf;
    char *oldv;
    int rc;

    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);
    
    jk_map_t *m=conf->worker_properties;
    
    value = map_replace_properties(value, m );

    oldv = map_get_string(m, name, NULL);

    if(oldv) {
        char *tmpv = apr_palloc(cmd->pool,
                                strlen(value) + strlen(oldv) + 3);
        if(tmpv) {
            char sep = '*';
            if(jk_is_path_poperty(name)) {
                sep = PATH_SEPERATOR;
            } else if(jk_is_cmd_line_poperty(name)) {
                sep = ' ';
            }
            
            sprintf(tmpv, "%s%c%s", 
                    oldv, sep, value);
        }                                
        value = tmpv;
    } else {
        value = ap_pstrdup(cmd->pool, value);
    }
    
    if(value) {
        void *old = NULL;
        map_put(m, name, value, &old);
        /*printf("Setting %s %s\n", name, value);*/
    } 
    return NULL;
}


/*
 * JkLogFile Directive Handling
 *
 * JkLogFile file
 */

static const char *jk_set_log_file(cmd_parms *cmd, 
                                   void *dummy, 
                                   const char *log_file)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    /* we need an absolut path */
    conf->log_file = ap_server_root_relative(cmd->pool,log_file);
 
    if (conf->log_file == NULL)
        return "JkLogFile file_name invalid";

    return NULL;
}

/*
 * JkLogLevel Directive Handling
 *
 * JkLogLevel debug/info/error/emerg
 */

static const char *jk_set_log_level(cmd_parms *cmd, 
                                    void *dummy, 
                                    const char *log_level)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    conf->log_level = jk_parse_log_level(log_level);

    return NULL;
}

/*
 * JkLogStampFormat Directive Handling
 *
 * JkLogStampFormat "[%a %b %d %H:%M:%S %Y] "
 */

static const char * jk_set_log_fmt(cmd_parms *cmd,
                      void *dummy,
                      const char * log_format)
{
    jk_set_log_format(log_format);
    return NULL;
}

/*
 * JkExtractSSL Directive Handling
 *
 * JkExtractSSL On/Off
 */

static const char *jk_set_enable_ssl(cmd_parms *cmd,
                                     void *dummy,
                                     int flag)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);
   
    /* Set up our value */
    conf->ssl_enable = flag ? JK_TRUE : JK_FALSE;

    return NULL;
}

/*
 * JkHTTPSIndicator Directive Handling
 *
 * JkHTTPSIndicator HTTPS
 */

static const char *jk_set_https_indicator(cmd_parms *cmd,
                                          void *dummy,
                                          const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    conf->https_indicator = ap_pstrdup(cmd->pool,indicator);

    return NULL;
}

/*
 * JkCERTSIndicator Directive Handling
 *
 * JkCERTSIndicator SSL_CLIENT_CERT
 */

static const char *jk_set_certs_indicator(cmd_parms *cmd,
                                          void *dummy,
                                          const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    conf->certs_indicator = ap_pstrdup(cmd->pool,indicator);

    return NULL;
}

/*
 * JkCIPHERIndicator Directive Handling
 *
 * JkCIPHERIndicator SSL_CIPHER
 */

static const char *jk_set_cipher_indicator(cmd_parms *cmd,
                                           void *dummy,
                                           const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    conf->cipher_indicator = ap_pstrdup(cmd->pool,indicator);

    return NULL;
}

/*
 * JkSESSIONIndicator Directive Handling
 *
 * JkSESSIONIndicator SSL_SESSION_ID
 */

static const char *jk_set_session_indicator(cmd_parms *cmd,
                                           void *dummy,
                                           const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    conf->session_indicator = ap_pstrdup(cmd->pool,indicator);

    return NULL;
}

/*
 * JkKEYSIZEIndicator Directive Handling
 *
 * JkKEYSIZEIndicator SSL_CIPHER_USEKEYSIZE
 */

static const char *jk_set_key_size_indicator(cmd_parms *cmd,
                                           void *dummy,
                                           const char *indicator)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    conf->key_size_indicator = ap_pstrdup(cmd->pool,indicator);

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

const char *jk_set_options(cmd_parms *cmd,
                           void *dummy,
                           const char *line)
{
    int  opt = 0;
    int  mask = 0;
    char action;
    char *w;

    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

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
            return ap_pstrcat(cmd->pool, "JkOptions: Illegal option '", w, "'", NULL);

        conf->options &= ~mask;

        if (action == '-') {
            conf->options &= ~opt;
        }
        else if (action == '+') {
            conf->options |=  opt;
        }
        else {            /* for now +Opt == Opt */
            conf->options |=  opt;
        }
    }
    return NULL;
}

/*
 * JkEnvVar Directive Handling
 *
 * JkEnvVar MYOWNDIR
 */

static const char *jk_add_env_var(cmd_parms *cmd,
                                  void *dummy,
                                  const char *env_name,
                                  const char *default_value)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    conf->envvars_in_use = JK_TRUE;

    apr_table_add(conf->envvars, env_name, default_value);

    return NULL;
}
    
static const command_rec jk_cmds[] =
    {
    /*
     * JkWorkersFile specifies a full path to the location of the worker
     * properties file.
     *
     * This file defines the different workers used by apache to redirect
     * servlet requests.
     */
    AP_INIT_TAKE1(
        "JkWorkersFile", jk_set_worker_file, NULL, RSRC_CONF,
        "the name of a worker file for the Jakarta servlet containers"),

    /*
     * JkWorker allows you to specify worker properties in server.xml.
     * They are added before any property in JkWorkersFile ( if any ), 
     * as a more convenient way to configure
     */
    AP_INIT_TAKE2(
        "JkWorker", jk_worker_property, NULL, RSRC_CONF,
        "worker property"),

    /*
     * JkAutoMount specifies that the list of handled URLs must be
     * asked to the servlet engine (autoconf feature)
     */
    AP_INIT_TAKE12(
        "JkAutoMount", jk_automount_context, NULL, RSRC_CONF,
        "automatic mount points to a Tomcat worker"),

    /*
     * JkMount mounts a url prefix to a worker (the worker need to be
     * defined in the worker properties file.
     */
    AP_INIT_TAKE23(
        "JkMount", jk_mount_context, NULL, RSRC_CONF,
        "A mount point from a context to a Tomcat worker"),

    /*
     * JkMountCopy specifies if mod_jk should copy the mount points
     * from the main server to the virtual servers.
     */
    AP_INIT_FLAG(
        "JkMountCopy", jk_set_mountcopy, NULL, RSRC_CONF,
        "Should the base server mounts be copied to the virtual server"),

    /*
     * JkLogFile & JkLogLevel specifies to where should the plugin log
     * its information and how much.
     * JkLogStampFormat specify the time-stamp to be used on log
     */
    AP_INIT_TAKE1(
        "JkLogFile", jk_set_log_file, NULL, RSRC_CONF,
        "Full path to the Jakarta Tomcat module log file"),
    AP_INIT_TAKE1(
        "JkLogLevel", jk_set_log_level, NULL, RSRC_CONF,
        "The Jakarta Tomcat module log level, can be debug, "
        "info, error or emerg"),
    AP_INIT_TAKE1(
        "JkLogStampFormat", jk_set_log_fmt, NULL, RSRC_CONF,
        "The Jakarta Tomcat module log format, follow strftime synthax"),

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
    AP_INIT_TAKE1(
        "JkHTTPSIndicator", jk_set_https_indicator, NULL, RSRC_CONF,
        "Name of the Apache environment that contains SSL indication"),
    AP_INIT_TAKE1(
        "JkCERTSIndicator", jk_set_certs_indicator, NULL, RSRC_CONF,
        "Name of the Apache environment that contains SSL client certificates"),
    AP_INIT_TAKE1(
        "JkCIPHERIndicator", jk_set_cipher_indicator, NULL, RSRC_CONF,
        "Name of the Apache environment that contains SSL client cipher"),
    AP_INIT_TAKE1(
        "JkSESSIONIndicator", jk_set_session_indicator, NULL, RSRC_CONF,
        "Name of the Apache environment that contains SSL session"),
    AP_INIT_TAKE1(
        "JkKEYSIZEIndicator", jk_set_key_size_indicator, NULL, RSRC_CONF,
        "Name of the Apache environment that contains SSL key size in use"),
    AP_INIT_FLAG(
        "JkExtractSSL", jk_set_enable_ssl, NULL, RSRC_CONF,
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
    AP_INIT_RAW_ARGS(
        "JkOptions", jk_set_options, NULL, RSRC_CONF, 
        "Set one of more options to configure the mod_jk module"),

    /*
     * JkEnvVar let user defines envs var passed from WebServer to
     * Servlet Engine
     */
    AP_INIT_TAKE2(
        "JkEnvVar", jk_add_env_var, NULL, RSRC_CONF,
        "Adds a name of environment variable that should be sent "
        "to servlet-engine"),

    {NULL}
};

/* ========================================================================= */
/* The JK module handlers                                                    */
/* ========================================================================= */

/** Util - cleanup endpoint.
 */
apr_status_t jk_cleanup_endpoint( void *data ) {
    jk_endpoint_t *end = (jk_endpoint_t *)data;    
    /*     printf("XXX jk_cleanup1 %ld\n", data); */
    end->done(&end, NULL);  
    return 0;
}

/** Main service method, called to forward a request to tomcat
 */
static int jk_handler(request_rec *r)
{   
    const char       *worker_name;
    jk_server_conf_t *xconf;
    jk_logger_t      *xl;
    jk_server_conf_t *conf;
    int              rc,dmt=1;

    /* We do DIR_MAGIC_TYPE here to make sure TC gets all requests, even
     * if they are directory requests, in case there are no static files
     * visible to Apache and/or DirectoryIndex was not used. This is only
     * used when JkOptions has ForwardDirectories set. */

    /* Not for me, try next handler */
    if(strcmp(r->handler,JK_HANDLER) && (dmt=strcmp(r->handler,DIR_MAGIC_TYPE)))
        return DECLINED;

    xconf = (jk_server_conf_t *)ap_get_module_config(r->server->module_config, 
                                                     &jk_module);

    /* Was the option to forward directories to Tomcat set? */
    if(!dmt && !(xconf->options & JK_OPT_FWDDIRS))
        return DECLINED;

    worker_name = apr_table_get(r->notes, JK_WORKER_ID);
    xl = xconf->log ? xconf->log : main_log;

    /* Set up r->read_chunked flags for chunked encoding, if present */
    if(rc = ap_setup_client_block(r, REQUEST_CHUNKED_DECHUNK)) {
        return rc;
    }

    if( worker_name == NULL ) {
      /* we may be here because of a manual directive ( that overrides 
         translate and
         sets the handler directly ). We still need to know the worker.
      */
      if( worker_env.num_of_workers == 1 ) {
          /** We have a single worker ( the common case ). 
              ( lb is a bit special, it should count as a single worker but 
              I'm not sure how ). We also have a manual config directive that
              explicitely give control to us. */
          worker_name=  worker_env.first_worker;
          jk_log(xl, JK_LOG_DEBUG, 
                 "Manual configuration for %s %s %d\n",
                 r->uri, worker_env.first_worker, worker_env.num_of_workers); 
      } else {
          worker_name = map_uri_to_worker(xconf->uw_map, r->uri, 
                                          xconf->log ? xconf->log : main_log);
          if( worker_name == NULL ) 
              worker_name=  worker_env.first_worker;
          jk_log(xl, JK_LOG_DEBUG, 
                 "Manual configuration for %s %d\n",
                 r->uri, worker_env.first_worker); 
      }
    }

    if (1) {
        jk_log(xl, JK_LOG_DEBUG, "Into handler r->proxyreq=%d "
               "r->handler=%s r->notes=%d worker=%s\n", 
               r->proxyreq, r->handler, r->notes, worker_name); 
    }

    conf=(jk_server_conf_t *)ap_get_module_config(r->server->module_config, 
                                                  &jk_module);

    /* If this is a proxy request, we'll notify an error */
    if(r->proxyreq) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    if(conf && ! worker_name ) {
        /* Direct mapping ( via setHandler ). Try overrides */
        worker_name = map_uri_to_worker(conf->uw_map, r->uri, 
                                        conf->log ? conf->log : main_log);
        if( ! worker_name ) {
            /* Since we are here, an explicit (native) mapping has been used */
            /* Use default worker */
            worker_name="ajp14"; /* XXX add a directive for default */
        }
        if(worker_name) {
            apr_table_setn(r->notes, JK_WORKER_ID, worker_name);
        }
    }
      
    if(worker_name) {
        jk_logger_t *l = conf->log ? conf->log : main_log;

        jk_worker_t *worker = wc_get_worker_for_name(worker_name, l);

        if(worker) {
            int rc = JK_FALSE;
            apache_private_data_t private_data;
            jk_ws_service_t s;
            jk_pool_atom_t buf[SMALL_POOL_SIZE];
            jk_open_pool(&private_data.p, buf, sizeof(buf));

            private_data.response_started = JK_FALSE;
            private_data.read_body_started = JK_FALSE;
            private_data.r = r;

            jk_init_ws_service(&s);

            s.ws_private = &private_data;
            s.pool = &private_data.p;            
            
            if(init_ws_service(&private_data, &s, conf)) {
                jk_endpoint_t *end = NULL;

        /* Use per/thread pool ( or "context" ) to reuse the 
           endpoint. It's a bit faster, but I don't know 
           how to deal with load balancing - but it's usefull for JNI
        */

/* Disable "re-use" for now since the following code is currently platform specific */
/* #ifdef REUSE_WORKER
        apr_pool_t *rpool=r->pool;
        apr_pool_t *parent_pool= apr_pool_get_parent( rpool );
        apr_pool_t *tpool= apr_pool_get_parent( parent_pool );
        
        apr_pool_userdata_get( (void **)&end, "jk_thread_endpoint", tpool );
        if(end==NULL ) {
            worker->get_endpoint(worker, &end, l);
            apr_pool_userdata_set( end , "jk_thread_endpoint", 
                                   &jk_cleanup_endpoint,  tpool );
        }
#else */
        worker->get_endpoint(worker, &end, l);
/* #endif */
        {   
            int is_recoverable_error = JK_FALSE;
                rc = end->service(end, &s, l, &is_recoverable_error);

            if (s.content_read < s.content_length ||
                (s.is_chunked && ! s.no_more_chunks)) {

                /*
                * If the servlet engine didn't consume all of the
                * request data, consume and discard all further
                * characters left to read from client
                */
                    char *buff = apr_palloc(r->pool, 2048);
                    if (buff != NULL) {
                    int rd;
                    while ((rd = ap_get_client_block(r, buff, 2048)) > 0) {
                        s.content_read += rd;
                    }
                }
            }
                                                                            
/* #ifndef REUSE_WORKER */
            end->done(&end, l); 
/* #endif */
                }
            }

            jk_close_pool(&private_data.p);

            if(rc) {
                return OK;    /* NOT r->status, even if it has changed. */
            } else
                return HTTP_INTERNAL_SERVER_ERROR;
        }
	else
		return HTTP_INTERNAL_SERVER_ERROR;

    }

    return DECLINED;
}

/** Standard apache hook, cleanup jk
 */
static apr_status_t jk_apr_pool_cleanup(void *data)
{
    server_rec *s = data;
    
    while (NULL != s)
    {
        jk_server_conf_t *conf =
            (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);
    
    
        if (conf)
        {
            wc_close(conf->log);
            if (conf->worker_properties)
                map_free(&conf->worker_properties);
            if (conf->uri_to_context)
                map_free(&conf->uri_to_context);
            if (conf->automount)
                map_free(&conf->automount);
            if (conf->uw_map)
                uri_worker_map_free(&conf->uw_map, conf->log);
            jk_close_file_logger(&conf->log);
        }
        s = s->next;
    }
	return APR_SUCCESS;
}

/** Create default jk_config. XXX This is mostly server-independent,
    all servers are using something similar - should go to common.
 */
static void *create_jk_config(apr_pool_t *p, server_rec *s)
{
    jk_server_conf_t *c =
        (jk_server_conf_t *) apr_pcalloc(p, sizeof(jk_server_conf_t));

    c->worker_properties = NULL;
    map_alloc(& c->worker_properties);
    c->worker_file     = NULL;
    c->log_file        = NULL;
    c->log_level       = -1;
    c->log             = NULL;
    c->mountcopy       = JK_FALSE;
    c->was_initialized = JK_FALSE;
    c->options         = JK_OPT_FWDURIDEFAULT;

    /*
     * By default we will try to gather SSL info.
     * Disable this functionality through JkExtractSSL
     */
    c->ssl_enable  = JK_TRUE;
    /*
     * The defaults ssl indicators match those in mod_ssl (seems
     * to be in more use).
     */
    c->https_indicator  = "HTTPS";
    c->certs_indicator  = "SSL_CLIENT_CERT";

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

    if(!map_alloc(&(c->uri_to_context))) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG, s, p, "Memory error");
    }
    if(!map_alloc(&(c->automount))) {
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
static void copy_jk_map(apr_pool_t *p, server_rec * s, jk_map_t * src, 
                        jk_map_t * dst)
{   
    int sz = map_size(src);
        int i;
        for(i = 0 ; i < sz ; i++) {
            void *old;
            char *name = map_name_at(src, i);
            if(map_get(src, name, NULL) == NULL) {
                if(!map_put(dst, name, 
                            apr_pstrdup(p, map_get_string(src, name, NULL)), 
                            &old)) {
                    jk_error_exit(APLOG_MARK, APLOG_EMERG, s, p, 
                                  "Memory error");
                }
            } 
        }
}

/** Standard apache callback, merge jk options specified in <Directory>
    context or <Host>.
 */
static void *merge_jk_config(apr_pool_t *p, 
                             void *basev, 
                             void *overridesv)
{
    jk_server_conf_t *base = (jk_server_conf_t *) basev;
    jk_server_conf_t *overrides = (jk_server_conf_t *)overridesv;
 
    if(base->ssl_enable) {
        overrides->ssl_enable        = base->ssl_enable;
        overrides->https_indicator   = base->https_indicator;
        overrides->certs_indicator   = base->certs_indicator;
        overrides->cipher_indicator  = base->cipher_indicator;
        overrides->session_indicator = base->session_indicator;
    }

    overrides->options = base->options;

    if(overrides->mountcopy) {
        copy_jk_map(p, overrides->s, base->uri_to_context, 
                    overrides->uri_to_context);
        copy_jk_map(p, overrides->s, base->automount, overrides->automount);
    }

    if(base->envvars_in_use) {
        overrides->envvars_in_use = JK_TRUE;
        overrides->envvars = apr_table_overlay(p, overrides->envvars, 
                                               base->envvars);
    }

    if(overrides->log_file && overrides->log_level >= 0) {
        if(!jk_open_file_logger(&(overrides->log), overrides->log_file, 
                                overrides->log_level)) {
            overrides->log = NULL;
        }
    }
    if(!uri_worker_map_alloc(&(overrides->uw_map), 
                             overrides->uri_to_context, 
                             overrides->log)) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG, overrides->s, 
                      overrides->s->process->pool, "Memory error");
    }
    
    if (base->secret_key)
        overrides->secret_key = base->secret_key;

    return overrides;
}

/** Standard apache callback, initialize jk.
 */
static void jk_child_init(apr_pool_t *pconf, 
              server_rec *s)
{
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

/*     init_jk( pconf, conf, s ); */
}

/** Initialize jk, using worker.properties. 
    We also use apache commands ( JkWorker, etc), but this use is 
    deprecated, as we'll try to concentrate all config in
    workers.properties, urimap.properties, and ajp14 autoconf.
    
    Apache config will only be used for manual override, using 
    SetHandler and normal apache directives ( but minimal jk-specific
    stuff )
*/
static void init_jk( apr_pool_t *pconf, jk_server_conf_t *conf, server_rec *s ) {
    /*     jk_map_t *init_map = NULL; */
    jk_map_t *init_map = conf->worker_properties;

   if(conf->log_file && conf->log_level >= 0) {
        if(!jk_open_file_logger(&(conf->log), 
                                conf->log_file, conf->log_level)) {
            conf->log = NULL;
        } else {
            main_log = conf->log;
        }
    }
    
    if(!uri_worker_map_alloc(&(conf->uw_map), 
                             conf->uri_to_context, conf->log)) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG, s, pconf, "Memory error");
    }

    /*     if(map_alloc(&init_map)) { */
    if( ! map_read_properties(init_map, conf->worker_file)) {
        if( map_size( init_map ) == 0 ) {
            ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, APLOG_EMERG, 
                         NULL, "No worker file and no worker options in httpd.conf \n"
                          "use JkWorkerFile to set workers\n");
            return;
        }
    }
    
    /* we add the URI->WORKER MAP since workers using AJP14
       will feed it */
    worker_env.uri_to_worker = conf->uw_map;
    worker_env.virtual       = "*";     /* for now */
    worker_env.server_name   = (char *)ap_get_server_version();
    if(wc_open(init_map, &worker_env, conf->log)) {
        ap_add_version_component(pconf, JK_EXPOSED_VERSION);
        return;
    }            
    return;
}

static int jk_post_config(apr_pool_t *pconf, 
                          apr_pool_t *plog, 
                          apr_pool_t *ptemp, 
                          server_rec *s)
{
    if(!s->is_virtual) {
        jk_server_conf_t *conf =
            (jk_server_conf_t *)ap_get_module_config(s->module_config, 
                                                     &jk_module);
        if(!conf->was_initialized) {
            conf->was_initialized = JK_TRUE;        
            init_jk( pconf, conf, s );
        }
    }
    return OK;
}

/** Use the internal mod_jk mappings to find if this is a request for
 *    tomcat and what worker to use. 
 */
static int jk_translate(request_rec *r)
{    
    if(!r->proxyreq) {        
        jk_server_conf_t *conf =
            (jk_server_conf_t *)ap_get_module_config(r->server->module_config,
                                                     &jk_module);

        if(conf) {
            char *worker;
            if( (r->handler != NULL ) && 
                (! strcmp( r->handler, JK_HANDLER ) )) {
                /* Somebody already set the handler, probably manual config
                 * or "native" configuration, no need for extra overhead
                 */
                jk_log(conf->log, JK_LOG_DEBUG, 
                       "Manually mapped, no need to call uri_to_worker\n");
                return DECLINED;
            }
            worker = map_uri_to_worker(conf->uw_map, r->uri, 
                                       conf->log ? conf->log : main_log);

            if(worker) {
                r->handler=apr_pstrdup(r->pool,JK_HANDLER);
                apr_table_setn(r->notes, JK_WORKER_ID, worker);
		
                /* This could be a sub-request, possibly from mod_dir */
                if(r->main)
                    apr_table_setn(r->main->notes, JK_WORKER_ID, worker);

                return OK;
            }
        }
    }

    return DECLINED;
}

#if (MODULE_MAGIC_NUMBER_MAJOR > 20010808)
/* bypass the directory_walk and file_walk for non-file requests */
static int jk_map_to_storage(request_rec *r)
{
    if (apr_table_get(r->notes, JK_WORKER_ID)) {

        /* First find just the name of the file, no directory */
        r->filename = (char *)apr_filename_of_pathname(r->uri);

        /* Only if sub-request for a directory, most likely from mod_dir */
        if (r->main && r->main->filename &&
            !*apr_filename_of_pathname(r->main->filename)){
	
            /* The filename from the main request will be set to what should
             * be picked up, aliases included. Tomcat will need to know about
             * those aliases or things won't work for them. Normal files should
             * be fine. */

            /* Need absolute path to stat */
            if (apr_filepath_merge(&r->filename,
                                   r->main->filename, r->filename,
                                   APR_FILEPATH_SECUREROOT |
                                   APR_FILEPATH_TRUENAME,
                                   r->pool)
                != APR_SUCCESS){
              return DECLINED; /* We should never get here, very bad */
            }

            /* Stat the file so that mod_dir knows it's there */
            apr_stat(&r->finfo, r->filename, APR_FINFO_TYPE, r->pool);
        }

        return OK;
    }
    return DECLINED;
}
#endif

static void jk_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(jk_handler, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_post_config(jk_post_config,NULL,NULL,APR_HOOK_MIDDLE);
    ap_hook_child_init(jk_child_init,NULL,NULL,APR_HOOK_MIDDLE);
    ap_hook_translate_name(jk_translate,NULL,NULL,APR_HOOK_FIRST);
#if (MODULE_MAGIC_NUMBER_MAJOR > 20010808)
    ap_hook_map_to_storage(jk_map_to_storage, NULL, NULL, APR_HOOK_MIDDLE);
#endif
}

module AP_MODULE_DECLARE_DATA jk_module =
{
    STANDARD20_MODULE_STUFF,
    NULL,                /* dir config creater */
    NULL,                /* dir merger --- default is to override */
    create_jk_config,    /* server config */
    merge_jk_config,    /* merge server config */
    jk_cmds,            /* command ap_table_t */
    jk_register_hooks    /* register hooks */
};

