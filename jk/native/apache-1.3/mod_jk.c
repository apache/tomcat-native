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
 * Description: Apache 1.3 plugin for Jakarta/Tomcat                       *
 *              See ../common/jk_service.h for general mod_jk info         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 *              Dan Milstein <danmil@shore.net>                            *
 *              Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/

/*
 * mod_jk: keeps all servlet/jakarta related ramblings together.
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
 * Jakarta (jk_) include files
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

module MODULE_VAR_EXPORT jk_module;
extern module dir_module;

/*
 * Configuration object for the mod_jk module.
 */
typedef struct {

    /*
     * Log stuff
     */
    char        *log_file;
    int         log_level;
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

    /*
     * Automatic context path apache alias
     */
    char *alias_dir;

    /*
     * Request Logging
     */

    char *format_string;
    array_header *format;

    /*
     * SSL Support
     */
    int  ssl_enable;
    char *https_indicator;
    char *certs_indicator;
    char *cipher_indicator;
    char *session_indicator;
	char *key_size_indicator;

    /*
     * Jk Options
     */
    int options;

    /*
     * Environment variables support
     */
    int envvars_in_use;
    table *envvars; 

    server_rec *s;
} jk_server_conf_t;


/*
 * The "private", or subclass portion of the web server service class for
 * Apache 1.3.  An instance of this class is created for each request
 * handled.  See jk_service.h for details about the ws_service object in
 * general.  
 */
struct apache_private_data {
    /* 
     * For memory management for this request.  Aliased to be identical to
     * the pool in the superclass (jk_ws_service).  
     */
    jk_pool_t p;    
    
    /* True iff response headers have been returned to client */
    int response_started;  

    /* True iff request body data has been read from Apache */
    int read_body_started;

    /* Apache request structure */
    request_rec *r; 
};
typedef struct apache_private_data apache_private_data_t;

typedef struct dir_config_struct {
    array_header *index_names;
} dir_config_rec;

static jk_logger_t 		*main_log = NULL;
static jk_worker_env_t	 worker_env;

static int JK_METHOD ws_start_response(jk_ws_service_t *s,
                                       int status,
                                       const char *reason,
                                       const char * const *header_names,
                                       const char * const *header_values,
                                       unsigned num_of_headers);

static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b,
                             unsigned l,
                             unsigned *a);

static int JK_METHOD ws_write(jk_ws_service_t *s,
                              const void *b,
                              unsigned l);
