/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */

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
 * @author:      Gal Shachor <shachor@il.ibm.com>                           *
 * @author:      Henri Gomez <hgomez@slib.fr>                               *
 * @author:      Costin Manolache
 *  
 */

int JK_METHOD jk2_worker_ajp14_factory(jk_env_t *env, jk_pool_t *pool,
                                       jk_bean_t *result,
                                       const char *type, const char *name);

                                   
int JK_METHOD jk2_worker_lb_factory(jk_env_t *env, jk_pool_t *pool,
                                    jk_bean_t *result,
                                    const char *type, const char *name);


int JK_METHOD jk2_worker_jni_factory(jk_env_t *env, jk_pool_t *pool,
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

int JK_METHOD jk2_worker_ajp12_factory(jk_env_t *env, jk_pool_t *pool,
                                       jk_bean_t *result,
                                       const char *type, const char *name);

/* Factories for 'new' types. We use the new factory interface,
 *  workers will be updated later 
 */
#ifdef HAS_APR
int JK_METHOD jk2_channel_apr_socket_factory(jk_env_t *env, jk_pool_t *pool,
                                             jk_bean_t *result,
					                         const char *type, const char *name);
#endif

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

int JK_METHOD jk2_config_factory(jk_env_t *env, jk_pool_t *pool, jk_bean_t *result,
                                 const char *type, const char *name);

