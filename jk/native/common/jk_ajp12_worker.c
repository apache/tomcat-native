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
 * Description: ajpv1.2 worker, used to call local or remote jserv hosts   *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Based on:    jserv_ajpv12.c from Jserv                                  *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_ajp12_worker.h"
#include "jk_pool.h"
#include "jk_connect.h"
#include "jk_util.h"
#include "jk_sockbuf.h"

#define AJP_DEF_HOST            ("localhost")
#define AJP_DEF_PORT            (8007)
#define READ_BUF_SIZE           (8*1024)
#define DEF_RETRY_ATTEMPTS      (1)

struct ajp12_worker {
    struct sockaddr_in worker_inet_addr;
    unsigned connect_retry_attempts;
    char *name; 
    jk_worker_t worker;
};

typedef struct ajp12_worker ajp12_worker_t;

struct ajp12_endpoint { 
    ajp12_worker_t *worker;
    
    int sd;
    jk_sockbuf_t sb;

    jk_endpoint_t endpoint;
};
typedef struct ajp12_endpoint ajp12_endpoint_t;

static int ajpv12_mark(ajp12_endpoint_t *p, 
                       unsigned char type);

static int ajpv12_sendstring(ajp12_endpoint_t *p, 
                             const char *buffer);

static int ajpv12_sendint(ajp12_endpoint_t *p, 
                          int d);

static int ajpv12_sendnbytes(ajp12_endpoint_t *p, 
                             const void *buffer, 
                             int bufferlen);

static int ajpv12_flush(ajp12_endpoint_t *p);

static int ajpv12_handle_response(ajp12_endpoint_t *p, 
                                  jk_ws_service_t *s,
                                  jk_logger_t *l);

static int ajpv12_handle_request(ajp12_endpoint_t *p, 
                                 jk_ws_service_t *s,
                                 jk_logger_t *l);

static int JK_METHOD service(jk_endpoint_t *e, 
                             jk_ws_service_t *s,
                             jk_logger_t *l,
                             int *is_recoverable_error)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_endpoint_t::service\n");

    if(e && e->endpoint_private && s && is_recoverable_error) {
        ajp12_endpoint_t *p = e->endpoint_private;
        unsigned attempt;

        *is_recoverable_error = JK_TRUE;

        for(attempt = 0 ; attempt < p->worker->connect_retry_attempts ; attempt++) {
            p->sd = jk_open_socket(&p->worker->worker_inet_addr, 
                                   JK_TRUE, 
                                   l);

            jk_log(l, JK_LOG_DEBUG, "In jk_endpoint_t::service, sd = %d\n", p->sd);
            if(p->sd >= 0) {
                break;
            }
        }
        if(p->sd >= 0) {

            /*
             * After we are connected, each error that we are going to
             * have is probably unrecoverable
             */
            *is_recoverable_error = JK_FALSE;
            jk_sb_open(&p->sb, p->sd);
            if(ajpv12_handle_request(p, s, l)) {
                jk_log(l, JK_LOG_DEBUG, "In jk_endpoint_t::service, sent request\n");
                return ajpv12_handle_response(p, s, l);
            }                        
        }
        jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, Error sd = %d\n", p->sd);
    } else {
        jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, NULL parameters\n");
    }

    return JK_FALSE;
}

static int JK_METHOD done(jk_endpoint_t **e,
                          jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_endpoint_t::done\n");
    if(e && *e && (*e)->endpoint_private) {
        ajp12_endpoint_t *p = (*e)->endpoint_private;
        if(p->sd > 0) {
            jk_close_socket(p->sd);
        }
        free(p);
        *e = NULL;
        return JK_TRUE;
    }

    jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::done, NULL parameters\n");
    return JK_FALSE;
}

static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t *props,                            
                              jk_worker_env_t *we,
                              jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::validate\n");

    if(pThis && pThis->worker_private) {        
        ajp12_worker_t *p = pThis->worker_private;
        int port = jk_get_worker_port(props, 
                                      p->name,
                                      AJP_DEF_PORT);

        char *host = jk_get_worker_host(props, 
                                        p->name,
                                        AJP_DEF_HOST);

        jk_log(l, JK_LOG_DEBUG, "In jk_worker_t::validate for worker %s contact is %s:%d\n", 
               p->name, host, port);

        if(port > 1024 && host) {
            if(jk_resolve(host, (short)port, &p->worker_inet_addr)) {
                return JK_TRUE;
            }
            jk_log(l, JK_LOG_ERROR, "In jk_worker_t::validate, resolve failed\n");
        }
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::validate, Error %s %d\n", host, port);
    } else {
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::validate, NULL parameters\n");
    }

    return JK_FALSE;
}

