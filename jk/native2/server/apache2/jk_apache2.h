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
 * Description: Apache 2 plugin for Jakarta/Tomcat                         
 * Author:      Gal Shachor <shachor@il.ibm.com>                           
 *              Henri Gomez <hgomez@apache.org>                            
 * Version:     $Revision$                                           
 */

#ifndef JK_APACHE2_H
#define JK_APACHE2_H

#include "ap_config.h"
#include "apr_lib.h"
#include "apr_date.h"
#include "apr_strings.h"
#include "apr_pools.h"
#include "apr_tables.h"
#include "apr_hash.h"

#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_env.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_workerEnv.h"
#include "jk_uriMap.h"
#include "jk_requtil.h"

/* changed with apr 1.0 */
#include "apr_version.h"
#if (APR_MAJOR_VERSION < 1)
#define apr_filepath_name_get apr_filename_of_pathname
#define apr_pool_parent_get apr_pool_get_parent
#endif

extern module AP_MODULE_DECLARE_DATA jk2_module;

int JK_METHOD jk2_service_apache2_init(jk_env_t *env, jk_ws_service_t *s);

int JK_METHOD jk2_logger_apache2_factory(jk_env_t *env, jk_pool_t *pool,
                                         jk_bean_t *result, const char *type,
                                         const char *name);

int JK_METHOD jk2_pool_apr_factory(jk_env_t *env, jk_pool_t *pool,
                                   jk_bean_t *result, const char *type,
                                   const char *name);

int JK_METHOD jk2_map_aprtable_factory(jk_env_t *env, jk_pool_t *pool,
                                       jk_bean_t *result,
                                       const char *type, const char *name);

#endif /* JK_APACHE2_H */
