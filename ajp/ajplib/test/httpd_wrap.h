/* Copyright 2000-2004 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef HTTPD_WRAP_H
#define HTTPD_WRAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "apr.h"
#include "apr_hooks.h"
#include "apr_optional_hooks.h"
#include "apr_thread_proc.h"

/* No WIN32 dll support */

#define AP_DECLARE(type)            type
#define AP_DECLARE_NONSTD(type)     type
#define AP_DECLARE_DATA

/** The default string lengths */
#define MAX_STRING_LEN HUGE_STRING_LEN
#define HUGE_STRING_LEN 8192

/** The size of the server's internal read-write buffers */
#define AP_IOBUFSIZE 8192

/* fake structure declarations */
typedef struct process_rec  process_rec;
typedef struct request_rec  request_rec;
typedef struct conn_rec     conn_rec;
typedef struct server_rec   server_rec;


/* fake structure definitions */
/** A structure that represents one process */
struct process_rec {
    /** Global pool. Cleared upon normal exit */
    apr_pool_t *pool;
};

/** A structure that represents the current request */
struct request_rec {
    /** The pool associated with the request */
    apr_pool_t *pool;
};

/** Structure to store things which are per connection */
struct conn_rec {
    /** Pool associated with this connection */
    apr_pool_t *pool;
    /** Physical vhost this conn came in on */
    server_rec *base_server;
    /* Information about the connection itself */
    /** local address */
    apr_sockaddr_t *local_addr;
    /** remote address */
    apr_sockaddr_t *remote_addr;
    /** Client's IP address */
    char *remote_ip;
    /** Client's DNS name, if known.  NULL if DNS hasn't been checked,
     *  "" if it has and no address was found.  N.B. Only access this though
     * get_remote_host() */
    char *remote_host; 
    /** How many times have we used it? */
    int keepalives;
    /** server IP address */
    char *local_ip;
    /** used for ap_get_server_name when UseCanonicalName is set to DNS
     *  (ignores setting of HostnameLookups) */
    char *local_host;
     /** ID of this connection; unique at any point in time */
    long id;
    /** send note from one module to another, must remain valid for all
     *  requests on this conn */
    apr_table_t *notes; 
    /** handle to scoreboard information for this connection */
    void *sbh; 
    /** The bucket allocator to use for all bucket/brigade creations */
    struct apr_bucket_alloc_t *bucket_alloc;
};

/** A structure to store information for each virtual server */
struct server_rec {
    /** The process this server is running in */
    process_rec *process;
};

/* Apache logging support */
#define APLOG_EMERG 0   /* system is unusable */
#define APLOG_ALERT 1   /* action must be taken immediately */
#define APLOG_CRIT  2   /* critical conditions */
#define APLOG_ERR   3   /* error conditions */
#define APLOG_WARNING   4   /* warning conditions */
#define APLOG_NOTICE    5   /* normal but significant condition */
#define APLOG_INFO  6   /* informational */
#define APLOG_DEBUG 7   /* debug-level messages */

#define APLOG_LEVELMASK 7   /* mask off the level value */


/* APLOG_NOERRNO is ignored and should not be used.  It will be
 * removed in a future release of Apache.
 */
#define APLOG_NOERRNO       (APLOG_LEVELMASK + 1)

/* Use APLOG_TOCLIENT on ap_log_rerror() to give content
 * handlers the option of including the error text in the 
 * ErrorDocument sent back to the client. Setting APLOG_TOCLIENT
 * will cause the error text to be saved in the request_rec->notes 
 * table, keyed to the string "error-notes", if and only if:
 * - the severity level of the message is APLOG_WARNING or greater
 * - there are no other "error-notes" set in request_rec->notes
 * Once error-notes is set, it is up to the content handler to
 * determine whether this text should be sent back to the client.
 * Note: Client generated text streams sent back to the client MUST 
 * be escaped to prevent CSS attacks.
 */
#define APLOG_TOCLIENT          ((APLOG_LEVELMASK + 1) * 2)

/* normal but significant condition on startup, usually printed to stderr */
#define APLOG_STARTUP           ((APLOG_LEVELMASK + 1) * 4) 

#ifndef DEFAULT_LOGLEVEL
#define DEFAULT_LOGLEVEL    APLOG_WARNING
#endif

extern int AP_DECLARE_DATA ap_default_loglevel;