static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t *props, 
                          jk_worker_env_t *we,
                          jk_logger_t *log)
{
    /* Nothing to do for now */
    return JK_TRUE;
}

static int JK_METHOD get_endpoint(jk_worker_t *pThis,
                                  jk_endpoint_t **pend,
                                  jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::get_endpoint\n");

    if(pThis && pThis->worker_private && pend) {        
        ajp12_endpoint_t *p = (ajp12_endpoint_t *)malloc(sizeof(ajp12_endpoint_t));
        if(p) {
            p->sd = -1;         
            p->worker = pThis->worker_private;
            p->endpoint.endpoint_private = p;
            p->endpoint.service = service;
            p->endpoint.done = done;
            *pend = &p->endpoint;
            return JK_TRUE;
        }
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::get_endpoint, malloc failed\n");
    } else {
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::get_endpoint, NULL parameters\n");
    }

    return JK_FALSE;
}

static int JK_METHOD destroy(jk_worker_t **pThis,
                             jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::destroy\n");
    if(pThis && *pThis && (*pThis)->worker_private) {
        ajp12_worker_t *private_data = (*pThis)->worker_private;
        free(private_data->name);
        free(private_data);

        return JK_TRUE;
    }

    jk_log(l, JK_LOG_ERROR, "In jk_worker_t::destroy, NULL parameters\n");
    return JK_FALSE;
}

int JK_METHOD ajp12_worker_factory(jk_worker_t **w,
                                   const char *name,
                                   jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into ajp12_worker_factory\n");
    if(NULL != name && NULL != w) {
        ajp12_worker_t *private_data = 
            (ajp12_worker_t *)malloc(sizeof(ajp12_worker_t));

        if(private_data) {
            private_data->name = strdup(name);          

            if(private_data->name) {
                private_data->connect_retry_attempts= DEF_RETRY_ATTEMPTS;
                private_data->worker.worker_private = private_data;

                private_data->worker.validate       = validate;
                private_data->worker.init           = init;
                private_data->worker.get_endpoint   = get_endpoint;
                private_data->worker.destroy        = destroy;

                *w = &private_data->worker;
                return JK_TRUE;
            }

            free(private_data);
        } 
        jk_log(l, JK_LOG_ERROR, "In ajp12_worker_factory, malloc failed\n");
    } else {
        jk_log(l, JK_LOG_ERROR, "In ajp12_worker_factory, NULL parameters\n");
    }

    return JK_FALSE;
}

static int ajpv12_sendnbytes(ajp12_endpoint_t *p, 
                             const void *buffer, 
                             int bufferlen) 
{
    unsigned char bytes[2];
    static const unsigned char null_b[2] = { (unsigned char)0xff, (unsigned char)0xff };

    if(buffer) {
        bytes[0] = (unsigned char) ( (bufferlen >> 8) & 0xff );
        bytes[1] = (unsigned char) ( bufferlen & 0xff );

        if(jk_sb_write(&p->sb, bytes, 2)) {
            return jk_sb_write(&p->sb, buffer, bufferlen);
        } else {
            return JK_FALSE;
        }
    } else {
        return jk_sb_write(&p->sb, null_b, 2);
    }
}

static int ajpv12_sendstring(ajp12_endpoint_t *p, 
                             const char *buffer) 
{
    int bufferlen;

    if(buffer && (bufferlen = strlen(buffer))) {
        return ajpv12_sendnbytes(p, buffer, bufferlen);
    } else {
        return ajpv12_sendnbytes(p, NULL, 0);
    }
}

static int ajpv12_mark(ajp12_endpoint_t *p, 
                       unsigned char type) 
{
    if(jk_sb_write(&p->sb, &type, 1)) {
        return JK_TRUE;
    } else {
        return JK_FALSE;
    }
}

static int ajpv12_sendint(ajp12_endpoint_t *p, 
                          int d)
{
    char buf[20];
    sprintf(buf, "%d", d);
    return ajpv12_sendstring(p, buf);
}

static int ajpv12_flush(ajp12_endpoint_t *p)
{
    return jk_sb_flush(&p->sb);
}

