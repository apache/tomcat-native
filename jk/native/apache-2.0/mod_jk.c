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
 * 		        Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

/*
 * mod_jk: keeps all servlet/jakarta related ramblings together.
 */

#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#include "util_script.h"
#include "util_date.h"
#include "apr_strings.h"
/*
 * Jakarta (jk_) include files
 */
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

/* module MODULE_VAR_EXPORT jk_module; */
AP_DECLARE_DATA module jk_module;


typedef struct {
    char *log_file;
    int  log_level;
    jk_logger_t *log;

    char *worker_file;
    int  mountcopy;
    jk_map_t *uri_to_context;

	char * secret_key;
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

static jk_logger_t *	 main_log = NULL;
static jk_worker_env_t   worker_env;


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
	            apr_table_set(r->headers_out, header_names[h], header_values[h]);
	        } else if(!strcasecmp(header_names[h], "Content-Length")) {
	            apr_table_set(r->headers_out, header_names[h], header_values[h]);
	        } else if(!strcasecmp(header_names[h], "Transfer-Encoding")) {
	            apr_table_set(r->headers_out, header_names[h], header_values[h]);
            } else if(!strcasecmp(header_names[h], "Last-Modified")) {
	            /*
	             * If the script gave us a Last-Modified header, we can't just
	             * pass it on blindly because of restrictions on future values.
	             */
	            ap_update_mtime(r, ap_parseHTTPdate(header_values[h]));
	            ap_set_last_modified(r);
	        } else {	            
	            apr_table_add(r->headers_out, header_names[h], header_values[h]);
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
                             unsigned l,
                             unsigned *a)
{
    if(s && s->ws_private && b && a) {
        apache_private_data_t *p = s->ws_private;
        if(!p->read_body_started) {
            if(!ap_setup_client_block(p->r, REQUEST_CHUNKED_DECHUNK)) {
                if(ap_should_client_block(p->r)) { 
                    p->read_body_started = JK_TRUE; 
                }
            }
        }

        if(p->read_body_started) {
            *a = ap_get_client_block(p->r, b, l);
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
#define CHUNK_SIZE 4096

static int JK_METHOD ws_write(jk_ws_service_t *s,
                              const void *b,
                              unsigned l)
{
    if(s && s->ws_private && b) {
        apache_private_data_t *p = s->ws_private;

        if(l) {
            /* BUFF *bf = p->r->connection->client; */
            size_t w = (size_t)l;
            size_t r = 0;
	    long ll=l;
	    char *bb=b;

            if(!p->response_started) {
                if(!s->start_response(s, 200, NULL, NULL, NULL, 0)) {
                    return JK_FALSE;
                }
            }
            
	    /* Debug - try to get around rwrite */
	    while( ll > 0 ) {
		long toSend=(ll>CHUNK_SIZE) ? CHUNK_SIZE : ll;
		r = ap_rwrite((const char *)bb, toSend, p->r );
		/* DEBUG */
#ifdef JK_DEBUG
		ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, 
			     NULL, "mod_jk: writing %ld (%ld) out of %ld \n",
			     toSend, r, ll );
#endif
		ll-=CHUNK_SIZE;
		bb+=CHUNK_SIZE;
		
		if(toSend != r) { 
		    return JK_FALSE; 
		} 

		/*
		 * To allow server push.
		 */
		if(ap_rflush(p->r) != APR_SUCCESS) {
		    ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, 
				 NULL, "mod_jk: Error flushing \n"  );
		    return JK_FALSE;
		}
	    }
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
	char *ssl_temp      = NULL;
    s->jvm_route        = NULL;	/* Used for sticky session routing */

	/* Copy in function pointers (which are really methods) */
    s->start_response   = ws_start_response;
    s->read             = ws_read;
    s->write            = ws_write;

    s->auth_type    = NULL_FOR_EMPTY(r->ap_auth_type);
    s->remote_user  = NULL_FOR_EMPTY(r->user);

    s->protocol     = r->protocol;
    s->remote_host  = (char *)ap_get_remote_host(r->connection, r->per_dir_config, REMOTE_HOST, NULL);
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

#ifdef NOTNEEDEDFORNOW
    /* Wrong:    s->server_name  = (char *)ap_get_server_name( r ); */
    s->server_name= (char *)(r->hostname ? r->hostname :
                 r->server->server_hostname);


    s->server_port= htons( r->connection->local_addr.sin_port );
    /* Wrong: s->server_port  = r->server->port; */

   
    /*    Winners:  htons( r->connection->local_addr.sin_port )
                      (r->hostname ? r->hostname :
                             r->server->server_hostname),
    */
    /* printf( "Port %u %u %u %s %s %s %d %d \n",
        ap_get_server_port( r ),
        htons( r->connection->local_addr.sin_port ),
        ntohs( r->connection->local_addr.sin_port ),
        ap_get_server_name( r ),
        (r->hostname ? r->hostname : r->server->server_hostname),
        r->hostname,
        r->connection->base_server->port,
        r->server->port
        );
    */
#else
	s->server_name  = (char *)ap_get_server_name( r );
	s->server_port  = r->server->port;
#endif

    s->server_software = ap_get_server_version();

    s->method       = (char *)r->method;
    s->content_length = get_content_length(r);
    s->query_string = r->args;

    /*
     * The 2.2 servlet spec errata says the uri from
     * HttpServletRequest.getRequestURI() should remain encoded.
     * [http://java.sun.com/products/servlet/errata_042700.html]
     */
    s->req_uri      = r->unparsed_uri;
    if (s->req_uri != NULL) {
        char *query_str = strchr(s->req_uri, '?');
        if (query_str != NULL) {
            *query_str = 0;
        }
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
            ssl_temp = (char *)apr_table_get(r->subprocess_env, conf->https_indicator);
            if(ssl_temp && !strcasecmp(ssl_temp, "on")) {
                s->is_ssl       = JK_TRUE;
                s->ssl_cert     = (char *)apr_table_get(r->subprocess_env, conf->certs_indicator);
                if(s->ssl_cert) {
                    s->ssl_cert_len = strlen(s->ssl_cert);
                }
				/* Servlet 2.3 API */
                s->ssl_cipher   = (char *)apr_table_get(r->subprocess_env, conf->cipher_indicator);
                s->ssl_session  = (char *)apr_table_get(r->subprocess_env, conf->session_indicator);

                /* Servlet 2.3 API */
                ssl_temp = (char *)apr_table_get(r->subprocess_env, conf->key_size_indicator);
                if (ssl_temp)
                    s->ssl_key_size = atoi(ssl_temp);

            }
        }

        if(conf->envvars_in_use) {
            apr_array_header_t *t = apr_table_elts(conf->envvars);
            if(t && t->nelts) {
                int i;
                apr_table_entry_t *elts = (apr_table_entry_t *)t->elts;
                s->attributes_names = apr_palloc(r->pool, sizeof(char *) * t->nelts);
                s->attributes_values = apr_palloc(r->pool, sizeof(char *) * t->nelts);

                for(i = 0 ; i < t->nelts ; i++) {
                    s->attributes_names[i] = elts[i].key;
                    s->attributes_values[i] = (char *)apr_table_get(r->subprocess_env, elts[i].key);
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
        apr_array_header_t *t = apr_table_elts(r->headers_in);        
        if(t && t->nelts) {
            int i;
            apr_table_entry_t *elts = (apr_table_entry_t *)t->elts;
            s->num_headers = t->nelts;
            s->headers_names  = apr_palloc(r->pool, sizeof(char *) * t->nelts);
            s->headers_values = apr_palloc(r->pool, sizeof(char *) * t->nelts);
            for(i = 0 ; i < t->nelts ; i++) {
                char *hname = apr_pstrdup(r->pool, elts[i].key);
                s->headers_values[i] = apr_pstrdup(r->pool, elts[i].val);
                s->headers_names[i] = hname;
                while(*hname) {
                    *hname = tolower(*hname);
                    hname++;
                }
            }
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

    /*
     * Add the new worker to the alias map.
     */
    char *old;
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

    conf->worker_file = worker_file;

    if (stat(worker_file, &statbuf) == -1)
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

    conf->log_file = log_file;

    return NULL;
}

/*
 * JkLogLevel Directive Handling
 *
 * JkLogLevel debug/info/error/emerg
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

    conf->https_indicator = indicator;

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

    conf->certs_indicator = indicator;

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

    conf->cipher_indicator = indicator;

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

    conf->session_indicator = indicator;

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

    conf->key_size_indicator = indicator;

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
    {"JkWorkersFile", jk_set_worker_file, NULL, RSRC_CONF, TAKE1,
     "the name of a worker file for the Jakarta servlet containers"},

    /*
     * JkAutoMount specifies that the list of handled URLs must be
     * asked to the servlet engine (autoconf feature)
     */
    {"JkAutoMount", jk_automount_context, NULL, RSRC_CONF, TAKE12,
     "automatic mount points to a Tomcat worker"},

    /*
     * JkMount mounts a url prefix to a worker (the worker need to be
     * defined in the worker properties file.
     */
    {"JkMount", jk_mount_context, NULL, RSRC_CONF, TAKE23,
     "A mount point from a context to a Tomcat worker"},

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
     "Full path to the Jakarta Tomcat module log file"},
    {"JkLogLevel", jk_set_log_level, NULL, RSRC_CONF, TAKE1,
     "The Jakarta Tomcat module log level, can be debug, info, error or emerg"},
    {"JkLogStampFormat", jk_set_log_fmt, NULL, RSRC_CONF, TAKE1,
     "The Jakarta Tomcat module log format, follow strftime synthax"},

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
     * JkEnvVar let user defines envs var passed from WebServer to
     * Servlet Engine
     */
    {"JkEnvVar", jk_add_env_var, NULL, RSRC_CONF, TAKE2,
     "Adds a name of environment variable that should be sent to servlet-engine"},

    {NULL}
};

/* ========================================================================= */
/* The JK module handlers                                                    */
/* ========================================================================= */

apr_status_t jk_cleanup_endpoint( void *data ) {
    jk_endpoint_t *end = (jk_endpoint_t *)data;    
    /*     printf("XXX jk_cleanup1 %ld\n", data); */
    end->done(&end, NULL);  
    return 0;
}

static int jk_handler(request_rec *r)
{   
    const char *worker_name;

    if(strcmp(r->handler,JK_HANDLER))	/* not for me, try next handler */
    return DECLINED;

	worker_name = apr_table_get(r->notes, JK_WORKER_ID);

	if (1)
	{
	jk_server_conf_t *xconf = (jk_server_conf_t *)ap_get_module_config(r->server->module_config, &jk_module);
	jk_logger_t *xl = xconf->log ? xconf->log : main_log;
	jk_log(xl, JK_LOG_DEBUG, "Into handler r->proxyreq=%d r->handler=%s r->notes=%d worker=%s\n", r->proxyreq, r->handler, r->notes, worker_name); 
	}

    /* If this is a proxy request, we'll notify an error */
    if(r->proxyreq) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }
      
    if(worker_name) {
        jk_server_conf_t *conf =
            (jk_server_conf_t *)ap_get_module_config(r->server->module_config, &jk_module);
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

#ifdef REUSE_WORKER
		apr_pool_t *rpool=r->pool;
		apr_pool_t *tpool=rpool->parent->parent;
		
		ap_get_userdata( &end, "jk_thread_endpoint", tpool );
                if(end==NULL ) {
		    worker->get_endpoint(worker, &end, l);
		    ap_set_userdata( end , "jk_thread_endpoint", &jk_cleanup_endpoint,  tpool );
		}
#else
		worker->get_endpoint(worker, &end, l);
#endif
		{   
		    int is_recoverable_error = JK_FALSE;
                    rc = end->service(end, 
                                      &s, 
                                      l, 
                                      &is_recoverable_error);

			if (s.content_read < s.content_length) {
			/* Toss all further characters left to read fm client */
				char *buff = apr_palloc(r->pool, 2048);
				if (buff != NULL) {
					int rd;
					while ((rd = ap_get_client_block(r, buff, 2048)) > 0) {
						s.content_read += rd;
					}
 				}
			}
                                                                            
#ifndef REUSE_WORKER		    
		    end->done(&end, l); 
#endif
                }
            }

            if(rc) {
                return OK;	/* NOT r->status, even if it has changed. */
            }
        }
    }

	return DECLINED;
}

static void *create_jk_config(apr_pool_t *p, server_rec *s)
{
    jk_server_conf_t *c =
        (jk_server_conf_t *) apr_pcalloc(p, sizeof(jk_server_conf_t));

    c->worker_file = NULL;
    c->log_file    = NULL;
    c->log_level   = -1;
    c->log         = NULL;
    c->mountcopy   = JK_FALSE;
    c->was_initialized = JK_FALSE;

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

    return c;
}


static void copy_jk_map(apr_pool_t *p, server_rec * s, jk_map_t * src, jk_map_t * dst)
{   
    int sz = map_size(src);
        int i;
        for(i = 0 ; i < sz ; i++) {
            void *old;
            char *name = map_name_at(src, i);
            if(map_get(src, name, NULL) == NULL) {
                if(!map_put(dst, name, apr_pstrdup(p, map_get_string(src, name, NULL)), &old)) {
                    jk_error_exit(APLOG_MARK, APLOG_EMERG, s, p, "Memory error");
                }
            } 
        }
}


static void *merge_jk_config(apr_pool_t *p, 
                             void *basev, 
                             void *overridesv)
{
    jk_server_conf_t *base = (jk_server_conf_t *) basev;
    jk_server_conf_t *overrides = (jk_server_conf_t *)overridesv;
 
    if(base->ssl_enable) {
        overrides->ssl_enable       = base->ssl_enable;
        overrides->https_indicator  = base->https_indicator;
        overrides->certs_indicator  = base->certs_indicator;
        overrides->cipher_indicator = base->cipher_indicator;
        overrides->session_indicator = base->session_indicator;
    }

    if(overrides->mountcopy) {
        copy_jk_map(p, overrides->s, base->uri_to_context, overrides->uri_to_context);
        copy_jk_map(p, overrides->s, base->automount, overrides->automount);
    }

    if(base->envvars_in_use) {
        overrides->envvars_in_use = JK_TRUE;
        overrides->envvars = apr_table_overlay(p, overrides->envvars, base->envvars);
    }

    if(overrides->log_file && overrides->log_level >= 0) {
        if(!jk_open_file_logger(&(overrides->log), overrides->log_file, overrides->log_level)) {
            overrides->log = NULL;
        }
    }
    if(!uri_worker_map_alloc(&(overrides->uw_map), 
                             overrides->uri_to_context, 
                             overrides->log)) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG, overrides->s, overrides->s->process->pool, "Memory error");
    }
    
    if (base->secret_key)
        overrides->secret_key = base->secret_key;

    return overrides;
}