#define APLOG_MARK  __FILE__,__LINE__


/**
 * One of the primary logging routines in Apache.  This uses a printf-like
 * format to log messages to the error_log.
 * @param file The file in which this function is called
 * @param line The line number on which this function is called
 * @param level The level of this error message
 * @param status The status code from the previous command
 * @param s The server on which we are logging
 * @param fmt The format string
 * @param ... The arguments to use to fill out fmt.
 * @tip Use APLOG_MARK to fill out file and line
 * @warning It is VERY IMPORTANT that you not include any raw data from 
 * the network, such as the request-URI or request header fields, within 
 * the format string.  Doing so makes the server vulnerable to a 
 * denial-of-service attack and other messy behavior.  Instead, use a 
 * simple format string like "%s", followed by the string containing the 
 * untrusted data.
 * @deffunc void ap_log_error(const char *file, int line, int level, apr_status_t status, const server_rec *s, const char *fmt, ...) 
 */
AP_DECLARE(void) ap_log_error(const char *file, int line, int level, 
                             apr_status_t status, const server_rec *s, 
                             const char *fmt, ...)
                __attribute__((format(printf,6,7)));

/**
 * The second of the primary logging routines in Apache.  This uses 
 * a printf-like format to log messages to the error_log.
 * @param file The file in which this function is called
 * @param line The line number on which this function is called
 * @param level The level of this error message
 * @param status The status code from the previous command
 * @param p The pool which we are logging for
 * @param fmt The format string
 * @param ... The arguments to use to fill out fmt.
 * @tip Use APLOG_MARK to fill out file and line
 * @warning It is VERY IMPORTANT that you not include any raw data from 
 * the network, such as the request-URI or request header fields, within 
 * the format string.  Doing so makes the server vulnerable to a 
 * denial-of-service attack and other messy behavior.  Instead, use a 
 * simple format string like "%s", followed by the string containing the 
 * untrusted data.
 * @deffunc void ap_log_perror(const char *file, int line, int level, apr_status_t status, apr_pool_t *p, const char *fmt, ...) 
 */
AP_DECLARE(void) ap_log_perror(const char *file, int line, int level, 
                             apr_status_t status, apr_pool_t *p, 
                             const char *fmt, ...)
                __attribute__((format(printf,6,7)));

/**
 * The last of the primary logging routines in Apache.  This uses 
 * a printf-like format to log messages to the error_log.
 * @param file The file in which this function is called
 * @param line The line number on which this function is called
 * @param level The level of this error message
 * @param status The status code from the previous command
 * @param s The request which we are logging for
 * @param fmt The format string
 * @param ... The arguments to use to fill out fmt.
 * @tip Use APLOG_MARK to fill out file and line
 * @warning It is VERY IMPORTANT that you not include any raw data from 
 * the network, such as the request-URI or request header fields, within 
 * the format string.  Doing so makes the server vulnerable to a 
 * denial-of-service attack and other messy behavior.  Instead, use a 
 * simple format string like "%s", followed by the string containing the 
 * untrusted data.
 * @deffunc void ap_log_rerror(const char *file, int line, int level, apr_status_t status, request_rec *r, const char *fmt, ...) 
 */
AP_DECLARE(void) ap_log_rerror(const char *file, int line, int level, 
                               apr_status_t status, const request_rec *r, 
                               const char *fmt, ...)
                __attribute__((format(printf,6,7)));
/**
 * create_connection is a RUN_FIRST hook which allows modules to create 
 * connections. In general, you should not install filters with the 
 * create_connection hook. If you require vhost configuration information 
 * to make filter installation decisions, you must use the pre_connection
 * or install_network_transport hook. This hook should close the connection
 * if it encounters a fatal error condition.
 *
 * @param p The pool from which to allocate the connection record
 * @param csd The socket that has been accepted
 * @param conn_id A unique identifier for this connection.  The ID only
 *                needs to be unique at that time, not forever.
 * @param sbh A handle to scoreboard information for this connection.
 * @return An allocated connection record or NULL.
 */ 
AP_DECLARE(conn_rec *) ap_run_create_connection(apr_pool_t *ptrans,
                                  server_rec *server,
                                  apr_socket_t *csd, long id, void *sbh,
                                  apr_bucket_alloc_t *alloc);






#ifdef __cplusplus
}
#endif

#endif /* HTTPD_WRAP_H */
