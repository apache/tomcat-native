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

/**
 * Manages the request mappings. It includes the internal mapper and all
 * properties associated with a location ( or virtual host ). The information
 * is set using:
 *   - various autoconfiguration mechanisms.
 *   - uriworkers.properties
 *   - JkMount directives
 *   - <SetHandler> and apache specific directives.
 *   - XXX workers.properties-like directives ( for a single config file )
 *   - other server-specific directives
 *
 * The intention is to allow the user to use whatever is more comfortable
 * and fits his needs. For 'basic' configuration the autoconf will be enough,
 * server-specific configs are the best for fine-tunning, properties are
 * easy to generate and edit.
 *
 *
 * Author: Gal Shachor <shachor@il.ibm.com>
 * author: Costin Manolache
 */
#ifndef JK_URIMAP_H
#define JK_URIMAP_H

#include "jk_global.h"
#include "jk_env.h"
#include "jk_logger.h"
#include "jk_uriEnv.h"
#include "jk_map.h"
#include "jk_pool.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct jk_uriMap;
struct jk_map;
struct jk_env;
struct jk_pool;
    
typedef struct jk_uriMap jk_uriMap_t;

struct jk_uriMap {
    struct jk_bean *mbean;

    /* All mappings */
    struct jk_map *maps;

    struct jk_workerEnv *workerEnv;

    /* Virtual host map. For each host and alias there is one
     * entry, the value is a uriEnv that corresponds to the vhost top
     * level.
     */
    struct jk_map *vhosts;

    /* Virtual host map cache. Once processed the mapped host
     * will be cached for performance reasons.
     */
    struct jk_map *vhcache;

    /* ---------- Methods ---------- */

    /** Initialize the map. This should be called after all workers
        were added. It'll check if mappings have valid workers.
    */
    int (*init)( struct jk_env *env, jk_uriMap_t *_this);

    void (*destroy)( struct jk_env *env, jk_uriMap_t *_this );
    
    int (*addUriEnv)(struct jk_env *env,
                     struct jk_uriMap *uriMap,
                     struct jk_uriEnv *uriEnv);

    /** Check the uri for potential security problems
     */
    int (*checkUri)( struct jk_env *env, jk_uriMap_t *_this,
                     const char *uri );

    /** Mapping the uri. To be thread safe, we need to pass a pool.
        Or even better, create the jk_service structure already.
        mapUri() can set informations on it as well.
        
        MapUri() method should behave exactly like the native apache2
        mapper - we need it since the mapping rules for servlets are
        different ( or we don't know yet how to 'tweak' apache config
        to do what we need ). Even when we'll know, uriMap will be needed
        for other servers. 
    */

    struct jk_uriEnv *(*mapUri)(struct jk_env *env, jk_uriMap_t *_this,
                                const char *vhost,
                                int port,
                                const char *uri);
    
    /* -------------------- @deprecated -------------------- */
    /* used by the mapper, temp storage ( ??? )*/

    /* pool for mappings. Mappings will change at runtime, we can't
     * recycle the main pool.
    */
    struct jk_pool  *pool;
};
    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_URI_MAP_H */
