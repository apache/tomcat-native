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

#ifndef JK_ENV_H
#define JK_ENV_H


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_map.h"
#include "jk_worker.h"

/** 
 *  Common environment for all jk functions. Provide uniform
 *  access to pools, logging, factories and other 'system' services.
 * 
 * 
 *
 * ( based on jk_worker.c, jk_worker_list.c )
 * @author Gal Shachor <shachor@il.ibm.com>                           
 * @author Henri Gomez <hgomez@slib.fr>                               
 * @author Costin Manolache
 * 
 */
struct jk_env;
typedef struct jk_env jk_env_t;

/**
 * Factory used to create all jk objects. Factories are registered with 
 * jk_env_registerFactory ( or automatically - LATER ), and created
 * with jk_env_getFactory.
 * 
 * Essentially, an abstract base class (or factory class) with a single
 * method -- think of it as createWorker() or the Factory Method Design
 * Pattern.  There is a different worker_factory function for each of the
 * different types of workers.  The set of all these functions is created
 * at startup from the list in jk_worker_list.h, and then the correct one
 * is chosen in jk_worker.c->wc_create_worker().  See jk_worker.c and
 * jk_ajp13_worker.c/jk_ajp14_worker.c for examples.
 *
 * This allows new workers to be written without modifing the plugin code
 * for the various web servers (since the only link is through
 * jk_worker_list.h).  
 */
typedef int (JK_METHOD *jk_env_objectFactory_t)(jk_env_t *env,
					      void **result, 
					      char *type,
					      char *name);

/** Get a pointer to the jk_env. We could support multiple 
 *  env 'instances' in future - for now it's a singleton.
 */
jk_env_t* JK_METHOD jk_env_getEnv( char *id );


/**
 *  The env will be used in a similar way with the JniEnv, to provide 
 *  access to various low level services ( pool, logging, system properties )
 *  in a consistent way. In time we should have all methods follow 
 *  the same pattern, with env as a first parameter, then the object ( this ) and 
 *  the other methods parameters.  
 */
struct jk_env {
  /*   jk_logger_t *logger; */
  /*jk_pool_t   global_pool; */

  /** Global properties ( similar with System properties in java)
   */
  /*   jk_map_t *properties; */
  /** Factory to create an instance of a particular type.
   * 
   */
  jk_env_objectFactory_t *
  (JK_METHOD *getFactory)( jk_env_t *env, char *type, char *name );


  /** Register a factory for a type ( channel, worker ).
   */
  void (JK_METHOD *registerFactory)( jk_env_t *env, char *type, char *name, 
				     jk_env_objectFactory_t factory);
  

  /** (Future)
   *  Register a factory for a type ( channel, worker ), using a .so 
   *  model.
   */
  void (JK_METHOD *registerExternalFactory)( jk_env_t *env, char *type, 
					     char *name, char *dll,
					     char *factorySymbol);

  /* private */
  jk_map_t *_registry;

};

void JK_METHOD jk_registry_init(jk_env_t *env);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif 
