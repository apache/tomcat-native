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
 * Description: AOLserver plugin for Jakarta/Tomcat                         
 * Author:      Alexander Leykekh, aolserver@aol.net 
 * Version:     $Revision$                                           
 */

#ifndef JK_NS_H
#define JK_NS_H

#include "apu_compat.h"
#include "apr_lib.h"
#include "apr_date.h"
#include "apr_strings.h"
#include "apr_pools.h"
#include "apr_tables.h"
#include "apr_hash.h"

#include "ns.h"

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_env.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_workerEnv.h"
#include "jk_uriMap.h"
#include "jk_requtil.h"

extern int Ns_ModuleVersion;

int Ns_ModuleInit(char *server, char *module);

int JK_METHOD jk2_service_ns_init(jk_env_t *env, jk_ws_service_t *s, char* serverNameOverride);

int JK_METHOD jk2_logger_ns_factory(jk_env_t *env, jk_pool_t *pool,
                                         jk_bean_t *result, const char *type, const char *name);

/* from ../../common/jk_pool_apr.c */
int JK_METHOD jk2_pool_apr_factory(jk_env_t *env, jk_pool_t *pool,
                                   jk_bean_t *result, const char *type, const char *name);

int JK_METHOD jk2_map_aprtable_factory(jk_env_t *env, jk_pool_t *pool,
                                       jk_bean_t *result,
                                       const char *type, const char *name);

#endif /* JK_NS_H */
