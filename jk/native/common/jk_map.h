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
 * Description: Map object header file                                     *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_MAP_H
#define JK_MAP_H


#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */


struct jk_map;
typedef struct jk_map jk_map_t;

int jk_map_alloc(jk_map_t **m);

int jk_map_free(jk_map_t **m);

int jk_map_open(jk_map_t *m);

int jk_map_close(jk_map_t *m);

void *jk_map_get(jk_map_t *m, const char *name, const void *def);

int jk_map_get_id(jk_map_t *m, const char *name);

int jk_map_get_int(jk_map_t *m, const char *name, int def);

double jk_map_get_double(jk_map_t *m, const char *name, double def);

int jk_map_get_bool(jk_map_t *m, const char *name, int def);

const char *jk_map_get_string(jk_map_t *m, const char *name, const char *def);

char **jk_map_get_string_list(jk_map_t *m,
                           const char *name,
                           unsigned *list_len, const char *def);

int jk_map_add(jk_map_t *m, const char *name, const void *value);

int jk_map_put(jk_map_t *m, const char *name, const void *value, void **old);

int jk_map_read_property(jk_map_t *m, const char *str, int allow_duplicates, jk_logger_t *l);

int jk_map_read_properties(jk_map_t *m, const char *f, time_t *modified, int allow_duplicates, jk_logger_t *l);

int jk_map_size(jk_map_t *m);

const char *jk_map_name_at(jk_map_t *m, int idex);

void *jk_map_value_at(jk_map_t *m, int idex);

int jk_map_load_property(jk_map_t *m, const char *str, jk_logger_t *l);

int jk_map_load_properties(jk_map_t *m, const char *f, time_t *modified, jk_logger_t *l);

/**
 *  Replace $(property) in value.
 *
 */
char *jk_map_replace_properties(jk_map_t *m, const char *value);

int jk_map_resolve_references(jk_map_t *m, const char *prefix, int wildcard, int depth, jk_logger_t *l);

int jk_map_inherit_properties(jk_map_t *m, const char *from, const char *to, jk_logger_t *l);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* JK_MAP_H */
