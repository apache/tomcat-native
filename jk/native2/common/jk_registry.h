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

#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_service.h"
#include "jk_env.h"

/***************************************************************************
 * Description: Worker list                                                *
 * Version:     $Revision$                                           *
 ***************************************************************************/

/** Static declarations for all 'hardcoded' modules. This is a hack, 
 *  needed to initialize the static 'type system' for jk. 
 *
 *  A better solution would use 'introspection' and dlsyms to locate the 
 *  factory, using the common naming schema. This is a bit harder to implement
 *  ( we can do it later ) and may have problems on some buggy OSs ( I assume
 *  most decent systems support .so/.dll and symbols, but you can never know )
 * 
 *  Both solutions could be used in paralel - for start we'll use this
 *  hack and static registration. Don't put anything else here, just 
 *  static declarations for the factory methods and the registrerFactory() call.
 * 
 * (based on jk_worker_list.h )
 * @author:      Gal Shachor <shachor@il.ibm.com>
 * @author:      Henri Gomez <hgomez@apache.org>
 * @author:      Costin Manolache
 *  
 */

int JK_METHOD jk2_worker_ajp13_factory(jk_env_t *env, jk_pool_t *pool,
                                       jk_bean_t *result,
                                       const char *type, const char *name);

                                   
int JK_METHOD jk2_worker_lb_factory(jk_env_t *env, jk_pool_t *pool,
                                    jk_bean_t *result,
                                    const char *type, const char *name);


int JK_METHOD jk2_worker_jni_factory(jk_env_t *env, jk_pool_t *pool,
                                     jk_bean_t *result,
                                     const char *type, const char *name);

int JK_METHOD jk2_vm_factory(jk_env_t *env, jk_pool_t *pool,
                                     jk_bean_t *result,
                                     const char *type, const char *name);

int JK_METHOD jk2_channel_jni_factory(jk_env_t *env, jk_pool_t *pool,
                                      jk_bean_t *result,
                                      const char *type, const char *name);

int JK_METHOD jk2_worker_status_factory(jk_env_t *env, jk_pool_t *pool,
                                        jk_bean_t *result,
                                        const char *type, const char *name);

int JK_METHOD jk2_worker_run_factory(jk_env_t *env, jk_pool_t *pool,
                                     jk_bean_t *result,
                                     const char *type, const char *name);

int JK_METHOD jk2_endpoint_factory(jk_env_t *env, jk_pool_t *pool,
                                     jk_bean_t *result,
                                     const char *type, const char *name);

int JK_METHOD jk2_worker_ajp12_factory(jk_env_t *env, jk_pool_t *pool,
                                       jk_bean_t *result,
                                       const char *type, const char *name);

int JK_METHOD jk2_channel_un_factory(jk_env_t *env, jk_pool_t *pool,
                                     jk_bean_t *result,
                                     const char *type, const char *name);

/* Factories for 'new' types. We use the new factory interface,
 *  workers will be updated later 
 */
int JK_METHOD jk2_channel_apr_socket_factory(jk_env_t *env, jk_pool_t *pool,
                                             jk_bean_t *result,
                                             const char *type, const char *name);

int JK_METHOD jk2_shm_factory(jk_env_t *env, jk_pool_t *pool,
                              jk_bean_t *result,
                              const char *type, const char *name);


int JK_METHOD jk2_channel_jni_factory(jk_env_t *env, jk_pool_t *pool,
                                      jk_bean_t *result,
                                      const char *type, const char *name);

int JK_METHOD jk2_channel_socket_factory(jk_env_t *env, jk_pool_t *pool,
                                        jk_bean_t *result,
					                    const char *type, const char *name);

int JK_METHOD jk2_workerEnv_factory(jk_env_t *env, jk_pool_t *pool,
                                    jk_bean_t *result,
                                    const char *type, const char *name);

int JK_METHOD jk2_logger_file_factory(jk_env_t *env, jk_pool_t *pool,
                                      jk_bean_t *result,
                                      const char *type, const char *name);


int JK_METHOD jk2_handler_logon_factory(jk_env_t *env, jk_pool_t *pool,
                                        jk_bean_t *result,
                                        const char *type, const char *name);

int JK_METHOD jk2_handler_response_factory(jk_env_t *env, jk_pool_t *pool,
                                           jk_bean_t *result,
                                           const char *type, const char *name);

int JK_METHOD jk2_uriMap_factory(jk_env_t *env, jk_pool_t *pool, jk_bean_t *result,
                                 const char *type, const char *name);

int JK_METHOD jk2_uriEnv_factory(jk_env_t *env, jk_pool_t *pool, jk_bean_t *result,
                                 const char *type, const char *name);

int JK_METHOD jk2_config_file_factory(jk_env_t *env, jk_pool_t *pool, jk_bean_t *result,
                                 const char *type, const char *name);

int JK_METHOD jk2_logger_win32_factory(jk_env_t *env, jk_pool_t *pool,
                                      jk_bean_t *result,
                                      const char *type, const char *name);

int JK_METHOD jk2_mutex_thread_factory( jk_env_t *env ,jk_pool_t *pool,
                                        jk_bean_t *result,
                                        const char *type, const char *name);

int JK_METHOD jk2_mutex_proc_factory( jk_env_t *env ,jk_pool_t *pool,
                                      jk_bean_t *result,
                                      const char *type, const char *name);

int JK_METHOD jk2_signal_factory( jk_env_t *env ,jk_pool_t *pool,
                                  jk_bean_t *result,
                                  const char *type, const char *name);

int JK_METHOD jk2_user_factory( jk_env_t *env ,jk_pool_t *pool,
                                jk_bean_t *result,
                                const char *type, const char *name);
