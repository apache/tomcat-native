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
    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_MAP_H */