static int ajpv12_handle_request(ajp12_endpoint_t *p, 
                                 jk_ws_service_t *s,
                                 jk_logger_t *l) 
{
    int ret;

    jk_log(l, JK_LOG_DEBUG, "Into ajpv12_handle_request\n");
    /*
     * Start the ajp 12 service sequence
     */
    jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_request, sending the ajp12 start sequence\n");
    
    ret = (ajpv12_mark(p, 1) &&
           ajpv12_sendstring(p, s->method) &&
           ajpv12_sendstring(p, 0) &&   /* zone */
           ajpv12_sendstring(p, 0) &&   /* servlet */
           ajpv12_sendstring(p, s->server_name) &&
           ajpv12_sendstring(p, 0) &&   /* doc root */
           ajpv12_sendstring(p, 0) &&   /* path info */
           ajpv12_sendstring(p, 0) &&   /* path translated */
           ajpv12_sendstring(p, s->query_string)&&
           ajpv12_sendstring(p, s->remote_addr) &&
           ajpv12_sendstring(p, s->remote_host) &&
           ajpv12_sendstring(p, s->remote_user) &&
           ajpv12_sendstring(p, s->auth_type)   &&
           ajpv12_sendint(p, s->server_port)    &&
           ajpv12_sendstring(p, s->method)      &&
           ajpv12_sendstring(p, s->req_uri)     &&
           ajpv12_sendstring(p, 0)              && /* */
           ajpv12_sendstring(p, 0)              && /* SCRIPT_NAME */
           ajpv12_sendstring(p, s->server_name) &&
           ajpv12_sendint(p, s->server_port)    &&
           ajpv12_sendstring(p, s->protocol)    &&
           ajpv12_sendstring(p, 0)              && /* SERVER_SIGNATURE */ 
           ajpv12_sendstring(p, s->server_software) &&
           ajpv12_sendstring(p, s->jvm_route)   && /* JSERV_ROUTE */
           ajpv12_sendstring(p, "")             && /* JSERV ajpv12 compatibility */
           ajpv12_sendstring(p, ""));              /* JSERV ajpv12 compatibility */

    if(!ret) {
        jk_log(l, JK_LOG_ERROR, 
              "In ajpv12_handle_request, failed to send the ajp12 start sequence\n");
        return JK_FALSE;
    }

    if(s->num_attributes > 0) {
        unsigned  i;
        jk_log(l, JK_LOG_DEBUG, 
               "ajpv12_handle_request, sending the environment variables\n");
    
        for(i = 0 ; i < s->num_attributes ; i++) {
            ret = (ajpv12_mark(p, 5)                            &&
	    	       ajpv12_sendstring(p, s->attributes_names[i]) &&
		           ajpv12_sendstring(p, s->attributes_values[i]));
            if(!ret) {
                jk_log(l, JK_LOG_ERROR, 
                       "In ajpv12_handle_request, failed to send environment\n");
                return JK_FALSE;
            }
        }
    }

    jk_log(l, JK_LOG_DEBUG, 
           "ajpv12_handle_request, sending the headers\n");

    /* Send the request headers */
    if(s->num_headers) {
        unsigned  i;
        for(i = 0 ; i < s->num_headers ; ++i) {
            ret = (ajpv12_mark(p, 3) &&
                   ajpv12_sendstring(p, s->headers_names[i]) &&
                   ajpv12_sendstring(p, s->headers_values[i]) );

            if(!ret) {
                jk_log(l, JK_LOG_ERROR, 
                       "In ajpv12_handle_request, failed to send headers\n");
                return JK_FALSE;
            }
        }
    }

    jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_request, sending the terminating mark\n");

    ret =  (ajpv12_mark(p, 4) && ajpv12_flush(p));
    if(!ret) {
        jk_log(l, JK_LOG_ERROR, 
               "In ajpv12_handle_request, failed to send the terminating mark\n");
        return JK_FALSE;
    }

    if(s->content_length) {
        char buf[READ_BUF_SIZE];
        unsigned so_far = 0;

        jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_request, sending the request body\n");

        while(so_far < s->content_length) {
            unsigned this_time = 0;
            unsigned to_read = s->content_length - so_far;
            if(to_read > READ_BUF_SIZE) {
                to_read = READ_BUF_SIZE;
            }

            if(!s->read(s, buf, to_read, &this_time)) {
                jk_log(l, JK_LOG_ERROR, 
                       "In ajpv12_handle_request, failed to read from the web server\n");
                return JK_FALSE;
            }
            jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_request, read %d bytes\n", this_time);
            if(this_time > 0) {
                so_far += this_time;
                if((int)this_time != send(p->sd, buf, this_time, 0)) {
                    jk_log(l, JK_LOG_ERROR, 
                           "In ajpv12_handle_request, failed to write to the container\n");
                    return JK_FALSE;
                }
                jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_request, sent %d bytes\n", this_time);
            } else if (this_time == 0) {
             jk_log(l, JK_LOG_ERROR,
                     "In ajpv12_handle_request, Error: short read. content length is %d, read %d\n",
                     s->content_length, so_far);
             return JK_FALSE;
            }
        }
    }

    jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_request done\n");
    return JK_TRUE;
}

