/*
 * Copyright (c) 1997-1999 The Java Apache Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * 4. The names "Apache JServ", "Apache JServ Servlet Engine" and 
 *    "Java Apache Project" must not be used to endorse or promote products 
 *    derived from this software without prior written permission.
 *
 * 5. Products derived from this software may not be called "Apache JServ"
 *    nor may "Apache" nor "Apache JServ" appear in their names without 
 *    prior written permission of the Java Apache Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *    
 * THIS SOFTWARE IS PROVIDED BY THE JAVA APACHE PROJECT "AS IS" AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE JAVA APACHE PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Java Apache Group. For more information
 * on the Java Apache Project and the Apache JServ Servlet Engine project,
 * please see <http://java.apache.org/>.
 *
 */

/***************************************************************************
 * Description: NSAPI plugin for Netscape servers                          *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                               *
 ***************************************************************************/


#include "jk_global.h"
#include "jk_util.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_ajp12_worker.h"

#include "nsapi.h"

#define URI_PATTERN "path"

struct nsapi_private_data {
    jk_pool_t p;
    
    int request_started;

    pblock *pb;
    Session *sn;
    Request *rq;
};
typedef struct nsapi_private_data nsapi_private_data_t;

static int init_on_other_thread_is_done = JK_FALSE;
static int init_on_other_thread_is_ok = JK_FALSE;

static jk_logger_t *logger = NULL;

static int JK_METHOD start_response(jk_ws_service_t *s,
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

NSAPI_PUBLIC int jk_init(pblock *pb, 
                         Session *sn, 
                         Request *rq);

NSAPI_PUBLIC void jk_term(void *p);

NSAPI_PUBLIC int jk_service(pblock *pb,
                            Session *sn,
                            Request *rq);

static int init_ws_service(nsapi_private_data_t *private_data,
                           jk_ws_service_t *s);

static int setup_http_headers(nsapi_private_data_t *private_data,
                              jk_ws_service_t *s);

static void init_workers_on_other_threads(void *init_d) 
{
    jk_map_t *init_map = (jk_map_t *)init_d;
    if(wc_open(init_map, logger)) {
        init_on_other_thread_is_ok = JK_TRUE;
    } else {
        jk_log(logger, JK_LOG_EMERG, "In init_workers_on_other_threads, failed\n");
    }

    init_on_other_thread_is_done = JK_TRUE;
}

static int JK_METHOD start_response(jk_ws_service_t *s,
                                    int status,
                                    const char *reason,
                                    const char * const *header_names,
                                    const char * const *header_values,
                                    unsigned num_of_headers)
{
    if(s && s->ws_private) {
        nsapi_private_data_t *p = s->ws_private;
        if(!p->request_started) {
            unsigned i;

            p->request_started = JK_TRUE;

            /* Remove "old" content type */
            param_free(pblock_remove("content-type", p->rq->srvhdrs));

            if(num_of_headers) {
                for (i = 0 ; i < (int)num_of_headers ; i++) {
                    pblock_nvinsert(header_names[i],
                                    header_values[i],
                                    p->rq->srvhdrs);    
                }
            } else {
                pblock_nvinsert("content-type",
                                "text/plain",
                                p->rq->srvhdrs);    
            }

            protocol_status(p->sn,
                            p->rq,
                            status,
                            (char *)reason);

            protocol_start_response(p->sn, p->rq);
        }
        return JK_TRUE;

    }
    return JK_FALSE;
}

static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b,
                             unsigned l,
                             unsigned *a)
{
    if(s && s->ws_private && b && a) {
        nsapi_private_data_t *p = s->ws_private;
        
        *a = 0;
        if(l) {
            char *buf = b;
            unsigned i;
            netbuf *inbuf = p->sn->inbuf;

/* Until we get a service pack for NW5.1 and earlier that has the latest */
/* Enterprise Server, we have to go through the else version of this code*/
#if defined(netbuf_getbytes) && !defined(NETWARE) 
            i = netbuf_getbytes(inbuf, b, l);
            if(NETBUF_EOF == i || NETBUF_ERROR == i) {
                return JK_FALSE;
            }

#else 
            int ch;
            for(i = 0 ; i < l ; i++) {
                ch = netbuf_getc(inbuf);           
                /*
                 * IO_EOF is 0 (zero) which is a very reasonable byte
                 * when it comes to binary data. So we are not breaking 
                 * out of the read loop when reading it.
                 *
                 * We are protected from an infinit loop by the Java part of
                 * Tomcat.
                 */
                if(IO_ERROR == ch) {
                    break;
                }

                buf[i] = ch;
            }

            if(0 == i) {
                return JK_FALSE;
            }
#endif
            *a = i;

        }
        return JK_TRUE;
    }
    return JK_FALSE;
}

