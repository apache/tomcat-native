/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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
 * Description: ISAPI plugin for IIS/PWS                                   *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Larry Isaacs <larryi@apache.org>                           *
 * Author:      Ignacio J. Ortega <nacho@apache.org>                       *
 * Author:      Mladen Turk <mturk@apache.org>                             *
 * Version:     $Revision$                                        *
 ***************************************************************************/

// This define is needed to include wincrypt,h, needed to get client certificates
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#include <httpext.h>
#include <httpfilt.h>
#include <wininet.h>

#include "jk_global.h"
#include "jk_url.h"
#include "jk_util.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_uri_worker_map.h"
#include "jk_shm.h"
#include "jk_ajp13.h"
#include "pcre.h"

#ifndef POSIX_MALLOC_THRESHOLD
#define POSIX_MALLOC_THRESHOLD (10)
#endif

#include <strsafe.h>

#define VERSION_STRING "Jakarta/ISAPI/" JK_EXPOSED_VERSION
#define SHM_DEF_NAME   "JKISAPISHMEM"
#define DEFAULT_WORKER_NAME ("ajp13")

/*
 * This is default value found inside httpd.conf
 * for MaxClients
 */
#define DEFAULT_WORKER_THREADS  250

/*
 * We use special headers to pass values from the filter to the
 * extension. These values are:
 *
 * 1. The real URI before redirection took place
 * 2. The name of the worker to be used.
 * 3. The contents of the Translate header, if any
 *
 */
#define URI_HEADER_NAME_BASE              ("TOMCATURI")
#define QUERY_HEADER_NAME_BASE            ("TOMCATQUERY")
#define WORKER_HEADER_NAME_BASE           ("TOMCATWORKER")
#define TOMCAT_TRANSLATE_HEADER_NAME_BASE ("TOMCATTRANSLATE")
#define CONTENT_LENGTH                    ("CONTENT_LENGTH:")
/* The template used to construct our unique headers
 * from the base name and module instance
 */
#define HEADER_TEMPLATE      ("%s%p:")
#define HTTP_HEADER_TEMPLATE ("HTTP_%s%p")

static char URI_HEADER_NAME[MAX_PATH];
static char QUERY_HEADER_NAME[MAX_PATH];
static char WORKER_HEADER_NAME[MAX_PATH];
static char TOMCAT_TRANSLATE_HEADER_NAME[MAX_PATH];

static char HTTP_URI_HEADER_NAME[MAX_PATH];
static char HTTP_QUERY_HEADER_NAME[MAX_PATH];
static char HTTP_WORKER_HEADER_NAME[MAX_PATH];

#define REGISTRY_LOCATION       ("Software\\Apache Software Foundation\\Jakarta Isapi Redirector\\1.0")
#define W3SVC_REGISTRY_KEY      ("SYSTEM\\CurrentControlSet\\Services\\W3SVC\\Parameters")
#define EXTENSION_URI_TAG       ("extension_uri")

#define URI_SELECT_TAG              ("uri_select")
#define URI_SELECT_PARSED_VERB      ("parsed")
#define URI_SELECT_UNPARSED_VERB    ("unparsed")
#define URI_SELECT_ESCAPED_VERB     ("escaped")
#define URI_SELECT_PROXY_VERB       ("proxy")
#define URI_REWRITE_TAG             ("rewrite_rule_file")
#define SHM_SIZE_TAG                ("shm_size")
#define WORKER_MOUNT_RELOAD_TAG     ("worker_mount_reload")
#define STRIP_SESSION_TAG           ("strip_session")
#define AUTH_COMPLETE_TAG           ("auth_complete")
#define REJECT_UNSAFE_TAG           ("reject_unsafe")
#define WATCHDOG_INTERVAL_TAG       ("watchdog_interval")


#define TRANSLATE_HEADER            ("Translate:")
#define TRANSLATE_HEADER_NAME       ("Translate")
#define TRANSLATE_HEADER_NAME_LC    ("translate")

#define BAD_REQUEST     -1
#define BAD_PATH        -2
#define MAX_SERVERNAME  128
#define MAX_INSTANCEID  32
#define MAX_PACKET_SIZE 65536

char HTML_ERROR_400[] =         "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n"
                                "<HTML><HEAD><TITLE>Bad request!</TITLE></HEAD>\n"
                                "<BODY><H1>Bad request!</H1>\n<P>"
                                "Your browser (or proxy) sent a request that "
                                "this server could not understand.</P></BODY></HTML>";

char HTML_ERROR_404[] =         "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n"
                                "<HTML><HEAD><TITLE>Object not found!</TITLE></HEAD>\n"
                                "<BODY><H1>The requested URL was not found on this server"
                                "</H1>\n<P>If you entered the URL manually please check your"
                                "spelling and try again.</P></BODY></HTML>";

char HTML_ERROR_500[] =         "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n"
                                "<HTML><HEAD><TITLE>Server error!</TITLE></HEAD>\n"
                                "<BODY><H1>Internal server error!</H1>\n<P>"
                                "The server encountered an internal error and was "
                                "unable to complete your request.</P></BODY></HTML>";

char HTML_ERROR_503[] =         "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n"
                                "<HTML><HEAD><TITLE>Service unavailable!</TITLE></HEAD>\n"
                                "<BODY><H1>Service temporary unavailable!</H1>\n<P>"
                                "The server is temporarily unable to service your "
                                "request due to maintenance downtime or capacity problems. "
                                "Please try again later.</P></BODY></HTML>";


#define STRNULL_FOR_NULL(x) ((x) ? (x) : "(null)")
#define JK_TOLOWER(x)   ((char)tolower((BYTE)(x)))

#define GET_SERVER_VARIABLE_VALUE(name, place)          \
  do {                                                  \
    (place) = NULL;                                     \
    huge_buf_sz = MAX_PACKET_SIZE;                      \
    if (get_server_value(private_data->lpEcb,           \
                        (name),                         \
                        huge_buf,                       \
                        huge_buf_sz)) {                 \
        (place) = jk_pool_strdup(&private_data->p,      \
                                 huge_buf);             \
  } } while(0)

#define GET_SERVER_VARIABLE_VALUE_INT(name, place, def)     \
  do {                                                      \
    huge_buf_sz = MAX_PACKET_SIZE;                          \
    if (get_server_value(private_data->lpEcb,               \
                        (name),                             \
                        huge_buf,                           \
                        huge_buf_sz)) {                     \
        (place) = atoi(huge_buf);                           \
        if (0 == (place)) {                                 \
            (place) = def;                                  \
        }                                                   \
    } else {                                                \
        (place) = def;                                      \
  } } while(0)

static char ini_file_name[MAX_PATH];
static int using_ini_file = JK_FALSE;
static int is_inited = JK_FALSE;
static int is_mapread = JK_FALSE;

static jk_uri_worker_map_t *uw_map = NULL;
static jk_map_t *workers_map = NULL;
static jk_map_t *rewrite_map = NULL;
static jk_map_t *rregexp_map = NULL;

static jk_logger_t *logger = NULL;
static char *SERVER_NAME = "SERVER_NAME";
static char *SERVER_SOFTWARE = "SERVER_SOFTWARE";
static char *INSTANCE_ID = "INSTANCE_ID";
static char *CONTENT_TYPE = "Content-Type:text/html\r\n\r\n";

static char extension_uri[INTERNET_MAX_URL_LENGTH] =
    "/jakarta/isapi_redirect.dll";
static char log_file[MAX_PATH * 2];
static int log_level = JK_LOG_DEF_LEVEL;
static char worker_file[MAX_PATH * 2];
static char worker_mount_file[MAX_PATH * 2] = {0};
static int  worker_mount_reload = JK_URIMAP_DEF_RELOAD;
static char rewrite_rule_file[MAX_PATH * 2] = {0};
static size_t shm_config_size = 0;
static int strip_session = 0;
static DWORD auth_notification_flags = 0;
static int   use_auth_notification_flags = 1;
static int reject_unsafe = 0;
static int watchdog_interval = 0;
static HANDLE watchdog_handle = NULL;

#define URI_SELECT_OPT_PARSED       0
#define URI_SELECT_OPT_UNPARSED     1
#define URI_SELECT_OPT_ESCAPED      2
#define URI_SELECT_OPT_PROXY        3

static int uri_select_option = URI_SELECT_OPT_PROXY;

static jk_worker_env_t worker_env;

typedef struct isapi_private_data_t isapi_private_data_t;
struct isapi_private_data_t
{
    jk_pool_t p;

    unsigned int bytes_read_so_far;
    LPEXTENSION_CONTROL_BLOCK lpEcb;
};

typedef struct isapi_log_data_t isapi_log_data_t;
struct isapi_log_data_t {
    char uri[INTERNET_MAX_URL_LENGTH];
    char query[INTERNET_MAX_URL_LENGTH];
};

static int JK_METHOD start_response(jk_ws_service_t *s,
                                    int status,
                                    const char *reason,
                                    const char *const *header_names,
                                    const char *const *header_values,
                                    unsigned int num_of_headers);

static int JK_METHOD read(jk_ws_service_t *s,
                          void *b, unsigned int l, unsigned int *a);

static int JK_METHOD write(jk_ws_service_t *s, const void *b, unsigned int l);

static int init_ws_service(isapi_private_data_t * private_data,
                           jk_ws_service_t *s, char **worker_name);

static int init_jk(char *serverName);

static int initialize_extension(void);

static int read_registry_init_data(void);

static int get_config_parameter(LPVOID src, const char *tag,
                                char *val, DWORD sz);

static int get_config_bool(LPVOID src, const char *tag, int def);

static int get_config_int(LPVOID src, const char *tag, int def);

static int get_registry_config_parameter(HKEY hkey,
                                         const char *tag, char *b, DWORD sz);

static int get_registry_config_number(HKEY hkey, const char *tag,
                                         int *val);


static int get_server_value(LPEXTENSION_CONTROL_BLOCK lpEcb,
                            char *name,
                            char *buf, DWORD bufsz);

static int base64_encode_cert_len(int len);

static int base64_encode_cert(char *encoded,
                              const char *string, int len);

static int get_auth_flags();

