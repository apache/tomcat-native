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

#define JK_INVOKE_WITH_RESPONSE 1
    
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

    /** The index in the workerEnv table */
    int id;

    /** The index in the env object table */
    int objId;
    
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

    /** Called by the RPC-like protocol or the JNI bridge. Will unmarshal the arguments 
     *  and dispatch to the real method. This is similar with 'dynamic invocation'/introspection/
     *  'scripting support', etc. 
     */
    int (JK_METHOD *invoke)(struct jk_env *env, struct jk_bean *target,
                            struct jk_endpoint *ae, int code, struct jk_msg *msg, int raw);

};
    

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif 
