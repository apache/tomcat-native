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
/**
 * @internal
 * modules should not used functions marked AP_CORE_DECLARE
 */
#ifndef AP_CORE_DECLARE
# define AP_CORE_DECLARE	AP_DECLARE
#endif 

/** The default string lengths */
#define MAX_STRING_LEN HUGE_STRING_LEN
#define HUGE_STRING_LEN 8192

/** The size of the server's internal read-write buffers */
#define AP_IOBUFSIZE 8192

#define DECLINED -1     /**< Module declines to handle */
#define DONE -2         /**< Module has served the response completely 
                 *  - it's safe to die() with no more output
                 */
#define OK 0            /**< Module has handled this stage. */


/**
 * @defgroup HTTP_Status HTTP Status Codes
 * @{
 */
/**
 * The size of the static array in http_protocol.c for storing
 * all of the potential response status-lines (a sparse table).
 * A future version should dynamically generate the apr_table_t at startup.
 */
#define RESPONSE_CODES 57

#define HTTP_CONTINUE                      100
#define HTTP_SWITCHING_PROTOCOLS           101
#define HTTP_PROCESSING                    102
#define HTTP_OK                            200
#define HTTP_CREATED                       201
#define HTTP_ACCEPTED                      202
#define HTTP_NON_AUTHORITATIVE             203
#define HTTP_NO_CONTENT                    204
#define HTTP_RESET_CONTENT                 205
#define HTTP_PARTIAL_CONTENT               206
#define HTTP_MULTI_STATUS                  207
#define HTTP_MULTIPLE_CHOICES              300
#define HTTP_MOVED_PERMANENTLY             301
#define HTTP_MOVED_TEMPORARILY             302
#define HTTP_SEE_OTHER                     303
#define HTTP_NOT_MODIFIED                  304
#define HTTP_USE_PROXY                     305
#define HTTP_TEMPORARY_REDIRECT            307
#define HTTP_BAD_REQUEST                   400
#define HTTP_UNAUTHORIZED                  401
#define HTTP_PAYMENT_REQUIRED              402
#define HTTP_FORBIDDEN                     403
#define HTTP_NOT_FOUND                     404
#define HTTP_METHOD_NOT_ALLOWED            405
#define HTTP_NOT_ACCEPTABLE                406
#define HTTP_PROXY_AUTHENTICATION_REQUIRED 407
#define HTTP_REQUEST_TIME_OUT              408
#define HTTP_CONFLICT                      409
#define HTTP_GONE                          410
#define HTTP_LENGTH_REQUIRED               411
#define HTTP_PRECONDITION_FAILED           412
#define HTTP_REQUEST_ENTITY_TOO_LARGE      413
#define HTTP_REQUEST_URI_TOO_LARGE         414
#define HTTP_UNSUPPORTED_MEDIA_TYPE        415
#define HTTP_RANGE_NOT_SATISFIABLE         416
#define HTTP_EXPECTATION_FAILED            417
#define HTTP_UNPROCESSABLE_ENTITY          422
#define HTTP_LOCKED                        423
#define HTTP_FAILED_DEPENDENCY             424
#define HTTP_UPGRADE_REQUIRED              426
#define HTTP_INTERNAL_SERVER_ERROR         500
#define HTTP_NOT_IMPLEMENTED               501
#define HTTP_BAD_GATEWAY                   502
#define HTTP_SERVICE_UNAVAILABLE           503
#define HTTP_GATEWAY_TIME_OUT              504
#define HTTP_VERSION_NOT_SUPPORTED         505
#define HTTP_VARIANT_ALSO_VARIES           506
#define HTTP_INSUFFICIENT_STORAGE          507
#define HTTP_NOT_EXTENDED                  510

/** is the status code informational */
#define ap_is_HTTP_INFO(x)         (((x) >= 100)&&((x) < 200))
/** is the status code OK ?*/
#define ap_is_HTTP_SUCCESS(x)      (((x) >= 200)&&((x) < 300))
/** is the status code a redirect */
#define ap_is_HTTP_REDIRECT(x)     (((x) >= 300)&&((x) < 400))
/** is the status code a error (client or server) */
#define ap_is_HTTP_ERROR(x)        (((x) >= 400)&&((x) < 600))
/** is the status code a client error  */
#define ap_is_HTTP_CLIENT_ERROR(x) (((x) >= 400)&&((x) < 500))
/** is the status code a server error  */
#define ap_is_HTTP_SERVER_ERROR(x) (((x) >= 500)&&((x) < 600))

