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

/**
 * Implementation of map using apr_table. This avoids copying the headers,
 * env, etc in jk_service - we can just wrap them.
 *
 * Note that this _require_ that apr pools are used ( can't be used
 * with jk_pools ), i.e. you must use apr for both pools and maps.
 *
 * @author Costin Manolache
 */

#include "jk_pool.h"
#include "jk_env.h"
#include "apr_pools.h"
#include "apr_strings.h"
#include "apr_tables.h"

#include "jk_apache2.h"


static void *jk2_map_aprtable_get(struct jk_env *env, struct jk_map *_this,
                                  const char *name)
{
    apr_table_t *aprMap = _this->_private;
    return (void *)apr_table_get(aprMap, name);
}

static int jk2_map_aprtable_put(struct jk_env *env, struct jk_map *_this,
                                const char *name, void *value,
                                void **oldValue)
{
    apr_table_t *aprMap = _this->_private;
    if (oldValue != NULL) {
        *oldValue = (void *)apr_table_get(aprMap, (char *)name);
    }

    apr_table_setn(aprMap, name, (char *)value);

    return JK_OK;
}

static int jk2_map_aprtable_add(struct jk_env *env, struct jk_map *_this,
                                const char *name, void *value)
{
    apr_table_t *aprMap = _this->_private;

    apr_table_addn(aprMap, name, (char *)value);

    return JK_OK;
}

static int jk2_map_aprtable_size(struct jk_env *env, struct jk_map *_this)
{
    apr_table_t *aprMap = _this->_private;
    const apr_array_header_t *ah = apr_table_elts(aprMap);

    return ah->nelts;

}

static char *jk2_map_aprtable_nameAt(struct jk_env *env, struct jk_map *_this,
                                     int pos)
{
    apr_table_t *aprMap = _this->_private;
    const apr_array_header_t *ah = apr_table_elts(aprMap);
    apr_table_entry_t *elts = (apr_table_entry_t *) ah->elts;


    return elts[pos].key;
}

static void *jk2_map_aprtable_valueAt(struct jk_env *env,
                                      struct jk_map *_this, int pos)
{
    apr_table_t *aprMap = _this->_private;
    const apr_array_header_t *ah = apr_table_elts(aprMap);
    apr_table_entry_t *elts = (apr_table_entry_t *) ah->elts;

    return elts[pos].val;
}

static void jk2_map_aprtable_init(jk_env_t *env, jk_map_t *m, int initialSize,
                                  void *wrappedObj)
{
    m->_private = wrappedObj;
}

static void jk2_map_aprtable_clear(jk_env_t *env, jk_map_t *m)
{

}


/* Not used yet */
int JK_METHOD jk2_map_aprtable_factory(jk_env_t *env, jk_pool_t *pool,
                                       jk_bean_t *result,
                                       const char *type, const char *name)
{
    jk_map_t *_this = (jk_map_t *)pool->calloc(env, pool, sizeof(jk_map_t));

    result->object = _this;

    _this->get = jk2_map_aprtable_get;
    _this->put = jk2_map_aprtable_put;
    _this->add = jk2_map_aprtable_add;
    _this->size = jk2_map_aprtable_size;
    _this->nameAt = jk2_map_aprtable_nameAt;
    _this->valueAt = jk2_map_aprtable_valueAt;
    _this->init = jk2_map_aprtable_init;
    _this->clear = jk2_map_aprtable_clear;

    return JK_OK;
}
