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

/***************************************************************************
 * Description: Simple memory pool                                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_pool.h"

#define DEFAULT_DYNAMIC 10


static void *jk_pool_dyn_alloc(jk_pool_t *p, 
                               size_t size);


void jk_open_pool(jk_pool_t *p,
                  jk_pool_atom_t *buf,
                  unsigned size)
{
    p->pos  = 0;
    p->size = size;
    p->buf  = (char *)buf;

    p->dyn_pos = 0;
    p->dynamic = NULL;
    p->dyn_size = 0;
}

void jk_close_pool(jk_pool_t *p)
{
    if(p) {
        jk_reset_pool(p);
        if(p->dynamic) {
            free(p->dynamic);
        }
    }
}

void jk_reset_pool(jk_pool_t *p)
{
    if(p && p->dyn_pos && p->dynamic) {
        unsigned i;
        for(i = 0 ; i < p->dyn_pos ; i++) {
            if(p->dynamic[i]) {
                free(p->dynamic[i]);
            }
        }
    }

    p->dyn_pos  = 0;
    p->pos      = 0;
}

void *jk_pool_alloc(jk_pool_t *p, 
                    size_t size)
{
    void *rc = NULL;

    if(p && size > 0) {
        /* Round size to the upper mult of 8 (or 16 on iSeries) */
	size--;
#ifdef AS400
        size /= 16;
        size = (size + 1) * 16;
#else
        size /= 8;
        size = (size + 1) * 8;
#endif
        if((p->size - p->pos) >= size) {
            rc = &(p->buf[p->pos]);
            p->pos += size;
        } else {
            rc = jk_pool_dyn_alloc(p, size);
        }
    }

    return rc;
}

void *jk_pool_realloc(jk_pool_t *p, 
                      size_t sz,
                      const void *old,
                      size_t old_sz)
{
    void *rc;

    if(!p || (!old && old_sz)) {
        return NULL;
    }

    rc = jk_pool_alloc(p, sz);
    if(rc) {
        memcpy(rc, old, old_sz);
    }

    return rc;
}

void *jk_pool_strdup(jk_pool_t *p, 
                     const char *s)
{
    char *rc = NULL;
    if(s && p) {
        size_t size = strlen(s);
    
        if(!size)  {
            return "";
        }

        size++;
        rc = jk_pool_alloc(p, size);
        if(rc) {
            memcpy(rc, s, size);
        }
    }

    return rc;
}

void jk_dump_pool(jk_pool_t *p, 
                  FILE *f)
{
    fprintf(f, "Dumping for pool [%x]\n", p);
    fprintf(f, "size             [%d]\n", p->size);
    fprintf(f, "pos              [%d]\n", p->pos);
    fprintf(f, "buf              [%x]\n", p->buf);  
    fprintf(f, "dyn_size         [%d]\n", p->dyn_size);
    fprintf(f, "dyn_pos          [%d]\n", p->dyn_pos);
    fprintf(f, "dynamic          [%x]\n", p->dynamic);

    fflush(f);
}

static void *jk_pool_dyn_alloc(jk_pool_t *p, 
                               size_t size)
{
    void *rc = NULL;

    if(p->dyn_size == p->dyn_pos) {
        unsigned new_dyn_size = p->dyn_size + DEFAULT_DYNAMIC;
        void **new_dynamic = (void **)malloc(new_dyn_size * sizeof(void *));
        if(new_dynamic) {
            if(p->dynamic) {
                memcpy(new_dynamic, 
                       p->dynamic, 
                       p->dyn_size * sizeof(void *));

                free(p->dynamic);
            }

            p->dynamic = new_dynamic;
            p->dyn_size = new_dyn_size;
        } else {
            return NULL;
        }
    } 

    rc = p->dynamic[p->dyn_pos] = malloc(size);
    if(p->dynamic[p->dyn_pos]) {
        p->dyn_pos ++;
    }

    return rc;
}