static char x2c(const char *what)
{
    register char digit;

    digit =
        ((what[0] >= 'A') ? ((what[0] & 0xdf) - 'A') + 10 : (what[0] - '0'));
    digit *= 16;
    digit +=
        (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A') + 10 : (what[1] - '0'));
    return (digit);
}

static int unescape_url(char *url)
{
    register int x, y, badesc, badpath;

    badesc = 0;
    badpath = 0;
    for (x = 0, y = 0; url[y]; ++x, ++y) {
        if (url[y] != '%')
            url[x] = url[y];
        else {
            if (!isxdigit(url[y + 1]) || !isxdigit(url[y + 2])) {
                badesc = 1;
                url[x] = '%';
            }
            else {
                url[x] = x2c(&url[y + 1]);
                y += 2;
                if (url[x] == '/' || url[x] == '\0')
                    badpath = 1;
            }
        }
    }
    url[x] = '\0';
    if (badesc)
        return BAD_REQUEST;
    else if (badpath)
        return BAD_PATH;
    else
        return 0;
}

static void getparents(char *name)
{
    int l, w;

    /* Four paseses, as per RFC 1808 */
    /* a) remove ./ path segments */

    for (l = 0, w = 0; name[l] != '\0';) {
        if (name[l] == '.' && name[l + 1] == '/'
            && (l == 0 || name[l - 1] == '/'))
            l += 2;
        else
            name[w++] = name[l++];
    }

    /* b) remove trailing . path, segment */
    if (w == 1 && name[0] == '.')
        w--;
    else if (w > 1 && name[w - 1] == '.' && name[w - 2] == '/')
        w--;
    name[w] = '\0';

    /* c) remove all xx/../ segments. (including leading ../ and /../) */
    l = 0;

    while (name[l] != '\0') {
        if (name[l] == '.' && name[l + 1] == '.' && name[l + 2] == '/' &&
            (l == 0 || name[l - 1] == '/')) {
            register int m = l + 3, n;

            l = l - 2;
            if (l >= 0) {
                while (l >= 0 && name[l] != '/')
                    l--;
                l++;
            }
            else
                l = 0;
            n = l;
            while ((name[n] = name[m]) != '\0') {
                n++;
                m++;
            }
        }
        else
            ++l;
    }

    /* d) remove trailing xx/.. segment. */
    if (l == 2 && name[0] == '.' && name[1] == '.')
        name[0] = '\0';
    else if (l > 2 && name[l - 1] == '.' && name[l - 2] == '.'
             && name[l - 3] == '/') {
        l = l - 4;
        if (l >= 0) {
            while (l >= 0 && name[l] != '/')
                l--;
            l++;
        }
        else
            l = 0;
        name[l] = '\0';
    }
}

/* Apache code to escape a URL */

#define T_OS_ESCAPE_PATH    (4)

static const BYTE test_char_table[256] = {
     0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 15, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    14,  0,  7,  6,  1,  6,  1,  1,  9,  9,  1,  0,  8,  0,  0, 10,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  8, 15, 15,  8, 15, 15,
     8,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 15, 15, 15,  7,  0,
     7,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 15,  7, 15,  1, 14,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6
};

#define TEST_CHAR(c, f) (test_char_table[(unsigned int)(c)] & (f))

static const char c2x_table[] = "0123456789abcdef";

static BYTE *c2x(unsigned int what, BYTE *where)
{
    *where++ = '%';
    *where++ = c2x_table[what >> 4];
    *where++ = c2x_table[what & 0xf];
    return where;
}

static char *status_reason(int status)
{
    static struct reasons {
        int status;
        char *reason;
    } *r, reasons[] = {
        { 100, "Continue" },
        { 101, "Switching Protocols" },
        { 200, "OK" },
        { 201, "Created" },
        { 202, "Accepted" },
        { 203, "Non-Authoritative Information" },
        { 204, "No Content" },
        { 205, "Reset Content" },
        { 206, "Partial Content" },
        { 300, "Multiple Choices" },
        { 301, "Moved Permanently" },
        { 302, "Moved Temporarily" },
        { 303, "See Other" },
        { 304, "Not Modified" },
        { 305, "Use Proxy" },
        { 400, "Bad Request" },
        { 401, "Unauthorized" },
        { 402, "Payment Required" },
        { 403, "Forbidden" },
        { 404, "Not Found" },
        { 405, "Method Not Allowed" },
        { 406, "Not Acceptable" },
        { 407, "Proxy Authentication Required" },
        { 408, "Request Timeout" },
        { 409, "Conflict" },
        { 410, "Gone" },
        { 411, "Length Required" },
        { 412, "Precondition Failed" },
        { 413, "Request Entity Too Large" },
        { 414, "Request-URI Too Long" },
        { 415, "Unsupported Media Type" },
        { 500, "Internal Server Error" },
        { 501, "Not Implemented" },
        { 502, "Bad Gateway" },
        { 503, "Service Unavailable" },
        { 504, "Gateway Timeout" },
        { 505, "HTTP Version Not Supported" },
        { 000, NULL}
    };

    r = reasons;
    while (r->status <= status)
        if (r->status == status)
            return r->reason;
        else
            r++;
    return "No Reason";
}

static int escape_url(const char *path, char *dest, int destsize)
{
    const BYTE *s = (const BYTE *)path;
    BYTE *d = (BYTE *)dest;
    BYTE *e = d + destsize - 1;
    BYTE *ee = d + destsize - 3;

    while (*s) {
        if (TEST_CHAR(*s, T_OS_ESCAPE_PATH)) {
            if (d >= ee)
                return JK_FALSE;
            d = c2x(*s, d);
        }
        else {
            if (d >= e)
                return JK_FALSE;
            *d++ = *s;
        }
        ++s;
    }
    *d = '\0';
    return JK_TRUE;
}

/*
 * Find the first occurrence of find in s.
 */
static char *stristr(const char *s, const char *find)
{
    char c, sc;
    size_t len;

    if ((c = tolower((unsigned char)(*find++))) != 0) {
        len = strlen(find);
        do {
            do {
                if ((sc = tolower((unsigned char)(*s++))) == 0)
                    return (NULL);
            } while (sc != c);
        } while (strnicmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}

static int uri_is_web_inf(const char *uri)
{
    if (stristr(uri, "/web-inf")) {
        return JK_TRUE;
    }
    if (stristr(uri, "/meta-inf")) {
        return JK_TRUE;
    }

    return JK_FALSE;
}

static void write_error_response(PHTTP_FILTER_CONTEXT pfc, char *status,
                                 char *msg)
{
    DWORD len = (DWORD)strlen(msg);

    /* reject !!! */
    pfc->AddResponseHeaders(pfc, CONTENT_TYPE, 0);
    pfc->ServerSupportFunction(pfc,
                               SF_REQ_SEND_RESPONSE_HEADER,
                               status, 0, 0);
    pfc->WriteClient(pfc, msg, &len, 0);
}

static void write_error_message(LPEXTENSION_CONTROL_BLOCK lpEcb, int err)
{
    DWORD len;
    if (err = 500) {
        lpEcb->ServerSupportFunction(lpEcb->ConnID,
                                     HSE_REQ_SEND_RESPONSE_HEADER,
                                     "500 Internal Server Error",
                                     0,
                                     (LPDWORD)CONTENT_TYPE);
        len = (DWORD)(sizeof(HTML_ERROR_500) - 1);
        lpEcb->WriteClient(lpEcb->ConnID,
                           HTML_ERROR_500, &len, 0);
    }
    else if (err == 503) {
        lpEcb->ServerSupportFunction(lpEcb->ConnID,
                                     HSE_REQ_SEND_RESPONSE_HEADER,
                                     "503 Service Unavailable",
                                     0,
                                     (LPDWORD)CONTENT_TYPE);
        len = (DWORD)(sizeof(HTML_ERROR_503) - 1);
        lpEcb->WriteClient(lpEcb->ConnID,
                           HTML_ERROR_503, &len, 0);
    }
    else {
        return;
    }
}


static int JK_METHOD start_response(jk_ws_service_t *s,
                                    int status,
                                    const char *reason,
                                    const char *const *header_names,
                                    const char *const *header_values,
                                    unsigned int num_of_headers)
{
    static char crlf[3] = { (char)13, (char)10, '\0' };

    JK_TRACE_ENTER(logger);
    if (status < 100 || status > 1000) {
        jk_log(logger, JK_LOG_ERROR,
               "invalid status %d",
               status);
        JK_TRACE_EXIT(logger);
        return JK_FALSE;
    }

    if (s && s->ws_private) {
        int rv = JK_TRUE;
        isapi_private_data_t *p = s->ws_private;
        if (!s->response_started) {
            char *status_str;
            DWORD status_str_len;
            char *headers_str = NULL;
            BOOL keep_alive = FALSE;
            s->response_started = JK_TRUE;

            /*
             * Create the status line
             */
            if (!reason) {
                reason = status_reason(status);
            }
            status_str = (char *)malloc((6 + strlen(reason)));
            StringCbPrintf(status_str, 6 + strlen(reason), "%d %s", status, reason);
            status_str_len = (DWORD)strlen(status_str);

            /*
             * Create response headers string
             */
            if (num_of_headers) {
                size_t i, len_of_headers = 0;
                for (i = 0, len_of_headers = 0; i < num_of_headers; i++) {
                    len_of_headers += strlen(header_names[i]);
                    len_of_headers += strlen(header_values[i]);
                    len_of_headers += 4;   /* extra for colon, space and crlf */
                }

                len_of_headers += 3;       /* crlf and terminating null char */
                headers_str = (char *)malloc(len_of_headers);
                headers_str[0] = '\0';

                for (i = 0; i < num_of_headers; i++) {
                    StringCbCat(headers_str, len_of_headers, header_names[i]);
                    StringCbCat(headers_str, len_of_headers, ": ");
                    StringCbCat(headers_str, len_of_headers, header_values[i]);
                    StringCbCat(headers_str, len_of_headers, crlf);
                }
                StringCbCat(headers_str, len_of_headers, crlf);
            }
            else {
                headers_str = crlf;
            }

            if (!p->lpEcb->ServerSupportFunction(p->lpEcb->ConnID,
                                                 HSE_REQ_SEND_RESPONSE_HEADER,
                                                 status_str,
                                                 &status_str_len,
                                                 (LPDWORD)headers_str)) {

                jk_log(logger, JK_LOG_ERROR,
                       "HSE_REQ_SEND_RESPONSE_HEADER failed with error=%08x",
                       GetLastError());
                rv = JK_FALSE;
            }
            if (headers_str)
                free(headers_str);
            if (status_str)
                free(status_str);
        }
        JK_TRACE_EXIT(logger);
        return rv;
    }

    JK_LOG_NULL_PARAMS(logger);
    JK_TRACE_EXIT(logger);
    return JK_FALSE;
}

static int JK_METHOD read(jk_ws_service_t *s,
                          void *b, unsigned int l, unsigned int *a)
{
    JK_TRACE_ENTER(logger);

    if (s && s->ws_private && b && a) {
        isapi_private_data_t *p = s->ws_private;

        if (JK_IS_DEBUG_LEVEL(logger)) {
            jk_log(logger, JK_LOG_DEBUG,
                   "Preparing to read %d bytes. "
                   "ECB reports %d bytes total, with %d available.",
                   l, p->lpEcb->cbTotalBytes, p->lpEcb->cbAvailable);
        }

        *a = 0;
        if (l) {
            char *buf = b;
            DWORD already_read = p->lpEcb->cbAvailable - p->bytes_read_so_far;

            if (already_read >= l) {
                if (JK_IS_DEBUG_LEVEL(logger)) {
                    jk_log(logger, JK_LOG_DEBUG,
                           "Already read %d bytes - supplying %d bytes from buffer",
                           already_read, l);
                }
                memcpy(buf, p->lpEcb->lpbData + p->bytes_read_so_far, l);
                p->bytes_read_so_far += l;
                *a = l;
            }
            else {
                /*
                 * Try to copy what we already have
                 */
                if (already_read > 0) {
                    if (JK_IS_DEBUG_LEVEL(logger)) {
                        jk_log(logger, JK_LOG_DEBUG,
                               "Supplying %d bytes from buffer",
                               already_read);
                    }
                    memcpy(buf, p->lpEcb->lpbData + p->bytes_read_so_far,
                           already_read);
                    buf += already_read;
                    l -= already_read;
                    p->bytes_read_so_far = p->lpEcb->cbAvailable;

                    *a = already_read;
                }

                /*
                 * Now try to read from the client ...
                 */
                if (JK_IS_DEBUG_LEVEL(logger)) {
                    jk_log(logger, JK_LOG_DEBUG,
                           "Attempting to read %d bytes from client", l);
                }
                if (p->lpEcb->ReadClient(p->lpEcb->ConnID, buf, (LPDWORD)&l)) {
                    *a += l;
                }
                else {
                    jk_log(logger, JK_LOG_ERROR,
                           "ReadClient failed with %08x", GetLastError());
                    JK_TRACE_EXIT(logger);
                    return JK_FALSE;
                }
            }
        }
        JK_TRACE_EXIT(logger);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(logger);
    JK_TRACE_EXIT(logger);
    return JK_FALSE;
}

static int JK_METHOD write(jk_ws_service_t *s, const void *b, unsigned int l)
{
    JK_TRACE_ENTER(logger);

    if (s && s->ws_private && b) {
        isapi_private_data_t *p = s->ws_private;

        if (l) {
            unsigned int written = 0;
            char *buf = (char *)b;

            if (!s->response_started) {
                start_response(s, 200, NULL, NULL, NULL, 0);
            }

            while (written < l) {
                DWORD try_to_write = l - written;
                if (!p->lpEcb->WriteClient(p->lpEcb->ConnID,
                                           buf + written, &try_to_write, 0)) {
                    jk_log(logger, JK_LOG_ERROR,
                           "WriteClient failed with %08x", GetLastError());
                    JK_TRACE_EXIT(logger);
                    return JK_FALSE;
                }
                written += try_to_write;
            }
        }

        JK_TRACE_EXIT(logger);
        return JK_TRUE;

    }

    JK_LOG_NULL_PARAMS(logger);
    JK_TRACE_EXIT(logger);
    return JK_FALSE;
}

BOOL WINAPI GetFilterVersion(PHTTP_FILTER_VERSION pVer)
{
    BOOL rv = TRUE;
    ULONG http_filter_revision = HTTP_FILTER_REVISION;

    pVer->dwFilterVersion = pVer->dwServerFilterVersion;

    if (pVer->dwFilterVersion > http_filter_revision) {
        pVer->dwFilterVersion = http_filter_revision;
    }
    if (!is_inited) {
        rv = initialize_extension();
    }
    if (auth_notification_flags == SF_NOTIFY_AUTH_COMPLETE) {
        pVer->dwFlags = SF_NOTIFY_ORDER_HIGH |
                        SF_NOTIFY_SECURE_PORT |
                        SF_NOTIFY_NONSECURE_PORT |
                        SF_NOTIFY_PREPROC_HEADERS |
                        SF_NOTIFY_LOG |
                        SF_NOTIFY_AUTH_COMPLETE;
    }
    else {
        pVer->dwFlags = SF_NOTIFY_ORDER_HIGH |
                        SF_NOTIFY_SECURE_PORT |
                        SF_NOTIFY_NONSECURE_PORT |
                        SF_NOTIFY_PREPROC_HEADERS;
    }

    StringCbCopy(pVer->lpszFilterDesc, SF_MAX_FILTER_DESC_LEN, VERSION_STRING);
    return rv;
}


#define AP_REG_ICASE    0x01 /** use a case-insensitive match */
#define AP_REG_NEWLINE  0x02 /** don't match newlines against '.' etc */
#define AP_REG_NOTBOL   0x04 /** ^ will not match against start-of-string */
#define AP_REG_NOTEOL   0x08 /** $ will not match against end-of-string */

#define AP_REG_EXTENDED (0)  /** unused */
#define AP_REG_NOSUB    (0)  /** unused */
/** The max number of regex captures that can be expanded by ap_pregsub */
#define AP_MAX_REG_MATCH 10

/* Error values: */
enum {
    AP_REG_ASSERT = 1,  /** internal error ? */
    AP_REG_ESPACE,      /** failed to get memory */
    AP_REG_INVARG,      /** invalid argument */
    AP_REG_NOMATCH      /** match failed */
};

/* The structure representing a compiled regular expression. */
typedef struct {
    void *re_pcre;
    size_t re_nsub;
    size_t re_erroffset;
    const char *real;
    const char *fake;
} ap_regex_t;

/* The structure in which a captured offset is returned. */
typedef struct {
    int rm_so;
    int rm_eo;
} ap_regmatch_t;


/* Table of error strings corresponding to POSIX error codes; must be
 * kept in synch with include/ap_regex.h's AP_REG_E* definitions. */

static const char *const pstring[] = {
  "",                                /* Dummy for value 0 */
  "internal error",                  /* AP_REG_ASSERT */
  "failed to get memory",            /* AP_REG_ESPACE */
  "bad argument",                    /* AP_REG_INVARG */
  "match failed"                     /* AP_REG_NOMATCH */
};

static size_t ap_regerror(int errcode, const ap_regex_t *preg,
                          char *errbuf, size_t errbuf_size)
{
    const char *message, *addmessage;
    size_t length, addlength;

    message = (errcode >= (int)(sizeof(pstring)/sizeof(char *))) ?
                                "unknown error code" : pstring[errcode];
    length = strlen(message) + 1;

    addmessage = " at offset ";
    addlength = (preg != NULL && (int)preg->re_erroffset != -1)?
                                        strlen(addmessage) + 6 : 0;

    if (errbuf_size > 0) {
        if (addlength > 0 && errbuf_size >= length + addlength)
            StringCbPrintf(errbuf, sizeof(errbuf), "%s%s%-6d",
                          message, addmessage,
                          (int)preg->re_erroffset);
        else {
            strncpy(errbuf, message, errbuf_size - 1);
            errbuf[errbuf_size-1] = 0;
        }
    }

    return length + addlength;
}

/*************************************************
 *           Free store held by a regex          *
 *************************************************/

static void ap_regfree(ap_regex_t *preg)
{
    (pcre_free)(preg->re_pcre);
}




/*************************************************
 *            Compile a regular expression       *
 *************************************************/

/*
Arguments:
  preg        points to a structure for recording the compiled expression
  pattern     the pattern to compile
  cflags      compilation flags

Returns:      0 on success
              various non-zero codes on failure
*/

static int ap_regcomp(ap_regex_t *preg, const char *pattern, int cflags)
{
    const char *errorptr;
    int erroffset;
    int options = 0;

    if ((cflags & AP_REG_ICASE) != 0) options |= PCRE_CASELESS;
    if ((cflags & AP_REG_NEWLINE) != 0) options |= PCRE_MULTILINE;

    preg->re_pcre = pcre_compile(pattern, options, &errorptr, &erroffset, NULL);
    preg->re_erroffset = erroffset;

    if (preg->re_pcre == NULL) return AP_REG_INVARG;

    preg->re_nsub = pcre_info((const pcre *)preg->re_pcre, NULL, NULL);
    return 0;
}

/*************************************************
 *              Match a regular expression       *
 *************************************************/

/* Unfortunately, PCRE requires 3 ints of working space for each captured
substring, so we have to get and release working store instead of just using
the POSIX structures as was done in earlier releases when PCRE needed only 2
ints. However, if the number of possible capturing brackets is small, use a
block of store on the stack, to reduce the use of malloc/free. The threshold is
in a macro that can be changed at configure time. */

static int ap_regexec(const ap_regex_t *preg, const char *string,
                      int nmatch, ap_regmatch_t pmatch[],
                      int eflags)
{
    int rc;
    int options = 0;
    int *ovector = NULL;
    int small_ovector[POSIX_MALLOC_THRESHOLD * 3];
    int allocated_ovector = 0;

    if ((eflags & AP_REG_NOTBOL) != 0) options |= PCRE_NOTBOL;
    if ((eflags & AP_REG_NOTEOL) != 0) options |= PCRE_NOTEOL;

    ((ap_regex_t *)preg)->re_erroffset = (size_t)(-1);  /* Only has meaning after compile */

    if (nmatch > 0) {
        if (nmatch <= POSIX_MALLOC_THRESHOLD) {
            ovector = &(small_ovector[0]);
        }
        else {
            ovector = (int *)malloc(sizeof(int) * nmatch * 3);
            if (ovector == NULL)
                return AP_REG_ESPACE;
            allocated_ovector = 1;
        }
    }

    rc = pcre_exec((const pcre *)preg->re_pcre, NULL, string,
                   (int)strlen(string),
                    0, options, ovector, nmatch * 3);

    if (rc == 0)
        rc = nmatch;    /* All captured slots were filled in */
    if (rc >= 0) {
        int i;
        for (i = 0; i < rc; i++) {
            pmatch[i].rm_so = ovector[i*2];
            pmatch[i].rm_eo = ovector[i*2+1];
        }
        if (allocated_ovector)
            free(ovector);
        for (; i < nmatch; i++)
            pmatch[i].rm_so = pmatch[i].rm_eo = -1;
        return 0;
    }
    else {
        if (allocated_ovector)
            free(ovector);
        switch(rc) {
            case PCRE_ERROR_NOMATCH: return AP_REG_NOMATCH;
            case PCRE_ERROR_NULL: return AP_REG_INVARG;
            case PCRE_ERROR_BADOPTION: return AP_REG_INVARG;
            case PCRE_ERROR_BADMAGIC: return AP_REG_INVARG;
            case PCRE_ERROR_UNKNOWN_NODE: return AP_REG_ASSERT;
            case PCRE_ERROR_NOMEMORY: return AP_REG_ESPACE;
#ifdef PCRE_ERROR_MATCHLIMIT
            case PCRE_ERROR_MATCHLIMIT: return AP_REG_ESPACE;
#endif
#ifdef PCRE_ERROR_BADUTF8
            case PCRE_ERROR_BADUTF8: return AP_REG_INVARG;
#endif
#ifdef PCRE_ERROR_BADUTF8_OFFSET
            case PCRE_ERROR_BADUTF8_OFFSET: return AP_REG_INVARG;
#endif
            default: return AP_REG_ASSERT;
        }
    }
}

/* This function substitutes for $0-$9, filling in regular expression
 * submatches. Pass it the same nmatch and pmatch arguments that you
 * passed ap_regexec(). pmatch should not be greater than the maximum number
 * of subexpressions - i.e. one more than the re_nsub member of ap_regex_t.
 *
 * input should be the string with the $-expressions, source should be the
 * string that was matched against.
 *
 * It returns the substituted string, or NULL on error.
 *
 * Parts of this code are based on Henry Spencer's regsub(), from his
 * AT&T V8 regexp package.
 */

static char *ap_pregsub(const char *input,
                        const char *source, size_t nmatch,
                        ap_regmatch_t pmatch[])
{
    const char *src = input;
    char *dest, *dst;
    char c;
    size_t no;
    int len;

    if (!source)
        return NULL;
    if (!nmatch)
        return strdup(src);

    /* First pass, find the size */
    len = 0;

    while ((c = *src++) != '\0') {
        if (c == '&')
            no = 0;
        else if (c == '$' && isdigit((unsigned char)*src))
            no = *src++ - '0';
        else
            no = 10;

        if (no > 9) {                /* Ordinary character. */
            if (c == '\\' && (*src == '$' || *src == '&'))
                c = *src++;
            len++;
        }
        else if (no < nmatch && pmatch[no].rm_so < pmatch[no].rm_eo) {
            len += pmatch[no].rm_eo - pmatch[no].rm_so;
        }

    }

    dest = dst = calloc(1, len + 1);

    /* Now actually fill in the string */

    src = input;

    while ((c = *src++) != '\0') {
        if (c == '&')
            no = 0;
        else if (c == '$' && isdigit((unsigned char)*src))
            no = *src++ - '0';
        else
            no = 10;

        if (no > 9) {                /* Ordinary character. */
            if (c == '\\' && (*src == '$' || *src == '&'))
                c = *src++;
            *dst++ = c;
        }
        else if (no < nmatch && pmatch[no].rm_so < pmatch[no].rm_eo) {
            len = pmatch[no].rm_eo - pmatch[no].rm_so;
            memcpy(dst, source + pmatch[no].rm_so, len);
            dst += len;
        }

    }
    *dst = '\0';
    return dest;
}

static int simple_rewrite(char *uri)
{
    if (rewrite_map) {
        int i;
        char buf[INTERNET_MAX_URL_LENGTH];
        for (i = 0; i < jk_map_size(rewrite_map); i++) {
            const char *src = jk_map_name_at(rewrite_map, i);
            if (*src == '~')
                continue;   /* Skip regexp rewrites */
            if (strncmp(uri, src, strlen(src)) == 0) {
                StringCbCopy(buf, INTERNET_MAX_URL_LENGTH, jk_map_value_at(rewrite_map, i));
                StringCbCat(buf,  INTERNET_MAX_URL_LENGTH, uri + strlen(src));
                StringCbCopy(uri, INTERNET_MAX_URL_LENGTH, buf);
                return 1;
            }
        }
    }
    return 0;
}

static int rregex_rewrite(char *uri)
{
    ap_regmatch_t regm[AP_MAX_REG_MATCH];

    if (rregexp_map) {
        int i;
        for (i = 0; i < jk_map_size(rregexp_map); i++) {
            ap_regex_t *regexp = (ap_regex_t *)jk_map_value_at(rregexp_map, i);
            if (!ap_regexec(regexp, uri, AP_MAX_REG_MATCH, regm, 0)) {
                char *subs = ap_pregsub(regexp->fake, uri,
                                       AP_MAX_REG_MATCH, regm);
                if (subs) {
                    char buf[INTERNET_MAX_URL_LENGTH];
                    size_t diffsz = strlen(subs) - (regm[0].rm_eo - regm[0].rm_so);
                    memcpy(&buf[0], uri, regm[0].rm_so);
                    StringCbCopy(&buf[regm[0].rm_so], INTERNET_MAX_URL_LENGTH - regm[0].rm_so, subs);
                    StringCbCat(&buf[0], INTERNET_MAX_URL_LENGTH, uri + regm[0].rm_eo);
                    StringCbCopy(uri, INTERNET_MAX_URL_LENGTH, &buf[0]);
                    free(subs);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int init_error = 0;

DWORD WINAPI HttpFilterProc(PHTTP_FILTER_CONTEXT pfc,
                            DWORD dwNotificationType, LPVOID pvNotification)
{
    /* Initialise jk */
    if (is_inited && !is_mapread) {
        char serverName[MAX_SERVERNAME] = "";
        char instanceId[MAX_INSTANCEID] = "";
        DWORD dwLen = MAX_SERVERNAME - MAX_INSTANCEID - 1;

        if (pfc->GetServerVariable(pfc, SERVER_NAME, serverName, &dwLen)) {
            if (dwLen > 0) {
                serverName[dwLen - 1] = '\0';
                dwLen = MAX_INSTANCEID;
                if (pfc->GetServerVariable(pfc, INSTANCE_ID, instanceId, &dwLen)) {
                    if (dwLen > 0) {
                        instanceId[dwLen - 1] = '\0';
                        StringCbCat(serverName, MAX_SERVERNAME, "_");
                        StringCbCat(serverName, MAX_SERVERNAME, instanceId);
                    }
                }
            }
            if (init_jk(serverName))
                is_mapread = JK_TRUE;
        }
        /* If we can't read the map we become dormant */
        if (!is_mapread)
            is_inited = JK_FALSE;
    }
    if (!is_inited && !is_mapread) {
        /* In case the initialization failed
         * return error. This will make entire IIS
         * unusable like with Apache servers
         */
         SetLastError(ERROR_INVALID_FUNCTION);
         return SF_STATUS_REQ_ERROR;
    }
    if (auth_notification_flags == dwNotificationType) {
        char uri[INTERNET_MAX_URL_LENGTH];
        char snuri[INTERNET_MAX_URL_LENGTH] = "/";
        char Host[INTERNET_MAX_URL_LENGTH] = "";
        char Port[INTERNET_MAX_URL_LENGTH] = "";
        char Translate[INTERNET_MAX_URL_LENGTH];
        char squery[INTERNET_MAX_URL_LENGTH] = "";
        BOOL(WINAPI * GetHeader)
            (struct _HTTP_FILTER_CONTEXT * pfc, LPSTR lpszName,
             LPVOID lpvBuffer, LPDWORD lpdwSize);
        BOOL(WINAPI * SetHeader)
            (struct _HTTP_FILTER_CONTEXT * pfc, LPSTR lpszName,
             LPSTR lpszValue);
        BOOL(WINAPI * AddHeader)
            (struct _HTTP_FILTER_CONTEXT * pfc, LPSTR lpszName,
             LPSTR lpszValue);
        char *query;
        DWORD sz = sizeof(uri);
        DWORD szHost = sizeof(Host);
        DWORD szPort = sizeof(Port);
        DWORD szTranslate = sizeof(Translate);

        if (auth_notification_flags == SF_NOTIFY_AUTH_COMPLETE) {
            GetHeader =
                ((PHTTP_FILTER_AUTH_COMPLETE_INFO) pvNotification)->GetHeader;
            SetHeader =
                ((PHTTP_FILTER_AUTH_COMPLETE_INFO) pvNotification)->SetHeader;
            AddHeader =
                ((PHTTP_FILTER_AUTH_COMPLETE_INFO) pvNotification)->AddHeader;
        }
        else {
            GetHeader =
                ((PHTTP_FILTER_PREPROC_HEADERS) pvNotification)->GetHeader;
            SetHeader =
                ((PHTTP_FILTER_PREPROC_HEADERS) pvNotification)->SetHeader;
            AddHeader =
                ((PHTTP_FILTER_PREPROC_HEADERS) pvNotification)->AddHeader;
        }

        if (JK_IS_DEBUG_LEVEL(logger))
            jk_log(logger, JK_LOG_DEBUG, "Filter started");

        /*
         * Just in case somebody set these headers in the request!
         */
        SetHeader(pfc, URI_HEADER_NAME, NULL);
        SetHeader(pfc, QUERY_HEADER_NAME, NULL);
        SetHeader(pfc, WORKER_HEADER_NAME, NULL);
        SetHeader(pfc, TOMCAT_TRANSLATE_HEADER_NAME, NULL);

        if (!GetHeader(pfc, "url", (LPVOID) uri, (LPDWORD) & sz)) {
            jk_log(logger, JK_LOG_ERROR,
                   "error while getting the url");
            return SF_STATUS_REQ_ERROR;
        }

        if (strlen(uri)) {
            int rc;
            const char *worker = NULL;
            query = strchr(uri, '?');
            if (query) {
                *query++ = '\0';
                StringCbCopy(squery, INTERNET_MAX_URL_LENGTH, query);
            }

            rc = unescape_url(uri);
            if (rc == BAD_REQUEST) {
                jk_log(logger, JK_LOG_ERROR,
                       "[%s] contains one or more invalid escape sequences.",
                       uri);
                write_error_response(pfc, "400 Bad Request",
                                     HTML_ERROR_400);
                return SF_STATUS_REQ_FINISHED;
            }
            else if (rc == BAD_PATH) {
                jk_log(logger, JK_LOG_EMERG,
                       "[%s] contains forbidden escape sequences.",
                       uri);
                write_error_response(pfc, "404 Not Found",
                                     HTML_ERROR_404);
                return SF_STATUS_REQ_FINISHED;
            }
            getparents(uri);
            if (pfc->
                GetServerVariable(pfc, SERVER_NAME, (LPVOID) Host,
                                  (LPDWORD) & szHost)) {
                if (szHost > 0) {
                    Host[szHost - 1] = '\0';
                }
            }
            Port[0] = '\0';
            if (pfc->
                GetServerVariable(pfc, "SERVER_PORT", (LPVOID) Port,
                                  (LPDWORD) & szPort)) {
                if (szPort > 0) {
                    Port[szPort - 1] = '\0';
                }
            }
            szPort = atoi(Port);
            if (szPort != 80 && szPort != 443 && szHost > 0) {
                StringCbCat(Host, INTERNET_MAX_URL_LENGTH, ":");
                StringCbCat(Host, INTERNET_MAX_URL_LENGTH, Port);
            }
            if (szHost > 0) {
                StringCbCat(snuri, INTERNET_MAX_URL_LENGTH, Host);
                worker = map_uri_to_worker(uw_map, uri, snuri, logger);
            }
            else {
                worker = map_uri_to_worker(uw_map, uri, NULL, logger);
            }
            /*
             * Check if somebody is feading us with his own TOMCAT data headers.
             * We reject such postings !
             */
            if (worker) {
                char *forwardURI;

                if (JK_IS_DEBUG_LEVEL(logger))
                    jk_log(logger, JK_LOG_DEBUG,
                           "check if [%s] points to the web-inf directory",
                        uri);

                if (uri_is_web_inf(uri)) {
                    jk_log(logger, JK_LOG_EMERG,
                           "[%s] points to the web-inf or meta-inf directory. "
                           "Somebody tries to hack into the site!!!",
                           uri);

                    write_error_response(pfc, "404 Not Found",
                                         HTML_ERROR_404);
                    return SF_STATUS_REQ_FINISHED;
                }

                /* This is a servlet, should redirect ... */
                if (JK_IS_DEBUG_LEVEL(logger))
                    jk_log(logger, JK_LOG_DEBUG,
                        "[%s] is a servlet url - should redirect to %s",
                        uri, worker);

                /* get URI we should forward */
                if (uri_select_option == URI_SELECT_OPT_UNPARSED) {
                    /* get original unparsed URI */
                    GetHeader(pfc, "url", (LPVOID) uri, (LPDWORD) & sz);
                    /* restore terminator for uri portion */
                    if (query)
                        *(query - 1) = '\0';
                    if (JK_IS_DEBUG_LEVEL(logger))
                        jk_log(logger, JK_LOG_DEBUG,
                               "forwarding original URI [%s]",
                               uri);
                    forwardURI = uri;
                }
                else if (uri_select_option == URI_SELECT_OPT_ESCAPED) {
                    if (!escape_url(uri, snuri, INTERNET_MAX_URL_LENGTH)) {
                        jk_log(logger, JK_LOG_ERROR,
                               "[%s] re-encoding request exceeds maximum buffer size.",
                               uri);
                        write_error_response(pfc, "400 Bad Request",
                                             HTML_ERROR_400);
                        return SF_STATUS_REQ_FINISHED;
                    }
                    if (JK_IS_DEBUG_LEVEL(logger))
                        jk_log(logger, JK_LOG_DEBUG,
                               "fowarding escaped URI [%s]",
                               snuri);
                    forwardURI = snuri;
                }
                else if (uri_select_option == URI_SELECT_OPT_PROXY) {
                    if (!jk_canonenc(uri, snuri, INTERNET_MAX_URL_LENGTH)) {
                        jk_log(logger, JK_LOG_ERROR,
                               "[%s] re-encoding request exceeds maximum buffer size.",
                               uri);
                        write_error_response(pfc, "400 Bad Request",
                                             HTML_ERROR_400);
                        return SF_STATUS_REQ_FINISHED;
                    }
                    if (JK_IS_DEBUG_LEVEL(logger))
                        jk_log(logger, JK_LOG_DEBUG,
                               "fowarding escaped URI [%s]",
                               snuri);
                    forwardURI = snuri;
                }
                else {
                    forwardURI = uri;
                }
                /* Do a simple rewrite .
                 * Note that URI can be escaped, so thus the rule has
                 * to be in that case.
                 *
                 * TODO: Add more advanced regexp rewrite.
                 */
                if (JK_IS_DEBUG_LEVEL(logger)) {
                    char duri[INTERNET_MAX_URL_LENGTH];
                    StringCbCopy(duri, INTERNET_MAX_URL_LENGTH, forwardURI);
                    if (simple_rewrite(forwardURI)) {
                        jk_log(logger, JK_LOG_DEBUG,
                               "rewritten URI [%s]->[%s]",
                               duri, forwardURI);
                    }
                    else if (rregex_rewrite(forwardURI)) {
                        jk_log(logger, JK_LOG_DEBUG,
                               "rewritten URI [%s]->[%s]",
                               duri, forwardURI);
                    }
                }
                else {
                    if (!simple_rewrite(forwardURI))
                        rregex_rewrite(forwardURI);
                }

                if (!AddHeader(pfc, URI_HEADER_NAME, forwardURI) ||
                    ((strlen(squery) > 0)
                     ? !AddHeader(pfc, QUERY_HEADER_NAME, squery) : FALSE) ||
                    !AddHeader(pfc, WORKER_HEADER_NAME, (LPSTR)worker) ||
                    !SetHeader(pfc, "url", extension_uri)) {
                    jk_log(logger, JK_LOG_ERROR,
                           "error while adding request headers");
                    SetLastError(ERROR_INVALID_PARAMETER);
                    return SF_STATUS_REQ_ERROR;
                }

                /* Move Translate: header to a temporary header so
                 * that the extension proc will be called.
                 * This allows the servlet to handle 'Translate: f'.
                 */
                if (GetHeader
                    (pfc, TRANSLATE_HEADER, (LPVOID) Translate,
                     (LPDWORD) & szTranslate) && Translate != NULL
                    && szTranslate > 0) {
                    if (!AddHeader
                        (pfc, TOMCAT_TRANSLATE_HEADER_NAME, Translate)) {
                        jk_log(logger, JK_LOG_ERROR,
                               "error while adding Tomcat-Translate headers");
                        return SF_STATUS_REQ_ERROR;
                    }
                    SetHeader(pfc, "Translate:", NULL);
                }
                if (!pfc->pFilterContext) {
                    isapi_log_data_t *ld = (isapi_log_data_t *)pfc->AllocMem(pfc, sizeof(isapi_log_data_t), 0);
                    if (!ld) {
                        jk_log(logger, JK_LOG_ERROR,
                               "error while allocating memory");
                        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                        return SF_STATUS_REQ_ERROR;
                    }
                    memset(ld, 0, sizeof(isapi_log_data_t));
                    StringCbCopy(ld->uri, INTERNET_MAX_URL_LENGTH, forwardURI);
                    StringCbCopy(ld->query, INTERNET_MAX_URL_LENGTH, squery);
                    pfc->pFilterContext = ld;
                } else {
                    isapi_log_data_t *ld = (isapi_log_data_t *)pfc->pFilterContext;
                    memset(ld, 0, sizeof(isapi_log_data_t));
                    StringCbCopy(ld->uri, INTERNET_MAX_URL_LENGTH, forwardURI);
                    StringCbCopy(ld->query, INTERNET_MAX_URL_LENGTH, squery);
                }
            }
            else {
                if (JK_IS_DEBUG_LEVEL(logger))
                    jk_log(logger, JK_LOG_DEBUG,
                           "[%s] is not a servlet url", uri);
                if (strip_session) {
                    char *jsessionid = strstr(uri, JK_PATH_SESSION_IDENTIFIER);
                    if (jsessionid) {
                        if (JK_IS_DEBUG_LEVEL(logger))
                            jk_log(logger, JK_LOG_DEBUG,
                                   "removing session identifier [%s] for non servlet url [%s]",
                                   jsessionid, uri);
                        *jsessionid = '\0';
                        SetHeader(pfc, "url", uri);
                    }
                }
            }
        }
    }
    else if (is_inited && (dwNotificationType == SF_NOTIFY_LOG)) {
        if (pfc->pFilterContext) {
            isapi_log_data_t *ld = (isapi_log_data_t *)pfc->pFilterContext;
            HTTP_FILTER_LOG  *pl = (HTTP_FILTER_LOG *)pvNotification;
            pl->pszTarget = ld->uri;
            pl->pszParameters = ld->query;
        }
    }
    return SF_STATUS_REQ_NEXT_NOTIFICATION;
}


BOOL WINAPI GetExtensionVersion(HSE_VERSION_INFO * pVer)
{
    pVer->dwExtensionVersion = MAKELONG(HSE_VERSION_MINOR, HSE_VERSION_MAJOR);

    StringCbCopy(pVer->lpszExtensionDesc, HSE_MAX_EXT_DLL_NAME_LEN, VERSION_STRING);


    if (!is_inited) {
        return initialize_extension();
    }

    return TRUE;
}

DWORD WINAPI HttpExtensionProc(LPEXTENSION_CONTROL_BLOCK lpEcb)
{
    DWORD rc = HSE_STATUS_ERROR;

    lpEcb->dwHttpStatusCode = HTTP_STATUS_SERVER_ERROR;

    JK_TRACE_ENTER(logger);

    /* Initialise jk */
    if (is_inited && !is_mapread) {
        char serverName[MAX_SERVERNAME] = { 0 };
        DWORD dwLen = sizeof(serverName);
        if (lpEcb->
            GetServerVariable(lpEcb->ConnID, SERVER_NAME, serverName,
                              &dwLen)) {
            if (dwLen > 0)
                serverName[dwLen - 1] = '\0';
            if (init_jk(serverName))
                is_mapread = JK_TRUE;
        }
        if (!is_mapread)
            is_inited = JK_FALSE;
    }

    if (is_inited) {
        isapi_private_data_t private_data;
        jk_ws_service_t s;
        jk_pool_atom_t buf[SMALL_POOL_SIZE];
        char *worker_name;

        if (!watchdog_interval)
            wc_maintain(logger);
        jk_init_ws_service(&s);
        jk_open_pool(&private_data.p, buf, sizeof(buf));

        private_data.bytes_read_so_far = 0;
        private_data.lpEcb = lpEcb;

        s.ws_private = &private_data;
        s.pool = &private_data.p;

        if (init_ws_service(&private_data, &s, &worker_name)) {
            jk_worker_t *worker = wc_get_worker_for_name(worker_name, logger);

            if (JK_IS_DEBUG_LEVEL(logger))
                jk_log(logger, JK_LOG_DEBUG,
                       "%s a worker for name %s",
                       worker ? "got" : "could not get", worker_name);

            if (worker) {
                jk_endpoint_t *e = NULL;
                if (worker->get_endpoint(worker, &e, logger)) {
                    int is_error = JK_HTTP_SERVER_ERROR;
                    int result;
                    if ((result = e->service(e, &s, logger, &is_error)) > 0) {
                        rc = HSE_STATUS_SUCCESS;
                        lpEcb->dwHttpStatusCode = HTTP_STATUS_OK;
                        if (JK_IS_DEBUG_LEVEL(logger))
                            jk_log(logger, JK_LOG_DEBUG,
                                   "service() returned OK");
                    }
                    else {
                        if ((result == JK_CLIENT_ERROR) && (is_error == JK_HTTP_OK)) {
                            jk_log(logger, JK_LOG_INFO,
                                   "service() failed because client aborted connection");
                        }
                        else {
                            jk_log(logger, JK_LOG_ERROR,
                                   "service() failed with http error %d", is_error);
                        }
                        lpEcb->dwHttpStatusCode = is_error;
                        write_error_message(lpEcb, is_error);
                    }
                    e->done(&e, logger);
                }
                else {
                    jk_log(logger, JK_LOG_ERROR,
                        "Failed to obtain an endpoint to service request - "
                        "your connection_pool_size is probably less than the threads in your web server!");
                }
            }
            else {
                jk_log(logger, JK_LOG_ERROR,
                       "could not get a worker for name %s",
                       worker_name);
            }
        }
        else {
            jk_log(logger, JK_LOG_ERROR,
                "failed to init service for request.");
         }
        jk_close_pool(&private_data.p);
    }
    else {
        jk_log(logger, JK_LOG_ERROR,
               "not initialized");
    }

    JK_TRACE_EXIT(logger);
    return rc;
}



BOOL WINAPI TerminateExtension(DWORD dwFlags)
{
    return TerminateFilter(dwFlags);
}

BOOL WINAPI TerminateFilter(DWORD dwFlags)
{
    UNREFERENCED_PARAMETER(dwFlags);

    if (is_inited) {
        is_inited = JK_FALSE;
        if (watchdog_handle) {
            WaitForSingleObject(watchdog_handle, INFINITE);
            CloseHandle(watchdog_handle);
            watchdog_handle = NULL;
        }
        if (is_mapread) {
            uri_worker_map_free(&uw_map, logger);
            is_mapread = JK_FALSE;
        }
        if (workers_map) {
            jk_map_free(&workers_map);
        }
        if (rewrite_map) {
            jk_map_free(&rewrite_map);
        }
        if (rregexp_map) {
            int i;
            for (i = 0; i < jk_map_size(rregexp_map); i++) {
                ap_regex_t *regexp = (ap_regex_t *)jk_map_value_at(rregexp_map, i);
                if (regexp) {
                    ap_regfree(regexp);
                    free(regexp);
                }
            }
            jk_map_free(&rregexp_map);
        }
        wc_close(logger);
        jk_shm_close();
        if (logger) {
            jk_close_file_logger(&logger);
        }
    }

    return TRUE;
}


BOOL WINAPI DllMain(HINSTANCE hInst,    // Instance Handle of the DLL
                    ULONG ulReason,     // Reason why NT called this DLL
                    LPVOID lpReserved)  // Reserved parameter for future use
{
    BOOL fReturn = TRUE;
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char fname[MAX_PATH];
    char file_name[MAX_PATH];

    UNREFERENCED_PARAMETER(lpReserved);

    switch (ulReason) {
    case DLL_PROCESS_ATTACH:
        if (GetModuleFileName(hInst, file_name, sizeof(file_name))) {
            _splitpath(file_name, drive, dir, fname, NULL);
            _makepath(ini_file_name, drive, dir, fname, ".properties");
        }
        else {
            fReturn = JK_FALSE;
        }
        /* Construct redirector headers to use for this redirector instance */
        StringCbPrintf(URI_HEADER_NAME, MAX_PATH, HEADER_TEMPLATE, URI_HEADER_NAME_BASE, hInst);
        StringCbPrintf(QUERY_HEADER_NAME, MAX_PATH, HEADER_TEMPLATE, QUERY_HEADER_NAME_BASE, hInst);
        StringCbPrintf(WORKER_HEADER_NAME, MAX_PATH, HEADER_TEMPLATE, WORKER_HEADER_NAME_BASE, hInst);
        StringCbPrintf(TOMCAT_TRANSLATE_HEADER_NAME, MAX_PATH, HEADER_TEMPLATE, TOMCAT_TRANSLATE_HEADER_NAME_BASE, hInst);

        StringCbPrintf(HTTP_URI_HEADER_NAME, MAX_PATH, HTTP_HEADER_TEMPLATE, URI_HEADER_NAME_BASE, hInst);
        StringCbPrintf(HTTP_QUERY_HEADER_NAME, MAX_PATH, HTTP_HEADER_TEMPLATE, QUERY_HEADER_NAME_BASE, hInst);
        StringCbPrintf(HTTP_WORKER_HEADER_NAME, MAX_PATH, HTTP_HEADER_TEMPLATE, WORKER_HEADER_NAME_BASE, hInst);

    break;
    case DLL_PROCESS_DETACH:
        __try {
            TerminateFilter(HSE_TERM_MUST_UNLOAD);
        }
        __except(1) {
        }
        break;

    default:
        break;
    }

    return fReturn;
}

static DWORD WINAPI watchdog_thread(void *param)
{
    int i;
    if (JK_IS_DEBUG_LEVEL(logger)) {
        jk_log(logger, JK_LOG_DEBUG,
               "Watchdog thread initialized with %u second interval",
               watchdog_interval);
    }
    while (is_inited) {
        for (i = 0; i < (watchdog_interval * 10); i++) {
            if (!is_inited)
                break;
            Sleep(100);
        }
        if (!is_inited)
            break;
        if (JK_IS_DEBUG_LEVEL(logger)) {
            jk_log(logger, JK_LOG_DEBUG,
                   "Watchdog thread running");
        }
        wc_maintain(logger);
    }
    if (JK_IS_DEBUG_LEVEL(logger)) {
        jk_log(logger, JK_LOG_DEBUG,
               "Watchdog thread finished");
    }
    return 0;
}

static int init_jk(char *serverName)
{
    char shm_name[MAX_PATH];
    int rc = JK_FALSE;

    if (!jk_open_file_logger(&logger, log_file, log_level)) {
        logger = NULL;
    }
    StringCbCopy(shm_name, MAX_PATH, SHM_DEF_NAME);

    jk_log(logger, JK_LOG_INFO, "Starting %s", VERSION_STRING );

    if (*serverName) {
        size_t i;
        StringCbCat(shm_name, MAX_PATH, "_");
        StringCbCat(shm_name, MAX_PATH, serverName);
        for(i = 0; i < strlen(shm_name); i++) {
            shm_name[i] = toupper(shm_name[i]);
            if (!isalnum(shm_name[i]))
                shm_name[i] = '_';
        }
    }

    jk_set_worker_def_cache_size(DEFAULT_WORKER_THREADS);

    /* Logging the initialization type: registry or properties file in virtual dir
     */
    if (JK_IS_DEBUG_LEVEL(logger)) {
        if (using_ini_file) {
            jk_log(logger, JK_LOG_DEBUG, "Using ini file %s.", ini_file_name);
        }
        else {
            jk_log(logger, JK_LOG_DEBUG, "Using registry.");
        }

        jk_log(logger, JK_LOG_DEBUG, "Using log file %s.", log_file);
        jk_log(logger, JK_LOG_DEBUG, "Using log level %d.", log_level);
        jk_log(logger, JK_LOG_DEBUG, "Using extension uri %s.", extension_uri);
        jk_log(logger, JK_LOG_DEBUG, "Using worker file %s.", worker_file);
        jk_log(logger, JK_LOG_DEBUG, "Using worker mount file %s.",
               worker_mount_file);
        jk_log(logger, JK_LOG_DEBUG, "Using rewrite rule file %s.",
               rewrite_rule_file);
        jk_log(logger, JK_LOG_DEBUG, "Using uri select %d.", uri_select_option);

        jk_log(logger, JK_LOG_DEBUG, "Using uri header %s.", URI_HEADER_NAME);
        jk_log(logger, JK_LOG_DEBUG, "Using query header %s.", QUERY_HEADER_NAME);
        jk_log(logger, JK_LOG_DEBUG, "Using worker header %s.", WORKER_HEADER_NAME);
        jk_log(logger, JK_LOG_DEBUG, "Using translate header %s.", TOMCAT_TRANSLATE_HEADER_NAME);
    }

    if (rewrite_rule_file[0] && jk_map_alloc(&rewrite_map)) {
        if (jk_map_read_properties(rewrite_map, rewrite_rule_file,
                                   NULL, JK_MAP_HANDLE_RAW, logger)) {
            int i;
            if (JK_IS_DEBUG_LEVEL(logger)) {
                jk_log(logger, JK_LOG_DEBUG, "Loaded rewrite rule file %s.",
                       rewrite_rule_file);

            }
            jk_map_alloc(&rregexp_map);
            for (i = 0; i < jk_map_size(rewrite_map); i++) {
                const char *src = jk_map_name_at(rewrite_map, i);
                if (*src == '~') {
                    ap_regex_t *regexp = malloc(sizeof(ap_regex_t));
                    const char *val = jk_map_value_at(rewrite_map, i);
                    /* Skip leading tilde */
                    regexp->real = src + 1;
                    regexp->fake = val;
                    if (!ap_regcomp(regexp, regexp->real, AP_REG_EXTENDED)) {
                        jk_map_add(rregexp_map, regexp->real, regexp);
                        if (JK_IS_DEBUG_LEVEL(logger)) {
                            jk_log(logger, JK_LOG_DEBUG,
                                   "Added regular expression rule %s -> %s",
                                   regexp->real, regexp->fake);
                        }
                    }
                    else {
                        jk_log(logger, JK_LOG_ERROR,
                               "Unable to compile regular expression %s",
                               regexp->real);
                        free(regexp);
                    }
                }
            }
        }
        else {
            jk_map_free(&rewrite_map);
            rewrite_map = NULL;
        }
    }

    if (uri_worker_map_alloc(&uw_map, NULL, logger)) {
        rc = JK_FALSE;
        if (reject_unsafe)
            uw_map->reject_unsafe = 1;
        else
            uw_map->reject_unsafe = 0;
        uw_map->fname = worker_mount_file;
        uw_map->reload = worker_mount_reload;
        if (worker_mount_file[0])
            rc = uri_worker_map_load(uw_map, logger);
    }
    if (rc) {
        rc = JK_FALSE;
        if (jk_map_alloc(&workers_map)) {
            if (jk_map_read_properties(workers_map, worker_file, NULL,
                                       JK_MAP_HANDLE_DUPLICATES, logger)) {
                int rv;

                /* we add the URI->WORKER MAP since workers using AJP14 will feed it */

                if (jk_map_resolve_references(workers_map, "worker.", 1, 1, logger) == JK_FALSE) {
                    jk_log(logger, JK_LOG_ERROR, "Error in resolving configuration references");
                }
                /*
                 * Create named shared memory for each server
                 */
                if (shm_config_size == 0)
                    shm_config_size = jk_shm_calculate_size(workers_map, logger);
                else {
                    jk_log(logger, JK_LOG_WARNING,
                           "The optimal shared memory size can now be determined automatically.");
                    jk_log(logger, JK_LOG_WARNING,
                           "You can remove the JkShmSize directive if you want to use the optimal size.");
                }
                if ((rv = jk_shm_open(shm_name, shm_config_size, logger)) != 0)
                    jk_log(logger, JK_LOG_ERROR,
                           "Initializing shm:%s errno=%d. Load balancing workers will not function properly.",
                           jk_shm_name(), rv);

                worker_env.uri_to_worker = uw_map;
                worker_env.server_name = serverName;

                if (wc_open(workers_map, &worker_env, logger)) {
                    rc = JK_TRUE;
                }
                uri_worker_map_ext(uw_map, logger);
            }
            else
                jk_log(logger, JK_LOG_EMERG,
                       "Unable to read worker file %s.", worker_file);
            if (rc != JK_TRUE) {
                jk_map_free(&workers_map);
                workers_map = NULL;
            }
        }
    }
    if (rc) {
        if (watchdog_interval) {
            DWORD  wi;
            watchdog_handle = CreateThread(NULL, 0, watchdog_thread,
                                           NULL, 0, &wi);
            if (!watchdog_handle) {
                rc = GetLastError();
                jk_log(logger, JK_LOG_ERROR, "Error creating Watchdog thread");
                return rc;
            }
        }
        jk_log(logger, JK_LOG_INFO, "%s initialized", (VERSION_STRING) );
    }
    return rc;
}

static int initialize_extension(void)
{

    if (read_registry_init_data()) {
        auth_notification_flags = get_auth_flags();
        is_inited = JK_TRUE;
    }
    return is_inited;
}

int parse_uri_select(const char *uri_select)
{
    if (0 == strcasecmp(uri_select, URI_SELECT_PARSED_VERB)) {
        return URI_SELECT_OPT_PARSED;
    }

    if (0 == strcasecmp(uri_select, URI_SELECT_UNPARSED_VERB)) {
        return URI_SELECT_OPT_UNPARSED;
    }

    if (0 == strcasecmp(uri_select, URI_SELECT_ESCAPED_VERB)) {
        return URI_SELECT_OPT_ESCAPED;
    }

    if (0 == strcasecmp(uri_select, URI_SELECT_PROXY_VERB)) {
        return URI_SELECT_OPT_PROXY;
    }

    return -1;
}

static int read_registry_init_data(void)
{
    char tmpbuf[MAX_PATH];
    int ok = JK_TRUE;
    LPVOID src;
    HKEY hkey;
    jk_map_t *map = NULL;

    if (jk_map_alloc(&map)) {
        if (jk_map_read_properties(map, ini_file_name, NULL,
                                   JK_MAP_HANDLE_DUPLICATES, logger)) {
            using_ini_file = JK_TRUE;
            src = map;
        }
        else {
            jk_map_free(&map);
        }
    }
    if (!using_ini_file) {
        long rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGISTRY_LOCATION,
                               (DWORD)0, KEY_READ, &hkey);
        if (ERROR_SUCCESS != rc) {
            return JK_FALSE;
        }
        else {
            src = &hkey;
        }
    }
    ok = ok && get_config_parameter(src, JK_LOG_FILE_TAG, log_file, sizeof(log_file));
    if (get_config_parameter(src, JK_LOG_LEVEL_TAG, tmpbuf, sizeof(tmpbuf))) {
        log_level = jk_parse_log_level(tmpbuf);
    }
    ok = ok && get_config_parameter(src, EXTENSION_URI_TAG, extension_uri, sizeof(extension_uri));
    ok = ok && get_config_parameter(src, JK_WORKER_FILE_TAG, worker_file, sizeof(worker_file));
    ok = ok && get_config_parameter(src, JK_MOUNT_FILE_TAG, worker_mount_file, sizeof(worker_mount_file));
    get_config_parameter(src, URI_REWRITE_TAG, rewrite_rule_file, sizeof(rewrite_rule_file));
    if (get_config_parameter(src, URI_SELECT_TAG, tmpbuf, sizeof(tmpbuf))) {
        int opt = parse_uri_select(tmpbuf);
        if (opt >= 0) {
            uri_select_option = opt;
        }
        else {
            ok = JK_FALSE;
        }
    }
    shm_config_size = (size_t) get_config_int(src, SHM_SIZE_TAG, 0);
    worker_mount_reload = get_config_int(src, WORKER_MOUNT_RELOAD_TAG, JK_URIMAP_DEF_RELOAD);
    strip_session = get_config_bool(src, STRIP_SESSION_TAG, JK_FALSE);
    use_auth_notification_flags = get_config_int(src, AUTH_COMPLETE_TAG, 1);
    reject_unsafe = get_config_bool(src, REJECT_UNSAFE_TAG, JK_FALSE);
    watchdog_interval = get_config_int(src, WATCHDOG_INTERVAL_TAG, 0);
    if (using_ini_file) {
        jk_map_free(&map);
    }
    else {
        RegCloseKey(hkey);
    }
    return ok;
}

static int get_config_parameter(LPVOID src, const char *tag,
                                char *val, DWORD sz)
{
    const char *tmp = NULL;
    if (using_ini_file) {
        tmp = jk_map_get_string((jk_map_t*)src, tag, NULL);
        if (tmp && (strlen(tmp) < sz)) {
            StringCbCopy(val, sz, tmp);
            return JK_TRUE;
        }
        else {
            return JK_FALSE;
        }
    } else {
        return get_registry_config_parameter(*((HKEY*)src), tag, val, sz);
    }
}

static int get_config_int(LPVOID src, const char *tag, int def)
{
    if (using_ini_file) {
        return jk_map_get_int((jk_map_t*)src, tag, def);
    } else {
        int val;
        if (get_registry_config_number(*((HKEY*)src), tag, &val) ) {
            return val;
        }
        else {
            return def;
        }
    }
}

static int get_config_bool(LPVOID src, const char *tag, int def)
{
    if (using_ini_file) {
        return jk_map_get_bool((jk_map_t*)src, tag, def);
    } else {
        char tmpbuf[128];
        if (get_registry_config_parameter(*((HKEY*)src), tag,
                                          tmpbuf, sizeof(tmpbuf))) {
            return jk_get_bool_code(tmpbuf, def);
        }
        else {
            return def;
        }
    }
}

static int get_registry_config_parameter(HKEY hkey,
                                         const char *tag, char *b, DWORD sz)
{
    DWORD type = 0;
    LONG lrc;

    sz = sz - 1; /* Reserve space for RegQueryValueEx to add null terminator */
    b[sz] = '\0'; /* Null terminate in case RegQueryValueEx doesn't */

    lrc = RegQueryValueEx(hkey, tag, (LPDWORD) 0, &type, (LPBYTE) b, &sz);
    if ((ERROR_SUCCESS != lrc) || (type != REG_SZ)) {
        return JK_FALSE;
    }

    return JK_TRUE;
}

static int get_registry_config_number(HKEY hkey,
                                      const char *tag, int *val)
{
    DWORD type = 0;
    DWORD data = 0;
    DWORD sz   = sizeof(DWORD);
    LONG lrc;

    lrc = RegQueryValueEx(hkey, tag, (LPDWORD)0, &type, (LPBYTE)&data, &sz);
    if ((ERROR_SUCCESS != lrc) || (type != REG_DWORD)) {
        return JK_FALSE;
    }

    *val = data;

    return JK_TRUE;
}

static int init_ws_service(isapi_private_data_t * private_data,
                           jk_ws_service_t *s, char **worker_name)
{
    char *huge_buf = NULL;   /* should be enough for all */

    DWORD huge_buf_sz;

    JK_TRACE_ENTER(logger);

    s->start_response = start_response;
    s->read = read;
    s->write = write;

    if (!(huge_buf = jk_pool_alloc(&private_data->p, MAX_PACKET_SIZE))) {
        JK_TRACE_EXIT(logger);
        return JK_FALSE;
    }
    huge_buf_sz = MAX_PACKET_SIZE;
    GET_SERVER_VARIABLE_VALUE(HTTP_WORKER_HEADER_NAME, (*worker_name));
    GET_SERVER_VARIABLE_VALUE(HTTP_URI_HEADER_NAME, s->req_uri);
    GET_SERVER_VARIABLE_VALUE(HTTP_QUERY_HEADER_NAME, s->query_string);

    if (JK_IS_DEBUG_LEVEL(logger)) {
        jk_log(logger, JK_LOG_DEBUG, "Reading extension header %s: %s", HTTP_WORKER_HEADER_NAME, (*worker_name) );
        jk_log(logger, JK_LOG_DEBUG, "Reading extension header %s: %s", HTTP_URI_HEADER_NAME, s->req_uri);
        jk_log(logger, JK_LOG_DEBUG, "Reading extension header %s: %s", HTTP_QUERY_HEADER_NAME, s->query_string);
    }

    if (s->req_uri == NULL) {
        if (JK_IS_DEBUG_LEVEL(logger))
            jk_log(logger, JK_LOG_DEBUG, "No URI header value provided. Defaulting to old behaviour" );
        s->query_string = private_data->lpEcb->lpszQueryString;
        *worker_name = DEFAULT_WORKER_NAME;
        GET_SERVER_VARIABLE_VALUE("URL", s->req_uri);
        if (unescape_url(s->req_uri) < 0) {
            JK_TRACE_EXIT(logger);
            return JK_FALSE;
        }
        getparents(s->req_uri);
    }

    GET_SERVER_VARIABLE_VALUE("AUTH_TYPE", s->auth_type);
    GET_SERVER_VARIABLE_VALUE("REMOTE_USER", s->remote_user);
    GET_SERVER_VARIABLE_VALUE("SERVER_PROTOCOL", s->protocol);
    GET_SERVER_VARIABLE_VALUE("REMOTE_HOST", s->remote_host);
    GET_SERVER_VARIABLE_VALUE("REMOTE_ADDR", s->remote_addr);
    GET_SERVER_VARIABLE_VALUE(SERVER_NAME, s->server_name);
    GET_SERVER_VARIABLE_VALUE_INT("SERVER_PORT", s->server_port, 80);
    GET_SERVER_VARIABLE_VALUE(SERVER_SOFTWARE, s->server_software);
    GET_SERVER_VARIABLE_VALUE_INT("SERVER_PORT_SECURE", s->is_ssl, 0);

    s->method = private_data->lpEcb->lpszMethod;
    s->content_length = (jk_uint64_t)private_data->lpEcb->cbTotalBytes;

    s->uw_map = uw_map;
    /*
     * Add SSL IIS environment
     */
    if (s->is_ssl) {
        char *ssl_env_names[9] = {
            "CERT_ISSUER",
            "CERT_SUBJECT",
            "CERT_COOKIE",
            "HTTPS_SERVER_SUBJECT",
            "CERT_FLAGS",
            "HTTPS_SECRETKEYSIZE",
            "CERT_SERIALNUMBER",
            "HTTPS_SERVER_ISSUER",
            "HTTPS_KEYSIZE"
        };
        char *ssl_env_values[9] = {
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
        };
        unsigned int i;
        unsigned int num_of_vars = 0;

        for (i = 0; i < 9; i++) {
            GET_SERVER_VARIABLE_VALUE(ssl_env_names[i], ssl_env_values[i]);
            if (ssl_env_values[i]) {
                num_of_vars++;
            }
        }
        /* XXX: To make the isapi plugin more consistent with the other web servers */
        /* we should also set s->ssl_cipher, s->ssl_session, and s->ssl_key_size. */
        if (num_of_vars) {
            unsigned int j;

            s->attributes_names =
                jk_pool_alloc(&private_data->p, num_of_vars * sizeof(char *));
            s->attributes_values =
                jk_pool_alloc(&private_data->p, num_of_vars * sizeof(char *));

            j = 0;
            for (i = 0; i < 9; i++) {
                if (ssl_env_values[i]) {
                    s->attributes_names[j] = ssl_env_names[i];
                    s->attributes_values[j] = ssl_env_values[i];
                    j++;
                }
            }
            s->num_attributes = num_of_vars;
            if (ssl_env_values[4] && ssl_env_values[4][0] == '1') {
                CERT_CONTEXT_EX cc;
                cc.cbAllocated = MAX_PACKET_SIZE;
                cc.CertContext.pbCertEncoded = (BYTE *) huge_buf;
                cc.CertContext.cbCertEncoded = 0;

                if (private_data->lpEcb->
                    ServerSupportFunction(private_data->lpEcb->ConnID,
                                          (DWORD) HSE_REQ_GET_CERT_INFO_EX,
                                          (LPVOID) & cc, NULL,
                                          NULL) != FALSE) {
                    jk_log(logger, JK_LOG_DEBUG,
                           "Client Certificate encoding:%d sz:%d flags:%ld",
                           cc.CertContext.
                           dwCertEncodingType & X509_ASN_ENCODING,
                           cc.CertContext.cbCertEncoded,
                           cc.dwCertificateFlags);
                    s->ssl_cert =
                        jk_pool_alloc(&private_data->p,
                                      base64_encode_cert_len(cc.CertContext.
                                                             cbCertEncoded));

                    s->ssl_cert_len = base64_encode_cert(s->ssl_cert,
                                                         huge_buf,
                                                         cc.CertContext.
                                                         cbCertEncoded) - 1;
                }
            }
        }
    }

    huge_buf_sz = MAX_PACKET_SIZE;
    if (get_server_value(private_data->lpEcb,
                         "ALL_HTTP", huge_buf, huge_buf_sz)) {
        unsigned int cnt = 0;
        char *tmp;

        for (tmp = huge_buf; *tmp; tmp++) {
            if (*tmp == '\n') {
                cnt++;
            }
        }

        if (cnt) {
            char *headers_buf = huge_buf;
            unsigned int i;
            size_t len_of_http_prefix = strlen("HTTP_");
            BOOL need_content_length_header = (s->content_length == 0);

            cnt -= 2;           /* For our two special headers:
                                 * HTTP_TOMCATURI_XXXXXXXX
                                 * HTTP_TOMCATWORKER_XXXXXXXX
                                 */
            /* allocate an extra header slot in case we need to add a content-length header */
            s->headers_names =
                jk_pool_alloc(&private_data->p, (cnt + 1) * sizeof(char *));
            s->headers_values =
                jk_pool_alloc(&private_data->p, (cnt + 1) * sizeof(char *));

            if (!s->headers_names || !s->headers_values || !headers_buf) {
                JK_TRACE_EXIT(logger);
                return JK_FALSE;
            }

            for (i = 0, tmp = headers_buf; *tmp && i < cnt;) {
                int real_header = JK_TRUE;

                /* Skipp the HTTP_ prefix to the beginning of th header name */
                tmp += len_of_http_prefix;

                if (!strnicmp(tmp, URI_HEADER_NAME, strlen(URI_HEADER_NAME))
                    || !strnicmp(tmp, WORKER_HEADER_NAME,
                                 strlen(WORKER_HEADER_NAME))) {
                    real_header = JK_FALSE;
                }
                else if (!strnicmp(tmp, QUERY_HEADER_NAME,
                                   strlen(QUERY_HEADER_NAME))) {
                    /* HTTP_TOMCATQUERY_XXXXXXXX was supplied,
                     * remove it from the count and skip
                     */
                    cnt--;
                    real_header = JK_FALSE;
                }
                else if (need_content_length_header &&
                         !strnicmp(tmp, CONTENT_LENGTH,
                                   strlen(CONTENT_LENGTH))) {
                    need_content_length_header = FALSE;
                    s->headers_names[i] = tmp;
                }
                else if (!strnicmp(tmp, TOMCAT_TRANSLATE_HEADER_NAME,
                                   strlen(TOMCAT_TRANSLATE_HEADER_NAME))) {
                    s->headers_names[i] = TRANSLATE_HEADER_NAME_LC;
                }
                else {
                    s->headers_names[i] = tmp;
                }

                while (':' != *tmp && *tmp) {
                    if ('_' == *tmp) {
                        *tmp = '-';
                    }
                    else {
                        *tmp = JK_TOLOWER(*tmp);
                    }
                    tmp++;
                }
                *tmp = '\0';
                tmp++;

                /* Skip all the WS chars after the ':' to the beginning of th header value */
                while (' ' == *tmp || '\t' == *tmp || '\v' == *tmp) {
                    tmp++;
                }

                if (real_header) {
                    s->headers_values[i] = tmp;
                }

                while (*tmp != '\n' && *tmp != '\r') {
                    tmp++;
                }
                *tmp = '\0';
                tmp++;

                /* skipp CR LF */
                while (*tmp == '\n' || *tmp == '\r') {
                    tmp++;
                }

                if (real_header) {
                    i++;
                }
            }
            /* Add a content-length = 0 header if needed.
             * Ajp13 assumes an absent content-length header means an unknown,
             * but non-zero length body.
             */
            if (need_content_length_header) {
                s->headers_names[cnt] = "Content-Length";
                s->headers_values[cnt] = "0";
                cnt++;
            }
            s->num_headers = cnt;
        }
        else {
            /* We must have our two headers */
            JK_TRACE_EXIT(logger);
            return JK_FALSE;
        }
    }
    else {
        JK_TRACE_EXIT(logger);
        return JK_FALSE;
    }

    /* Dump all connection param so we can trace what's going to
     * the remote tomcat
     */
    if (JK_IS_DEBUG_LEVEL(logger)) {
        jk_log(logger, JK_LOG_DEBUG,
               "Service protocol=%s method=%s host=%s addr=%s name=%s port=%d auth=%s user=%s uri=%s",
               STRNULL_FOR_NULL(s->protocol),
               STRNULL_FOR_NULL(s->method),
               STRNULL_FOR_NULL(s->remote_host),
               STRNULL_FOR_NULL(s->remote_addr),
               STRNULL_FOR_NULL(s->server_name),
               s->server_port,
               STRNULL_FOR_NULL(s->auth_type),
               STRNULL_FOR_NULL(s->remote_user),
               STRNULL_FOR_NULL(s->req_uri));
    }

    JK_TRACE_EXIT(logger);
    return JK_TRUE;
}

static int get_server_value(LPEXTENSION_CONTROL_BLOCK lpEcb,
                            char *name, char *buf, DWORD bufsz)
{
    DWORD sz = bufsz;
    buf[0]   = '\0';
    if (!lpEcb->GetServerVariable(lpEcb->ConnID, name,
                                  buf, (LPDWORD) &sz))
        return JK_FALSE;

    if (sz <= bufsz)
        buf[sz-1] = '\0';
    return JK_TRUE;
}

static const char begin_cert[] = "-----BEGIN CERTIFICATE-----\r\n";

static const char end_cert[] = "-----END CERTIFICATE-----\r\n";

static const char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode_cert_len(int len)
{
    int n = ((len + 2) / 3 * 4) + 1;    /* base64 encoded size */
    n += (n + 63 / 64) * 2;             /* add CRLF's */
    n += sizeof(begin_cert) + sizeof(end_cert) - 2;  /* add enclosing strings. */
    return n;
}

static int base64_encode_cert(char *encoded,
                              const char *string, int len)
{
    int i, c;
    char *p;
    const char *t;

    p = encoded;

    t = begin_cert;
    while (*t != '\0')
        *p++ = *t++;

    c = 0;
    for (i = 0; i < len - 2; i += 3) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        *p++ = basis_64[((string[i] & 0x3) << 4) |
                        ((int)(string[i + 1] & 0xF0) >> 4)];
        *p++ = basis_64[((string[i + 1] & 0xF) << 2) |
                        ((int)(string[i + 2] & 0xC0) >> 6)];
        *p++ = basis_64[string[i + 2] & 0x3F];
        c += 4;
        if (c >= 64) {
            *p++ = '\r';
            *p++ = '\n';
            c = 0;
        }
    }
    if (i < len) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        if (i == (len - 1)) {
            *p++ = basis_64[((string[i] & 0x3) << 4)];
            *p++ = '=';
        }
        else {
            *p++ = basis_64[((string[i] & 0x3) << 4) |
                            ((int)(string[i + 1] & 0xF0) >> 4)];
            *p++ = basis_64[((string[i + 1] & 0xF) << 2)];
        }
        *p++ = '=';
        c++;
    }
    if (c != 0) {
        *p++ = '\r';
        *p++ = '\n';
    }

    t = end_cert;
    while (*t != '\0')
        *p++ = *t++;

    *p++ = '\0';
    return (int)(p - encoded);
}

static int get_auth_flags()
{
    HKEY hkey;
    long rc;
    int maj, sz;
    int rv = SF_NOTIFY_PREPROC_HEADERS;
    int use_auth = JK_FALSE;
    /* Retreive the IIS version Major */
    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                      W3SVC_REGISTRY_KEY, (DWORD) 0, KEY_READ, &hkey);
    if (ERROR_SUCCESS != rc) {
        return rv;
    }
    sz = sizeof(int);
    rc = RegQueryValueEx(hkey, "MajorVersion", NULL, NULL,
                         (LPBYTE) & maj, &sz);
    if (ERROR_SUCCESS != rc) {
        CloseHandle(hkey);
        return rv;
    }
    CloseHandle(hkey);
    if (use_auth_notification_flags && maj > 4)
        rv = SF_NOTIFY_AUTH_COMPLETE;

    return rv;
}
