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
 * Description: Simple memory pool                                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                               *
 ***************************************************************************/

#include "jk_pool.h"

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
        /* Round size to the upper mult of 8. */
        size -= 1;
        size /= 8;
        size = (size + 1) * 8;
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
