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
 * Description: ISAPI plugin for IIS/PWS                                   *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Larry Isaacs <larryi@apache.org>                           *
 * Author:      Ignacio J. Ortega <nacho@apache.org>                       *
 * Author:      Mladen Turk <mturk@apache.org>                             *
 * Version:     $Revision$                                          *
 ***************************************************************************/

// This define is needed to include wincrypt,h, needed to get client certificates
#define _WIN32_WINNT 0x0400

#include <httpext.h>
#include <httpfilt.h>
#include <wininet.h>

#include "jk_global.h"
#include "jk_util.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_uri_worker_map.h"

#define VERSION_STRING "Jakarta/ISAPI/" JK_VERSTRING

#define DEFAULT_WORKER_NAME ("ajp13")
/*
 * We use special headers to pass values from the filter to the 
 * extension. These values are:
 *
 * 1. The real URI before redirection took place
 * 2. The name of the worker to be used.
 * 3. The contents of the Translate header, if any
 *
 */
#define URI_HEADER_NAME              ("TOMCATURI:")
#define QUERY_HEADER_NAME            ("TOMCATQUERY:")
#define WORKER_HEADER_NAME           ("TOMCATWORKER:")
#define TOMCAT_TRANSLATE_HEADER_NAME ("TOMCATTRANSLATE:")
#define CONTENT_LENGTH               ("CONTENT_LENGTH:")

#define HTTP_URI_HEADER_NAME         ("HTTP_TOMCATURI")
#define HTTP_QUERY_HEADER_NAME       ("HTTP_TOMCATQUERY")
#define HTTP_WORKER_HEADER_NAME      ("HTTP_TOMCATWORKER")

#define REGISTRY_LOCATION       ("Software\\Apache Software Foundation\\Jakarta Isapi Redirector\\1.0")
#define EXTENSION_URI_TAG       ("extension_uri")

#define URI_SELECT_TAG              ("uri_select")
#define URI_SELECT_PARSED_VERB      ("parsed")
#define URI_SELECT_UNPARSED_VERB    ("unparsed")
#define URI_SELECT_ESCAPED_VERB     ("escaped")

#define BAD_REQUEST     -1
#define BAD_PATH        -2
#define MAX_SERVERNAME  128

#define HTML_ERROR_400          "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">"  \
                                "<HTML><HEAD><TITLE>Bad request!</TITLE></HEAD>"                    \
                                "<BODY><H1>Bad request!</H1><DL><DD>\n"                             \
                                "Your browser (or proxy) sent a request that "                      \
                                "this server could not understand.</DL></DD></BODY></HTML>"

#define HTML_ERROR_403          "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">"  \
                                "<HTML><HEAD><TITLE>Access forbidden!!</TITLE></HEAD>"              \
                                "<BODY><H1>Access forbidden!</H1><DL><DD>\n"                        \
                                "You don't have permission to access the requested object."         \
                                "It is either read-protected or not readable by the server.</DL></DD></BODY></HTML>"

 
#define JK_TOLOWER(x)   ((char)tolower((BYTE)(x)))

#define GET_SERVER_VARIABLE_VALUE(name, place)          \
  do {                                                  \
    (place) = NULL;                                     \
    huge_buf_sz = sizeof(huge_buf);                     \
    if (get_server_value(private_data->lpEcb,           \
                        (name),                         \
                        huge_buf,                       \
                        huge_buf_sz,                    \
                        "")) {                          \
        (place) = jk_pool_strdup(&private_data->p,      \
                                 huge_buf);             \
  } } while(0)