/* srevilak - new function prototypes */
static void jk_server_cleanup(void *data);
static void jk_generic_cleanup(server_rec *s);



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
                                       const char * const *header_names,
                                       const char * const *header_values,
                                       unsigned num_of_headers)
{
    if(s && s->ws_private) {
        unsigned h;

        /* Obtain a subclass-specific "this" pointer */
        apache_private_data_t *p = s->ws_private;
        request_rec *r = p->r;
        
        if(!reason) {
            reason = "";
        }
        r->status = status;
        r->status_line = ap_psprintf(r->pool, "%d %s", status, reason);

        for(h = 0 ; h < num_of_headers ; h++) {
            if(!strcasecmp(header_names[h], "Content-type")) {
                char *tmp = ap_pstrdup(r->pool, header_values[h]);
                ap_content_type_tolower(tmp);
                r->content_type = tmp;
            } else if(!strcasecmp(header_names[h], "Location")) {
                ap_table_set(r->headers_out, header_names[h], header_values[h]);
            } else if(!strcasecmp(header_names[h], "Content-Length")) {
                ap_table_set(r->headers_out, header_names[h], header_values[h]);
            } else if(!strcasecmp(header_names[h], "Transfer-Encoding")) {
                ap_table_set(r->headers_out, header_names[h], header_values[h]);
            } else if(!strcasecmp(header_names[h], "Last-Modified")) {
                /*
                 * If the script gave us a Last-Modified header, we can't just
                 * pass it on blindly because of restrictions on future values.
                 */
                ap_update_mtime(r, ap_parseHTTPdate(header_values[h]));
                ap_set_last_modified(r);
            } else {                
                ap_table_add(r->headers_out, header_names[h], header_values[h]);
            }
        }

        ap_send_http_header(r);
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
            long rv;
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
static int JK_METHOD ws_write(jk_ws_service_t *s,
                              const void *b,
                              unsigned len)
{
    if(s && s->ws_private && b) {
        apache_private_data_t *p = s->ws_private;

        if(len) {
            BUFF *bf = p->r->connection->client;
            char *buf = (char *)b;
            int w = (int)len;
            int r = 0;

            if(!p->response_started) {
                if(!s->start_response(s, 200, NULL, NULL, NULL, 0)) {
                    return JK_FALSE;
                }
            }

            if (p->r->header_only) {
                ap_bflush(bf);
                return JK_TRUE;
            }

            while(len && !p->r->connection->aborted) {
                w = ap_bwrite(p->r->connection->client, &buf[r], len);
                if (w > 0) {
                    /* reset timeout after successful write */
                    ap_reset_timeout(p->r); 
                    r += w;
                    len -= w;
                } else if (w < 0) {
                    /* Error writing data -- abort */
                    if(!p->r->connection->aborted) {
                        ap_bsetflag(p->r->connection->client, B_EOUT, 1);
                        p->r->connection->aborted = 1;
                    }
                    return JK_FALSE;
                }
                
            }
           
            /*
             * To allow server push.
             */
            ap_bflush(bf);
        }

        return JK_TRUE;
    }
    return JK_FALSE;
}

/* ====================================================================== */
/* Utility functions                                                      */
/* ====================================================================== */

/* Log something to JK log file then exit */
static void jk_error_exit(const char *file, 
                          int line, 
                          int level, 
                          const server_rec *s,
                          ap_pool *p,
                          const char *fmt, ...) 
{
    va_list ap;
    char *res;

    va_start(ap, fmt);
    res = ap_pvsprintf(p, fmt, ap);
    va_end(ap);

    ap_log_error(file, line, level, s, res);

    /* Exit process */
    exit(1);
}

/* Return the content length associated with an Apache request structure */
static int get_content_length(request_rec *r)
{
    if(r->clength > 0) {
        return r->clength;
    } else {
        char *lenp = (char *)ap_table_get(r->headers_in, "Content-Length");

        if(lenp) {
            int rc = atoi(lenp);
            if(rc > 0) {
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
static int init_ws_service(apache_private_data_t *private_data,
                           jk_ws_service_t *s,
                           jk_server_conf_t *conf)
{
    request_rec *r      = private_data->r;
    char *ssl_temp      = NULL;
    s->jvm_route        = NULL; /* Used for sticky session routing */
    
    /* Copy in function pointers (which are really methods) */
    s->start_response   = ws_start_response;
    s->read             = ws_read;
    s->write            = ws_write;

    /* Clear RECO status */
    s->reco_status  = RECO_NONE;

    s->auth_type    = NULL_FOR_EMPTY(r->connection->ap_auth_type);
    s->remote_user  = NULL_FOR_EMPTY(r->connection->user);

    s->protocol     = r->protocol;
    s->remote_host  = (char *)ap_get_remote_host(r->connection, r->per_dir_config, REMOTE_HOST);
    s->remote_host  = NULL_FOR_EMPTY(s->remote_host);

    s->remote_addr  = NULL_FOR_EMPTY(r->connection->remote_ip);

    /* get server name */
    /* s->server_name  = (char *)(r->hostname ? r->hostname : r->server->server_hostname); */
    /* XXX : à la jk2 */
    s->server_name  = (char *) ap_get_server_name(r);
    
    /* get the real port (otherwise redirect failed) */
    /* s->server_port     = htons( r->connection->local_addr.sin_port ); */
    /* XXX : à la jk2 */
    s->server_port  = ap_get_server_port(r);

    s->server_software = (char *)ap_get_server_version();

    s->method         = (char *)r->method;
    s->content_length = get_content_length(r);
    s->is_chunked     = r->read_chunked;
    s->no_more_chunks = 0;
    s->query_string   = r->args;

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
    s->ssl_cipher   = NULL;		/* required by Servlet 2.3 Api, allready in original ajp13 */
    s->ssl_session  = NULL;
	s->ssl_key_size = -1;		/* required by Servlet 2.3 Api, added in jtc */

    if(conf->ssl_enable || conf->envvars_in_use) {
        ap_add_common_vars(r);

        if(conf->ssl_enable) {
            ssl_temp = (char *)ap_table_get(r->subprocess_env, conf->https_indicator);
            if(ssl_temp && !strcasecmp(ssl_temp, "on")) {
                s->is_ssl       = JK_TRUE;
                s->ssl_cert     = (char *)ap_table_get(r->subprocess_env, conf->certs_indicator);
                if(s->ssl_cert) {
                    s->ssl_cert_len = strlen(s->ssl_cert);
                }
				/* Servlet 2.3 API */
                s->ssl_cipher   = (char *)ap_table_get(r->subprocess_env, conf->cipher_indicator);
                s->ssl_session  = (char *)ap_table_get(r->subprocess_env, conf->session_indicator);

                if (conf->options & JK_OPT_FWDKEYSIZE) {
				    /* Servlet 2.3 API */
                    ssl_temp = (char *)ap_table_get(r->subprocess_env, conf->key_size_indicator);
				    if (ssl_temp) 
            		    s->ssl_key_size = atoi(ssl_temp);
                }
            }
        }

        if(conf->envvars_in_use) {
            array_header *t = ap_table_elts(conf->envvars);        
            if(t && t->nelts) {
                int i;
                table_entry *elts = (table_entry *)t->elts;
                s->attributes_names = ap_palloc(r->pool, sizeof(char *) * t->nelts);
                s->attributes_values = ap_palloc(r->pool, sizeof(char *) * t->nelts);

                for(i = 0 ; i < t->nelts ; i++) {
                    s->attributes_names[i] = elts[i].key;
                    s->attributes_values[i] = (char *)ap_table_get(r->subprocess_env, elts[i].key);
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
    if(r->headers_in && ap_table_elts(r->headers_in)) {
        int need_content_length_header = (!s->is_chunked && s->content_length == 0) ? JK_TRUE : JK_FALSE;
        array_header *t = ap_table_elts(r->headers_in);        
        if(t && t->nelts) {
            int i;
            table_entry *elts = (table_entry *)t->elts;
            s->num_headers = t->nelts;
            /* allocate an extra header slot in case we need to add a content-length header */
            s->headers_names  = ap_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
            s->headers_values = ap_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
            if(!s->headers_names || !s->headers_values)
                return JK_FALSE;
            for(i = 0 ; i < t->nelts ; i++) {
                char *hname = ap_pstrdup(r->pool, elts[i].key);
                s->headers_values[i] = ap_pstrdup(r->pool, elts[i].val);
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
            s->headers_names  = ap_palloc(r->pool, sizeof(char *));
            s->headers_values = ap_palloc(r->pool, sizeof(char *));
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
                                    char *context,
                                    char *worker,
                                    char *maybe_cookie)
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
 */

static const char *jk_automount_context(cmd_parms *cmd,
                                        void *dummy,
										char *worker,
										char *virtualhost)
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
                                      char *worker_file)
{
    server_rec *s = cmd->server;
    struct stat statbuf;

    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    /* we need an absolut path */
    conf->worker_file = ap_server_root_relative(cmd->pool,worker_file);

#ifdef CHROOTED_APACHE
    ap_server_strip_chroot(conf->worker_file,0);
#endif

    if (conf->worker_file == worker_file)
        conf->worker_file = ap_pstrdup(cmd->pool,worker_file);
 
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

static const char *jk_set_log_file(cmd_parms *cmd, 
                                   void *dummy, 
                                   char *log_file)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    /* we need an absolut path */
    conf->log_file = ap_server_root_relative(cmd->pool,log_file);

#ifdef CHROOTED_APACHE
    ap_server_strip_chroot(conf->log_file,0);
#endif

    if ( conf->log_file == log_file)
        conf->log_file = ap_pstrdup(cmd->pool,log_file);
 
    if (conf->log_file == NULL)
        return "JkLogFile file_name invalid";

    return NULL;
}

/*
 * JkLogLevel Directive Handling
 *
 * JkLogLevel debug/info/request/error/emerg
 */

static const char *jk_set_log_level(cmd_parms *cmd,
                                    void *dummy,
                                    char *log_level)
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
                   char * log_format)
{
    jk_set_log_format(log_format);
    return NULL;
}

/*
 * JkAutoAlias Directive Handling
 * 
 * JkAutoAlias application directory
 */

static const char *jk_set_auto_alias(cmd_parms *cmd,
                                    void *dummy,
                                    char *directory)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

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
                                                             
typedef struct {                                             
    item_key_func func;
    char *arg;         
} request_log_format_item;

static const char *process_item(request_rec *r,
                          request_log_format_item *item)
{
    const char *cp;

    cp = (*item->func) (r,item->arg);
    return cp ? cp : "-";
}

static void request_log_transaction(request_rec *r,
                                  jk_server_conf_t *conf)
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
    jk_log(conf->log ? conf->log : main_log, JK_LOG_REQUEST, "%s", str);
}

/*****************************************************************
 *
 * Parsing the log format string
 */

static char *format_integer(pool *p, int i)
{
    return ap_psprintf(p, "%d", i);
}
 
static char *pfmt(pool *p, int i)
{
    if (i <= 0) {
        return "-";
    }
    else {
        return format_integer(p, i);
    }
}

static const char *constant_item(request_rec *dummy, char *stuff)
{
    return stuff;
}

static const char *log_worker_name(request_rec *r, char *a)
{
    return ap_table_get(r->notes, JK_WORKER_ID);
}


static const char *log_request_duration(request_rec *r, char *a)
{
    return ap_table_get(r->notes, JK_DURATION);
}
 
static const char *log_request_line(request_rec *r, char *a)
{
            /* NOTE: If the original request contained a password, we
             * re-write the request line here to contain XXXXXX instead:
             * (note the truncation before the protocol string for HTTP/0.9 requests)
             * (note also that r->the_request contains the unmodified request)
             */
    return (r->parsed_uri.password) ? ap_pstrcat(r->pool, r->method, " ",
                                         ap_unparse_uri_components(r->pool, &r->parsed_uri, 0),
                                         r->assbackwards ? NULL : " ", r->protocol, NULL)
                                        : r->the_request;
}

/* These next two routines use the canonical name:port so that log
 * parsers don't need to duplicate all the vhost parsing crud.
 */
static const char *log_virtual_host(request_rec *r, char *a)
{
    return r->server->server_hostname;
}
 
static const char *log_server_port(request_rec *r, char *a)
{
    return ap_psprintf(r->pool, "%u",
        r->server->port ? r->server->port : ap_default_port(r));
}
 
/* This respects the setting of UseCanonicalName so that
 * the dynamic mass virtual hosting trick works better.
 */
static const char *log_server_name(request_rec *r, char *a)
{
    return ap_get_server_name(r);
}

static const char *log_request_uri(request_rec *r, char *a)
{
    return r->uri;
}
static const char *log_request_method(request_rec *r, char *a)
{
    return r->method;
}

static const char *log_request_protocol(request_rec *r, char *a)
{
    return r->protocol;
}
static const char *log_request_query(request_rec *r, char *a)
{
    return (r->args != NULL) ? ap_pstrcat(r->pool, "?", r->args, NULL)
                             : "";
}                  
static const char *log_status(request_rec *r, char *a)
{                  
    return pfmt(r->pool,r->status);
}                  
                   
static const char *clf_log_bytes_sent(request_rec *r, char *a)
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
    
static const char *log_bytes_sent(request_rec *r, char *a)
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

static struct log_item_list {
    char ch;
    item_key_func func;
} log_item_keys[] = {

    {
        'T', log_request_duration
    },
    {
        'r', log_request_line
    },
    {
        'U', log_request_uri
    },
    {
        's', log_status
    },
    {
        'b', clf_log_bytes_sent
    },
    {
        'B', log_bytes_sent
    },
    {
        'V', log_server_name
    },
    {
        'v', log_virtual_host
    },
    {
        'p', log_server_port
    },
    {
        'H', log_request_protocol
    },
    {
        'm', log_request_method
    },
    {
        'q', log_request_query
    },
    {
        'w', log_worker_name
    },
    {
        '\0'
    }
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

static char *parse_request_log_misc_string(pool *p,
                                           request_log_format_item *it,
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

static char *parse_request_log_item(pool *p,
                                    request_log_format_item *it,
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

static array_header *parse_request_log_string(pool *p, const char *s,
                                              const char **err)
{
    array_header *a = ap_make_array(p, 15, sizeof(request_log_format_item));
    char *res;

    while (*s) {
        if ((res = parse_request_log_item(p, (request_log_format_item *) ap_push_array(a), &s))) {
            *err = res;
            return NULL;
        }
    }    
     
    s = "\n";
    parse_request_log_item(p, (request_log_format_item *) ap_push_array(a), &s);
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

static const char *jk_set_request_log_format(cmd_parms *cmd,
                                             void *dummy,    
                                             char *format)
{
    const char *err_string = NULL;
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    conf->format_string = ap_pstrdup(cmd->pool,format);
    if( format != NULL ) {
        conf->format = parse_request_log_string(cmd->pool, format, &err_string);
    }
    if( conf->format == NULL )
        return "JkRequestLogFormat format array NULL";

    return err_string;
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
                                          char *indicator)
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
                                          char *indicator)
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
                                           char *indicator)
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
                                           char *indicator)
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
                                           char *indicator)
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
                                  char *env_name,
                                  char *default_value)
{
    server_rec *s = cmd->server;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    conf->envvars_in_use = JK_TRUE;
    
    ap_table_add(conf->envvars, env_name, default_value);

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
    {"JkWorkersFile", jk_set_worker_file, NULL, RSRC_CONF, TAKE1,
     "the name of a worker file for the Jakarta servlet containers"},

	/*
	 * JkAutoMount specifies that the list of handled URLs must be 	
	 * asked to the servlet engine (autoconf feature)
	 */
	{"JkAutoMount", jk_automount_context, NULL, RSRC_CONF, TAKE12,
     "automatic mount points to a servlet-engine worker"},

    /*
     * JkMount mounts a url prefix to a worker (the worker need to be
     * defined in the worker properties file.
     */
    {"JkMount", jk_mount_context, NULL, RSRC_CONF, TAKE23,
     "A mount point from a context to a servlet-engine worker"},

    /*
     * JkMountCopy specifies if mod_jk should copy the mount points
     * from the main server to the virtual servers.
     */
    {"JkMountCopy", jk_set_mountcopy, NULL, RSRC_CONF, FLAG,
     "Should the base server mounts be copied to the virtual server"},

    /*
     * JkLogFile & JkLogLevel specifies to where should the plugin log
     * its information and how much.
     * JkLogStampFormat specify the time-stamp to be used on log
     */
    {"JkLogFile", jk_set_log_file, NULL, RSRC_CONF, TAKE1,
     "Full path to the Jakarta mod_jk module log file"},
    {"JkLogLevel", jk_set_log_level, NULL, RSRC_CONF, TAKE1,
     "The Jakarta mod_jk module log level, can be debug, info, request, error, or emerg"},
    {"JkLogStampFormat", jk_set_log_fmt, NULL, RSRC_CONF, TAKE1,
     "The Jakarta mod_jk module log format, follow strftime synthax"},
    {"JkRequestLogFormat", jk_set_request_log_format, NULL, RSRC_CONF, TAKE1,
     "The Jakarta mod_jk module request log format string"},

    /*
     * Automatically Alias webapp context directories into the Apache
     * document space.
     */
    {"JkAutoAlias", jk_set_auto_alias, NULL, RSRC_CONF, TAKE1,
     "The Jakarta mod_jk module automatic context apache alias directory"},

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
     */
    {"JkOptions", jk_set_options, NULL, RSRC_CONF, RAW_ARGS,
     "Set one of more options to configure the mod_jk module"},

	/*
	 * JkEnvVar let user defines envs var passed from WebServer to 
	 * Servlet Engine
	 */
    {"JkEnvVar", jk_add_env_var, NULL, RSRC_CONF, TAKE2,
     "Adds a name of environment variable that should be sent to servlet-engine"},     

    {NULL}
};

/* ====================================================================== */
/* The JK module handlers                                                 */
/* ====================================================================== */

/*
 * Called to handle a single request.
 */ 
static int jk_handler(request_rec *r)
{   
    /* Retrieve the worker name stored by jk_translate() */
    const char *worker_name = ap_table_get(r->notes, JK_WORKER_ID);
    int rc;

    if(r->proxyreq) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    /* Set up r->read_chunked flags for chunked encoding, if present */
    if(rc = ap_setup_client_block(r, REQUEST_CHUNKED_DECHUNK)) {
	return rc;
    }
      
    if(worker_name) {
        jk_server_conf_t *conf =
            (jk_server_conf_t *)ap_get_module_config(r->server->module_config, &jk_module);
        jk_logger_t *l = conf->log ? conf->log : main_log;

        jk_worker_t *worker = wc_get_worker_for_name(worker_name, l);

        if(worker) {
            struct timeval tv_begin,tv_end;
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
#ifndef NO_GETTIMEOFDAY
            if(conf->format != NULL) {
                gettimeofday(&tv_begin, NULL);
            }
#endif

            if(init_ws_service(&private_data, &s, conf)) {
                jk_endpoint_t *end = NULL;
                if(worker->get_endpoint(worker, &end, l)) {
                    int is_recoverable_error = JK_FALSE;
                    rc = end->service(end, &s, l, &is_recoverable_error);
                
                    if (s.content_read < s.content_length ||
                        (s.is_chunked && ! s.no_more_chunks)) {
                        /*
                         * If the servlet engine didn't consume all of the
                         * request data, consume and discard all further
                         * characters left to read from client 
                         */
                        char *buff = ap_palloc(r->pool, 2048);
                        if (buff != NULL) {
                            int rd;
                            while ((rd = ap_get_client_block(r, buff, 2048)) > 0) {
                                s.content_read += rd;
                            }
                        }
                    }
                    end->done(&end, l);
                }
#ifndef NO_GETTIMEOFDAY
                if(conf->format != NULL) {
                    char *duration = NULL;
                    char *status = NULL;
                    long micro,seconds;
                    gettimeofday(&tv_end, NULL);
                    if( tv_end.tv_usec < tv_begin.tv_usec ) {
                        tv_end.tv_usec += 1000000;
                        tv_end.tv_sec--;
                    }
                    micro = tv_end.tv_usec - tv_begin.tv_usec;
                    seconds = tv_end.tv_sec - tv_begin.tv_sec;
                    duration = ap_psprintf(r->pool,"%.1d.%.6d",seconds,micro);
                    ap_table_setn(r->notes, JK_DURATION, duration);
                    request_log_transaction(r,conf);
                }
#endif
            }

            jk_close_pool(&private_data.p);

            if(rc) {
                /* If tomcat returned no body and the status is not OK,
                   let apache handle the error code */
                if( !r->sent_bodyct && r->status >= HTTP_BAD_REQUEST ) {
                    return r->status;
                }
                return OK;  /* NOT r->status, even if it has changed. */
            }
        }
    }

    return HTTP_INTERNAL_SERVER_ERROR;
}

/*
 * Create a default config object.
 */
static void *create_jk_config(ap_pool *p, server_rec *s)
{
    jk_server_conf_t *c =
        (jk_server_conf_t *) ap_pcalloc(p, sizeof(jk_server_conf_t));

    c->worker_properties = NULL;
    map_alloc(&c->worker_properties);
    c->worker_file   = NULL;
    c->log_file      = NULL;
    c->log_level     = -1;
    c->log           = NULL;
    c->alias_dir     = NULL;
    c->format_string = NULL;
    c->format        = NULL;
    c->mountcopy     = JK_FALSE;
    c->options       = JK_OPT_FWDURIDEFAULT;

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
	c->key_size_indicator = NULL;	
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
    c->envvars = ap_make_table(p, 0);

    c->s = s;

    return c;
}


static void copy_jk_map(ap_pool *p, server_rec * s, jk_map_t * src, jk_map_t * dst)
{
	int sz = map_size(src);
        int i;
        for(i = 0 ; i < sz ; i++) {
            void *old;
            char *name = map_name_at(src, i);
            if(map_get(src, name, NULL) == NULL) {
                if(!map_put(dst, name, ap_pstrdup(p, map_get_string(src, name, NULL)), &old)) {
                    jk_error_exit(APLOG_MARK, APLOG_EMERG, s, p, "Memory error");
                }
            }
        }
}

static void *merge_jk_config(ap_pool *p, 
                             void *basev, 
                             void *overridesv)
{
    jk_server_conf_t *base = (jk_server_conf_t *) basev;
    jk_server_conf_t *overrides = (jk_server_conf_t *)overridesv;

    if(base->ssl_enable) {
        overrides->ssl_enable         = base->ssl_enable;
        overrides->https_indicator    = base->https_indicator;
        overrides->certs_indicator    = base->certs_indicator;
        overrides->cipher_indicator   = base->cipher_indicator;
        overrides->session_indicator  = base->session_indicator;
        overrides->key_size_indicator = base->key_size_indicator;
    }
    
    overrides->options = base->options;

    if(overrides->mountcopy) {
		copy_jk_map(p, overrides->s, base->uri_to_context, overrides->uri_to_context);
		copy_jk_map(p, overrides->s, base->automount, overrides->automount);
    }

    if(base->envvars_in_use) {
        overrides->envvars_in_use = JK_TRUE;
        overrides->envvars = ap_overlay_tables(p, overrides->envvars, base->envvars);
    }

    if(overrides->log_file && overrides->log_level >= 0) {
        if(!jk_open_file_logger(&(overrides->log), overrides->log_file, overrides->log_level)) {
            overrides->log = NULL;
        }
    }
    if(!uri_worker_map_alloc(&(overrides->uw_map), overrides->uri_to_context, overrides->log)) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG, overrides->s, p, "Memory error");
    }

	if (base->secret_key)
		overrides->secret_key = base->secret_key;
   
    return overrides;
}

static void jk_init(server_rec *s, ap_pool *p)
{
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);
    jk_map_t *init_map = conf->worker_properties;

    /* Open up log file */
    if(conf->log_file && conf->log_level >= 0) {
        if(!jk_open_file_logger(&(conf->log), conf->log_file, conf->log_level)) {
#ifdef CHROOTED_APACHE
            conf->log = main_log;
#else
            conf->log = NULL;
#endif
        } else {
            main_log = conf->log;
        }
    }

    /* SREVILAK -- register cleanup handler to clear resources on restart,
     * to make sure log file gets closed in the parent process  */
    ap_register_cleanup(p, s, jk_server_cleanup, ap_null_cleanup);
    
/*
{ int i;
jk_log(conf->log, JK_LOG_DEBUG, "default secret key = %s\n", conf->secret_key);
for (i = 0; i < map_size(conf->automount); i++)
{
            char *name = map_name_at(conf->automount, i);
			jk_log(conf->log, JK_LOG_DEBUG, "worker = %s and virtualhost = %s\n", name, map_get_string(conf->automount, name, NULL));
}
}
*/

    /* Create mapping from uri's to workers, and start up all the workers */
    if(!uri_worker_map_alloc(&(conf->uw_map), conf->uri_to_context, conf->log)) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG, s, p, "Memory error");
    }

    /*if(map_alloc(&init_map)) {*/

    if(map_read_properties(init_map, conf->worker_file)) {

#if MODULE_MAGIC_NUMBER >= 19980527
        /* Tell apache we're here */
        ap_add_version_component(JK_EXPOSED_VERSION);
#endif
            
        /* we add the URI->WORKER MAP since workers using AJP14 will feed it */
        worker_env.uri_to_worker = conf->uw_map;
        worker_env.virtual       = "*";     /* for now */
        worker_env.server_name   = (char *)ap_get_server_version();
        if(wc_open(init_map, &worker_env, conf->log)) {
            /* we don't need this any more so free it */
            return;
        }            
    }
    
    ap_log_error(APLOG_MARK, APLOG_ERR, NULL,
                 "Error while opening the workers, jk will not work\n");
}

/*
 * Perform uri to worker mapping, and store the name of the relevant worker
 * in the notes fields of the request_rec object passed in.  This will then
 * get picked up in jk_handler().
 */
static int jk_translate(request_rec *r)
{    
    if(!r->proxyreq) {        
        jk_server_conf_t *conf =
            (jk_server_conf_t *)ap_get_module_config(r->server->module_config,
                                                     &jk_module);

        if(conf) {
            jk_logger_t *l = conf->log ? conf->log : main_log;
            char *uri = ap_pstrdup(r->pool, r->uri);
            char *worker = map_uri_to_worker(conf->uw_map, uri, l);

            /* Don't know the worker, ForwardDirectories is set, there is a
             * previous request for which the handler is JK_HANDLER (as set by
             * jk_fixups) and the request is for a directory:
             * --> forward to Tomcat, via default worker */
            if(!worker && (conf->options & JK_OPT_FWDDIRS) &&
               r->prev && !strcmp(r->prev->handler,JK_HANDLER) &&
               r->uri[strlen(r->uri)-1] == '/'){

                /* Nothing here to do but assign the first worker since we
                 * already tried mapping and it didn't work out */
                worker=worker_env.first_worker;

                jk_log(l, JK_LOG_DEBUG, "Manual configuration for %s %s\n",
                       r->uri, worker_env.first_worker);
            }

            if(worker) {
                r->handler = ap_pstrdup(r->pool, JK_HANDLER);
                ap_table_setn(r->notes, JK_WORKER_ID, worker);
            } else if(conf->alias_dir != NULL) {
                char *clean_uri = uri;
                ap_no2slash(clean_uri);
                /* Automatically map uri to a context static file */
                jk_log(l, JK_LOG_DEBUG,
                    "mod_jk::jk_translate, check alias_dir: %s\n",conf->alias_dir);
                if (strlen(clean_uri) > 1) {
                    /* Get the context directory name */
                    char *context_dir = NULL;
                    char *context_path = NULL;
                    char *child_dir = NULL;
                    char *index = clean_uri;
                    char *suffix = strchr(index+1,'/');
                    if( suffix != NULL ) {
                        int size = suffix - index;
                        context_dir = ap_pstrndup(r->pool,index,size);
                        /* Get the context child directory name */
                        index = index + size + 1;
                        suffix = strchr(index,'/');
                        if( suffix != NULL ) {
                            size = suffix - index;
                            child_dir = ap_pstrndup(r->pool,index,size);
                        } else {
                            child_dir = index;
                        }
                        /* Deny access to WEB-INF and META-INF directories */
                        if( child_dir != NULL ) {
                            jk_log(l, JK_LOG_DEBUG,             
                                "mod_jk::jk_translate, AutoAlias child_dir: %s\n",
                                 child_dir);
                            if( !strcasecmp(child_dir,"WEB-INF") ||
                                !strcasecmp(child_dir,"META-INF") ) {
                                jk_log(l, JK_LOG_DEBUG,
                                    "mod_jk::jk_translate, AutoAlias FORBIDDEN for URI: %s\n",
                                    r->uri);
                                return FORBIDDEN;
                            }
                        }
                    } else {
                        context_dir = ap_pstrdup(r->pool,index);
                    }

                    context_path = ap_pstrcat(r->pool,conf->alias_dir,
                                              ap_os_escape_path(r->pool,context_dir,1),
                                              NULL);
                    if( context_path != NULL ) {
                        DIR *dir = ap_popendir(r->pool,context_path);
                        if( dir != NULL ) {
                            char *escurl = ap_os_escape_path(r->pool, clean_uri, 1);
                            char *ret = ap_pstrcat(r->pool,conf->alias_dir,escurl,NULL);
                            ap_pclosedir(r->pool,dir);
                            /* Add code to verify real path ap_os_canonical_name */
                            if( ret != NULL ) {
                                jk_log(l, JK_LOG_DEBUG,
                                    "mod_jk::jk_translate, AutoAlias OK for file: %s\n",ret);
                                r->filename = ret;
                                return OK;
                            }
                        } else {
                            /* Deny access to war files in web app directory */
                            int size = strlen(context_dir);
                            if( size > 4 && !strcasecmp(context_dir+(size-4),".war") ) {
                                jk_log(l, JK_LOG_DEBUG,
                                    "mod_jk::jk_translate, AutoAlias FORBIDDEN for URI: %s\n",
                                    r->uri);
                                return FORBIDDEN;
                            }
                        }
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
static int jk_fixups(request_rec *r)
{
    /* This is a sub-request, probably from mod_dir */
    if(r->main){
        jk_server_conf_t *conf = (jk_server_conf_t *)
            ap_get_module_config(r->server->module_config, &jk_module);
        char *worker = (char *) ap_table_get(r->notes, JK_WORKER_ID);

        /* Only if we have no worker and ForwardDirectories is set */
        if(!worker && (conf->options & JK_OPT_FWDDIRS)){
            char *dummy_ptr[1], **names_ptr, *idx;
            int num_names;
            dir_config_rec *d = (dir_config_rec *)
                ap_get_module_config(r->per_dir_config, &dir_module);
            jk_logger_t *l = conf->log ? conf->log : main_log;

            /* Direct lift from mod_dir */
            if (d->index_names) {
                names_ptr = (char **)d->index_names->elts;
                num_names = d->index_names->nelts;
            } else {
                dummy_ptr[0] = DEFAULT_INDEX;
                names_ptr = dummy_ptr;
                num_names = 1;
            }

            /* Where the index file would start within the filename */
            idx=r->filename + strlen(r->filename) -
                              strlen(names_ptr[num_names - 1]);

            /* The requested filename has the last index file at the end */
            if(idx >= r->filename && !strcmp(idx,names_ptr[num_names - 1])){
                r->uri=r->main->uri;       /* Trick mod_dir with URI */
                r->finfo.st_mode=S_IFREG;  /* Trick mod_dir with file stat */

                /* We'll be checking for handler in r->prev later on */
                r->main->handler=ap_pstrdup(r->pool, JK_HANDLER);

                jk_log(l, JK_LOG_DEBUG, "ForwardDirectories on: %s\n", r->uri);
             }
        }
    }

    return DECLINED;
}

static void exit_handler (server_rec *s, ap_pool *p)
{
    /* srevilak - refactor cleanup body to jk_generic_cleanup() */
    jk_generic_cleanup(s);
}
 

/** srevilak -- registered as a cleanup handler in jk_init */
static void jk_server_cleanup(void *data) 
{
    jk_generic_cleanup((server_rec *) data);
}


/** BEGIN SREVILAK 
 * body taken from exit_handler()
 */
static void jk_generic_cleanup(server_rec *s) 
{

    server_rec *tmp = s;

	/* loop through all available servers to clean up all configuration
	 * records we've created
	 */
    while (NULL != tmp)
    {
        jk_server_conf_t *conf =
            (jk_server_conf_t *)ap_get_module_config(tmp->module_config, &jk_module);

        if (NULL != conf)
        {
            wc_close(conf->log);
            uri_worker_map_free(&(conf->uw_map), conf->log);
            map_free(&(conf->uri_to_context));
            map_free(&(conf->worker_properties));
            map_free(&(conf->automount));
            if (conf->log)
                jk_close_file_logger(&(conf->log));
        }
        tmp = tmp->next;
    }
}
/** END SREVILAK **/


static const handler_rec jk_handlers[] =
{
    { JK_MAGIC_TYPE, jk_handler },
    { JK_HANDLER, jk_handler },    
    { NULL }
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
    NULL,                       /* apache child process initializer */
    exit_handler,               /* apache child process exit/cleanup */
    NULL                        /* [1] post read_request handling */
#ifdef EAPI
    /*
     * Extended module APIs, needed when using SSL.
     * STDC say that we do not have to have them as NULL but
     * why take a chance
     */
    ,NULL,                      /* add_module */
    NULL,                       /* remove_module */
    NULL,                       /* rewrite_command */
    NULL,                       /* new_connection */
    NULL                       /* close_connection */
#endif /* EAPI */

};