/** should the status code drop the connection */
#define ap_status_drops_connection(x) \
                                   (((x) == HTTP_BAD_REQUEST)           || \
                                    ((x) == HTTP_REQUEST_TIME_OUT)      || \
                                    ((x) == HTTP_LENGTH_REQUIRED)       || \
                                    ((x) == HTTP_REQUEST_ENTITY_TOO_LARGE) || \
                                    ((x) == HTTP_REQUEST_URI_TOO_LARGE) || \
                                    ((x) == HTTP_INTERNAL_SERVER_ERROR) || \
                                    ((x) == HTTP_SERVICE_UNAVAILABLE) || \
                    ((x) == HTTP_NOT_IMPLEMENTED))
                    
/* Maximum number of dynamically loaded modules */
#ifndef DYNAMIC_MODULE_LIMIT
#define DYNAMIC_MODULE_LIMIT 64
#endif
/* Default administrator's address */
#define DEFAULT_ADMIN "[no address given]"
/* The timeout for waiting for messages */
#ifndef DEFAULT_TIMEOUT
#define DEFAULT_TIMEOUT 300 
#endif
/* The timeout for waiting for keepalive timeout until next request */
#ifndef DEFAULT_KEEPALIVE_TIMEOUT
#define DEFAULT_KEEPALIVE_TIMEOUT 15
#endif
/* The number of requests to entertain per connection */
#ifndef DEFAULT_KEEPALIVE
#define DEFAULT_KEEPALIVE 100
#endif
#ifndef DEFAULT_LIMIT_REQUEST_LINE
#define DEFAULT_LIMIT_REQUEST_LINE 8190
#endif /* default limit on bytes in Request-Line (Method+URI+HTTP-version) */
#ifndef DEFAULT_LIMIT_REQUEST_FIELDSIZE
#define DEFAULT_LIMIT_REQUEST_FIELDSIZE 8190
#endif /* default limit on bytes in any one header field  */
#ifndef DEFAULT_LIMIT_REQUEST_FIELDS
#define DEFAULT_LIMIT_REQUEST_FIELDS 100
#endif /* default limit on number of request header fields */
#ifndef DEFAULT_CONTENT_TYPE
#define DEFAULT_CONTENT_TYPE "text/plain"
#endif
/**
 * The address 255.255.255.255, when used as a virtualhost address,
 * will become the "default" server when the ip doesn't match other vhosts.
 */
#define DEFAULT_VHOST_ADDR 0xfffffffful
 
/* options for get_remote_host() */
/* REMOTE_HOST returns the hostname, or NULL if the hostname
 * lookup fails.  It will force a DNS lookup according to the
 * HostnameLookups setting.
 */
#define REMOTE_HOST (0)

/* REMOTE_NAME returns the hostname, or the dotted quad if the
 * hostname lookup fails.  It will force a DNS lookup according
 * to the HostnameLookups setting.
 */
#define REMOTE_NAME (1)

/* REMOTE_NOLOOKUP is like REMOTE_NAME except that a DNS lookup is
 * never forced.
 */
#define REMOTE_NOLOOKUP (2)

/* REMOTE_DOUBLE_REV will always force a DNS lookup, and also force
 * a double reverse lookup, regardless of the HostnameLookups
 * setting.  The result is the (double reverse checked) hostname,
 * or NULL if any of the lookups fail.
 */
#define REMOTE_DOUBLE_REV (3)
                     
/** @} */
/**
 * @defgroup Methods List of Methods recognized by the server
 * @{
 */
/**
 * Methods recognized (but not necessarily handled) by the server.
 * These constants are used in bit shifting masks of size int, so it is
 * unsafe to have more methods than bits in an int.  HEAD == M_GET.
 * This list must be tracked by the list in http_protocol.c in routine
 * ap_method_name_of().
 */
#define M_GET                   0       /* RFC 2616: HTTP */
#define M_PUT                   1       /*  :             */
#define M_POST                  2
#define M_DELETE                3
#define M_CONNECT               4
#define M_OPTIONS               5
#define M_TRACE                 6       /* RFC 2616: HTTP */
#define M_PATCH                 7       /* no rfc(!)  ### remove this one? */
#define M_PROPFIND              8       /* RFC 2518: WebDAV */
#define M_PROPPATCH             9       /*  :               */
#define M_MKCOL                 10
#define M_COPY                  11
#define M_MOVE                  12
#define M_LOCK                  13
#define M_UNLOCK                14      /* RFC 2518: WebDAV */
#define M_VERSION_CONTROL       15      /* RFC 3253: WebDAV Versioning */
#define M_CHECKOUT              16      /*  :                          */
#define M_UNCHECKOUT            17
#define M_CHECKIN               18
#define M_UPDATE                19
#define M_LABEL                 20
#define M_REPORT                21
#define M_MKWORKSPACE           22
#define M_MKACTIVITY            23
#define M_BASELINE_CONTROL      24
#define M_MERGE                 25
#define M_INVALID               26      /* RFC 3253: WebDAV Versioning */