static void jk_child_init(apr_pool_t *pconf, 
			  server_rec *s)
{
    jk_map_t *init_map = NULL;
    jk_server_conf_t *conf =
        (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);

    if(conf->log_file && conf->log_level >= 0) {
        if(!jk_open_file_logger(&(conf->log), conf->log_file, conf->log_level)) {
            conf->log = NULL;
        } else {
            main_log = conf->log;
        }
    }
    
    if(!uri_worker_map_alloc(&(conf->uw_map), conf->uri_to_context, conf->log)) {
        jk_error_exit(APLOG_MARK, APLOG_EMERG, s, pconf, "Memory error");
    }

    if(map_alloc(&init_map)) {
        if(map_read_properties(init_map, conf->worker_file)) {
        /* we add the URI->WORKER MAP since workers using AJP14 will feed it */
        worker_env.uri_to_worker = conf->uw_map;
        worker_env.server_name   = (char *)ap_get_server_version();
	    if(wc_open(init_map, &worker_env, conf->log)) {
		return;
        }            
        }
    }

    jk_error_exit(APLOG_MARK, APLOG_EMERG, s, pconf, "Error while opening the workers");
}

static void jk_post_config(apr_pool_t *pconf, 
                           apr_pool_t *plog, 
                           apr_pool_t *ptemp, 
                           server_rec *s)
{
    if(!s->is_virtual) {
        jk_map_t *init_map = NULL;
        jk_server_conf_t *conf =
            (jk_server_conf_t *)ap_get_module_config(s->module_config, &jk_module);
        if(!conf->was_initialized) {
            conf->was_initialized = JK_TRUE;        
            if(conf->log_file && conf->log_level >= 0) {
                if(!jk_open_file_logger(&(conf->log), conf->log_file, conf->log_level)) {
                    conf->log = NULL;
                } else {
                    main_log = conf->log;
                }
            }
    
            if(!uri_worker_map_alloc(&(conf->uw_map), conf->uri_to_context, conf->log)) {
                jk_error_exit(APLOG_MARK, APLOG_EMERG, s, plog, "Memory error");
            }

            if(map_alloc(&init_map)) {
                if(map_read_properties(init_map, conf->worker_file)) {
					ap_add_version_component(pconf, JK_EXPOSED_VERSION);
						/* May be allready done in init ??? */
        				worker_env.uri_to_worker = conf->uw_map;
        				worker_env.server_name   = (char *)ap_get_server_version();
                        if(wc_open(init_map, &worker_env, conf->log)) {
                            return;
                        }            
                }
            }

            jk_error_exit(APLOG_MARK, APLOG_EMERG, s, plog, "Error while opening the workers");
        }
    }
}

