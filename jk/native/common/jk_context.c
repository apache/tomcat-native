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
 * Description: Context handling (Autoconf)                                *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_global.h"
#include "jk_context.h"
#include "jk_ajp_common.h"


/*
 * Set the virtual name of the context 
 */

int context_set_virtual(jk_context_t *c, char *virtual)
{
    if (c) {

        if (virtual) {
            c->virtual = jk_pool_strdup(&c->p, virtual);

            if (! c->virtual)
                return JK_FALSE;
        }

        return JK_TRUE;
    }

    return JK_FALSE;
}

/*
 * Init the context info struct
 */

int context_open(jk_context_t *c, char *virtual)
{
    if (c) {
        jk_open_pool(&c->p, c->buf, sizeof(jk_pool_atom_t) * SMALL_POOL_SIZE);
        c->size  	= 0;
        c->capacity = 0;
        c->contexts = NULL;

        return context_set_virtual(c, virtual);
    }

    return JK_FALSE;
}

/*
 * Close the context info struct
 */

int context_close(jk_context_t *c)
{
    if (c) {

        jk_close_pool(&c->p);
        return JK_TRUE;
    }

    return JK_FALSE;
}

/*
 * Allocate and open context
 */

int context_alloc(jk_context_t **c, char * virtual)
{
    if (c) 
        return context_open(*c = (jk_context_t *)malloc(sizeof(jk_context_t)), virtual);
  
    return JK_FALSE;
}
 
/*
 * Close and destroy context
 */

int context_free(jk_context_t **c)
{
    if (c && *c) {
        context_close(*c);  
        free(*c);
        *c = NULL;
        return JK_TRUE;
    }
    
    return JK_FALSE;
}


/*
 * Ensure there will be memory in context info to store Context Bases
 */

static int context_realloc(jk_context_t *c)
{
    if (c->size == c->capacity) {
        jk_context_item_t **contexts;
        int  capacity = c->capacity + CBASE_INC_SIZE;
        
        contexts = (jk_context_item_t **)jk_pool_alloc(&c->p, sizeof(jk_context_item_t *) * capacity);

        if (! contexts)
            return JK_FALSE;

        if (c->capacity && c->contexts)
            memcpy(contexts, c->contexts, sizeof(jk_context_item_t *) * c->capacity);

        c->contexts = contexts;
        c->capacity = capacity;
    }

    return JK_TRUE;
}

/*
 * Ensure there will be memory in context info to URIS
 */
 
static int context_item_realloc(jk_context_t *c, jk_context_item_t *ci)
{
    if (ci->size == ci->capacity) {
            char **uris;
            int capacity = ci->capacity + URI_INC_SIZE;

            uris = (char **)jk_pool_alloc(&c->p, sizeof(char *) * capacity);

        if (! uris)
            return JK_FALSE;

        memcpy(uris, ci->uris, sizeof(char *) * ci->capacity);

        ci->uris     = uris;
        ci->capacity = capacity;
    }
    
    return JK_TRUE;
}

        
/*
 * Locate a context base in context list
 */

jk_context_item_t * context_find_base(jk_context_t *c, char *cbase)
{
    int                 i;
    jk_context_item_t * ci;

    if (! c || ! cbase)
        return NULL;

    for (i = 0 ; i < c->size ; i++) {
        
        ci = c->contexts[i];

        if (! ci)
            continue;

        if (! strcmp(ci->cbase, cbase))
            return ci;
    }

    return NULL;
}

/*
 * Locate an URI in a context item
 */

char * context_item_find_uri(jk_context_item_t *ci, char *uri)
{
    int i;

    if (! ci || ! uri)
        return NULL;

    for (i = 0 ; i < ci->size ; i++) {
        if (! strcmp(ci->uris[i], uri))
            return ci->uris[i];
    }

    return NULL;
}

void context_dump_uris(jk_context_t *c, char *cbase, FILE * f)
{
    jk_context_item_t * ci;
    int                 i;

    ci = context_find_base(c, cbase);

    if (! ci)
        return;

    for (i = 0; i < ci->size; i++)
        fprintf(f, "/%s/%s\n", ci->cbase, ci->uris[i]);

    fflush(f); 
}


/*
 * Add a new context item to context
 */

jk_context_item_t * context_add_base(jk_context_t *c, char *cbase)
{
    jk_context_item_t *  ci;

    if (! c || !cbase)
        return NULL;

    /* Check if the context base was not allready created */
    ci = context_find_base(c, cbase);

    if (ci)
        return ci;

    if (context_realloc(c) != JK_TRUE)
        return NULL;

    ci = (jk_context_item_t *)jk_pool_alloc(&c->p, sizeof(jk_context_item_t));

    if (! ci)
        return NULL;

    c->contexts[c->size] = ci;
    c->size++;
    ci->cbase       = jk_pool_strdup(&c->p, cbase);
    ci->status      = 0;
    ci->size        = 0;
    ci->capacity    = 0;
    ci->uris        = NULL;

    return ci;
}

/*
 * Add a new URI to a context item
 */

int context_add_uri(jk_context_t *c, char *cbase, char * uri)
{
    jk_context_item_t *  ci;

    if (! uri)
        return JK_FALSE;

    /* Get/Create the context base */
    ci = context_add_base(c, cbase);
        
    if (! ci)
        return JK_FALSE;

    if (context_item_find_uri(ci, uri) != NULL)
        return JK_TRUE;

    if (context_item_realloc(c, ci) == JK_FALSE)
        return JK_FALSE;

    ci->uris[ci->size] = jk_pool_strdup(&c->p, uri);

    if (ci->uris[ci->size] == NULL)
        return JK_FALSE;

    ci->size++;
    return JK_TRUE;
}

