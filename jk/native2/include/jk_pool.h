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
 * Description: Memory Pool object header file                             *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/
#ifndef _JK_POOL_H
#define _JK_POOL_H

#include "jk_global.h"
#include "jk_env.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @file jk_pool.h
 * @brief Jk memory allocation
 *
 * Similar with apr_pools, but completely unsynchronized.
 * XXX use same names
 * 
 */

/*
 * The pool atom (basic pool alocation unit) is an 8 byte long. 
 * Each allocation (even for 1 byte) will return a round up to the 
 * number of atoms. 
 * 
 * This is to help in alignment of 32/64 bit machines ...
 * G.S
 */
#ifdef WIN32
    typedef __int64     jk_pool_atom_t;
#elif defined(AIX)
    typedef long long   jk_pool_atom_t;
#elif defined(SOLARIS)
    typedef long long   jk_pool_atom_t;
#elif defined(LINUX)
    typedef long long   jk_pool_atom_t;
#elif defined(FREEBSD)
    typedef long long   jk_pool_atom_t;
#elif defined(OS2)
    typedef long long   jk_pool_atom_t;
#elif defined(NETWARE)
    typedef long long   jk_pool_atom_t;
#elif defined(HPUX11)
    typedef long long   jk_pool_atom_t;
#elif defined(IRIX)
    typedef long long   jk_pool_atom_t;
#elif defined(AS400)
    typedef void *      jk_pool_atom_t;
#else
    typedef long long   jk_pool_atom_t;
#endif

/* 
 * Pool size in number of pool atoms.
 */
#define TINY_POOL_SIZE 256                  /* Tiny 1/4K atom pool. */
#define SMALL_POOL_SIZE 512                 /* Small 1/2K atom pool. */
#define BIG_POOL_SIZE   2*SMALL_POOL_SIZE   /* Bigger 1K atom pool. */
#define HUGE_POOL_SIZE  2*BIG_POOL_SIZE     /* Huge 2K atom pool. */

struct jk_pool;
struct jk_env;
typedef struct jk_pool jk_pool_t;


/** The pool can grow by (m)allocating more storage, but it'll first use buf.
*/
struct jk_pool {
    /** Create a child pool. Size is a hint for the
     *  estimated needs of the child.
     */
    jk_pool_t *(*create)(struct jk_env *env, jk_pool_t *_this,
                         int size );
    
    void (*close)(struct jk_env *env, jk_pool_t *_this);

    /** Empty the pool using the same space.
     *  XXX should we rename it to clear() - consistent with apr ?
     */
    void (*reset)(struct jk_env *env, jk_pool_t *_this);

    /* Memory allocation */
    
    void *(*alloc)(struct jk_env *env, jk_pool_t *_this, size_t size);

    void *(*realloc)(struct jk_env *env, jk_pool_t *_this, size_t size,
                     const void *old, size_t old_sz);

    void *(*calloc)(struct jk_env *env, jk_pool_t *_this, size_t size);

    void *(*pstrdup)(struct jk_env *env, jk_pool_t *_this, const char *s);

    void *(*pstrcat)(struct jk_env *env, jk_pool_t *_this, ...);

    void *(*pstrdup2ascii)(struct jk_env *env, jk_pool_t *_this, const char *s);

    void *(*pstrdup2ebcdic)(struct jk_env *env, jk_pool_t *_this, const char *s);

    /** Points to the private data. In the case of APR,
        it's a apr_pool you can use directly */
    void *_private;
};

/** Create a pool. Use it for the initial allocation ( in mod_jk ). All other
    pools should be created using the virtual method.

    XXX move this to the factory
 */
int jk2_pool_create( struct jk_env *env, jk_pool_t **newPool, jk_pool_t *parent, int size );

int JK_METHOD jk2_pool_apr_create( struct jk_env *env, struct jk_pool **newPool,
                                   struct jk_pool *parent, void *aprPool );


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _JK_POOL_H */
