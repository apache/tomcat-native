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
 * Description: Context Stuff (Autoconf)                                   *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/
#ifndef JK_CONTEXT_H
#define JK_CONTEXT_H

#include "jk_pool.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define CBASE_INC_SIZE   (8)    /* Allocate memory by step of 8 URIs : ie 8 URI by context */
#define URI_INC_SIZE (8)        /* Allocate memory by step of 8 CONTEXTs : ie 8 contexts by worker */

typedef struct {

    /*
     * Context base (ie examples) 
     */
 
    char *      cbase;
 
    /*
     * Status (Up/Down)
     */
 
    int         status;

    /*
     * Num of URI handled 
     */
    
    int         size;

    /*
     * Capacity
     */
   
    int         capacity;

    /*
     * URL/URIs (autoconf)
     */

    char **     uris;
}
jk_context_item_t;


typedef struct {

    /*
     * Memory Pool
     */

    jk_pool_t       p;
    jk_pool_atom_t  buf[SMALL_POOL_SIZE];

	/*
	 * Virtual Server (if use)
	 */

	char *		            virtual;

    /*
     * Num of context handled (ie: examples, admin...)
     */

    int                     size;

    /*
     * Capacity
     */

    int                     capacity; 

    /*
     * Context list, context / URIs
     */

    jk_context_item_t **    contexts;
} 
jk_context_t;


/*
 * functions defined here 
 */

int context_set_virtual(jk_context_t *c, char *virtual);

int context_open(jk_context_t *c, char *virtual);

int context_close(jk_context_t *c);

int context_alloc(jk_context_t **c, char *virtual);

int context_free(jk_context_t **c);

jk_context_item_t *context_find_base(jk_context_t *c, char *cbase);

char *context_item_find_uri(jk_context_item_t *ci, char *uri);

void context_dump_uris(jk_context_t *c, char *cbase, FILE *f);

jk_context_item_t *context_add_base(jk_context_t *c, char *cbase);

int context_add_uri(jk_context_t *c, char *cbase, char *uri);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_CONTEXT_H */
