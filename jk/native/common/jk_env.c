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
 
#include "jk_env.h"

/* Singleton for now */
jk_env_t *jk_env_singleton;

/* Private methods 
*/
static jk_env_initEnv( jk_env_t *env, char *id );
static jk_env_objectFactory_t *jk_env_getFactory(jk_env_t *env, 
						 char *type,
						 char *name );
static void jk_env_registerFactory(jk_env_t *env, 
				   char *type,
				   char *name, 
				   jk_env_objectFactory_t fact);

/* XXX We should have one env per thread to avoid sync problems. 
   The env will provide access to pools, etc 
*/

jk_env_t* JK_METHOD jk_env_getEnv( char *id ) {
  if( jk_env_singleton == NULL ) {
    jk_env_singleton=(jk_env_t *)malloc( sizeof( jk_env_t ));
    jk_env_initEnv( (jk_env_t *)jk_env_singleton, id );
  }
  return jk_env_singleton;
}

/* ==================== Implementation ==================== */

static jk_env_initEnv( jk_env_t *env, char *id ) {
  /*   env->logger=NULL; */
  /*   map_alloc( & env->properties ); */
  env->getFactory= (void *)jk_env_getFactory; 
  env->registerFactory= (void *)jk_env_registerFactory; 
  map_alloc( & env->_registry);
  jk_registry_init(env);

}

static jk_env_objectFactory_t *jk_env_getFactory(jk_env_t *env, 
						 char *type,
						 char *name )
{
  jk_env_objectFactory_t *result;

  if( type==NULL || name==NULL ) {
    // throw NullPointerException
    return NULL;
  }

  /** XXX add check for the type */
  result=(jk_env_objectFactory_t *)map_get( env->_registry, name, NULL );
  return result;
}

static void jk_env_registerFactory(jk_env_t *env, 
 				  char *type,
				  char *name, 
				  jk_env_objectFactory_t fact)
{
  void *old;
  
  map_put( env->_registry, name, fact, &old );
}








