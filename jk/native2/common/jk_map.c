/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2003 The Apache Software Foundation.          *
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
 * Description: General purpose map object                                 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_map.h"

#define CAPACITY_INC_SIZE (50)
#define LENGTH_OF_LINE    (1024)

typedef struct jk_map_private {
    char **names;
    void **values;
    apr_uint32_t  *keys;

    int capacity;
    int size;
} jk_map_private_t;

#if APR_CHARSET_EBCDIC
#define CASE_MASK 0xbfbfbfbf
#else
#define CASE_MASK 0xdfdfdfdf
#endif

/* Compute the "checksum" for a key, consisting of the first
 * 4 bytes, normalized for case-insensitivity and packed into
 * an int...this checksum allows us to do a single integer
 * comparison as a fast check to determine whether we can
 * skip a strcasecmp
 */
#define COMPUTE_KEY_CHECKSUM(key, checksum)    \
{                                              \
    const char *k = (key);                     \
    apr_uint32_t c = (apr_uint32_t)*k;         \
    (checksum) = c;                            \
    (checksum) <<= 8;                          \
    if (c) {                                   \
        c = (apr_uint32_t)*++k;                \
        checksum |= c;                         \
    }                                          \
    (checksum) <<= 8;                          \
    if (c) {                                   \
        c = (apr_uint32_t)*++k;                \
        checksum |= c;                         \
    }                                          \
    (checksum) <<= 8;                          \
    if (c) {                                   \
        c = (apr_uint32_t)*++k;                \
        checksum |= c;                         \
    }                                          \
    checksum &= CASE_MASK;                     \
}

static void *jk2_map_default_get(jk_env_t *env, jk_map_t *m,
                                 const char *name)
{
    int i;
    jk_map_private_t *mPriv;
    apr_uint32_t checksum;

    if(name==NULL )
        return NULL;
    mPriv=(jk_map_private_t *)m->_private;

    COMPUTE_KEY_CHECKSUM(name, checksum);

    for(i = 0 ; i < mPriv->size ; i++) {
        if (mPriv->keys[i] == checksum && 
            strcmp(mPriv->names[i], name) == 0) {
            /*fprintf(stderr, "jk_map.get found %s %s \n", name, mPriv->values[i]  ); */
            return  mPriv->values[i];
        }
    }
    return NULL;
}

/* Make space for more elements
 */
static int jk2_map_default_realloc(jk_env_t *env, jk_map_t *m)
{
    jk_map_private_t *mPriv=m->_private;
    
    if(mPriv->size >= mPriv->capacity) {
        char **names;
        void **values;
        apr_uint32_t *keys;
        int  capacity = mPriv->capacity + CAPACITY_INC_SIZE;

        names = (char **)m->pool->calloc(env, m->pool,
                                        sizeof(char *) * capacity);
        values = (void **)m->pool->calloc(env, m->pool,
                                         sizeof(void *) * capacity);

        keys = (apr_uint32_t *)m->pool->calloc(env, m->pool,
                                                 sizeof(apr_uint32_t) * capacity);
        if( names== NULL || values==NULL || keys==NULL) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "map.realloc(): AllocationError\n");
            return JK_ERR;
        }
        m->keys=names;
        m->values=values;

        if (mPriv->capacity && mPriv->names) 
            memcpy(names, mPriv->names, sizeof(char *) * mPriv->capacity);
        
        if (mPriv->capacity && mPriv->values)
            memcpy(values, mPriv->values, sizeof(void *) * mPriv->capacity);

        if (mPriv->capacity && mPriv->keys)
            memcpy(keys, mPriv->keys, sizeof(apr_uint32_t) * mPriv->capacity);
        
        mPriv->names = ( char **)names;
        mPriv->values = ( void **)values;
        mPriv->keys = keys;
        mPriv->capacity = capacity;
        
        return JK_OK;
    }

    return JK_OK;
}


static int jk2_map_default_put(jk_env_t *env, jk_map_t *m,
                               const char *name, void *value,
                               void **old)
{
    int rc = JK_ERR;
    int i;
    jk_map_private_t *mPriv;
    apr_uint32_t checksum;

    if( name==NULL ) 
        return JK_ERR;

    mPriv=(jk_map_private_t *)m->_private;
    
    COMPUTE_KEY_CHECKSUM(name, checksum);

    for(i = 0 ; i < mPriv->size ; i++) {
        if (mPriv->keys[i] == checksum && 
            strcmp(mPriv->names[i], name) == 0) {
            break;
        }
    }

    /* Old value found */
    if(i < mPriv->size) {
        if( old!=NULL )
            *old = (void *) mPriv->values[i]; /* DIRTY */
        mPriv->values[i] = value;
        return JK_OK;
    }
    
    jk2_map_default_realloc(env, m);
    
    if(mPriv->size < mPriv->capacity) {
        mPriv->values[mPriv->size] = value;
        /* XXX this is wrong - either we take ownership and copy both
           name and value,
           or none. The caller should do that if he needs !
           Sure, but we should have our copy...
        mPriv->names[mPriv->size] =  (char *)name; 
        */
        mPriv->names[mPriv->size] = m->pool->pstrdup(env,m->pool, name);
        mPriv->keys[mPriv->size] = checksum;
        mPriv->size ++;
        rc = JK_OK;
    }
    return rc;
}

