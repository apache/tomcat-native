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

/**
 * @package Request Handling
 * @author  Pier Fumagalli <mailto:pier@betaversion.org>
 * @version $Id$
 */
#ifndef _WA_REQUEST_H_
#define _WA_REQUEST_H_

/**
 * The host description structure.
 */
struct wa_hostdata {
    /** The host name. */
    char *host;
    /** The host address (as a string - no worries about IPv6) */
    char *addr;
    /** The port number. */
    int port;
};

/**
 * The SSL data structure.
 */
struct wa_ssldata {
    /** The client certificate. */
    char *cert;
    /** The cipher algorithm. */
    char *ciph;
    /** The SSL session name. */
    char *sess;
    /** Number of bits in the key. */
    int size;
};

/**
 * The webserver request handler callback structure.
 */
struct wa_handler {
    void (*log)(wa_request *r, const char *file, const int line, char *msg);
    void (*setstatus)(wa_request *r, int status, char *message);
    void (*setctype)(wa_request *r, char *type);
    void (*setheader)(wa_request *r, char *name, char *value);
    void (*commit)(wa_request *r);
    void (*flush)(wa_request *r);
    int (*read)(wa_request *r, char *buf, int len);
    int (*write)(wa_request *r, char *buf, int len);
};

/**
 * The WebApp Library HTTP request structure.
 * <br>
 * This structure encapsulates an HTTP request to be handled within the scope
 * of one of the configured applications.
 */
struct wa_request {
    /** The APR memory pool where this request is allocated. */
    apr_pool_t *pool;
    /** The request handler structure associated with this request. */
    wa_handler *hand;
    /** The web-server specific callback data.*/
    void *data;
    /** The server host data. */
    wa_hostdata *serv;
    /** The client host data. */
    wa_hostdata *clnt;
    /** The HTTP method (ex. GET, POST...). */
    char *meth;
    /** The HTTP request URI (ex. /webapp/index.html). */
    char *ruri;
    /** The URL-encoded list of HTTP query arguments from the request. */
    char *args;
    /** The HTTP protocol name and version (ex. HTTP/1.0, HTTP/1.1...). */
    char *prot;
    /** The HTTP request URL scheme (the part before ://, ex http, https). */
    char *schm;
    /** The remote user name or <b>NULL</b>. */
    char *user;
    /** The authentication method or <b>NULL</b>. */
    char *auth;
    /** The content length of this request. */
    long clen;
    /** The content type of this request. */
    char *ctyp;
    /** The number of bytes read out of this request body. */
    long rlen;
    /** The SSL data structure. */
    wa_ssldata *ssld;
    /** The current headers table. */
    apr_table_t *hdrs;
};

/**
 * Allocate a new request structure.
 *
 * @param r A pointer to where the newly allocated <code>wa_request</code>
 *          structure must be allocated.
 * @param h The web-server specific handler for this request.
 * @param d The web-server specific data for this request.
 * @return An error message on faliure or <b>NULL</b>.
 */
const char *wa_ralloc(wa_request **r, wa_handler *h, void *d);

/**
 * Clean up and free the memory used by a request structure.
 *
 * @param r The request structure to destroy.
 * @return An error message on faliure or <b>NULL</b>.
 */
const char *wa_rfree(wa_request *r);

/**
 * Report an HTTP error to the client.
 *
 * @param r The WebApp Library request structure.
 * @param s The HTTP response status number.
 * @param fmt The message format string (printf style).
 * @param ... The parameters to the format string.
 * @return The HTTP result code of this operation.
 */
int wa_rerror(const char *file, const int line, wa_request *r, int s,
              const char *fmt, ...);

/**
 * Invoke a request in a web application.
 *
 * @param r The WebApp Library request structure.
 * @param a The application to which this request needs to be forwarded.
 * @return The HTTP result code of this operation.
 */
int wa_rinvoke(wa_request *r, wa_application *a);

void wa_rlog(wa_request *r, const char *f, const int l, const char *fmt, ...);
void wa_rsetstatus(wa_request *r, int status, char *message);
void wa_rsetctype(wa_request *r, char *type);
void wa_rsetheader(wa_request *r, char *name, char *value);
void wa_rcommit(wa_request *r);
void wa_rflush(wa_request *r);
int wa_rread(wa_request *r, char *buf, int len);
int wa_rwrite(wa_request *r, char *buf, int len);
int wa_rprintf(wa_request *r, const char *fmt, ...);

#endif /* ifndef _WA_REQUEST_H_ */
