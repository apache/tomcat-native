/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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
#include "jk_mt.h"

#define MATCH_TYPE_EXACT            0x0001
/* deprecated
#define MATCH_TYPE_CONTEXT          0x0002
 */
/* match all context path URIs with a path component suffix */
/* deprecated
#define MATCH_TYPE_CONTEXT_PATH     0x0004
 */
/* deprecated
#define MATCH_TYPE_SUFFIX           0x0010
 */
/* match all URIs of the form *ext */
/* deprecated
#define MATCH_TYPE_GENERAL_SUFFIX   0x0020
 */
/* match multiple wild characters (*) and (?) */
#define MATCH_TYPE_WILDCHAR_PATH    0x0040
#define MATCH_TYPE_NO_MATCH         0x1000
#define MATCH_TYPE_DISABLED         0x2000
/* deprecated
#define MATCH_TYPE_STOPPED          0x4000
 */

#define SOURCE_TYPE_WORKERDEF       0x0001
#define SOURCE_TYPE_JKMOUNT         0x0002
#define SOURCE_TYPE_URIMAP          0x0003
#define SOURCE_TYPE_DISCOVER        0x0004
#define SOURCE_TYPE_TEXT_WORKERDEF  ("worker definition")
#define SOURCE_TYPE_TEXT_JKMOUNT    ("JkMount")
#define SOURCE_TYPE_TEXT_URIMAP     ("uriworkermap")
#define SOURCE_TYPE_TEXT_DISCOVER   ("ajp14")

#define JK_MAX_URI_LEN              4095

struct rule_extension
{
    /* reply_timeout overwrite */
    int reply_timeout;
    /* activation state overwrites for load balancers */
    /* Number of elements in the array activations. */
    int activation_size;
    /* Dynamically allocated array with one entry per lb member. */
    int *activation;
    /* Temporary storage for the original extension strings. */
    char *active;
    char *disabled;
    char *stopped;
    /* fail_on_status overwrites */
    /* Number of elements in the array fail_on_status. */
    int fail_on_status_size;
    /* Dynamically allocated array with one entry per status. */
    int *fail_on_status;
    /* Temporary storage for the original extension strings. */
    char *fail_on_status_str;
    /* Use server error pages for responses >= 400. */
    int use_server_error_pages;
};
typedef struct rule_extension rule_extension_t;

struct uri_worker_record
{
    /* Original uri for logging */
    char *uri;

    /* Name of worker mapped */
    const char *worker_name;

    /* Base context */
    const char *context;

    /* Match type */
    unsigned int match_type;

    /* Definition source type */
    unsigned int source_type;

    /* char length of the context */
    size_t context_len;

    /* extended mapping properties */
    rule_extension_t extensions;
};
typedef struct uri_worker_record uri_worker_record_t;

struct jk_uri_worker_map
{
    /* Memory Pool */
    jk_pool_t p;
    jk_pool_atom_t buf[BIG_POOL_SIZE];

    /* Index 0 or 1, which map instance do we use */
    /* Needed to make map reload more atomically */
    int index;

    /* Memory Pool - cleared when doing reload */
    /* Use this pool to allocate objects, that are deleted */
    /* when the map gets dynamically reloaded from uriworkermap.properties. */
    jk_pool_t p_dyn[2];
    jk_pool_atom_t buf_dyn[2][BIG_POOL_SIZE];

    /* map URI->WORKER */
    uri_worker_record_t **maps[2];
    
    /* Map Number */
    unsigned int size[2];

    /* Map Capacity */
    unsigned int capacity[2];

    /* NoMap Number */
    unsigned int nosize[2];

    /* Dynamic config support */

    JK_CRIT_SEC cs;
    /* should we forward potentially unsafe URLs */
    int reject_unsafe;    
    /* uriworkermap filename */
    const char *fname;    
    /* uriworkermap reload check interval */
    int reload;    
    /* Last modified time */
    time_t  modified;
    /* Last checked time */
    time_t  checked;
};
typedef struct jk_uri_worker_map jk_uri_worker_map_t;

const char *uri_worker_map_get_source(uri_worker_record_t *uwr, jk_logger_t *l);

char *uri_worker_map_get_match(uri_worker_record_t *uwr, char *buf, jk_logger_t *l);

int uri_worker_map_alloc(jk_uri_worker_map_t **uw_map,
                         jk_map_t *init_data, jk_logger_t *l);

int uri_worker_map_free(jk_uri_worker_map_t **uw_map, jk_logger_t *l);

int uri_worker_map_open(jk_uri_worker_map_t *uw_map,
                        jk_map_t *init_data, jk_logger_t *l);

void uri_worker_map_ext(jk_uri_worker_map_t *uw_map, jk_logger_t *l);

int uri_worker_map_add(jk_uri_worker_map_t *uw_map,
                       const char *puri, const char *worker,
                       unsigned int source_type, jk_logger_t *l);

const char *map_uri_to_worker(jk_uri_worker_map_t *uw_map,
                              const char *uri, const char *vhost,
                              jk_logger_t *l);

const char *map_uri_to_worker_ext(jk_uri_worker_map_t *uw_map,
                                  const char *uri, const char *vhost,
                                  rule_extension_t **extensions,
                                  int *index,
                                  jk_logger_t *l);

rule_extension_t *get_uri_to_worker_ext(jk_uri_worker_map_t *uw_map,
                                        int index);

int uri_worker_map_load(jk_uri_worker_map_t *uw_map,
                        jk_logger_t *l);

int uri_worker_map_update(jk_uri_worker_map_t *uw_map,
                          int force, jk_logger_t *l);

#ifdef __cplusplus
}
#endif    /* __cplusplus */
#endif    /* JK_URI_WORKER_MAP_H */
