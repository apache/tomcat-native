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
#include "jk_bean.h"

#define JK_LINE __FILE__,__LINE__


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
struct jk_pool;
struct jk_env;
struct jk_logger;
struct jk_map;
struct jk_bean;
typedef struct jk_env jk_env_t;

extern struct jk_env *jk_env_globalEnv;
    
/** Get a pointer to the jk_env. We could support multiple 
 *  env 'instances' in future - for now it's a singleton.
 */
jk_env_t* JK_METHOD jk2_env_getEnv( char *id, struct jk_pool *pool );

struct jk_exception {
    char *file;
    int line;

    char *type;
    char *msg;
    
    struct jk_exception *next;
};

typedef struct jk_exception jk_exception_t;
    
/**
 *  The env will be used in a similar way with the JniEnv, to provide 
 *  access to various low level services ( pool, logging, system properties )
 *  in a consistent way. In time we should have all methods follow 
 *  the same pattern, with env as a first parameter, then the object ( this )
 *  and the other methods parameters.  
 */
struct jk_env {
    struct jk_logger *l;
    struct jk_pool   *globalPool;

    /** Pool used for local allocations. It'll be reset when the
        env is released ( the equivalent of 'detach' ). Can be
        used for temp. allocation of small objects.
    */
    struct jk_pool *tmpPool;

    /* -------------------- Get/release ent -------------------- */
    
    /** Get an env instance. Must be called from each thread. The object
     *  can be reused in the thread, or it can be get/released on each used.
     *
     *  The env will store the exception status and the tmp pool - the pool will
     *  be recycled when the env is released, use it only for tmp things.
     */
    struct jk_env *(JK_METHOD *getEnv)(struct jk_env *parent);

    /** Release the env instance. The tmpPool will be recycled.
     */
    int (JK_METHOD *releaseEnv)(struct jk_env *parent, struct jk_env *chld);

    int (JK_METHOD *recycleEnv)(struct jk_env *env);

    /* -------------------- Exceptions -------------------- */
    
    /* Exceptions.
     *   TODO: create a 'stack trace' (i.e. a stack of errors )
     *   TODO: set 'error state'
     *  XXX Not implemented/not used
     */
    void (JK_METHOD *jkThrow)( jk_env_t *env,
                               const char *file, int line,
                               const char *type,
                               const char *fmt, ... );

    /** re-throw the exception and record the current pos.
     *  in the stack trace
     *  XXX Not implemented/not used
     */
    void (JK_METHOD *jkReThrow)( jk_env_t *env,
                                 const char *file, int line );

    /* Last exception that occured
     *  XXX Not implemented/not used
     */
    struct jk_exception *(JK_METHOD *jkException)( jk_env_t *env );

    /** Clear the exception state
     *  XXX Not implemented/not used
     */
    void (JK_METHOD *jkClearException)( jk_env_t *env );
    
    /* -------------------- Object management -------------------- */
    /* Register types, create instances, get by name */
    
    /** Create an object using the name. Use the : separated prefix as
     *  type. XXX This should probably replace createInstance.
     *
     *  @param parentPool  The pool of the parent. The object is created in its own pool,
     *                     but if the parent is removed all childs will be removed as well. Use a long
     *                     lived pool ( env->globalPool, workerEnv->pool ) if you don't want this.
     *  @param objName. It must follow the documented convention, with the type as prefix, then ':'
     */
    struct jk_bean *(*createBean)( struct jk_env *env, struct jk_pool *parentPool, char *objName );

    /** Same as createBean, but pass the split name
     */
    struct jk_bean *(*createBean2)( struct jk_env *env, struct jk_pool *parentPool,
                                    char *type, char *localName );
    
    /** Register an alias for a name ( like the local part, etc ), for simpler config.
     */
    void (JK_METHOD *alias)(struct jk_env *env, const char *name, const char *alias );
    
    /** Get an object by name, using the full name
     */
    void *
    (JK_METHOD *getByName)( struct jk_env *env, const char *name );

    /** Get an object by name, using the split name ( type + localName )
     */
    void *
    (JK_METHOD *getByName2)(struct jk_env *env, const char *type, const char *localName);

    /** Return the configuration object
     */
    struct jk_bean *
    (JK_METHOD *getBean)( struct jk_env *env, const char *name );
    
    /** Return the configuration object
     */
    struct jk_bean *
    (JK_METHOD *getBean2)( struct jk_env *env, const char *type, const char *localName );

    /** Register a factory for a type ( channel, worker ).
     */
    void (JK_METHOD *registerFactory)( jk_env_t *env, const char *type,
                                       jk_env_objectFactory_t factory);
    
    
    /* private */
    struct jk_map *_registry;
    struct jk_map *_objects;
    struct jk_objCache *envCache; 
    struct jk_exception *lastException;
    int id;
    int debug;
};

void JK_METHOD jk2_registry_init(jk_env_t *env);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif 
