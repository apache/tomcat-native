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
 * Description: URI to worker mapper header file                           *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_URI_WORKER_MAP_H
#define JK_URI_WORKER_MAP_H


#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

#include "jk_global.h"
#include "jk_map.h"
#include "jk_logger.h"
#include "jk_shm.h"

#define MATCH_TYPE_EXACT            0x0001
#define MATCH_TYPE_CONTEXT          0x0002
/* match all context path URIs with a path component suffix */
#define MATCH_TYPE_CONTEXT_PATH     0x0004
#define MATCH_TYPE_SUFFIX           0x0010
/* match all URIs of the form *ext */
#define MATCH_TYPE_GENERAL_SUFFIX   0x0020
/* match multiple wild characters (*) and (?) */
#define MATCH_TYPE_WILDCHAR_PATH    0x0040
#define MATCH_TYPE_NO_MATCH         0x1000
#define MATCH_TYPE_DISABLED         0x2000

struct uri_worker_record
{
    /* Original uri for logging */
    char *uri;

    /* Name of worker mapped */
    const char *worker_name;

    /* Suffix of uri */
    const char *suffix;

    /* Base context */
    const char *context;

    /* Match type */
    unsigned int match_type;

    /* char length of the context */
    size_t context_len;
};
typedef struct uri_worker_record uri_worker_record_t;

struct jk_uri_worker_map
{
    /* Memory Pool */
    jk_pool_t p;
    jk_pool_atom_t buf[BIG_POOL_SIZE];

    /* map URI->WORKER */
    uri_worker_record_t **maps;
    
    /* Map Number */
    unsigned int size;

    /* Map Capacity */
    unsigned int capacity;

    /* NoMap Number */
    unsigned int nosize;

};
typedef struct jk_uri_worker_map jk_uri_worker_map_t;

int uri_worker_map_alloc(jk_uri_worker_map_t **uw_map,
                         jk_map_t *init_data, jk_logger_t *l);

int uri_worker_map_free(jk_uri_worker_map_t **uw_map, jk_logger_t *l);

int uri_worker_map_open(jk_uri_worker_map_t *uw_map,
                        jk_map_t *init_data, jk_logger_t *l);

int uri_worker_map_add(jk_uri_worker_map_t *uw_map,
                       const char *puri, const char *pworker, jk_logger_t *l);

const char *map_uri_to_worker(jk_uri_worker_map_t *uw_map,
                              char *uri, jk_logger_t *l);

#ifdef __cplusplus
}
#endif    /* __cplusplus */
#endif    /* JK_URI_WORKER_MAP_H */
