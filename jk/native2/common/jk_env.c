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

/* We should have one env per thread to avoid sync problems. 
   The env provides access to tmp pools, exception 
*/

/* -------------------- Env management -------------------- */

/** Public method, creates/get the global env
 */
jk_env_t* JK_METHOD jk2_env_getEnv( char *id, jk_pool_t *pool ) {
  if( jk_env_globalEnv == NULL ) {
      if( pool == NULL ) return NULL;
      jk_env_globalEnv=(jk_env_t *)pool->calloc( NULL, pool, sizeof( jk_env_t ));
      jk_env_globalEnv->globalPool = pool;
      jk2_env_initEnv( (jk_env_t *)jk_env_globalEnv, id );
      fprintf( stderr, "env: top level env %p\n", jk_env_globalEnv);
  }
  return jk_env_globalEnv;
}


/** Get a local env - either a new one or a recycled one
 *  XXX Try TLD too 
 */
static jk_env_t * JK_METHOD jk2_env_get( jk_env_t *parentEnv )
{
    jk_env_t *env=(jk_env_t *)parentEnv->envCache->get( parentEnv, parentEnv->envCache);
    if( env == NULL ) {
        jk_pool_t *parentPool=parentEnv->globalPool;
        env=(jk_env_t *)parentPool->calloc( parentEnv, parentPool, sizeof( jk_env_t ));
        
        env->tmpPool=parentPool->create(parentEnv, parentPool, HUGE_POOL_SIZE);;
        
        env->registerFactory= parentEnv->registerFactory;
        env->getByName= parentEnv->getByName; 
        env->getByName2= parentEnv->getByName2; 
        env->getBean2= parentEnv->getBean2; 
        env->getBean= parentEnv->getBean; 
        env->alias= parentEnv->alias; 
        env->createBean2= parentEnv->createBean2;
        env->createBean= parentEnv->createBean;
        env->getEnv= parentEnv->getEnv; 
        env->releaseEnv= parentEnv->releaseEnv; 
        env->jkClearException=parentEnv->jkClearException;
        env->jkException=parentEnv->jkException;
        
        env->_registry=parentEnv->_registry;
        env->_objects=parentEnv->_objects;
        env->l=parentEnv->l;
        env->globalPool=parentEnv->globalPool;
        env->envCache=parentEnv->envCache;

        fprintf( stderr, "env:Create child env %p %p\n", parentEnv, env);
    }
    return env;
}

/** Release the env ( clean and recycle )
 */
static int JK_METHOD jk2_env_recycleEnv( jk_env_t *env )
{
    env->tmpPool->reset(env, env->tmpPool);
    env->jkClearException(env);
    return JK_OK;
}

/** Release the env ( clean and recycle )
 */
static int JK_METHOD jk2_env_put( jk_env_t *parent, jk_env_t *chld )
{
    jk2_env_recycleEnv( chld );
    return parent->envCache->put( parent, parent->envCache, chld);
}

/* -------------------- Object management -------------------- */

/** Create a jk component, using only the name.
 *  Now things are simpler - the 'type' is the prefix, separated by ':' - no
 *  guessing involved.
 */
static jk_bean_t *jk2_env_createBean( jk_env_t *env, jk_pool_t *pool, char *objName )
{
    char *type=NULL;
//    void *obj;
    char *localName;

    localName=strchr( objName, ':' );
    if( localName==NULL ) {
        type=objName;
    } else {
        /* Funny pointer arithmetic. I hope I got it right */
        type=env->tmpPool->calloc( env, env->tmpPool, localName - objName + 2 );
        strncpy( type, objName, localName - objName );
    }
    
    return env->createBean2( env, pool, type, objName );
}

/** Create a component using type and local part ( pre-cooked ).
 */
static jk_bean_t *jk2_env_createBean2( jk_env_t *env, jk_pool_t *pool,
                                       char *type, char *localName )
{
    jk_env_objectFactory_t fac;
    jk_bean_t *result=NULL;
    jk_pool_t *workerPool;
    char *name;

    if( localName!=NULL ) 
        result=env->getBean2( env, type, localName );
    if( result!=NULL )
        return result;
    
    if( pool==NULL ) {
        pool=env->globalPool;
    }

    if( type==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "env.createBean2(): NullPointerException\n");
        return NULL;
    }

    if( localName!=NULL && strncmp( localName, type, strlen( type )) == 0 ) {
        /* Common error, make it 'localName' */
        if( strcmp( type, localName ) == 0 ) {
            localName="";
        } else {
            localName= localName + strlen(type) + 1;
        }
    }

    if( env->l != NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "env.createBean2(): Create [%s] %s\n", type, localName);
    } else {
            fprintf(stderr, "env.createBean2(): Create [%s] %s\n", type, localName);
    }
    
    fac=(jk_env_objectFactory_t)env->_registry->get( env, env->_registry, type);
    
    if( fac==NULL ) {
        if( env->l ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "env.createBean2(): Error getting factory for [%s] %s\n", type, localName);
        } else {
            fprintf( stderr, "Error getting factory for %s \n",  type );
        }
        return NULL;
    }

    workerPool=pool->create(env, pool, HUGE_POOL_SIZE);
    
    /** Generate a unique name if none is specified
     */
    if( localName==NULL ) {
        localName=workerPool->calloc( env, workerPool,  10 );
        sprintf( (char *)localName, "%d", env->id++ );
    }

    name=workerPool->calloc( env, workerPool,  strlen( type ) + strlen( localName ) + 2 );
    strcpy( name, type );
    strcat( name, ":");
    strcat( name, localName );
    
    result=(jk_bean_t *)workerPool->calloc( env, workerPool, sizeof( jk_bean_t ));
    result->pool=workerPool;
    result->type= workerPool->pstrdup( env, workerPool, type);
    result->name= workerPool->pstrdup( env, workerPool, name);
    result->localName=workerPool->pstrdup( env, workerPool, localName);
    result->settings=NULL;
    result->getAttributeInfo=NULL;
    result->setAttributeInfo=NULL;
    
    fac( env, workerPool, result, result->type, result->name );

    if( result->object==NULL ) {
        if( env->l ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "env.createBean2(): Factory error creating %s ( %s, %s)\n", name,
                          type, localName);
        } else {
            fprintf(stderr, "env.createBean2(): Factory error creating %s ( %s, %s)\n", name,
                    type, localName);
        }
        return NULL;
    }

    fprintf(stderr,"env.createBean2(): register %s %p\n", result->name, result->object);

    jk_env_globalEnv->_objects->put( env, jk_env_globalEnv->_objects, result->name, result, NULL );

    if( strcmp(localName,"")==0 ) {
        /* "" for local name is used as 'default'. Allow "type" as an alias for "type:"
         */
        jk_env_globalEnv->_objects->put( env, jk_env_globalEnv->_objects, result->type, result, NULL );
    }

    return result;
}

