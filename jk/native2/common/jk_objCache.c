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

/* See jk_objCache.h for docs */

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_logger.h"
#include "jk_env.h"

#include "jk_objCache.h"

static int 
jk2_objCache_put(jk_env_t *env, jk_objCache_t *_this, void *obj)
{
    int rc;
    
    if(_this->size <= 0 )
        return JK_ERR;

    if( _this->cs != NULL )
        _this->cs->lock(env, _this->cs );

    rc=JK_FALSE;
    
    if( _this->count >= _this->size && _this->maxSize== -1 ) {
        /* Realloc */
        void **oldData=_this->data;
        _this->data =
            (void **)_this->pool->calloc(env, _this->pool,
                                         2 * sizeof(void *) * _this->size);
        memcpy( _this->data, oldData, _this->size );
        _this->size *= 2;
        /* XXX Can we destroy the old data ? Probably not.
           It may be better to have a list - but it's unlikely
           we'll have too many reallocations */
    }
    
    if( _this->count < _this->size ) {
        _this->data[_this->count++]=obj;
    }
    
    if( _this->cs != NULL )
        _this->cs->unLock(env, _this->cs );

    return JK_OK;
}

static int
jk2_objCache_init(jk_env_t *env, jk_objCache_t *_this, int cacheSize ) {
    jk_bean_t *jkb;
    
    if( cacheSize <= 0 ) {
        _this->size= 64;
    } else {
        _this->size= cacheSize ;
    }
    _this->count=0;
    _this->maxSize=cacheSize;
    
    _this->data =
        (void **)_this->pool->calloc(env, _this->pool,
                                     sizeof(void *) * _this->size);

    if( _this->data==NULL )
        return JK_ERR;
    
    jkb=env->createBean2(env, _this->pool,"threadMutex", NULL);
    if( jkb != NULL ) {
        _this->cs=jkb->object;
        jkb->init(env, jkb );
    }

    return JK_OK;
}

static int  
jk2_objCache_destroy(jk_env_t *env, jk_objCache_t *_this ) {

    if( _this->cs != NULL ) 
        _this->cs->mbean->destroy( env, _this->cs->mbean );

    _this->count=0;
    /* Nothing to free, we use pools */

    return JK_OK;
}


static void * 
jk2_objCache_get(jk_env_t *env, jk_objCache_t *_this )
{
    void *ae=NULL;

    if( _this->cs != NULL )
        _this->cs->lock(env, _this->cs );

    if( _this->count > 0 ) {
        _this->count--;
        ae=_this->data[ _this->count ];
    }

    if( _this->cs != NULL )
        _this->cs->unLock(env, _this->cs );

    return ae;
}

jk_objCache_t *jk2_objCache_create(jk_env_t *env, jk_pool_t *pool ) {
    jk_objCache_t *_this=pool->calloc( env, pool, sizeof( jk_objCache_t ));

    _this->pool=pool;

    _this->count=0;
    _this->size=0;
    _this->maxSize=-1;
    
    _this->get=jk2_objCache_get;
    _this->put=jk2_objCache_put;
    _this->init=jk2_objCache_init;
    _this->destroy=jk2_objCache_destroy;
    
    return _this;
}
