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

/***************************************************************************
 * Description: Map object header file                                     *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_MAP_H
#define JK_MAP_H

#include "jk_pool.h"
#include "jk_env.h"
#include "jk_logger.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct jk_logger;
struct jk_pool;
struct jk_map;
struct jk_env;
typedef struct jk_map jk_map_t;

/** Map interface. This include only the basic operations, and
 *   supports both name and index access.
 */
struct jk_map {

    void *(*get)(struct jk_env *env, struct jk_map *_this,
                 const char *name);

    /** Set the value, overriding previous values */
    int (*put)(struct jk_env *env, struct jk_map *_this,
               const char *name, void *value,
               void **oldValue);

    /** Multi-value support */
    int (*add)(struct jk_env *env, struct jk_map *_this,
               const char *name, void *value );

    /* Similar with apr_table, elts can be accessed by id
     */
    
    int (*size)(struct jk_env *env, struct jk_map *_this);
    
    char *(*nameAt)(struct jk_env *env, struct jk_map *m,
                    int pos);

    void *(*valueAt)(struct jk_env *env, struct jk_map *m,
                     int pos);

    /* Admin operations */
    void (*init)(struct jk_env *env, struct jk_map *m,
                 int initialSize, void *wrappedNativeObj);

    
    /* Empty the map, remove all values ( but it can keep
       allocated storage for [] )
    */
    void (*clear)(struct jk_env *env, struct jk_map *m);

    /* Sort the map, 
    */
    void (*sort)(struct jk_env *env, struct jk_map *m);

    struct jk_pool *pool;
    void *_private;

    /* For debuging purpose. NULL if not supported.
       The default impl will set them to the content
    */
    char **keys;
    void **values;
};

int jk2_map_default_create(struct jk_env *env, jk_map_t **m, 
                          struct jk_pool *pool); 

int jk2_map_read(struct jk_env *env, jk_map_t *m,const char *file);

char *jk2_map_concatKeys( struct jk_env *env, jk_map_t *map, char *delim );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_MAP_H */
