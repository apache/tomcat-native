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

/**
 * Description: Apache 2 plugin for Jakarta/Tomcat                         
 * Author:      Gal Shachor <shachor@il.ibm.com>                           
 *                 Henri Gomez <hgomez@slib.fr>                            
 * Version:     $Revision$                                           
 */

#include "apu_compat.h"
#include "ap_config.h"
#include "apr_lib.h"
#include "apr_date.h"
#include "apr_strings.h"

#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"

#include "util_script.h"
/*
 * Jakarta (jk_) include files
 */
#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_env.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_workerEnv.h"
#include "jk_uriMap.h"
#include "jk_requtil.h"

#include "jk_apache2.h"

#define NULL_FOR_EMPTY(x)   ((x && !strlen(x)) ? NULL : x) 

static int JK_METHOD ws_start_response(jk_ws_service_t *s,
                                       int status,
                                       const char *reason,
                                       const char * const *header_names,
                                       const char * const *header_values,
                                       unsigned num_of_headers)
{
    if(s && s->ws_private) {
        unsigned h;
        request_rec *r = (request_rec *)s->ws_private;  
        
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
                             void *b,
                             unsigned len,
                             unsigned *actually_read)
{
    if(s && s->ws_private && b && actually_read) {
        if(!s->read_body_started) {
           if(ap_should_client_block(s->ws_private)) {
                s->read_body_started = JK_TRUE;
            }
        }

        if(s->read_body_started) {
            long rv;
            if ((rv = ap_get_client_block(s->ws_private, b, len)) < 0) {
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
                              unsigned len)
{
    jk_logger_t *l=s->workerEnv->l;
    
    if(s && s->ws_private && b) {
        if(l) {
            /* BUFF *bf = p->r->connection->client; */
            size_t w = (size_t)l;
            size_t r = 0;
            long ll=len;
            char *bb=(char *)b;
            
            if(!s->response_started) {
                l->jkLog(l, JK_LOG_DEBUG, 
                       "Write without start, starting with defaults\n");
                if(!s->start_response(s, 200, NULL, NULL, NULL, 0)) {
                    return JK_FALSE;
                }
            }
            
            /* Debug - try to get around rwrite */
            while( ll > 0 ) {
                unsigned long toSend=(ll>CHUNK_SIZE) ? CHUNK_SIZE : ll;
                r = ap_rwrite((const char *)bb, toSend, s->ws_private );
                l->jkLog(l, JK_LOG_DEBUG, 
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
            if(ap_rflush(s->ws_private) != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, 
                             NULL, "mod_jk: Error flushing \n"  );
                return JK_FALSE;
            }

        }
        
        return JK_TRUE;
    }
    return JK_FALSE;
}

/* ========================================================================= */
/* Utility functions                                                         */
/* ========================================================================= */

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

static int init_ws_service(jk_ws_service_t *s,
                           jk_endpoint_t *e,
                           void *serverObj)
{
    apr_port_t port;
    char *ssl_temp      = NULL;
    jk_workerEnv_t *workerEnv=e->worker->workerEnv;
    jk_logger_t *l=workerEnv->l;
    request_rec *r=serverObj;

    /* Common initialization */
    /* XXX Probably not needed, we're duplicating */
    jk_requtil_initRequest(s);
    
    s->ws_private = r;
    s->pool=e->cPool;
    s->response_started = JK_FALSE;
    s->read_body_started = JK_FALSE;
    s->workerEnv=workerEnv;

    s->jvm_route        = NULL;    /* Used for sticky session routing */

    s->auth_type    = NULL_FOR_EMPTY(r->ap_auth_type);
    s->remote_user  = NULL_FOR_EMPTY(r->user);

    s->protocol     = r->protocol;
    s->remote_host  = (char *)ap_get_remote_host(r->connection, 
                                                 r->per_dir_config, 
                                                 REMOTE_HOST, NULL);
    s->remote_host  = NULL_FOR_EMPTY(s->remote_host);
    s->remote_addr  = NULL_FOR_EMPTY(r->connection->remote_ip);

    /* get server name */
    s->server_name= (char *)(r->hostname ? r->hostname :
                 r->server->server_hostname);

    /* get the real port (otherwise redirect failed) */
    apr_sockaddr_port_get(&port,r->connection->local_addr);
    s->server_port = port;

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

    switch (workerEnv->options & JK_OPT_FWDURIMASK) {

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

    if(workerEnv->ssl_enable || workerEnv->envvars_in_use) {
        ap_add_common_vars(r);

        if(workerEnv->ssl_enable) {
            ssl_temp = 
                (char *)apr_table_get(r->subprocess_env, 
                                      workerEnv->https_indicator);
            if(ssl_temp && !strcasecmp(ssl_temp, "on")) {
                s->is_ssl       = JK_TRUE;
                s->ssl_cert     = 
                    (char *)apr_table_get(r->subprocess_env, 
                                          workerEnv->certs_indicator);
                if(s->ssl_cert) {
                    s->ssl_cert_len = strlen(s->ssl_cert);
                }
                /* Servlet 2.3 API */
                s->ssl_cipher   = 
                    (char *)apr_table_get(r->subprocess_env, 
                                          workerEnv->cipher_indicator);
                 s->ssl_session  = 
                    (char *)apr_table_get(r->subprocess_env, 
                                          workerEnv->session_indicator);

                if (workerEnv->options & JK_OPT_FWDKEYSIZE) {
                    /* Servlet 2.3 API */
                    ssl_temp = (char *)apr_table_get(r->subprocess_env, 
                                                 workerEnv->key_size_indicator);
                    if (ssl_temp)
                        s->ssl_key_size = atoi(ssl_temp);
                }
            }
        }

        if(workerEnv->envvars_in_use) {
            const apr_array_header_t *t = apr_table_elts(workerEnv->envvars);
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
        int need_content_length_header =
            (!s->is_chunked && s->content_length == 0) ? JK_TRUE : JK_FALSE;
        
        const apr_array_header_t *t = apr_table_elts(r->headers_in);
        if(t && t->nelts) {
            int i;
            apr_table_entry_t *elts = (apr_table_entry_t *)t->elts;
            s->num_headers = t->nelts;
            /* allocate an extra header slot in case we need to add a
               content-length header */
            s->headers_names  =
                apr_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
            s->headers_values =
                apr_palloc(r->pool, sizeof(char *) * (t->nelts + 1));
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
 * If the servlet engine didn't consume all of the
 * request data, consume and discard all further
 * characters left to read from client
 *
 *  XXX Is it the right thing to do ????? Why spend the
 *  bandwith, the servlet decided not to read the POST then
 *  jk shouldn't do it instead, and the user should get the
 *  error message !
 */
static void jk_service_apache2_afterRequest(jk_ws_service_t *s )
{
    
    if (s->content_read < s->content_length ||
        (s->is_chunked && ! s->no_more_chunks)) {
        
        request_rec *r=s->ws_private;

        char *buff = apr_palloc(r->pool, 2048);
        if (buff != NULL) {
            int rd;
            while ((rd = ap_get_client_block(r, buff, 2048)) > 0) {
                s->content_read += rd;
            }
        }
    }
}

int jk_service_apache2_factory(jk_env_t *env,
                               jk_pool_t *pool,
                               void **result,
                               char *type,
                               char *name)
{
    jk_ws_service_t *s = *result;
    if( s==NULL ) {
        s=(jk_ws_service_t *)pool->calloc(pool, sizeof(jk_ws_service_t));
    }

    if(s==NULL ) {
        return JK_FALSE;
    }

    s->start_response   = ws_start_response;
    s->read             = ws_read;
    s->write            = ws_write;
    s->init             = init_ws_service;
    s->afterRequest     = jk_service_apache2_afterRequest;
    
    *result=(void *)s;

    return JK_TRUE;
}
