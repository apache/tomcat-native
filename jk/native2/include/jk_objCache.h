/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2003 The Apache Software Foundation.          *
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

#ifndef JK_OBJCACHE_H
#define JK_OBJCACHE_H

#include "jk_global.h"
#include "jk_env.h"
#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_msg.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct jk_env;
struct jk_objCache;
struct jk_logger;
struct jk_pool;
typedef struct jk_objCache jk_objCache_t;

#define JK_OBJCACHE_DEFAULT_SZ          (128)

    
jk_objCache_t *jk2_objCache_create(struct jk_env *env, struct jk_pool *pool );
    
/**
 * Simple object cache ( or pool for java people - don't confuse with the
 *  mem pool ).
 *
 * Used to avoid creating expensive objects, like endpoints ( which would
 * require TCP connection on startup ).
 *
 * This is very simple - only one object kind, no expiry.
 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           
 * Author:      Henri Gomez <hgomez@apache.org>
 * Author:      Costin Manolache
*/
struct jk_objCache {
    /** Return an object to the pool.
     *  @return JK_FALSE the object can't be taken back, caller must free it.
     */
    int (*put)(struct jk_env *env, jk_objCache_t *_this, void *obj);

    void *(*get)(struct jk_env *env, jk_objCache_t *_this);

    int (*init)(struct jk_env *env, jk_objCache_t *_this, int cacheSize);

    int (*destroy)(struct jk_env *env, jk_objCache_t *_this);

    /** Cache max size. -1 for unbound ( i.e. growing ). */
    int maxSize;

    /* Current size of the table */
    int size;

    /** Number of elements in the cache.
     *  Postition where next element will be inserted.
     */
    int count;

    /* Sync.
     */
    struct jk_mutex *cs;

    /** Objects in the cache */
    void **data;
    struct jk_pool *pool;
};
    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif 
