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

    int capacity;
    int size;
} jk_map_private_t;

static int  jk2_map_default_realloc(jk_env_t *env, jk_map_t *m);

static void *jk2_map_default_get(jk_env_t *env, jk_map_t *m,
                                 const char *name)
{
    int i;
    jk_map_private_t *mPriv;
    
    if(name==NULL )
        return NULL;
    mPriv=(jk_map_private_t *)m->_private;

    for(i = 0 ; i < mPriv->size ; i++) {
        if(0 == strcmp(mPriv->names[i], name)) {
            /*fprintf(stderr, "jk_map.get found %s %s \n", name, mPriv->values[i]  ); */
            return  mPriv->values[i];
        }
    }
    return NULL;
}


static int jk2_map_default_put(jk_env_t *env, jk_map_t *m,
                               const char *name, void *value,
                               void **old)
{
    int rc = JK_FALSE;
    int i;
    jk_map_private_t *mPriv;

    if( name==NULL ) 
        return JK_FALSE;

    mPriv=(jk_map_private_t *)m->_private;
    
    for(i = 0 ; i < mPriv->size ; i++) {
        if(0 == strcmp(mPriv->names[i], name)) {
            break;
        }
    }

    /* Old value found */
    if(i < mPriv->size) {
        if( old!=NULL )
            *old = (void *) mPriv->values[i]; /* DIRTY */
        mPriv->values[i] = value;
        return JK_TRUE;
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
        /* fprintf(stderr, "jk_map.set %s %s \n", name, value  ); */
        mPriv->size ++;
        rc = JK_TRUE;
    }
    return rc;
}

static int jk2_map_default_add(jk_env_t *env, jk_map_t *m,
                               const char *name, void *value)
{
    int rc = JK_FALSE;
    int i;
    jk_map_private_t *mPriv;

    if( name==NULL ) 
        return JK_FALSE;

    mPriv=(jk_map_private_t *)m->_private;
    
    jk2_map_default_realloc(env, m);
    
    if(mPriv->size < mPriv->capacity) {
        mPriv->values[mPriv->size] = value;
        /* XXX this is wrong - either we take ownership and copy both
           name and value,
           or none. The caller should do that if he needs !
        */
        /*     mPriv->names[mPriv->size] = m->pool->pstrdup(m->pool, name); */
        mPriv->names[mPriv->size] =  (char *)name; 
        mPriv->size ++;
        rc = JK_TRUE;
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
            if( rc != JK_TRUE )
                return rc;
        }
    }
    return JK_TRUE;
}
                               
/* ==================== */
/* Internal utils */


int jk2_map_default_create(jk_env_t *env, jk_map_t **m, jk_pool_t *pool )
{
    jk_map_t *_this;
    jk_map_private_t *mPriv;

    if( m== NULL )
        return JK_FALSE;
    
    _this=(jk_map_t *)pool->alloc(env, pool, sizeof(jk_map_t));
    mPriv=(jk_map_private_t *)pool->alloc(env, pool, sizeof(jk_map_private_t));
    *m=_this;

    if( _this == NULL || mPriv==NULL )
        return JK_FALSE;
    
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
    

    return JK_TRUE;
}

/* int map_free(jk_map_t **m) */
/* { */
/*     int rc = JK_FALSE; */

/*     if(m && *m) { */
/*         (*m)->pool->close((*m)->pool); */
/*         rc = JK_TRUE; */
/*         *m = NULL; */
/*     } */
/*     return rc; */
/* } */


static int jk2_map_default_realloc(jk_env_t *env, jk_map_t *m)
{
    jk_map_private_t *mPriv=m->_private;
    
    if(mPriv->size == mPriv->capacity) {
        char **names;
        void **values;
        int  capacity = mPriv->capacity + CAPACITY_INC_SIZE;

        names = (char **)m->pool->alloc(env, m->pool,
                                        sizeof(char *) * capacity);
        values = (void **)m->pool->alloc(env, m->pool,
                                         sizeof(void *) * capacity);
        
        if(values && names) {
            if (mPriv->capacity && mPriv->names) 
                memcpy(names, mPriv->names, sizeof(char *) * mPriv->capacity);

            if (mPriv->capacity && mPriv->values)
                memcpy(values, mPriv->values, sizeof(void *) * mPriv->capacity);

            mPriv->names = ( char **)names;
            mPriv->values = ( void **)values;
            mPriv->capacity = capacity;

            return JK_TRUE;
        }
    }

    return JK_FALSE;
}
