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

#ifndef IIS_H
#define IIS_H

#define _WIN32_WINNT 0x0400

#include <httpext.h>
#include <httpfilt.h>
#include <wininet.h>

#include "jk_global.h"
//#include "jk_util.h"
#include "jk_pool.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */


#define VERSION_STRING "Jakarta/ISAPI/2.0Dev"

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
#define URI_HEADER_NAME          ("TOMCATURI:")
#define QUERY_HEADER_NAME        ("TOMCATQUERY:")
#define WORKER_HEADER_NAME       ("TOMCATWORKER:")
#define TOMCAT_TRANSLATE_HEADER_NAME ("TOMCATTRANSLATE:")
#define CONTENT_LENGTH           ("CONTENT_LENGTH:")

#define HTTP_URI_HEADER_NAME     ("HTTP_TOMCATURI")
#define HTTP_QUERY_HEADER_NAME   ("HTTP_TOMCATQUERY")
#define HTTP_WORKER_HEADER_NAME  ("HTTP_TOMCATWORKER")

#define SERVER_NAME              ("SERVER_NAME" )

#define SERVER_SOFTWARE          ("SERVER_SOFTWARE")

#define REGISTRY_LOCATION        ("Software\\Apache Software Foundation\\Jakarta Isapi Redirector\\2.0")
#define W3SVC_REGISTRY_KEY       ("SYSTEM\\CurrentControlSet\\Services\\W3SVC\\Parameters")

#define BAD_REQUEST             -1
#define BAD_PATH                -2
#define MAX_SERVERNAME          128

#define HTML_ERROR_400          "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">" \
                                "<HTML><HEAD><TITLE>Bad request!</TITLE></HEAD>" \
                                "<BODY><H1>Bad request!</H1><DL><DD>\n" \
                                "Your browser (or proxy) sent a request that " \
                                "this server could not understand.</DL></DD></BODY></HTML>"

#define HTML_ERROR_403          "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">" \
                                "<HTML><HEAD><TITLE>Access forbidden!!</TITLE></HEAD>" \
                                "<BODY><H1>Access forbidden!</H1><DL><DD>\n" \
                                "You don't have permission to access the requested object." \
                                "It is either read-protected or not readable by the server.</DL></DD></BODY></HTML>"


#define GET_SERVER_VARIABLE_VALUE(pool, name, place) {    \
    (place) = NULL;                                 \
    huge_buf_sz = sizeof(huge_buf);                 \
    if (get_server_value(lpEcb,                      \
                        (name),                     \
                        huge_buf,                   \
                        huge_buf_sz,                \
                        "")) {                      \
        (place) = (pool)->pstrdup(env,(pool),huge_buf);   \
    }   \
}

#define GET_SERVER_VARIABLE_VALUE_INT(name, place, def) {   \
    huge_buf_sz = sizeof(huge_buf);                 \
    if (get_server_value(lpEcb,                     \
                        (name),                     \
                        huge_buf,                   \
                        huge_buf_sz,                \
                        "")) {                      \
        (place) = atoi(huge_buf);                   \
        if (0 == (place)) {                         \
            (place) = def;                          \
        }                                           \
    } else {    \
        (place) = def;  \
    }           \
}


    static int JK_METHOD jk2_service_iis_head(jk_env_t *env,
                                              jk_ws_service_t *s);

    static int JK_METHOD jk2_service_iis_read(jk_env_t *env,
                                              jk_ws_service_t *s, void *b,
                                              unsigned len,
                                              unsigned *actually_read);

    static int JK_METHOD jk2_service_iis_write(jk_env_t *env,
                                               jk_ws_service_t *s,
                                               const void *b, unsigned l);

    static int JK_METHOD jk2_service_iis_init_ws_service(struct jk_env *env,
                                                         jk_ws_service_t
                                                         *_this,
                                                         struct jk_worker *w,
                                                         void *serverObj);

    int jk2_service_iis_init(jk_env_t *env, jk_ws_service_t *s);

    int get_server_value(LPEXTENSION_CONTROL_BLOCK lpEcb,
                         char *name, char *buf, DWORD bufsz, char *def_val);

    DWORD WINAPI HttpExtensionProcWorker(LPEXTENSION_CONTROL_BLOCK lpEcb,
                                         jk_ws_service_t *service);

    int jk2_iis_init_pool(jk_env_t *env);
    int jk2_iis_thread_pool(LPEXTENSION_CONTROL_BLOCK lpEcb);
    int jk2_iis_close_pool(jk_env_t *env);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif
