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
#include "jk_objCache.h"

jk_env_t *jk_env_globalEnv;

/* Private methods 
*/
static void jk_env_initEnv( jk_env_t *env, char *id );

/* XXX We should have one env per thread to avoid sync problems. 
   The env will provide access to pools, etc 
*/

jk_env_t* JK_METHOD jk_env_getEnv( char *id, jk_pool_t *pool ) {
  if( jk_env_globalEnv == NULL ) {
      jk_env_globalEnv=(jk_env_t *)pool->calloc( NULL, pool, sizeof( jk_env_t ));
      jk_env_globalEnv->globalPool = pool;
      jk_env_initEnv( (jk_env_t *)jk_env_globalEnv, id );
  }
  return jk_env_globalEnv;
}

/* ==================== Implementation ==================== */

static jk_env_t * JK_METHOD jk_env_get( jk_env_t *env )
{
    return NULL;
}

static int JK_METHOD jk_env_put( jk_env_t *parent, jk_env_t *chld )
{

    return JK_TRUE;
}

static jk_env_objectFactory_t JK_METHOD jk_env_getFactory(jk_env_t *env, 
                                                          const char *type,
                                                          const char *name )
{
  jk_env_objectFactory_t result;
  /* malloc/free: this is really temporary, and is executed only at setup
     time, not during request execution. We could create a subpool or alloc
     from stack */
  char *typeName=
      (char *)malloc( (strlen(name) + strlen(type) + 2 ) * sizeof( char ));

  strcpy(typeName, type );
  strcat( typeName, "/" );
  strcat( typeName, name );

  if( type==NULL || name==NULL ) {
    // throw NullPointerException
    return NULL;
  }

  /** XXX add check for the type */
  result=(jk_env_objectFactory_t)env->_registry->get( env, env->_registry,
                                                      typeName);
  free( typeName );
  return result;
}

static void *jk_env_getInstance( jk_env_t *_this, jk_pool_t *pool,
                                 const char *type, const char *name )
{
    jk_env_objectFactory_t fac;
    void *result;

    /* prevent core... */
    if (name==NULL)
        return(NULL);

    fac=_this->getFactory( _this, type, name);
    if( fac==NULL ) {
        if( _this->l )
            _this->l->jkLog(_this, _this->l, JK_LOG_ERROR,
                            "Error getting factory for %s:%s\n", type, name);
        return NULL;
    }

    fac( _this, pool, &result, type, name );
    if( result==NULL ) {
        if( _this->l )
            _this->l->jkLog(_this, _this->l, JK_LOG_ERROR,
                                "Error getting instance for %s:%s\n", type, name);
        return NULL;
    }
    
    return result;
}


static void JK_METHOD jk_env_registerFactory(jk_env_t *env, 
                                             const char *type,
                                             const char *name, 
                                             jk_env_objectFactory_t fact)
{
  void *old;
  char *typeName;
  int size=( sizeof( char ) * (strlen(name) + strlen(type) + 2 ));

  typeName=(char *)env->globalPool->calloc(env, env->globalPool, size);


  strcpy(typeName, type );
  strcat( typeName, "/" );
  strcat( typeName, name );
  env->_registry->put( env, env->_registry, typeName, fact, &old );
}

static void jk_env_initEnv( jk_env_t *env, char *id ) {
  /*   env->logger=NULL; */
  /*   map_alloc( & env->properties ); */
  env->getFactory= jk_env_getFactory; 
  env->registerFactory= jk_env_registerFactory;
  env->getInstance= jk_env_getInstance; 
  jk_map_default_create( env, & env->_registry, env->globalPool );
  jk_registry_init(env);
}