/** Define an alias, for simpler config / less typing
 */
static void JK_METHOD jk2_env_alias(jk_env_t *env, const char *name, const char *alias)
{
    jk_bean_t *jkb=env->getBean(env, name);

    if( jkb==NULL ) {
        if( env->l==NULL ) {
            fprintf(stderr,"env.alias(): Not found %s\n", name);
        } else {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "env.alias(): Not found %s\n", name);
        }
        return ;
    }
    
    fprintf(stderr,"env.alias(): alias %s %s\n", name, alias);
    jk_env_globalEnv->_objects->put( env, jk_env_globalEnv->_objects, alias, jkb, NULL );
}

/** Get the object by name. Returns the real object, not the wrapper
 */
static void * JK_METHOD jk2_env_getByName(jk_env_t *env, const char *name)
{
    jk_bean_t *result=env->getBean( env, name );
        
    if( result==NULL ) {
        fprintf(stderr,"env.alias(): Can't find %p %s\n", env, name);
        return NULL;
    }
    
    fprintf(stderr,"env.alias(): Get by name %s %p\n", name, result->object);
    return result->object;
}    

static void* JK_METHOD jk2_env_getByName2(jk_env_t *env, const char *type, const char *localName)
{
    jk_bean_t *result = env->getBean2( env, type, localName);
    if( result==NULL ) return NULL;
    return result->object;
}    

/** Get the wrapper for the named object
 */
static jk_bean_t* JK_METHOD jk2_env_getBean(jk_env_t *env, const char *name)
{
    if( name==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                        "env.getBean(): NullPointerException\n");
        return NULL;
    }

    return (jk_bean_t *)env->_objects->get( env, env->_objects, name );
}    

static jk_bean_t* JK_METHOD jk2_env_getBean2(jk_env_t *env, const char *type, const char *localName)
{
    char *name;
    if( type==NULL || localName==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                        "env.getBean2(): NullPointerException\n");
        return NULL;
    }

    name=env->tmpPool->calloc( env, env->tmpPool, strlen( type ) + strlen( localName ) +2 );
    strcpy(name, type);
    strcat( name,":");
    strcat( name,localName);

    return (jk_bean_t *)env->_objects->get( env, env->_objects, name );
}    

/** Register the type and the factory
 */
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

/* -------------------- Exceptions -------------------- */

/* Exceptions.
 */
static void JK_METHOD jkThrow( jk_env_t *env,
                               const char *file, int line,
                               const char *type,
                               const char *fmt, ... )
{
    va_list args;
//    char *buf;

    va_start(args, fmt);
    env->l->jkVLog(env, env->l, file, line, JK_LOG_ERROR_LEVEL, fmt, args );
    va_end(args);

}

/** re-throw the exception and record the current pos.
 *  in the stack trace
 *  XXX Not implemented/not used
 */
static void JK_METHOD jkReThrow( jk_env_t *env,
                                 const char *file, int line )
{
    /* Nothing yet. It should record the file/line for stack trace */
}

/* Last exception that occured
 */
static jk_exception_t* JK_METHOD  jk_env_jkException( jk_env_t *env ) 
{
    return env->lastException;
}

/** Clear the exception state
 */
static void JK_METHOD jk_env_jkClearException( jk_env_t *env ) {
    env->lastException=NULL;
}


/* -------------------- Init method -------------------- */


static void jk2_env_initEnv( jk_env_t *env, char *id ) {

    env->registerFactory= jk2_env_registerFactory;
    env->getByName= jk2_env_getByName; 
    env->getByName2= jk2_env_getByName2; 
    env->getBean= jk2_env_getBean; 
    env->getBean2= jk2_env_getBean2; 
    env->createBean2= jk2_env_createBean2;
    env->createBean= jk2_env_createBean;
    env->alias= jk2_env_alias;
    env->getEnv= jk2_env_get; 
    env->recycleEnv= jk2_env_recycleEnv; 
    env->releaseEnv= jk2_env_put; 

    env->jkClearException=jk_env_jkClearException;
    env->jkException=jk_env_jkException;

    env->id=0;
    
    jk2_map_default_create( env, & env->_registry, env->globalPool );
    jk2_map_default_create( env, & env->_objects, env->globalPool );
    
    env->envCache= jk2_objCache_create( env, env->globalPool  );
    env->envCache->init( env, env->envCache, 64 );
    env->envCache->maxSize=-1;
    env->tmpPool=env->globalPool->create(env, env->globalPool, HUGE_POOL_SIZE);;

    jk2_registry_init(env);
}




