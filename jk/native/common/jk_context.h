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
 * Description: Context Stuff (Autoconf)                                   *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                           *
 ***************************************************************************/
#ifndef JK_CONTEXT_H
#define JK_CONTEXT_H

#include "jk_pool.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

#define CBASE_INC_SIZE   (8)    /* Allocate memory by step of 8 URIs : ie 8 URI by context */
#define URI_INC_SIZE (8)        /* Allocate memory by step of 8 CONTEXTs : ie 8 contexts by worker */

    typedef struct
    {

        /*
         * Context base (ie examples) 
         */

        char *cbase;

        /*
         * Status (Up/Down)
         */

        int status;

        /*
         * Num of URI handled 
         */

        int size;

        /*
         * Capacity
         */

        int capacity;

        /*
         * URL/URIs (autoconf)
         */

        char **uris;
    }
    jk_context_item_t;


    typedef struct
    {

        /*
         * Memory Pool
         */

        jk_pool_t p;
        jk_pool_atom_t buf[SMALL_POOL_SIZE];

        /*
         * Virtual Server (if use)
         */

        char *virtual;

        /*
         * Num of context handled (ie: examples, admin...)
         */

        int size;

        /*
         * Capacity
         */

        int capacity;

        /*
         * Context list, context / URIs
         */

        jk_context_item_t **contexts;
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

    void context_dump_uris(jk_context_t *c, char *cbase, FILE * f);

    jk_context_item_t *context_add_base(jk_context_t *c, char *cbase);

    int context_add_uri(jk_context_t *c, char *cbase, char *uri);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* JK_CONTEXT_H */
