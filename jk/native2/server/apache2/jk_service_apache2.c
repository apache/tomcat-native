/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2003 The Apache Software Foundation.          *
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
 *                 Henri Gomez <hgomez@apache.org>                            
 * Version:     $Revision$                                           
 */

/*
 * Jakarta (jk_) include files
 */
#include "jk_apache2.h"

#include "util_script.h"

/* #define USE_APRTABLES  */

#define NULL_FOR_EMPTY(x)   ((((x)!=NULL) && (strlen((x))!=0)) ? (x) : NULL ) 

static int JK_METHOD jk2_service_apache2_head(jk_env_t *env, jk_ws_service_t *s )
{
    int h;
    int numheaders;
    request_rec *r;
    jk_map_t *headers;
    int debug=1;
    
    if(s==NULL ||  s->ws_private==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.head() NullPointerException\n");
        return JK_ERR;
    }
    
    if( s->uriEnv != NULL )
        debug=s->uriEnv->mbean->debug;

    r = (request_rec *)s->ws_private;  
        
    if(s->msg==NULL) {
        s->msg = "";
    }
    r->status = s->status;
    r->status_line = apr_psprintf(r->pool, "%d %s", s->status, s->msg);
    headers=s->headers_out;

#ifdef USE_APRTABLES
    {
        char *val= headers->get( env, headers, "Content-Type");
        if( val!=NULL ) {
            char *tmp = apr_pstrdup(r->pool, val);
            ap_content_type_tolower(tmp); 
            /* It should be done like this in Apache 2.0 */
            /* This way, Apache 2.0 will be able to set the output filter */
            /* and it make jk useable with deflate using AddOutputFilterByType DEFLATE text/html */
            ap_set_content_type(r, tmp);
        }
        val= headers->get( env, headers, "Last-Modified");
        if( val!=NULL ) {
            /*
             * If the script gave us a Last-Modified header, we can't just
             * pass it on blindly because of restrictions on future values.
             */
            ap_update_mtime(r, apr_date_parse_http(val));
            ap_set_last_modified(r);
        }

        /* No other change required - headers is the same as req->headers_out,
           just with a different interface
        */
    }
#else
    numheaders = headers->size(env, headers);
    /* XXX As soon as we switch to jk_map_apache2, this will not be needed ! */
    if( debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                      "service.head() %d %d %#lx\n", s->status,
                      numheaders, s->uriEnv);
    
    for(h = 0 ; h < numheaders; h++) {
        char *name=headers->nameAt( env, headers, h );
        char *val=headers->valueAt( env, headers, h );
        name=s->pool->pstrdup( env, s->pool, name );
        val=s->pool->pstrdup( env, s->pool, val );

        if( debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                          "service.head() %s: %s %d %d\n", name, val, h, headers->size( env, headers ));

        /* the cmp can also be avoided in we do this earlier and use
           the header id */
        if(!strcasecmp(name, "Content-type")) {
            /* XXX should be done in handler ! */
            char *tmp = apr_pstrdup(r->pool, val);
            ap_content_type_tolower(tmp); 
            r->content_type = tmp;
            apr_table_set(r->headers_out, name, val);
        } else if(!strcasecmp(name, "Location")) {
            /* XXX setn */
            apr_table_set(r->headers_out, name, val);
        } else if(!strcasecmp(name, "Content-Length")) {
            apr_table_set(r->headers_out, name, val );
        } else if(!strcasecmp(name, "Transfer-Encoding")) {
            apr_table_set(r->headers_out,name, val);
        } else if(!strcasecmp(name, "Last-Modified")) {
            /*
             * If the script gave us a Last-Modified header, we can't just
             * pass it on blindly because of restrictions on future values.
             */
            ap_update_mtime(r, apr_date_parse_http(val));
            ap_set_last_modified(r);
            apr_table_set(r->headers_out, name, val);
        } else {     
            /* All other headers may have multiple values like
             * Set-Cookie, so use the table_add to allow that.
             */
             apr_table_add(r->headers_out, name, val);
            /* apr_table_set(r->headers_out, name, val); */
        }
    }
#endif
    
    /* this NOP function was removed in apache 2.0 alpha14 */
    /* ap_send_http_header(r); */
    s->response_started = JK_TRUE;
    
    return JK_OK;
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
static int JK_METHOD jk2_service_apache2_read(jk_env_t *env, jk_ws_service_t *s,
                                              void *b, unsigned len,
                                              unsigned *actually_read)
{
    if(s==NULL || s->ws_private==NULL ||  b==NULL || actually_read==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.read() NullPointerException\n");
        return JK_ERR;
    }

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
    }
    return JK_OK;
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

static int JK_METHOD jk2_service_apache2_write(jk_env_t *env, jk_ws_service_t *s,
                                               const void *b, unsigned int len)
{
    size_t r = 0;
    long ll=len;
    char *bb=(char *)b;
    request_rec *rr;
    int debug=1;
    int rc;
    
    if(s==NULL  || s->ws_private == NULL ||  b==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.write() NullPointerException\n");
        return JK_ERR;
    }
    if( s->uriEnv != NULL )
        debug=s->uriEnv->mbean->debug;
    
    if(len==0 ) {
        return JK_OK;
    }

    /* BUFF *bf = p->r->connection->client; */
    /* size_t w = (size_t)l; */
    rr=s->ws_private;
    
    if(!s->response_started) {
        if( debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                          "service.write() default head\n");
        rc=s->head(env, s);
        if( rc != JK_OK) {
            return rc;
        }
        
        {
            const apr_array_header_t *t = apr_table_elts(rr->headers_out);
            if(t && t->nelts) {
                int i;
                
                apr_table_entry_t *elts = (apr_table_entry_t *)t->elts;
                
                if( debug > 0 ) {
                    for(i = 0 ; i < t->nelts ; i++) {
                        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "OutHeaders %s: %s\n",
                                      elts[i].key, elts[i].val);
                    }
                }
            }
        }
    }

    if (rr->header_only) {
        ap_rflush(rr);
        return JK_OK;
    }
    
    /* Debug - try to get around rwrite */
    while( ll > 0 ) {
        unsigned long toSend=(ll>CHUNK_SIZE) ? CHUNK_SIZE : ll;
        r = ap_rwrite((const char *)bb, toSend, rr );
        /*  env->l->jkLog(env, env->l, JK_LOG_INFO,  */
        /*     "service.write()  %ld (%ld) out of %ld \n",toSend, r, ll ); */
        ll-=CHUNK_SIZE;
        bb+=CHUNK_SIZE;
        
        if(toSend != r) { 
            return JK_ERR; 
        } 
        
    }
    
    /*
     * To allow server push. After writing full buffers
     */
    if(ap_rflush(rr) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, 
                     NULL, "mod_jk: Error flushing"  );
        return JK_ERR;
    }
    
    return JK_OK;
}

