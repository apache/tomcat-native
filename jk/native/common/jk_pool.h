/*
 * Copyright (c) 1997-1999 The Java Apache Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * 4. The names "Apache JServ", "Apache JServ Servlet Engine" and 
 *    "Java Apache Project" must not be used to endorse or promote products 
 *    derived from this software without prior written permission.
 *
 * 5. Products derived from this software may not be called "Apache JServ"
 *    nor may "Apache" nor "Apache JServ" appear in their names without 
 *    prior written permission of the Java Apache Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *    
 * THIS SOFTWARE IS PROVIDED BY THE JAVA APACHE PROJECT "AS IS" AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE JAVA APACHE PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Java Apache Group. For more information
 * on the Java Apache Project and the Apache JServ Servlet Engine project,
 * please see <http://java.apache.org/>.
 *
 */

/***************************************************************************
 * Description: Memory Pool object header file                             *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                               *
 ***************************************************************************/
#ifndef _JK_POOL_H
#define _JK_POOL_H

#include "jk_global.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DEFAULT_DYNAMIC 10

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

struct jk_pool {
    unsigned size;      
    unsigned pos;       
    char     *buf;      
    unsigned dyn_size;  
    unsigned dyn_pos;   
    void     **dynamic; 
};

typedef struct jk_pool jk_pool_t;

void jk_open_pool(jk_pool_t *p,
                  jk_pool_atom_t *buf,
                  unsigned size);

void jk_close_pool(jk_pool_t *p);

void jk_reset_pool(jk_pool_t *p);

void *jk_pool_alloc(jk_pool_t *p, 
                    size_t sz);

void *jk_pool_realloc(jk_pool_t *p, 
                      size_t sz,
                      const void *old,
                      size_t old_sz);

void *jk_pool_strdup(jk_pool_t *p, 
                     const char *s);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _JK_POOL_H */