static int JK_METHOD ws_write(jk_ws_service_t *s,
                              const void *b,
                              unsigned l)
{
    if(s && s->ws_private && b) {
        nsapi_private_data_t *p = s->ws_private;

        if(l) {
            if(!p->request_started) {
                start_response(s, 200, NULL, NULL, NULL, 0);
            }

            if(net_write(p->sn->csd, (char *)b, (int)l) == IO_ERROR) {
                return JK_FALSE;
            }
        }

        return JK_TRUE;

    }
    return JK_FALSE;
}

NSAPI_PUBLIC int jk_init(pblock *pb, 
                         Session *sn, 
                         Request *rq)
{
    char *worker_prp_file = pblock_findval(JK_WORKER_FILE_TAG, pb); 
    char *log_level_str = pblock_findval(JK_LOG_LEVEL_TAG, pb); 
    char *log_file = pblock_findval(JK_LOG_FILE_TAG, pb); 

    int rc = REQ_ABORTED;
    jk_map_t *init_map;

    fprintf(stderr, "In jk_init %s %s %s\n",worker_prp_file, log_level_str,  log_file);
    if(!worker_prp_file) {
        worker_prp_file = JK_WORKER_FILE_DEF;
    }

    if(!log_level_str) {
        log_level_str = JK_LOG_LEVEL_DEF;
    }    
    
    if(!jk_open_file_logger(&logger, log_file, 
                            jk_parse_log_level(log_level_str))) {
        logger = NULL;
    }

    if(map_alloc(&init_map)) {
        if(map_read_properties(init_map, worker_prp_file)) {
            int sleep_cnt;
            SYS_THREAD s;
            
            s = systhread_start(SYSTHREAD_DEFAULT_PRIORITY, 
                                0, 
                                init_workers_on_other_threads, 
                                init_map);
            for(sleep_cnt = 0 ; sleep_cnt < 60 ; sleep_cnt++) {
                systhread_sleep(1000);
                jk_log(logger, JK_LOG_DEBUG, "jk_init, a second passed\n");
                if(init_on_other_thread_is_done) {
                    break;
                }
            }

            if(init_on_other_thread_is_done &&
               init_on_other_thread_is_ok) {
                magnus_atrestart(jk_term, NULL);
                rc = REQ_PROCEED;
            }

/*            if(wc_open(init_map, logger)) {
                magnus_atrestart(jk_term, NULL);
                rc = REQ_PROCEED;
            }
*/
        }
        
        map_free(&init_map);
    }

    return rc;    
}

NSAPI_PUBLIC void jk_term(void *p)
{
    wc_close(logger);
    if(logger) {
        jk_close_file_logger(&logger);
    }
}

NSAPI_PUBLIC int jk_service(pblock *pb,
                            Session *sn,
                            Request *rq)
{
    char *worker_name = pblock_findval(JK_WORKER_NAME_TAG, pb); 
    char *uri_pattern = pblock_findval(URI_PATTERN, pb);     
    jk_worker_t *worker;    
    int rc = REQ_ABORTED;

    if(uri_pattern) {
        char *uri = pblock_findval("uri", rq->reqpb);

        if(0 != shexp_match(uri, uri_pattern)) {
            return REQ_NOACTION;
        }        
    }

    if(!worker_name) {
        worker_name = JK_AJP12_WORKER_NAME;
    }

    worker = wc_get_worker_for_name(worker_name, logger);
    if(worker) {
        nsapi_private_data_t private_data;
        jk_ws_service_t s;
        jk_pool_atom_t buf[SMALL_POOL_SIZE];

        jk_open_pool(&private_data.p, buf, sizeof(buf));

        private_data.request_started = JK_FALSE;
        private_data.pb = pb;
        private_data.sn = sn;
        private_data.rq = rq;

        jk_init_ws_service(&s);

        s.ws_private = &private_data;
        s.pool = &private_data.p;

        if(init_ws_service(&private_data, &s)) {
            jk_endpoint_t *e = NULL;
            if(worker->get_endpoint(worker, &e, logger)) {                
                int recover = JK_FALSE;
                if(e->service(e, &s, logger, &recover)) {
                    rc = REQ_PROCEED;
                }
                e->done(&e, logger);
            }
        }
        jk_close_pool(&private_data.p);
    }

    return rc;
}