static int jk_translate(request_rec *r)
{    
    if(!r->proxyreq) {        
        jk_server_conf_t *conf =
            (jk_server_conf_t *)ap_get_module_config(r->server->module_config, &jk_module);

        if(conf) {
            char *worker = map_uri_to_worker(conf->uw_map, r->uri, conf->log ? conf->log : main_log);

            if(worker) {
                r->handler=apr_pstrdup(r->pool,JK_HANDLER);
                apr_table_setn(r->notes, JK_WORKER_ID, worker);
                return OK;
            }
        }
    }

    return DECLINED;
}

static void jk_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(jk_handler, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_post_config(jk_post_config,NULL,NULL,APR_HOOK_MIDDLE);
    ap_hook_child_init(jk_child_init,NULL,NULL,APR_HOOK_MIDDLE);
    ap_hook_translate_name(jk_translate,NULL,NULL,APR_HOOK_FIRST);

}

module AP_MODULE_DECLARE_DATA jk_module =
{
    STANDARD20_MODULE_STUFF,
    NULL,	            /* dir config creater */
    NULL,	            /* dir merger --- default is to override */
    create_jk_config,	/* server config */
    merge_jk_config,	/* merge server config */
    jk_cmds,			/* command ap_table_t */
    jk_register_hooks	/* register hooks */
};