/**
 * METHODS needs to be equal to the number of bits
 * we are using for limit masks.
 */
#define METHODS     64

/**
 * The method mask bit to shift for anding with a bitmask.
 */
#define AP_METHOD_BIT ((apr_int64_t)1)

/*
 * This is a convenience macro to ease with checking a mask
 * against a method name.
 */
#define AP_METHOD_CHECK_ALLOWED(mask, methname) \
    ((mask) & (AP_METHOD_BIT << ap_method_number_of((methname))))

/** @} */

/** default HTTP Server protocol */
#define AP_SERVER_PROTOCOL "HTTP/1.1"
/** Internal representation for a HTTP protocol number, e.g., HTTP/1.1 */

#define HTTP_VERSION(major,minor) (1000*(major)+(minor))
/** Major part of HTTP protocol */
#define HTTP_VERSION_MAJOR(number) ((number)/1000)
/** Minor part of HTTP protocol */
#define HTTP_VERSION_MINOR(number) ((number)%1000)

/**
 * @defgroup values_request_rec_body Possible values for request_rec.read_body 
 * @{
 * Possible values for request_rec.read_body (set by handling module):
 */

/** Send 413 error if message has any body */
#define REQUEST_NO_BODY          0
/** Send 411 error if body without Content-Length */
#define REQUEST_CHUNKED_ERROR    1
/** If chunked, remove the chunks for me. */
#define REQUEST_CHUNKED_DECHUNK  2
/** @} */
 

/* fake structure declarations */
typedef struct process_rec  process_rec;
typedef struct request_rec  request_rec;
typedef struct conn_rec     conn_rec;
typedef struct server_rec   server_rec;
typedef struct ap_conf_vector_t ap_conf_vector_t;
 

/* fake structure definitions */
/** A structure that represents one process */
struct process_rec {
    /** Global pool. Cleared upon normal exit */
    apr_pool_t *pool;
    /** Configuration pool. Cleared upon restart */
    apr_pool_t *pconf;
    /** Number of command line arguments passed to the program */
    int argc;
    /** The command line arguments */
    const char * const *argv;
    /** The program name used to execute the program */
    const char *short_name;
};

/** A structure that represents the current request */
struct request_rec {
    /** The pool associated with the request */
    apr_pool_t *pool;
    /** The connection to the client */
    conn_rec *connection;
    /** The virtual host for this request */
    server_rec *server;
    /** First line of request */
    char *the_request;
    /** HTTP/0.9, "simple" request (e.g. GET /foo\n w/no headers) */
    int assbackwards;
    /** A proxy request (calculated during post_read_request/translate_name)
     *  possible values PROXYREQ_NONE, PROXYREQ_PROXY, PROXYREQ_REVERSE,
     *                  PROXYREQ_RESPONSE
     */
    int proxyreq;
    /** HEAD request, as opposed to GET */
    int header_only;
    /** Protocol string, as given to us, or HTTP/0.9 */
    char *protocol;
    /** Protocol version number of protocol; 1.1 = 1001 */
    int proto_num;
    /** Host, as set by full URI or Host: */
    const char *hostname;

    /** Time when the request started */
    apr_time_t request_time;

    /** Status line, if set by script */
    const char *status_line;
    /** Status line */
    int status;

    /* Request method, two ways; also, protocol, etc..  Outside of protocol.c,
     * look, but don't touch.
     */

    /** Request method (eg. GET, HEAD, POST, etc.) */
    const char *method;
    /** M_GET, M_POST, etc. */
    int method_number;
 
    /** MIME header environment from the request */
    apr_table_t *headers_in;
    /** MIME header environment for the response */
    apr_table_t *headers_out;
    /** MIME header environment for the response, printed even on errors and
     * persist across internal redirects */
    apr_table_t *err_headers_out;
    /** Array of environment variables to be used for sub processes */
    apr_table_t *subprocess_env;
    /** Notes from one module to another */
    apr_table_t *notes;

    /* content_type, handler, content_encoding, and all content_languages 
     * MUST be lowercased strings.  They may be pointers to static strings;
     * they should not be modified in place.
     */
    /** The content-type for the current request */
    const char *content_type;   /* Break these out --- we dispatch on 'em */
    /** The handler string that we use to call a handler function */
    const char *handler;    /* What we *really* dispatch on */

