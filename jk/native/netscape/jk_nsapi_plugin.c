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
 * Description: NSAPI plugin for Netscape servers                          *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/


#include "jk_global.h"
#include "jk_util.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"

#include "nsapi.h"

#define URI_PATTERN "path"
#define DEFAULT_WORKER_NAME ("ajp13")

struct nsapi_private_data
{
    jk_pool_t p;

    int request_started;

    pblock *pb;
    Session *sn;
    Request *rq;
};
typedef struct nsapi_private_data nsapi_private_data_t;

static int init_on_other_thread_is_done = JK_FALSE;
static int init_on_other_thread_is_ok = JK_FALSE;

static const char ssl_cert_start[] = "-----BEGIN CERTIFICATE-----\r\n";
static const char ssl_cert_end[] = "\r\n-----END CERTIFICATE-----\r\n";

static jk_logger_t *logger = NULL;
static jk_worker_env_t worker_env;

#ifdef NETWARE
int (*PR_IsSocketSecure) (SYS_NETFD * csd);     /* pointer to PR_IsSocketSecure function */
#endif

static int JK_METHOD start_response(jk_ws_service_t *s,
                                    int status,
                                    const char *reason,
                                    const char *const *header_names,
                                    const char *const *header_values,
                                    unsigned num_of_headers);

static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b, unsigned l, unsigned *a);

static int JK_METHOD ws_write(jk_ws_service_t *s, const void *b, unsigned l);

NSAPI_PUBLIC int jk_init(pblock * pb, Session * sn, Request * rq);

NSAPI_PUBLIC void jk_term(void *p);

NSAPI_PUBLIC int jk_service(pblock * pb, Session * sn, Request * rq);

static int init_ws_service(nsapi_private_data_t * private_data,
                           jk_ws_service_t *s);

static int setup_http_headers(nsapi_private_data_t * private_data,
                              jk_ws_service_t *s);

static void init_workers_on_other_threads(void *init_d)
{
    jk_map_t *init_map = (jk_map_t *)init_d;
    /* we add the URI->WORKER MAP since workers using AJP14 will feed it */
    /* but where are they here in Netscape ? */
    if (wc_open(init_map, &worker_env, logger)) {
        init_on_other_thread_is_ok = JK_TRUE;
    }
    else {
        jk_log(logger, JK_LOG_EMERG,
               "In init_workers_on_other_threads, failed\n");
    }

    init_on_other_thread_is_done = JK_TRUE;
}

static int JK_METHOD start_response(jk_ws_service_t *s,
                                    int status,
                                    const char *reason,
                                    const char *const *header_names,
                                    const char *const *header_values,
                                    unsigned num_of_headers)
{
    if (s && s->ws_private) {
        nsapi_private_data_t *p = s->ws_private;
        if (!p->request_started) {
            unsigned i;

            p->request_started = JK_TRUE;

            /* Remove "old" content type */
            param_free(pblock_remove("content-type", p->rq->srvhdrs));

            if (num_of_headers) {
                for (i = 0; i < (int)num_of_headers; i++) {
                    pblock_nvinsert(header_names[i],
                                    header_values[i], p->rq->srvhdrs);
                }
            }
            else {
                pblock_nvinsert("content-type", "text/plain", p->rq->srvhdrs);
            }

            protocol_status(p->sn, p->rq, status, (char *)reason);

            protocol_start_response(p->sn, p->rq);
        }
        return JK_TRUE;

    }
    return JK_FALSE;
}

static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b, unsigned l, unsigned *a)
{
    if (s && s->ws_private && b && a) {
        nsapi_private_data_t *p = s->ws_private;

        *a = 0;
        if (l) {
            char *buf = b;
            unsigned i;
            netbuf *inbuf = p->sn->inbuf;

/* Until we get a service pack for NW5.1 and earlier that has the latest */
/* Enterprise Server, we have to go through the else version of this code*/
#if defined(netbuf_getbytes) && !defined(NETWARE)
            i = netbuf_getbytes(inbuf, b, l);
            if (NETBUF_EOF == i || NETBUF_ERROR == i) {
                return JK_FALSE;
            }

#else
            int ch;
            for (i = 0; i < l; i++) {
                ch = netbuf_getc(inbuf);
                /*
                 * IO_EOF is 0 (zero) which is a very reasonable byte
                 * when it comes to binary data. So we are not breaking 
                 * out of the read loop when reading it.
                 *
                 * We are protected from an infinit loop by the Java part of
                 * Tomcat.
                 */
                if (IO_ERROR == ch) {
                    break;
                }

                buf[i] = ch;
            }

            if (0 == i) {
                return JK_FALSE;
            }
#endif
            *a = i;

        }
        return JK_TRUE;
    }
    return JK_FALSE;
}