/* ========================================================================= */
/* Utility functions                                                         */
/* ========================================================================= */

static long jk2_get_content_length(jk_env_t *env, request_rec *r)
{
    if(r->clength > 0) {
        return (long)(r->clength);
    } else {
        char *lenp = (char *)apr_table_get(r->headers_in, "Content-Length");

        if(lenp) {
            long rc = atol(lenp);
            if(rc > 0) {
                return rc;
            }
        }
    }

    return 0;
}

static int JK_METHOD jk2_init_ws_service(jk_env_t *env, jk_ws_service_t *s,
                                         jk_worker_t *worker, void *serverObj)
{
    char *ssl_temp      = NULL;
    jk_workerEnv_t *workerEnv;
    request_rec *r=serverObj;
    int need_content_length_header=JK_FALSE;

    workerEnv = worker->workerEnv;
    /* Common initialization */
    /* XXX Probably not needed, we're duplicating */
    jk2_requtil_initRequest(env, s);


    s->ws_private = r;
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

    /* get server name like in jk 1.2.x */
    s->server_name= (char *)ap_get_server_name(r);

    /* get the real port (otherwise redirect failed) */
    s->server_port = r->connection->local_addr->port;

    s->server_software = (char *)ap_get_server_version();

    s->method         = (char *)r->method;
    s->content_length = jk2_get_content_length(env, r);
    s->is_chunked     = r->read_chunked;
    s->no_more_chunks = 0;
    s->query_string   = r->args;

    s->startTime = r->request_time;
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
            return JK_ERR;
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

#ifdef USE_APRTABLES
        /* We can't do that - the filtering should happen in
           common to enable that.
           
          jk2_map_aprtable_factory( workerEnv->env, s->pool,
          &s->attributes,
          "map", "aprtable" );
          s->attributes->init( NULL, s->attributes, 0, XXX);
        */
        jk2_map_default_create(env, &s->attributes, s->pool );
#else
        jk2_map_default_create(env, &s->attributes, s->pool );
#endif
        
        if(workerEnv->envvars_in_use) {
            int envCnt=workerEnv->envvars->size( env, workerEnv->envvars );
            int i;

            for( i=0; i< envCnt ; i++ ) {
                char *name= workerEnv->envvars->nameAt( env, workerEnv->envvars, i );
                char *val= (char *)apr_table_get(r->subprocess_env, name);
                if(val==NULL) {
                    val=workerEnv->envvars->valueAt( env, workerEnv->envvars, i );
                }
                s->attributes->put( env, s->attributes, name, val, NULL );
            }
        }
    }