static int jk2_map_default_add(jk_env_t *env, jk_map_t *m,
                               const char *name, void *value)
{
    int rc = JK_ERR;
    jk_map_private_t *mPriv;

    if( name==NULL ) 
        return JK_ERR;

    mPriv=(jk_map_private_t *)m->_private;
    
    jk2_map_default_realloc(env, m);
    
    if(mPriv->size < mPriv->capacity) {
        apr_uint32_t checksum;
    
        COMPUTE_KEY_CHECKSUM(name, checksum);
        mPriv->values[mPriv->size] = value;
        /* XXX this is wrong - either we take ownership and copy both
           name and value,
           or none. The caller should do that if he needs !
        */
        /*     mPriv->names[mPriv->size] = m->pool->pstrdup(m->pool, name); */
        mPriv->names[mPriv->size] =  (char *)name; 
        mPriv->keys[mPriv->size] = checksum;
        mPriv->size ++;
        rc = JK_OK;
    }
    return rc;
}

static int jk2_map_default_size(jk_env_t *env, jk_map_t *m)
{
    jk_map_private_t *mPriv;

    /* assert(m!=NULL) -- we call it via m->... */
    mPriv=(jk_map_private_t *)m->_private;
    return mPriv->size;
}

static char *jk2_map_default_nameAt(jk_env_t *env, jk_map_t *m,
                                    int idex)
{
    jk_map_private_t *mPriv;

    mPriv=(jk_map_private_t *)m->_private;

    if(idex < 0 || idex > mPriv->size ) 
        return NULL;
    
    return (char *)mPriv->names[idex]; 
}

static void *jk2_map_default_valueAt(jk_env_t *env, jk_map_t *m,
                                     int idex)
{
    jk_map_private_t *mPriv;

    mPriv=(jk_map_private_t *)m->_private;

    if(idex < 0 || idex > mPriv->size )
        return NULL;
    
    return (void *) mPriv->values[idex]; 
}

static void jk2_map_default_clear(jk_env_t *env, jk_map_t *m )
{
    jk_map_private_t *mPriv;

    /* assert(m!=NULL) -- we call it via m->... */
    mPriv=(jk_map_private_t *)m->_private;
    mPriv->size=0;

}

static void jk2_map_default_init(jk_env_t *env, jk_map_t *m, int initialSize,
                                 void *wrappedObj)
{

}

static int jk2_map_append(jk_env_t *env, jk_map_t * dst, jk_map_t * src )
{
    /* This was badly broken in the original ! */
    int sz = src->size(env, src);
    int i;
    for(i = 0 ; i < sz ; i++) {
        char *name = src->nameAt(env, src, i);
        void *value = src->valueAt(env, src, i);

        if( dst->get(env, dst, name ) == NULL) {
            int rc= dst->put(env, dst, name, value, NULL );
            if( rc != JK_OK )
                return rc;
        }
    }
    return JK_OK;
}
                               
char * jk2_map_concatKeys( jk_env_t *env, jk_map_t *map, char *delim )
{
    int i;
    char *buf;
    int len=0;
    int delimLen=strlen( delim );
    
    int sz=map->size( env, map );
    for( i=0; i<sz; i++ ) {
        if( map->keys[i] != NULL ) {
            len+=strlen( map->keys[i] );
            len+=delimLen;
        }
    }
    buf=env->tmpPool->calloc( env, env->tmpPool, len + 10 );
    len=0;
    for( i=0; i<sz; i++ ) {
        if( map->keys[i] != NULL ) {
            sprintf( buf + len, "%s%s", delim, map->keys[i] );
            len += strlen( map->keys[i] ) + delimLen;
        }
    }
    buf[len]='\0';
    return buf;
}

void qsort2(char **a, void **d, int n)
{
    int i, j;
    char *x, *w;
    
    do {
        i = 0; j = n - 1;
        x = a[j/2];
        do {
            /* XXX: descending length sorting */
            while (strlen(a[i]) > strlen(x)) i++;
            while (strlen(a[j]) < strlen(x)) j--;
            if (i > j)
                break;
            w = a[i]; a[i] = a[j]; a[j] = w;
            w = d[i]; d[i] = d[j]; d[j] = w;
        }
        while (++i <= --j);
        if (j + 1 < n - i) {
            if (j > 0)
                qsort2(a, d, j + 1);
            a += i; d += i; n -= i;
        }
        else {
            if (i < n - 1) qsort2(a + i, d + i, n - i);
            n = j + 1;
        }
    }
    while (n > 1);
}



static void jk2_map_qsort(jk_env_t *env, jk_map_t *map)
{
    int n = map->size(env, map);
    
    if (n < 2)
        return;
    qsort2(map->keys, map->values, n);
}



/* ==================== */
/* Internal utils */


int jk2_map_default_create(jk_env_t *env, jk_map_t **m, jk_pool_t *pool )
{
    jk_map_t *_this;
    jk_map_private_t *mPriv;

    if( m== NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "map.create(): NullPointerException\n");
        return JK_ERR;
    }
    
    _this=(jk_map_t *)pool->calloc(env, pool, sizeof(jk_map_t));
    mPriv=(jk_map_private_t *)pool->calloc(env, pool, sizeof(jk_map_private_t));
    *m=_this;

    if( _this == NULL || mPriv==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "map.create(): AllocationError\n");
        return JK_ERR;
    }
    
    _this->pool = pool;
    _this->_private=mPriv;
    
    mPriv->capacity = 0;
    mPriv->size     = 0;
    mPriv->names    = NULL;
    mPriv->values   = NULL;

    _this->get=jk2_map_default_get;
    _this->put=jk2_map_default_put;
    _this->add=jk2_map_default_add;
    _this->size=jk2_map_default_size;
    _this->nameAt=jk2_map_default_nameAt;
    _this->valueAt=jk2_map_default_valueAt;
    _this->init=jk2_map_default_init;
    _this->clear=jk2_map_default_clear;
    _this->sort=jk2_map_qsort;
    return JK_OK;
}