static int JK_METHOD ws_write(jk_ws_service_t *s, const void *b, unsigned l)
{
    if (s && s->ws_private && b) {
        nsapi_private_data_t *p = s->ws_private;

        if (l) {
            if (!p->request_started) {
                start_response(s, 200, NULL, NULL, NULL, 0);
            }

            if (net_write(p->sn->csd, (char *)b, (int)l) == IO_ERROR) {
                return JK_FALSE;
            }
        }

        return JK_TRUE;

    }
    return JK_FALSE;
}

NSAPI_PUBLIC int jk_init(pblock * pb, Session * sn, Request * rq)
{
    char *worker_prp_file = pblock_findval(JK_WORKER_FILE_TAG, pb);
    char *log_level_str = pblock_findval(JK_LOG_LEVEL_TAG, pb);
    char *log_file = pblock_findval(JK_LOG_FILE_TAG, pb);

    int rc = REQ_ABORTED;
    jk_map_t *init_map;

    fprintf(stderr,
            "In jk_init.\n   Worker file = %s.\n   Log level = %s.\n   Log File = %s\n",
            worker_prp_file, log_level_str, log_file);
    if (!worker_prp_file) {
        worker_prp_file = JK_WORKER_FILE_DEF;
    }

    if (!log_level_str) {
        log_level_str = JK_LOG_LEVEL_DEF;
    }

    if (!jk_open_file_logger(&logger, log_file,
                             jk_parse_log_level(log_level_str))) {
        logger = NULL;
    }

    if (jk_map_alloc(&init_map)) {
        if (jk_map_read_properties(init_map, worker_prp_file)) {
            int sleep_cnt;
            SYS_THREAD s;

            s = systhread_start(SYSTHREAD_DEFAULT_PRIORITY,
                                0, init_workers_on_other_threads, init_map);
            for (sleep_cnt = 0; sleep_cnt < 60; sleep_cnt++) {
                systhread_sleep(1000);
                jk_log(logger, JK_LOG_DEBUG, "jk_init, a second passed\n");
                if (init_on_other_thread_is_done) {
                    break;
                }
            }

            if (init_on_other_thread_is_done && init_on_other_thread_is_ok) {
                magnus_atrestart(jk_term, NULL);
                rc = REQ_PROCEED;
            }

/*            if(wc_open(init_map, NULL, logger)) {
                magnus_atrestart(jk_term, NULL);
                rc = REQ_PROCEED;
            }
*/
        }

        jk_map_free(&init_map);
    }

#ifdef NETWARE
    PR_IsSocketSecure =
        (int (*)(void **))ImportSymbol(GetNLMHandle(), "PR_IsSocketSecure");
#endif
    return rc;
}

NSAPI_PUBLIC void jk_term(void *p)
{
#ifdef NETWARE
    if (NULL != PR_IsSocketSecure) {
        UnimportSymbol(GetNLMHandle(), "PR_IsSocketSecure");
        PR_IsSocketSecure = NULL;
    }
#endif
    wc_close(logger);
    if (logger) {
        jk_close_file_logger(&logger);
    }
}

NSAPI_PUBLIC int jk_service(pblock * pb, Session * sn, Request * rq)
{
    char *worker_name = pblock_findval(JK_WORKER_NAME_TAG, pb);
    char *uri_pattern = pblock_findval(URI_PATTERN, pb);
    jk_worker_t *worker;
    int rc = REQ_ABORTED;

    if (uri_pattern) {
        char *uri = pblock_findval("uri", rq->reqpb);

        if (0 != shexp_match(uri, uri_pattern)) {
            return REQ_NOACTION;
        }
    }

    if (!worker_name) {
        worker_name = DEFAULT_WORKER_NAME;
    }

    worker = wc_get_worker_for_name(worker_name, logger);
    if (worker) {
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
        if (init_ws_service(&private_data, &s)) {
            jk_endpoint_t *e = NULL;
            /* Update retries for this worker */
            s.retries = worker->retries;                 
            if (worker->get_endpoint(worker, &e, logger)) {
                int recover = JK_FALSE;
                if (e->service(e, &s, logger, &recover)) {
                    rc = REQ_PROCEED;
                }
                e->done(&e, logger);
            }
        }
        jk_close_pool(&private_data.p);
    }

    return rc;
}

