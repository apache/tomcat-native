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
static void jk2_env_initEnv( jk_env_t *env, char *id );

/* XXX We should have one env per thread to avoid sync problems. 
   The env will provide access to pools, etc 
*/

jk_env_t* JK_METHOD jk2_env_getEnv( char *id, jk_pool_t *pool ) {
  if( jk_env_globalEnv == NULL ) {
      jk_env_globalEnv=(jk_env_t *)pool->calloc( NULL, pool, sizeof( jk_env_t ));
      jk_env_globalEnv->globalPool = pool;
      jk2_env_initEnv( (jk_env_t *)jk_env_globalEnv, id );
  }
  return jk_env_globalEnv;
}

/* ==================== Implementation ==================== */

static jk_env_t * JK_METHOD jk2_env_get( jk_env_t *env )
{
    return NULL;
}

static int JK_METHOD jk2_env_put( jk_env_t *parent, jk_env_t *chld )
{

    return JK_TRUE;
}

static jk_env_objectFactory_t JK_METHOD jk2_env_getFactory(jk_env_t *env, 
                                                           const char *type )
{
  if( type==NULL ) {
      env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "env.getFactory(): NullPointerException\n");
      return NULL;
  }

  return (jk_env_objectFactory_t)env->_registry->get( env, env->_registry, type);
}

static void *jk2_env_createInstance(jk_env_t *env, jk_pool_t *pool,
                                    const char *type, const char *name)
{
    jk_env_objectFactory_t fac;
    jk_bean_t *result;

    if( type==NULL  ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                        "env.createInstance(): NullPointerException\n");
        return NULL;
    }

    if( name==NULL ) {
        name=pool->calloc( env, pool, 10 );
        snprintf( (char *)name, 10, "%s.%d", type, env->id++ );
    }

    fac=jk2_env_getFactory( env, type);
    if( fac==NULL ) {
        if( env->l ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                            "Error getting factory for %s:%s\n", type, name);
        } else {
            fprintf( stderr, "Error getting factory for %s \n",  type );
        }
        return NULL;
    }

    result=(jk_bean_t *)pool->calloc( NULL, pool, sizeof( jk_bean_t ));
    result->type=type;
    result->name=name;
    result->settings=NULL;
    
    fac( env, pool, result, type, name );
    if( result->object==NULL ) {
        if( env->l )
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                                "Error getting instance for %s:%s\n", type, name);
        return NULL;
    }

    env->_objects->put( env, env->_objects, name, result, NULL );
    
    return result->object;
}

static void *jk2_env_getByName(jk_env_t *env, const char *name)
{
    jk_bean_t *result;

    if( name==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                        "env.getByName(): NullPointerException\n");
        return NULL;
    }

    
    result=(jk_bean_t *)env->_objects->get( env, env->_objects, name );
    if( result==NULL ) return NULL;
    
    return result->object;
}    

static jk_bean_t JK_METHOD *jk2_env_getBean(jk_env_t *env, const char *name)
{
    if( name==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                        "env.getByName(): NullPointerException\n");
        return NULL;
    }

    return (jk_bean_t *)env->_objects->get( env, env->_objects, name );
}    

static void JK_METHOD jk2_env_registerFactory(jk_env_t *env, 
                                             const char *type,
                                             jk_env_objectFactory_t fact)
{
    if( type==NULL || fact==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                        "env.registerFactory(): NullPointerException\n");
        return;
    }
    env->_registry->put( env, env->_registry, type, fact, NULL );
}

static void jk2_env_initEnv( jk_env_t *env, char *id ) {
  /*   env->logger=NULL; */
  /*   map_alloc( & env->properties ); */
  env->registerFactory= jk2_env_registerFactory;
  env->getByName= jk2_env_getByName; 
  env->getMBean= jk2_env_getBean; 
  env->createInstance= jk2_env_createInstance;
  env->id=0;
  jk2_map_default_create( env, & env->_registry, env->globalPool );
  jk2_map_default_create( env, & env->_objects, env->globalPool );
  jk2_registry_init(env);
}









