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
