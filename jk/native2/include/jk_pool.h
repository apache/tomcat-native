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
 * Description: Memory Pool object header file                             *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/
#ifndef _JK_POOL_H
#define _JK_POOL_H

#include "jk_global.h"

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
typedef struct jk_pool jk_pool_t;


/** XXX Move it to impl, make it incomplete 
 */
struct jk_pool {

    void (*open)(jk_pool_t *_this, 
                 jk_pool_atom_t *buf,
                 unsigned size);
    
    void (*close)(jk_pool_t *_this);
    
    void (*reset)(jk_pool_t *_this);

    void *(*alloc)(jk_pool_t *_this, size_t size);

    void *(*realloc)(jk_pool_t *_this, size_t size,
                     const void *old, size_t old_sz);

    void *(*calloc)(jk_pool_t *_this, size_t size);

    void *(*pstrdup)(jk_pool_t *_this, const char *s);

    /** Points to the private data. In the case of APR,
        it's a apr_pool you can use directly */
    void *_private;
    
    /* Private data - XXX move to impl */
    unsigned size;      
    unsigned pos;       
    char     *buf;      
    unsigned dyn_size;  
    unsigned dyn_pos;   
    void     **dynamic;
};

/** Create a pool
 */
int jk_pool_create( jk_pool_t **newPool, jk_pool_t *parent );

/** Open a pool using allocated space. The pool can grow by
    (m)allocating more storage, but it'll first use buf.
    This is used in jk with stack-allocated pools.
*/
void jk_pool_open(jk_pool_t *p, jk_pool_atom_t *buf, unsigned size);


void jk_open_pool(jk_pool_t *p,
                  jk_pool_atom_t *buf,
                  unsigned size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _JK_POOL_H */