static int init_ws_service(nsapi_private_data_t * private_data,
                           jk_ws_service_t *s)
{
    char *tmp;
    int rc;

    s->jvm_route = NULL;
    s->start_response = start_response;
    s->read = ws_read;
    s->write = ws_write;

    /* Clear RECO status */
    s->reco_status = RECO_NONE;

    s->auth_type = pblock_findval("auth-type", private_data->rq->vars);
    s->remote_user = pblock_findval("auth-user", private_data->rq->vars);

    s->content_length = 0;
    tmp = NULL;
    rc = request_header("content-length",
                        &tmp, private_data->sn, private_data->rq);

    if ((rc != REQ_ABORTED) && tmp) {
        s->content_length = atoi(tmp);
    }

    s->method = pblock_findval("method", private_data->rq->reqpb);
    s->protocol = pblock_findval("protocol", private_data->rq->reqpb);

    s->remote_host = session_dns(private_data->sn);
    s->remote_addr = pblock_findval("ip", private_data->sn->client);

    s->req_uri = pblock_findval("uri", private_data->rq->reqpb);
    s->query_string = pblock_findval("query", private_data->rq->reqpb);

    s->server_name = server_hostname;

#ifdef NETWARE
    /* On NetWare, since we have virtual servers, we have a different way of
     * getting the port that we need to try first.
     */
    tmp = pblock_findval("server_port", private_data->sn->client);
    if (NULL != tmp)
        s->server_port = atoi(tmp);
    else
#endif
        s->server_port = server_portnum;
    s->server_software = system_version();


    s->headers_names = NULL;
    s->headers_values = NULL;
    s->num_headers = 0;

#ifdef NETWARE
    /* on NetWare, we can have virtual servers that are secure.  
     * PR_IsSocketSecure is an api made available with virtual servers to check
     * if the socket is secure or not
     */
    if (NULL != PR_IsSocketSecure)
        s->is_ssl = PR_IsSocketSecure(private_data->sn->csd);
    else
#endif
        s->is_ssl = security_active;

    s->ssl_key_size = -1;       /* required by Servlet 2.3 Api, added in jtc */
    if (s->is_ssl) {
        char *ssl_cert = pblock_findval("auth-cert", private_data->rq->vars);
        if (ssl_cert != NULL) {
            s->ssl_cert = jk_pool_alloc(s->pool, sizeof(ssl_cert_start)+
                                                 strlen(ssl_cert)+
                                                 sizeof(ssl_cert_end));
            strcpy(s->ssl_cert, ssl_cert_start);
            strcat(s->ssl_cert, ssl_cert);
            strcat(s->ssl_cert, ssl_cert_end);
            s->ssl_cert_len = strlen(s->ssl_cert);
        }
        s->ssl_cipher = pblock_findval("cipher", private_data->sn->client);
        s->ssl_session = pblock_findval("ssl-id", private_data->sn->client);
    }
    else {
        s->ssl_cert = NULL;
        s->ssl_cert_len = 0;
        s->ssl_cipher = NULL;
        s->ssl_session = NULL;
    }

    return setup_http_headers(private_data, s);
}

static int setup_http_headers(nsapi_private_data_t * private_data,
                              jk_ws_service_t *s)
{
    int need_content_length_header =
        (s->content_length == 0) ? JK_TRUE : JK_FALSE;

    pblock *headers_jar = private_data->rq->headers;
    int cnt;
    int i;

    for (i = 0, cnt = 0; i < headers_jar->hsize; i++) {
        struct pb_entry *h = headers_jar->ht[i];
        while (h && h->param) {
            cnt++;
            h = h->next;
        }
    }

    s->headers_names = NULL;
    s->headers_values = NULL;
    s->num_headers = cnt;
    if (cnt) {
        /* allocate an extra header slot in case we need to add a content-length header */
        s->headers_names =
            jk_pool_alloc(&private_data->p, (cnt + 1) * sizeof(char *));
        s->headers_values =
            jk_pool_alloc(&private_data->p, (cnt + 1) * sizeof(char *));

        if (s->headers_names && s->headers_values) {
            for (i = 0, cnt = 0; i < headers_jar->hsize; i++) {
                struct pb_entry *h = headers_jar->ht[i];
                while (h && h->param) {
                    s->headers_names[cnt] = h->param->name;
                    s->headers_values[cnt] = h->param->value;
                    if (need_content_length_header &&
                        !strncmp(h->param->name, "content-length", 14)) {
                        need_content_length_header = JK_FALSE;
                    }
                    cnt++;
                    h = h->next;
                }
            }
            /* Add a content-length = 0 header if needed. 
             * Ajp13 assumes an absent content-length header means an unknown, 
             * but non-zero length body. 
             */
            if (need_content_length_header) {
                s->headers_names[cnt] = "content-length";
                s->headers_values[cnt] = "0";
                cnt++;
            }
            s->num_headers = cnt;
            return JK_TRUE;
        }
    }
    else {
        if (need_content_length_header) {
            s->headers_names =
                jk_pool_alloc(&private_data->p, sizeof(char *));
            s->headers_values =
                jk_pool_alloc(&private_data->p, sizeof(char *));
            if (s->headers_names && s->headers_values) {
                s->headers_names[0] = "content-length";
                s->headers_values[0] = "0";
                s->num_headers++;
                return JK_TRUE;
            }
        }
        else
            return JK_TRUE;
    }

    return JK_FALSE;
}