#ifdef USE_APRTABLES
    jk2_map_aprtable_factory( env, s->pool,
                             (void *)&s->headers_in,
                             "map", "aprtable" );
    s->headers_in->init( env, s->headers_in, 0, r->headers_in);
#else
    jk2_map_default_create(env, &s->headers_in, s->pool );

    if(r->headers_in && apr_table_elts(r->headers_in)) {
        const apr_array_header_t *t = apr_table_elts(r->headers_in);
        if(t && t->nelts) {
            int i;

            apr_table_entry_t *elts = (apr_table_entry_t *)t->elts;

            for(i = 0 ; i < t->nelts ; i++) {
                s->headers_in->add( env, s->headers_in,
                                    elts[i].key, elts[i].val);
            }
        }
    }
#endif

    if(!s->is_chunked && s->content_length == 0) {
        /* XXX if r->contentLength == 0 I assume there's no header
           or a header with '0'. In the second case, put will override it 
         */
        s->headers_in->put( env, s->headers_in, "content-length", "0", NULL );
    }

#ifdef USE_APRTABLES
    jk2_map_aprtable_factory( env, s->pool, (void *)&s->headers_out,
                             "map", "aprtable" );
    s->headers_in->init( env, s->headers_out, 0, r->headers_out);
#else
    jk2_map_default_create(env, &s->headers_out, s->pool );
#endif

    return JK_OK;
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
static void JK_METHOD jk2_service_apache2_afterRequest(jk_env_t *env, jk_ws_service_t *s )
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
    if (s->realWorker) {
        struct jk_worker *w = s->realWorker;
        if (w != NULL && w->channel != NULL 
            && w->channel->afterRequest != NULL) {
            w->channel->afterRequest( env, w->channel, w, NULL, s );
        }
    }
}

int JK_METHOD jk2_service_apache2_init(jk_env_t *env, jk_ws_service_t *s)
{
    if(s==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.init() NullPointerException\n");
        return JK_ERR;
    }

    s->head   = jk2_service_apache2_head;
    s->read   = jk2_service_apache2_read;
    s->write  = jk2_service_apache2_write;
    s->init   = jk2_init_ws_service;
    s->afterRequest     = jk2_service_apache2_afterRequest;

    return JK_OK;
}