static int ajpv12_handle_response(ajp12_endpoint_t *p, 
                                  jk_ws_service_t *s,
                                  jk_logger_t *l) 
{
    int status      = 200;
    char *reason    = NULL;
    char **names    = NULL;
    char **values   = NULL;
    int  headers_capacity = 0;
    int  headers_len = 0;
    int  write_to_ws;

    jk_log(l, JK_LOG_DEBUG, "Into ajpv12_handle_response\n");
    /*
     * Read headers ...
     */
    while(1) {
        char *line  = NULL;
        char *name  = NULL;
        char *value = NULL;

        if(!jk_sb_gets(&p->sb, &line)) {
            jk_log(l, JK_LOG_ERROR, "ajpv12_handle_response, error reading header line\n");
            return JK_FALSE;
        }
        
        jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response, read %s\n", line);
        if(0 == strlen(line)) {
            jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response, headers are done\n");
            break;      /* Empty line -> end of headers */
        }

        name = line;
        while(isspace(*name) && *name) {
            name++;     /* Skip leading white chars */
        }
        if(!*name) {    /* Empty header name */
            jk_log(l, JK_LOG_ERROR, "ajpv12_handle_response, empty header name\n");
            return JK_FALSE;
        }
        if(!(value = strchr(name, ':'))) { 
            jk_log(l, JK_LOG_ERROR, "ajpv12_handle_response, no value supplied\n");
            return JK_FALSE; /* No value !!! */
        }
        *value = '\0';
        value++;
        while(isspace(*value) && *value) { 
            value++;    /* Skip leading white chars */
        }
        if(!*value) {   /* Empty header value */
            jk_log(l, JK_LOG_ERROR, "ajpv12_handle_response, empty header value\n");
            return JK_FALSE;
        }

        jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response, read %s=%s\n", name, value);
        if(0 == strcmp("Status", name)) {
            char *numeric = strtok(value, " \t");

            status = atoi(numeric);
            if(status < 100 || status > 999) {
                jk_log(l, JK_LOG_ERROR, "ajpv12_handle_response, invalid status code\n");
                return JK_FALSE;
            }
            reason = strtok(NULL, " \t");
        } else {
            if(headers_capacity == headers_len) {
                jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response, allocating header arrays\n");
                names = (char **)jk_pool_realloc(s->pool,  
                                                 sizeof(char *) * (headers_capacity + 5),
                                                 names,
                                                 sizeof(char *) * headers_capacity);
                values = (char **)jk_pool_realloc(s->pool,  
                                                  sizeof(char *) * (headers_capacity + 5),
                                                  values,
                                                  sizeof(char *) * headers_capacity);
                if(!values || !names) {
                    jk_log(l, JK_LOG_ERROR, "ajpv12_handle_response, malloc error\n");
                    return JK_FALSE;
                }
                headers_capacity = headers_capacity + 5;
            }
            names[headers_len] = jk_pool_strdup(s->pool, name);
            values[headers_len] = jk_pool_strdup(s->pool, value);
            headers_len++;
        }
    }

    jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response, starting response\n");
    if(!s->start_response(s, 
                          status, 
                          reason, 
                          (const char * const *)names, 
                          (const char * const *)values, 
                          headers_len)) {
        jk_log(l, JK_LOG_ERROR, "ajpv12_handle_response, error starting response\n");
        return JK_FALSE;
    }

    jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response, reading response body\n");
    /*
     * Read response body
     */
    write_to_ws = JK_TRUE;
    while(1) {
        unsigned to_read = READ_BUF_SIZE;
        unsigned acc = 0;
        char *buf = NULL;
    
        if(!jk_sb_read(&p->sb, &buf, to_read, &acc)) {
            jk_log(l, JK_LOG_ERROR, "ajpv12_handle_response, error reading from \n");
            return JK_FALSE;
        }

        if(!acc) {
            jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response, response body is done\n");
            break;
        }

        if(write_to_ws) {
            if(!s->write(s, buf, acc)) {
                jk_log(l, JK_LOG_ERROR, "ajpv12_handle_response, error writing back to server\n");
                write_to_ws = JK_FALSE;
            }
        }
    }

    jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response done\n");
    return JK_TRUE;
}
