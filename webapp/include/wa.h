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
 * @author  Pier Fumagalli <mailto:pier@betaversion.org>
 * @version $Id$
 */
#ifndef _WA_H_
#define _WA_H_

/* C standard includes */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef WIN32
#define vsnprintf _vsnprintf
#else
#include <unistd.h>
#endif /* ifdef WIN32 */

/* APR Library includes */
#include <apr_general.h>
#include <apr_pools.h>
#include <apr_strings.h>
#include <apr_tables.h>
#include <apr_tables.h>
#include <apr_time.h>
#include <apr_network_io.h>
#include <apr_file_info.h>
#if APR_HAS_THREADS
#include <apr_thread_mutex.h>
#include <apr_atomic.h>
#endif

/* WebApp Library type definitions. */
typedef enum {
    wa_false,
    wa_true,
} wa_boolean;

typedef struct wa_chain wa_chain;

typedef struct wa_application wa_application;
typedef struct wa_virtualhost wa_virtualhost;
typedef struct wa_connection wa_connection;

typedef struct wa_request wa_request;
typedef struct wa_ssldata wa_ssldata;
typedef struct wa_hostdata wa_hostdata;
typedef struct wa_handler wa_handler;

typedef struct wa_provider wa_provider;

/* All declared providers */
extern wa_provider wa_provider_info;
extern wa_provider wa_provider_warp;
/*extern wa_provider wa_provider_jni;*/

/* WebApp Library includes */
#include <wa_main.h>
#include <wa_config.h>
#include <wa_request.h>
#include <wa_version.h>

/* Debugging mark */
#define WA_MARK __FILE__,__LINE__

#endif /* ifndef _WA_H_ */