static int init_ws_service(nsapi_private_data_t *private_data,
                           jk_ws_service_t *s) 
{
    char *tmp;
    int rc;

    s->jvm_route = NULL;
    s->start_response = start_response;
    s->read = ws_read;
    s->write = ws_write;
    
    s->auth_type        = pblock_findval("auth-type", private_data->rq->vars);
    s->remote_user      = pblock_findval("auth-user", private_data->rq->vars);

    s->content_length   = 0;
    tmp = NULL;
    rc  = request_header("content-length", 
                         &tmp, 
                         private_data->sn, 
                         private_data->rq);

    if((rc != REQ_ABORTED) && tmp) {
        s->content_length = atoi(tmp);
    }
    
    s->method           = pblock_findval("method", private_data->rq->reqpb);
    s->protocol         = pblock_findval("protocol", private_data->rq->reqpb);
    
    s->remote_host      = session_dns(private_data->sn);
    s->remote_addr      = pblock_findval("ip", private_data->sn->client);
    
    s->req_uri          = pblock_findval("uri", private_data->rq->reqpb);
    s->query_string     = pblock_findval("query", private_data->rq->reqpb);

    s->server_name      = server_hostname;
    s->server_port      = server_portnum;
    s->server_software  = system_version();


    s->headers_names    = NULL;
    s->headers_values   = NULL;
    s->num_headers      = 0;
    
    s->is_ssl           = security_active;
    if(s->is_ssl) {
        s->ssl_cert     = pblock_findval("auth-cert", private_data->rq->vars);
        if(s->ssl_cert) {
            s->ssl_cert_len = strlen(s->ssl_cert);
        }
        s->ssl_cipher   = pblock_findval("cipher", private_data->sn->client);
        s->ssl_session  = pblock_findval("ssl-id", private_data->sn->client);
    } else {
        s->ssl_cert     = NULL;
        s->ssl_cert_len = 0;
        s->ssl_cipher   = NULL;
        s->ssl_session  = NULL;
    }

    return setup_http_headers(private_data, s);
}

static int setup_http_headers(nsapi_private_data_t *private_data,
                              jk_ws_service_t *s) 
{
    pblock *headers_jar = private_data->rq->headers;
    int cnt;
    int i;

    for(i = 0, cnt = 0 ; i < headers_jar->hsize ; i++) {
        struct pb_entry *h = headers_jar->ht[i];
        while(h && h->param) {
            cnt++;
            h = h->next;
        }
    }

    if(cnt) {
        s->headers_names  = jk_pool_alloc(&private_data->p, cnt * sizeof(char *));
        s->headers_values = jk_pool_alloc(&private_data->p, cnt * sizeof(char *));

        if(s->headers_names && s->headers_values) {            
            for(i = 0, cnt = 0 ; i < headers_jar->hsize ; i++) {
                struct pb_entry *h = headers_jar->ht[i];
                while(h && h->param) {
                    s->headers_names[cnt]  = h->param->name;
                    s->headers_values[cnt] = h->param->value;
                    cnt++;
                    h = h->next;
                }
            }
            s->num_headers = cnt;
            return JK_TRUE;
        }
    } else {
        s->num_headers = cnt;
        s->headers_names  = NULL;
        s->headers_values = NULL;
        return JK_TRUE;
    }

    return JK_FALSE;
}