#define GET_SERVER_VARIABLE_VALUE_INT(name, place, def)     \
  do {                                                      \
    huge_buf_sz = sizeof(huge_buf);                         \
    if (get_server_value(private_data->lpEcb,               \
                        (name),                             \
                        huge_buf,                           \
                        huge_buf_sz,                        \
                        "")) {                              \
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
static int iis5 = -1;

static jk_uri_worker_map_t *uw_map = NULL;
static jk_logger_t *logger = NULL;
static char *SERVER_NAME = "SERVER_NAME";
static char *SERVER_SOFTWARE = "SERVER_SOFTWARE";
static char *CONTENT_TYPE = "Content-Type:text/html\r\n\r\n";

static char extension_uri[INTERNET_MAX_URL_LENGTH] =
    "/jakarta/isapi_redirect.dll";
static char log_file[MAX_PATH * 2];
static int log_level = JK_LOG_EMERG_LEVEL;
static char worker_file[MAX_PATH * 2];
static char worker_mount_file[MAX_PATH * 2];

#define URI_SELECT_OPT_PARSED       0
#define URI_SELECT_OPT_UNPARSED     1
#define URI_SELECT_OPT_ESCAPED      2

static int uri_select_option = URI_SELECT_OPT_PARSED;

static jk_worker_env_t worker_env;

typedef struct isapi_private_data_t isapi_private_data_t;
struct isapi_private_data_t
{
    jk_pool_t p;

    int request_started;
    unsigned int bytes_read_so_far;
    LPEXTENSION_CONTROL_BLOCK lpEcb;
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

static int get_registry_config_parameter(HKEY hkey,
                                         const char *tag, char *b, DWORD sz);


static int get_server_value(LPEXTENSION_CONTROL_BLOCK lpEcb,
                            char *name,
                            char *buf, DWORD bufsz, char *def_val);

static int base64_encode_cert_len(int len);

static int base64_encode_cert(char *encoded,
                              const char *string, int len);


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
    if (stristr(uri, "web-inf")) {
        return JK_TRUE;
    }
    if (stristr(uri, "meta-inf")) {
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
               "invalid status %d\n",
               status);
        JK_TRACE_EXIT(logger);
        return JK_FALSE;
    }

    if (s && s->ws_private) {
        isapi_private_data_t *p = s->ws_private;
        if (!p->request_started) {
            size_t len_of_status;
            char *status_str;
            char *headers_str;

            p->request_started = JK_TRUE;

            /*
             * Create the status line
             */
            if (!reason) {
                reason = "";
            }
            status_str = (char *)_alloca((6 + strlen(reason)) * sizeof(char));
            sprintf(status_str, "%d %s", status, reason);
            len_of_status = strlen(status_str);

            /*
             * Create response headers string
             */
            if (num_of_headers) {
                size_t i, len_of_headers;
                for (i = 0, len_of_headers = 0; i < num_of_headers; i++) {
                    len_of_headers += strlen(header_names[i]);
                    len_of_headers += strlen(header_values[i]);
                    len_of_headers += 4;        /* extra for colon, space and crlf */
                }

                len_of_headers += 3;    /* crlf and terminating null char */
                headers_str = (char *)_alloca(len_of_headers * sizeof(char));
                headers_str[0] = '\0';

                for (i = 0; i < num_of_headers; i++) {
                    strcat(headers_str, header_names[i]);
                    strcat(headers_str, ": ");
                    strcat(headers_str, header_values[i]);
                    strcat(headers_str, crlf);
                }
                strcat(headers_str, crlf);
            }
            else {
                headers_str = crlf;
            }

            if (!p->lpEcb->ServerSupportFunction(p->lpEcb->ConnID,
                                                 HSE_REQ_SEND_RESPONSE_HEADER,
                                                 status_str,
                                                 (LPDWORD) &len_of_status,
                                                 (LPDWORD) headers_str)) {
                jk_log(logger, JK_LOG_ERROR,
                       "HSE_REQ_SEND_RESPONSE_HEADER failed\n");
                JK_TRACE_EXIT(logger);
                return JK_FALSE;
            }
        }
        JK_TRACE_EXIT(logger);
        return JK_TRUE;

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

        *a = 0;
        if (l) {
            char *buf = b;
            DWORD already_read = p->lpEcb->cbAvailable - p->bytes_read_so_far;

            if (already_read >= l) {
                memcpy(buf, p->lpEcb->lpbData + p->bytes_read_so_far, l);
                p->bytes_read_so_far += l;
                *a = l;
            }
            else {
                /*
                 * Try to copy what we already have 
                 */
                if (already_read > 0) {
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
                if (p->lpEcb->ReadClient(p->lpEcb->ConnID, buf, (LPDWORD)&l)) {
                    *a += l;
                }
                else {
                    jk_log(logger, JK_LOG_ERROR,
                           "ReadClient failed with %08x\n", GetLastError());
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
    jk_log(logger, JK_LOG_DEBUG, "Into jk_ws_service_t::write\n");

    if (s && s->ws_private && b) {
        isapi_private_data_t *p = s->ws_private;

        if (l) {
            unsigned int written = 0;
            char *buf = (char *)b;

            if (!p->request_started) {
                start_response(s, 200, NULL, NULL, NULL, 0);
            }

            while (written < l) {
                DWORD try_to_write = l - written;
                if (!p->lpEcb->WriteClient(p->lpEcb->ConnID,
                                           buf + written, &try_to_write, 0)) {
                    jk_log(logger, JK_LOG_ERROR,
                           "WriteClient failed with %08x\n", GetLastError());
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
    ULONG http_filter_revision = HTTP_FILTER_REVISION;

    pVer->dwFilterVersion = pVer->dwServerFilterVersion;

    if (pVer->dwFilterVersion > http_filter_revision) {
        pVer->dwFilterVersion = http_filter_revision;
    }

    pVer->dwFlags = SF_NOTIFY_ORDER_HIGH |
                    SF_NOTIFY_SECURE_PORT |
                    SF_NOTIFY_NONSECURE_PORT |
                    SF_NOTIFY_PREPROC_HEADERS |
                    SF_NOTIFY_AUTH_COMPLETE;

    strcpy(pVer->lpszFilterDesc, VERSION_STRING);

    if (!is_inited) {
        return initialize_extension();
    }

    return TRUE;
}

DWORD WINAPI HttpFilterProc(PHTTP_FILTER_CONTEXT pfc,
                            DWORD dwNotificationType, LPVOID pvNotification)
{
    /* Initialise jk */
    if (is_inited && !is_mapread) {
        char serverName[MAX_SERVERNAME];
        DWORD dwLen = sizeof(serverName);

        if (pfc->GetServerVariable(pfc, SERVER_NAME, serverName, &dwLen)) {
            if (dwLen > 0)
                serverName[dwLen - 1] = '\0';
            if (init_jk(serverName))
                is_mapread = JK_TRUE;
        }
        /* If we can't read the map we become dormant */
        if (!is_mapread)
            is_inited = JK_FALSE;
    }

    if (is_inited && (iis5 < 0)) {
        char serverSoftware[256];
        DWORD dwLen = sizeof(serverSoftware);
        iis5 = 0;
        if (pfc->
            GetServerVariable(pfc, SERVER_SOFTWARE, serverSoftware, &dwLen)) {
            iis5 = (atof(serverSoftware + 14) >= 5.0);
            if (iis5) {
                jk_log(logger, JK_LOG_INFO, "Detected IIS >= 5.0\n");
            }
            else {
                jk_log(logger, JK_LOG_INFO, "Detected IIS < 5.0\n");
            }
        }
    }

    if (is_inited &&
        (((SF_NOTIFY_PREPROC_HEADERS == dwNotificationType) && !iis5) ||
         ((SF_NOTIFY_AUTH_COMPLETE == dwNotificationType) && iis5)
        )
        ) {
        char uri[INTERNET_MAX_URL_LENGTH];
        char snuri[INTERNET_MAX_URL_LENGTH] = "/";
        char Host[INTERNET_MAX_URL_LENGTH] = "";
        char Port[INTERNET_MAX_URL_LENGTH] = "";
        char Translate[INTERNET_MAX_URL_LENGTH];
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

        if (iis5) {
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


        jk_log(logger, JK_LOG_DEBUG, "Filter started\n");


        /*
         * Just in case somebody set these headers in the request!
         */
        SetHeader(pfc, URI_HEADER_NAME, NULL);
        SetHeader(pfc, QUERY_HEADER_NAME, NULL);
        SetHeader(pfc, WORKER_HEADER_NAME, NULL);
        SetHeader(pfc, TOMCAT_TRANSLATE_HEADER_NAME, NULL);

        if (!GetHeader(pfc, "url", (LPVOID) uri, (LPDWORD) & sz)) {
            jk_log(logger, JK_LOG_ERROR,
                   "error while getting the url\n");
            return SF_STATUS_REQ_ERROR;
        }

        if (strlen(uri)) {
            int rc;
            char *worker = 0;
            query = strchr(uri, '?');
            if (query) {
                *query++ = '\0';
            }

            rc = unescape_url(uri);
            if (rc == BAD_REQUEST) {
                jk_log(logger, JK_LOG_ERROR,
                       "[%s] contains one or more invalid escape sequences.\n",
                       uri);
                write_error_response(pfc, "400 Bad Request",
                                     HTML_ERROR_400);
                return SF_STATUS_REQ_FINISHED;
            }
            else if (rc == BAD_PATH) {
                jk_log(logger, JK_LOG_EMERG,
                       "[%s] contains forbidden escape sequences.\n",
                       uri);
                write_error_response(pfc, "403 Forbidden",
                                     HTML_ERROR_403);
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
                strcat(Host, ":");
                strcat(Host, Port);
            }
            if (szHost > 0) {
                strcat(snuri, Host);
                strcat(snuri, uri);
                jk_log(logger, JK_LOG_DEBUG,
                       "Virtual Host redirection of %s\n",
                       snuri);
                worker = map_uri_to_worker(uw_map, snuri, logger);
            }
            if (!worker) {
                jk_log(logger, JK_LOG_DEBUG,
                       "Default redirection of %s\n",
                       uri);
                worker = map_uri_to_worker(uw_map, uri, logger);
            }
            /*
             * Check if somebody is feading us with his own TOMCAT data headers.
             * We reject such postings !
             */
            jk_log(logger, JK_LOG_DEBUG,
                   "check if [%s] is points to the web-inf directory\n",
                   uri);

            if (uri_is_web_inf(uri)) {
                jk_log(logger, JK_LOG_EMERG,
                       "[%s] points to the web-inf or meta-inf directory.\nSomebody try to hack into the site!!!\n",
                       uri);

                write_error_response(pfc, "403 Forbidden",
                                     HTML_ERROR_403);
                return SF_STATUS_REQ_FINISHED;
            }

            if (worker) {
                char *forwardURI;

                /* This is a servlet, should redirect ... */
                jk_log(logger, JK_LOG_DEBUG,
                       "[%s] is a servlet url - should redirect to %s\n",
                       uri, worker);

                /* get URI we should forward */
                if (uri_select_option == URI_SELECT_OPT_UNPARSED) {
                    /* get original unparsed URI */
                    GetHeader(pfc, "url", (LPVOID) uri, (LPDWORD) & sz);
                    /* restore terminator for uri portion */
                    if (query)
                        *(query - 1) = '\0';
                    jk_log(logger, JK_LOG_DEBUG,
                           "fowarding original URI [%s]\n",
                           uri);
                    forwardURI = uri;
                }
                else if (uri_select_option == URI_SELECT_OPT_ESCAPED) {
                    if (!escape_url(uri, snuri, INTERNET_MAX_URL_LENGTH)) {
                        jk_log(logger, JK_LOG_ERROR,
                               "[%s] re-encoding request exceeds maximum buffer size.\n",
                               uri);
                        write_error_response(pfc, "400 Bad Request",
                                             HTML_ERROR_400);
                        return SF_STATUS_REQ_FINISHED;
                    }
                    jk_log(logger, JK_LOG_DEBUG,
                           "fowarding escaped URI [%s]\n",
                           snuri);
                    forwardURI = snuri;
                }
                else {
                    forwardURI = uri;
                }

                if (!AddHeader(pfc, URI_HEADER_NAME, forwardURI) ||
                    ((query != NULL && strlen(query) > 0)
                     ? !AddHeader(pfc, QUERY_HEADER_NAME, query) : FALSE) ||
                    !AddHeader(pfc, WORKER_HEADER_NAME, worker) ||
                    !SetHeader(pfc, "url", extension_uri)) {
                    jk_log(logger, JK_LOG_ERROR,
                           "error while adding request headers\n");
                    return SF_STATUS_REQ_ERROR;
                }

                /* Move Translate: header to a temporary header so
                 * that the extension proc will be called.
                 * This allows the servlet to handle 'Translate: f'.
                 */
                if (GetHeader
                    (pfc, "Translate:", (LPVOID) Translate,
                     (LPDWORD) & szTranslate) && Translate != NULL
                    && szTranslate > 0) {
                    if (!AddHeader
                        (pfc, TOMCAT_TRANSLATE_HEADER_NAME, Translate)) {
                        jk_log(logger, JK_LOG_ERROR,
                               "error while adding Tomcat-Translate headers\n");
                        return SF_STATUS_REQ_ERROR;
                    }
                    SetHeader(pfc, "Translate:", NULL);
                }
            }
            else {
                jk_log(logger, JK_LOG_DEBUG,
                       "[%s] is not a servlet url\n", uri);
            }
        }
    }
    return SF_STATUS_REQ_NEXT_NOTIFICATION;
}


BOOL WINAPI GetExtensionVersion(HSE_VERSION_INFO * pVer)
{
    pVer->dwExtensionVersion = MAKELONG(HSE_VERSION_MINOR, HSE_VERSION_MAJOR);

    strcpy(pVer->lpszExtensionDesc, VERSION_STRING);


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
        char serverName[MAX_SERVERNAME];
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

        jk_init_ws_service(&s);
        jk_open_pool(&private_data.p, buf, sizeof(buf));

        private_data.request_started = JK_FALSE;
        private_data.bytes_read_so_far = 0;
        private_data.lpEcb = lpEcb;

        s.ws_private = &private_data;
        s.pool = &private_data.p;

        if (init_ws_service(&private_data, &s, &worker_name)) {
            jk_worker_t *worker = wc_get_worker_for_name(worker_name, logger);

            jk_log(logger, JK_LOG_DEBUG,
                   "%s a worker for name %s\n",
                   worker ? "got" : "could not get", worker_name);

            if (worker) {
                jk_endpoint_t *e = NULL;
                /* Update retries for this worker */
                s.retries = worker->retries;                
                if (worker->get_endpoint(worker, &e, logger)) {
                    int recover = JK_FALSE;
                    if (e->service(e, &s, logger, &recover)) {
                        rc = HSE_STATUS_SUCCESS;
                        lpEcb->dwHttpStatusCode = HTTP_STATUS_OK;
                        jk_log(logger, JK_LOG_DEBUG,
                               "service() returned OK\n");
                    }
                    else {
                        jk_log(logger, JK_LOG_ERROR,
                               "service() failed\n");
                    }
                    e->done(&e, logger);
                }
            }
            else {
                jk_log(logger, JK_LOG_ERROR,
                       "could not get a worker for name %s\n",
                       worker_name);
            }
        }
        jk_close_pool(&private_data.p);
    }
    else {
        jk_log(logger, JK_LOG_ERROR,
               "not initialized\n");
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

        if (is_mapread) {
            uri_worker_map_free(&uw_map, logger);
            is_mapread = JK_FALSE;
        }
        wc_close(logger);
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
    char fname[_MAX_FNAME];
    char file_name[_MAX_PATH];

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

static int init_jk(char *serverName)
{
    int rc = JK_FALSE;
    jk_map_t *map;

    if (!jk_open_file_logger(&logger, log_file, log_level)) {
        logger = NULL;
    }
    /* Logging the initialization type: registry or properties file in virtual dir
     */
    if (using_ini_file) {
        jk_log(logger, JK_LOG_DEBUG, "Using ini file %s.\n", ini_file_name);
    }
    else {
        jk_log(logger, JK_LOG_DEBUG, "Using registry.\n");
    }
    jk_log(logger, JK_LOG_DEBUG, "Using log file %s.\n", log_file);
    jk_log(logger, JK_LOG_DEBUG, "Using log level %d.\n", log_level);
    jk_log(logger, JK_LOG_DEBUG, "Using extension uri %s.\n", extension_uri);
    jk_log(logger, JK_LOG_DEBUG, "Using worker file %s.\n", worker_file);
    jk_log(logger, JK_LOG_DEBUG, "Using worker mount file %s.\n",
           worker_mount_file);
    jk_log(logger, JK_LOG_DEBUG, "Using uri select %d.\n", uri_select_option);

    if (jk_map_alloc(&map)) {
        if (jk_map_read_properties(map, worker_mount_file)) {
            /* remove non-mapping entries (assume they were string substitutions) */
            jk_map_t *map2;
            if (jk_map_alloc(&map2)) {
                int sz, i;
                void *old;

                sz = jk_map_size(map);
                for (i = 0; i < sz; i++) {
                    char *name = jk_map_name_at(map, i);
                    if (*name == '/' || *name == '!') {
                        jk_map_put(map2, name, jk_map_value_at(map, i), &old);
                    }
                    else {
                        jk_log(logger, JK_LOG_DEBUG,
                               "Ignoring worker mount file entry %s=%s.\n",
                               name, jk_map_value_at(map, i));
                    }
                }

                if (uri_worker_map_alloc(&uw_map, map2, logger)) {
                    rc = JK_TRUE;
                }

                jk_map_free(&map2);
            }
        }
        else {
            jk_log(logger, JK_LOG_EMERG,
                   "Unable to read worker mount file %s.\n",
                   worker_mount_file);
        }
        jk_map_free(&map);
    }

    if (rc) {
        rc = JK_FALSE;
        if (jk_map_alloc(&map)) {
            if (jk_map_read_properties(map, worker_file)) {
                /* we add the URI->WORKER MAP since workers using AJP14 will feed it */

                worker_env.uri_to_worker = uw_map;
                worker_env.server_name = serverName;

                if (wc_open(map, &worker_env, logger)) {
                    rc = JK_TRUE;
                }
            }
            else {
                jk_log(logger, JK_LOG_EMERG,
                       "Unable to read worker file %s.\n", worker_file);
            }
            jk_map_free(&map);
        }
    }

    return rc;
}

static int initialize_extension(void)
{

    if (read_registry_init_data()) {
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

    return -1;
}

static int read_registry_init_data(void)
{
    char tmpbuf[INTERNET_MAX_URL_LENGTH];
    HKEY hkey;
    long rc;
    int ok = JK_TRUE;
    char *tmp;
    jk_map_t *map;

    if (jk_map_alloc(&map)) {
        if (jk_map_read_properties(map, ini_file_name)) {
            using_ini_file = JK_TRUE;
        }
    }
    if (using_ini_file) {
        tmp = jk_map_get_string(map, JK_LOG_FILE_TAG, NULL);
        if (tmp) {
            strcpy(log_file, tmp);
        }
        else {
            ok = JK_FALSE;
        }
        tmp = jk_map_get_string(map, JK_LOG_LEVEL_TAG, NULL);
        if (tmp) {
            log_level = jk_parse_log_level(tmp);
        }
        else {
            ok = JK_FALSE;
        }
        tmp = jk_map_get_string(map, EXTENSION_URI_TAG, NULL);
        if (tmp) {
            strcpy(extension_uri, tmp);
        }
        else {
            ok = JK_FALSE;
        }
        tmp = jk_map_get_string(map, JK_WORKER_FILE_TAG, NULL);
        if (tmp) {
            strcpy(worker_file, tmp);
        }
        else {
            ok = JK_FALSE;
        }
        tmp = jk_map_get_string(map, JK_MOUNT_FILE_TAG, NULL);
        if (tmp) {
            strcpy(worker_mount_file, tmp);
        }
        else {
            ok = JK_FALSE;
        }
        tmp = jk_map_get_string(map, URI_SELECT_TAG, NULL);
        if (tmp) {
            int opt = parse_uri_select(tmp);
            if (opt >= 0) {
                uri_select_option = opt;
            }
            else {
                ok = JK_FALSE;
            }
        }

    }
    else {
        rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                          REGISTRY_LOCATION, (DWORD) 0, KEY_READ, &hkey);
        if (ERROR_SUCCESS != rc) {
            return JK_FALSE;
        }

        if (get_registry_config_parameter(hkey,
                                          JK_LOG_FILE_TAG,
                                          tmpbuf, sizeof(log_file))) {
            strcpy(log_file, tmpbuf);
        }
        else {
            ok = JK_FALSE;
        }

        if (get_registry_config_parameter(hkey,
                                          JK_LOG_LEVEL_TAG,
                                          tmpbuf, sizeof(tmpbuf))) {
            log_level = jk_parse_log_level(tmpbuf);
        }
        else {
            ok = JK_FALSE;
        }

        if (get_registry_config_parameter(hkey,
                                          EXTENSION_URI_TAG,
                                          tmpbuf, sizeof(extension_uri))) {
            strcpy(extension_uri, tmpbuf);
        }
        else {
            ok = JK_FALSE;
        }

        if (get_registry_config_parameter(hkey,
                                          JK_WORKER_FILE_TAG,
                                          tmpbuf, sizeof(worker_file))) {
            strcpy(worker_file, tmpbuf);
        }
        else {
            ok = JK_FALSE;
        }

        if (get_registry_config_parameter(hkey,
                                          JK_MOUNT_FILE_TAG,
                                          tmpbuf,
                                          sizeof(worker_mount_file))) {
            strcpy(worker_mount_file, tmpbuf);
        }
        else {
            ok = JK_FALSE;
        }

        if (get_registry_config_parameter(hkey,
                                          URI_SELECT_TAG,
                                          tmpbuf, sizeof(tmpbuf))) {
            int opt = parse_uri_select(tmpbuf);
            if (opt >= 0) {
                uri_select_option = opt;
            }
            else {
                ok = JK_FALSE;
            }
        }

        RegCloseKey(hkey);
    }
    return ok;
}

static int get_registry_config_parameter(HKEY hkey,
                                         const char *tag, char *b, DWORD sz)
{
    DWORD type = 0;
    LONG lrc;

    lrc = RegQueryValueEx(hkey, tag, (LPDWORD) 0, &type, (LPBYTE) b, &sz);
    if ((ERROR_SUCCESS != lrc) || (type != REG_SZ)) {
        return JK_FALSE;
    }

    b[sz] = '\0';

    return JK_TRUE;
}

static int init_ws_service(isapi_private_data_t * private_data,
                           jk_ws_service_t *s, char **worker_name)
{
    char huge_buf[16 * 1024];   /* should be enough for all */

    DWORD huge_buf_sz;

    s->jvm_route = NULL;

    s->start_response = start_response;
    s->read = read;
    s->write = write;

    /* Clear RECO status */
    s->reco_status = RECO_NONE;

    GET_SERVER_VARIABLE_VALUE(HTTP_WORKER_HEADER_NAME, (*worker_name));
    GET_SERVER_VARIABLE_VALUE(HTTP_URI_HEADER_NAME, s->req_uri);
    GET_SERVER_VARIABLE_VALUE(HTTP_QUERY_HEADER_NAME, s->query_string);

    if (s->req_uri == NULL) {
        s->query_string = private_data->lpEcb->lpszQueryString;
        *worker_name = DEFAULT_WORKER_NAME;
        GET_SERVER_VARIABLE_VALUE("URL", s->req_uri);
        if (unescape_url(s->req_uri) < 0)
            return JK_FALSE;
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
    s->content_length = private_data->lpEcb->cbTotalBytes;

    s->ssl_cert = NULL;
    s->ssl_cert_len = 0;
    s->ssl_cipher = NULL;
    s->ssl_session = NULL;
    s->ssl_key_size = -1;

    s->headers_names = NULL;
    s->headers_values = NULL;
    s->num_headers = 0;

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
                cc.cbAllocated = sizeof(huge_buf);
                cc.CertContext.pbCertEncoded = (BYTE *) huge_buf;
                cc.CertContext.cbCertEncoded = 0;

                if (private_data->lpEcb->
                    ServerSupportFunction(private_data->lpEcb->ConnID,
                                          (DWORD) HSE_REQ_GET_CERT_INFO_EX,
                                          (LPVOID) & cc, NULL,
                                          NULL) != FALSE) {
                    jk_log(logger, JK_LOG_DEBUG,
                           "Client Certificate encoding:%d sz:%d flags:%ld\n",
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

    huge_buf_sz = sizeof(huge_buf);
    if (get_server_value(private_data->lpEcb,
                         "ALL_HTTP", huge_buf, huge_buf_sz, "")) {
        unsigned int cnt = 0;
        char *tmp;

        for (tmp = huge_buf; *tmp; tmp++) {
            if (*tmp == '\n') {
                cnt++;
            }
        }

        if (cnt) {
            char *headers_buf = jk_pool_strdup(&private_data->p, huge_buf);
            unsigned int i;
            size_t len_of_http_prefix = strlen("HTTP_");
            BOOL need_content_length_header = (s->content_length == 0);

            cnt -= 2;           /* For our two special headers */
            /* allocate an extra header slot in case we need to add a content-length header */
            s->headers_names =
                jk_pool_alloc(&private_data->p, (cnt + 1) * sizeof(char *));
            s->headers_values =
                jk_pool_alloc(&private_data->p, (cnt + 1) * sizeof(char *));

            if (!s->headers_names || !s->headers_values || !headers_buf) {
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
                else if (need_content_length_header &&
                         !strnicmp(tmp, CONTENT_LENGTH,
                                   strlen(CONTENT_LENGTH))) {
                    need_content_length_header = FALSE;
                    s->headers_names[i] = tmp;
                }
                else if (!strnicmp(tmp, TOMCAT_TRANSLATE_HEADER_NAME,
                                   strlen(TOMCAT_TRANSLATE_HEADER_NAME))) {
                    tmp += 6;   /* TOMCAT */
                    s->headers_names[i] = tmp;
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
            return JK_FALSE;
        }
    }
    else {
        return JK_FALSE;
    }

    return JK_TRUE;
}

static int get_server_value(LPEXTENSION_CONTROL_BLOCK lpEcb,
                            char *name, char *buf, DWORD bufsz, char *def_val)
{
    if (!lpEcb->GetServerVariable(lpEcb->ConnID,
                                  name, buf, (LPDWORD) &bufsz)) {
        strcpy(buf, def_val);
        return JK_FALSE;
    }

    if (bufsz > 0) {
        buf[bufsz - 1] = '\0';
    }

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
