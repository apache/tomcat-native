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

#ifndef JK_BEAN_H
#define JK_BEAN_H


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "jk_global.h"
#include "jk_env.h"
#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_map.h"
#include "jk_worker.h"

struct jk_pool;
struct jk_env;
struct jk_logger;
struct jk_map;
struct jk_bean;
typedef struct jk_bean jk_bean_t;

#define JK_STATE_DISABLED 0
#define JK_STATE_NEW 1
#define JK_STATE_INIT 2
    
/**
 * Factory used to create all jk objects. Factories are registered with 
 * jk2_env_registerFactory. The 'core' components are registered in
 * jk_registry.c
 *
 * Each jk component must be configurable using the setAttribute methods
 * in jk_bean. The factory is responsible to set up the config methods.
 *
 * The mechanism provide modularity and manageability to jk.
 */
typedef int (JK_METHOD *jk_env_objectFactory_t)(struct jk_env *env,
                                                struct jk_pool *pool,
                                                struct jk_bean *mbean, 
                                                const char *type,
                                                const char *name);

/** Each jk object will use this mechanism for configuration
 *
 *  Lifecycle:
 *  - object is created using env->createBean() or by jk_config ( if you want
 *    the object config to be saved )
 *
 *  - the name is parsed and the 'type' and 'localName' extracted.
 * 
 *  - 'type' will be looked up in registry, to find jk_env_objectFactory_t
 *
 *  - the factory method is called. It will create the object ( and eventually look
 *    up other objects )
 *
 *  - setAttribute() is called for each configured property.
 *
 *  - init() is called, after this the component is operational.
 *
 *  - destroy() should clean up any resources ( the pool and all objects allocated
 *    in the pool can be cleaned up automatically )
 */
struct jk_bean {
    /* Type of this object ( "channel.socket", "workerEnv", etc )
     */
    char *type;

    /* Full name of the object ( "channel.socket:localhost:8080" ).
     * Used to construct the object.
     */
    char *name;

    /* Local part of the name ( localhost:8080 )
     */
    char *localName;

    /* The wrapped object ( points to the real struct: jk_worker_t *, jk_channel_t *, etc )
     */
    void *object;

    /** Common information - if not 0 the component should print
     *  verbose information about its operation
    */
    int debug;
    
    int state;
    
    /* Common information - if set the component will not be
     * initialized or used
     */
    int disabled;

    /** Object 'version'. Used to detect changes in config.
     *  XXX will be set to the timestamp of the last config modification.
     */
    long ver;
    
    /** Unprocessed settings that are set on this bean by the config
        apis ( i.e. with $() in it ).

        It'll be != NULL for each component that was created or set using
        jk_config.

        This is what jk_config will save.
    */
    struct jk_map *settings;

    /* Object pool. The jk_bean and the object itself are created in this
     * pool. If this pool is destroyed or recycled, the object and all its
     * data are destroyed as well ( assuming the pool corectly cleans child pools
     * and object data are not created explicitely in a different pool ).
     *
     * Object should create sub-pools if they want to create/destroy long-lived
     * data, and env->tmpPool for data that is valid during the transaction.
     */
    struct jk_pool *pool;
    
    /* Temp - will change !*/

    /* Multi-value attributes. Must be declared so config knows how
       to save them. XXX we'll have to use a special syntax for that,
       the Preferences API and registry don't seem to support them.
    */
    char **multiValueInfo;

    /* Attributes supported by getAttribute method */
    char **getAttributeInfo;
    
    /* Attributes supported by setAttribute method */
    char **setAttributeInfo;
    
    /** Set a jk property. This is similar with the mechanism
     *  used by java side ( with individual setters for
     *  various properties ), except we use a single method
     *  and a big switch
     *
     *  As in java beans, setting a property may have side effects
     *  like changing the log level or reading a secondary
     *  properties file.
     *
     *  Changing a property at runtime will also be supported for
     *  some properties.
     *  XXX Document supported properties as part of
     *  workers.properties doc.
     *  XXX Implement run-time change in the status/ctl workers.
     */
    int  ( JK_METHOD *setAttribute)(struct jk_env *env, struct jk_bean *bean,
                                    char *name, void *value );

    void *( JK_METHOD *getAttribute)(struct jk_env *env, struct jk_bean *bean, char *name );

    /* Init the component
     */
    int (JK_METHOD *init)(struct jk_env *env, struct jk_bean *bean);

    int (JK_METHOD *destroy)(struct jk_env *env, struct jk_bean *bean );

};
    

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif 
