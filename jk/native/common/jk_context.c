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
 * Description: Context handling (Autoconf)                                *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_global.h"
#include "jk_ajp14.h"
#include "jk_context.h"

/*
 * Init the context info struct
 */

int context_open(jk_context_t *c)
{
    if (c) {
        jk_open_pool(&c->p, c->buf, sizeof(jk_pool_atom_t) * SMALL_POOL_SIZE);
		c->virtual  = NULL;
		c->cbase  	= NULL,
        c->status   = AJP14_CONTEXT_DOWN;
        c->nuri  	= 0;
        c->uris     = NULL;
        return JK_TRUE;
    }

    return JK_FALSE;
}


/*
 * Create the context info struct
 */

int context_alloc(jk_context_t **c)
{
    if (c) {
        return context_open(*c = (jk_context_t *)malloc(sizeof(jk_context_t)));
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
 * Delete the context info struct
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
 * Find a context in the Contextes List
 */

jk_context_t * context_find(jk_context_list_t *l, char * virtual, char * cbase)
{
	int	i;

	if (! l)
		return NULL;

	for (i = 0; i < l->ncontext; i++) {
		if (virtual) 
			if (strcmp(l->contexts[i]->virtual, virtual))
				continue;

		if (cbase) 
			if (! strcmp(l->contexts[i]->cbase, cbase))
				return (l->contexts[i]);
	}

	return NULL;
}


/*
 * Add an URI to context
 */

int  context_add_uri(jk_context_t *c, char * uri)
{
    int i;

    if (! c)
        return JK_FALSE;

    return NULL;
}


