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
#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_md5.h"
#include "apr_network_io.h"
#include "apr_pools.h"
#include "apr_strings.h"
#include "apr_uri.h"
#include "apr_date.h"
#include "apr_fnmatch.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
 
#include "apr_hooks.h"
#include "apr_optional_hooks.h"
#include "apr_buckets.h"

#include "httpd_wrap.h"

#if APR_HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if APR_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

int main(int argc, const char * const * argv, const char * const *env)
{
    int rv = 0;

    apr_app_initialize(&argc, &argv, &env);

    /* This is done in httpd.conf using LogLevel debug directive.
     * We are setting this directly.
     */
    ap_default_loglevel = APLOG_DEBUG;

    ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, "Testing ajp...");


    ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, "%s", rv == 0 ? "OK" : "FAILED");
    apr_terminate();
}