    /** Remaining bytes left to read from the request body */
    apr_off_t remaining;
    /** Number of bytes that have been read  from the request body */
    apr_off_t read_length;

    /** How to encode the data */
    const char *content_encoding;
    /** Array of strings representing the content languages */
    apr_array_header_t *content_languages;
    /** If an authentication check was made, this gets set to the user name. */
    char *user; 
    /** If an authentication check was made, this gets set to the auth type. */
    char *ap_auth_type;
     /** The URI without any parsing performed */
    char *unparsed_uri; 
    /** The path portion of the URI */
    char *uri;
    /** The filename on disk corresponding to this response */
    char *filename;
    /* XXX: What does this mean? Please define "canonicalize" -aaron */
    /** The true filename, we canonicalize r->filename if these don't match */
    char *canonical_filename;
    /** The PATH_INFO extracted from this request */
    char *path_info;
    /** The QUERY_ARGS extracted from this request */
    char *args; 
    /** ST_MODE set to zero if no such file */
    apr_finfo_t finfo;
    /** A struct containing the components of URI */
    apr_uri_t parsed_uri;
    /** Options set in config files, etc. */
    struct ap_conf_vector_t *per_dir_config;
    /** Notes on *this* request */
    struct ap_conf_vector_t *request_config;         
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
    /** Notes on *this* connection */
    struct ap_conf_vector_t *conn_config;
    /** send note from one module to another, must remain valid for all
     *  requests on this conn */
    apr_table_t *notes; 
    /** handle to scoreboard information for this connection */
    void *sbh; 
    /** The bucket allocator to use for all bucket/brigade creations */
    struct apr_bucket_alloc_t *bucket_alloc;
    /** This doesn't exists in the original, but it is here to simulate the network */
    apr_bucket_brigade *bb;    
};

/** A structure to be used for Per-vhost config */
typedef struct server_addr_rec server_addr_rec;
struct server_addr_rec {
    /** The next server in the list */
    server_addr_rec *next;
    /** The bound address, for this server */
    apr_sockaddr_t *host_addr;
    /** The bound port, for this server */
    apr_port_t host_port;
    /** The name given in <VirtualHost> */
    char *virthost;
};

/** A structure to store information for each virtual server */
struct server_rec {
    /** The process this server is running in */
    process_rec *process;
    /* Contact information */
    /** The admin's contact information */
    char *server_admin;
     /** The server hostname */
    char *server_hostname;
    /** for redirects, etc. */
    apr_port_t port;
    /** The log level for this server */
    int loglevel;

    server_addr_rec *addrs; 
    /** Timeout, as an apr interval, before we give up */
    apr_interval_time_t timeout;
    /** The apr interval we will wait for another request */
    apr_interval_time_t keep_alive_timeout;
    /** Maximum requests per connection */
    int keep_alive_max;
    /** Use persistent connections? */
    int keep_alive;
 
    /** true if this is the virtual server */
    int is_virtual; 
    /** limit on size of the HTTP request line    */
    int limit_req_line;
    /** limit on size of any request header field */
    int limit_req_fieldsize;
    /** limit on number of request header fields  */
    int limit_req_fields;

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

/**
 * Get the current server name from the request
 * @param r The current request
 * @return the server name
 * @deffunc const char *ap_get_server_name(request_rec *r)
 */
AP_DECLARE(const char *) ap_get_server_name(request_rec *r);

/**
 * Lookup the remote client's DNS name or IP address
 * @param conn The current connection
 * @param dir_config The directory config vector from the request
 * @param type The type of lookup to perform.  One of:
 * <pre>
 *     REMOTE_HOST returns the hostname, or NULL if the hostname
 *                 lookup fails.  It will force a DNS lookup according to the
 *                 HostnameLookups setting.
 *     REMOTE_NAME returns the hostname, or the dotted quad if the
 *                 hostname lookup fails.  It will force a DNS lookup according
 *                 to the HostnameLookups setting.
 *     REMOTE_NOLOOKUP is like REMOTE_NAME except that a DNS lookup is
 *                     never forced.
 *     REMOTE_DOUBLE_REV will always force a DNS lookup, and also force
 *                   a double reverse lookup, regardless of the HostnameLookups
 *                   setting.  The result is the (double reverse checked) 
 *                   hostname, or NULL if any of the lookups fail.
 * </pre>
 * @param str_is_ip unless NULL is passed, this will be set to non-zero on output when an IP address 
 *        string is returned
 * @return The remote hostname
 * @deffunc const char *ap_get_remote_host(conn_rec *conn, void *dir_config, int type, int *str_is_ip)
 */
AP_DECLARE(const char *) ap_get_remote_host(conn_rec *conn, void *dir_config, int type, int *str_is_ip);

/* Reading a block of data from the client connection (e.g., POST arg) */

/**
 * Setup the client to allow Apache to read the request body.
 * @param r The current request
 * @param read_policy How the server should interpret a chunked 
 *                    transfer-encoding.  One of: <pre>
 *    REQUEST_NO_BODY          Send 413 error if message has any body
 *    REQUEST_CHUNKED_ERROR    Send 411 error if body without Content-Length
 *    REQUEST_CHUNKED_DECHUNK  If chunked, remove the chunks for me.
 * </pre>
 * @return either OK or an error code
 * @deffunc int ap_setup_client_block(request_rec *r, int read_policy)
 */
AP_DECLARE(int) ap_setup_client_block(request_rec *r, int read_policy);

/**
 * Determine if the client has sent any data.  This also sends a 
 * 100 Continue response to HTTP/1.1 clients, so modules should not be called
 * until the module is ready to read content.
 * @warning Never call this function more than once.
 * @param r The current request
 * @return 0 if there is no message to read, 1 otherwise
 * @deffunc int ap_should_client_block(request_rec *r)
 */
AP_DECLARE(int) ap_should_client_block(request_rec *r);

/**
 * Call this in a loop.  It will put data into a buffer and return the length
 * of the input block
 * @param r The current request
 * @param buffer The buffer in which to store the data
 * @param bufsiz The size of the buffer
 * @return Number of bytes inserted into the buffer.  When done reading, 0
 *         if EOF, or -1 if there was an error
 * @deffunc long ap_get_client_block(request_rec *r, char *buffer, apr_size_t bufsiz)
 */
AP_DECLARE(long) ap_get_client_block(request_rec *r, char *buffer, apr_size_t bufsiz);

/**
 * In HTTP/1.1, any method can have a body.  However, most GET handlers
 * wouldn't know what to do with a request body if they received one.
 * This helper routine tests for and reads any message body in the request,
 * simply discarding whatever it receives.  We need to do this because
 * failing to read the request body would cause it to be interpreted
 * as the next request on a persistent connection.
 * @param r The current request
 * @return error status if request is malformed, OK otherwise 
 * @deffunc int ap_discard_request_body(request_rec *r)
 */
AP_DECLARE(int) ap_discard_request_body(request_rec *r);

/**
 * Set the content type for this request (r->content_type). 
 * @param r The current request
 * @param ct The new content type
 * @deffunc void ap_set_content_type(request_rec *r, const char* ct)
 * @warning This function must be called to set r->content_type in order 
 * for the AddOutputFilterByType directive to work correctly.
 */
AP_DECLARE(void) ap_set_content_type(request_rec *r, const char *ct);

/**
 * Setup the config vector for a request_rec
 * @param p The pool to allocate the config vector from
 * @return The config vector
 */
AP_CORE_DECLARE(ap_conf_vector_t*) ap_create_request_config(apr_pool_t *p);

/* For http_connection.c... */
/**
 * Setup the config vector for a connection_rec
 * @param p The pool to allocate the config vector from
 * @return The config vector
 */
AP_CORE_DECLARE(ap_conf_vector_t*) ap_create_conn_config(apr_pool_t *p);

/**
 * Get the method number associated with the given string, assumed to
 * contain an HTTP method.  Returns M_INVALID if not recognized.
 * @param method A string containing a valid HTTP method
 * @return The method number
 */
AP_DECLARE(int) ap_method_number_of(const char *method);

/**
 * create the request_rec structure from fake client connection 
 */
AP_DECLARE(request_rec *) ap_wrap_create_request(conn_rec *conn);

/**
 * create the server_rec structure from process_rec. 
 */
AP_DECLARE(server_rec *) ap_wrap_create_server(process_rec *process, apr_pool_t *p);

/**
 * create the main process_rec. 
 */
AP_DECLARE(process_rec *) ap_wrap_create_process(int argc, const char * const *argv);

/**
 * Fill the request_rec with data. 
 * @param r      The current request
 * @param url    The full url http://[username:password@]hostname[:port]/uri
 * @param method Method "GET", "POST", etc...
 * @param content_type The post data content type
 * @param content_encoding The post data transfer encoding
 * @param content_length The post data content length
 * @param content The post content data.
 * @return APR_SUCCESS or error
 */
AP_DECLARE(apr_status_t) ap_wrap_make_request(request_rec *r, const char *url,
                                              const char *method,
                                              const char *content_type,
                                              const char *content_encoding,
                                              apr_size_t content_length, char *content);

#ifdef __cplusplus
}
#endif

#endif /* HTTPD_WRAP_H */
