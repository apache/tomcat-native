/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *         Copyright (c) 1999, 2000  The Apache Software Foundation.         *
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
 * 4. The names  "The  Jakarta  Project",  "Tomcat",  and  "Apache  Software *
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

// CVS $Id$
// Author: Pier Fumagalli <mailto:pier.fumagalli@eng.sun.com>

#include <wa.h>

/**
 * Create a new memory pool.
 */
wa_memorypool *wa_memorypool_create() {
    wa_memorypool *pool=(wa_memorypool *)malloc(sizeof(wa_memorypool));
    pool->curr=0;
    pool->next=NULL;
}

/**
 * Allocate memory into a memory pool.
 */
void *wa_memorypool_alloc(wa_memorypool *pool, size_t size) {
    // Check the pool
    if (pool==NULL) return(NULL);
    
    // If we still have space in the current pool, allocate memory here.
    if (pool->curr<WA_MEMORYPOOL_SIZE) {
        void *ptr=(void *)malloc(size);
        if (ptr==NULL) return(NULL);
        pool->ptrs[pool->curr]=ptr;
        pool->curr++;
        return(ptr);
    }

    // Aparently we don't have space left in the current pool, allocate
    // a new child memory pool (if we don't have it) and go from there.
    if (pool->next==NULL) {
        wa_memorypool *child=wa_memorypool_create();
        child->next=pool->next;
        pool->next=child;
    }

    // Allocate memory in the child memory pool.
    return(wa_memorypool_alloc(pool->next,size));
}

/**
 * Clear a memory pool and free all allocated space.
 */
void wa_memorypool_free(wa_memorypool *pool) {
    int x=0;

    // Check the pool
    if (pool==NULL) return;
    
    // If we have a child memory pool free that up first (recursively)
    if (pool->next!=NULL) {
        wa_memorypool_free(pool->next);
    }
    // Now that all children are clear we can clear up this memory pool.
    for (x=0; x<pool->curr; x++) free(pool->ptrs[x]);
    free(pool);
}

/**
 * Format a string allocating memory from a pool.
 */
char *wa_memorypool_sprintf(wa_memorypool *pool, const char *fmt, ...) {
    char buf[4096];
    va_list ap;
    int len=0;
    
    // Check the pool and the format string
    if (pool==NULL) return(NULL);
    if (fmt==NULL) return(NULL);
    
    // Print the string to the buffer
    va_start(ap,fmt);
    len=vsnprintf(buf,4096,fmt,ap);
    va_end(ap);

    // Trim the buffer (just to be on the safe side)
    if (len==4096) len--;
    buf[len]='\0';

    // Duplicate and return the string
    return(wa_memorypool_strdup(pool,buf));
}

/**
 * Duplicate a string allocating memory in a pool.
 */
char *wa_memorypool_strdup(wa_memorypool *pool, char *string) {
    char *buf=NULL;
    int x=0;
    int l=0;

    // Check the pool and the format string
    if (pool==NULL) return(NULL);
    if (string==NULL) return(NULL);

    // Count the characters and allocate some memory
    while(string[l++]!='\0');
    buf=(char *)wa_memorypool_alloc(pool,l*sizeof(char));
    if (buf==NULL) return(NULL);

    // Copy the source string in the allocated buffer
    for (x=0;x<l;x++) buf[x]=string[x];
    return(buf);
}
