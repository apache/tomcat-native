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
 */

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_shm.h"

#include "apr_shm.h"
#include "apr_rmm.h"
#include "apr_errno.h"
#include "apr_general.h"
#include "apr_lib.h" 


#define SHM_WRITE_SLOT 2
#define SHM_RESET 5
#define SHM_DUMP 6


static int JK_METHOD jk2_shm_destroy(jk_env_t *env, jk_shm_t *shmem)
{
    apr_status_t rv = APR_SUCCESS;
#if APR_HAS_SHARED_MEMORY
    apr_shm_t *shm = (apr_shm_t *)shmem->privateData;

    if (shm) {
        if (shmem->attached)
            rv = apr_shm_detach(shm);
        else
            rv = apr_shm_destroy(shm);
    }
#endif
    shmem->head = NULL;
    shmem->image = NULL;

    return rv == APR_SUCCESS ? JK_OK : JK_ERR;
}

static int jk2_shm_create(jk_env_t *env, jk_shm_t *shmem)
{
    apr_status_t rc = APR_EGENERAL;
    apr_shm_t *shm = NULL;
    apr_pool_t *globalShmPool;

    globalShmPool = (apr_pool_t *)env->getAprPool(env);

    if (!globalShmPool) {
        return JK_ERR;
    }
    
    if (shmem->inmem) {
        shmem->head = apr_pcalloc(globalShmPool, sizeof(jk_shm_head_t) + shmem->slotMaxCount);
        shmem->image = apr_pcalloc(globalShmPool, shmem->slotMaxCount * shmem->slotSize);
        shmem->head->structSize = sizeof(jk_shm_head_t) + shmem->slotMaxCount;
        shmem->head->slotSize = shmem->slotSize;
        shmem->head->slotMaxCount = shmem->slotMaxCount;
        return JK_OK;
    }
#if APR_HAS_SHARED_MEMORY
    /* XXX: deal with anonymous */
    if (strncmp(shmem->fname, "anon", 4) == 0) {
        rc = apr_shm_create(&shm, shmem->size, NULL, globalShmPool);
        if (APR_STATUS_IS_ENOTIMPL(rc)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                          "shm.create(): Anonymous shared memory not implemented %d\n");
            shmem->privateData=NULL;
            return JK_ERR;
        }
    }
    if (rc != APR_SUCCESS) {
        rc = apr_shm_create(&shm, shmem->size, shmem->fname, globalShmPool);
        if (rc == APR_EEXIST) {
            /*
            * The shm could have already been created (i.e. we may be a child process).
            * See if we can attach to the existing shared memory.
            */
            rc = apr_shm_attach(&shm, shmem->fname, globalShmPool); 
            shmem->attached = 1;
        }
    }
    if (rc != APR_SUCCESS) {
        char ebuf[256];
        apr_strerror(rc, ebuf, 256);

        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "shm.create(): error creating shm %d %s\n",
                      rc, ebuf);
        shmem->privateData = NULL;
        return JK_ERR;
    } 
    shmem->privateData = shm;
    shmem->head = (jk_shm_head_t *)apr_shm_baseaddr_get(shm);

    if (!shmem->attached) {
        /* Allocate header */
        apr_size_t head_size = sizeof(jk_shm_head_t) + shmem->slotMaxCount;
        /* align to slotSize */
        head_size = APR_ALIGN(head_size, shmem->slotSize);
        memset(shmem->head, 0, head_size);
        if (shmem->head) {
            shmem->head->structSize = (int)head_size;
            shmem->head->slotSize = shmem->slotSize;
            shmem->head->slotMaxCount = shmem->slotMaxCount;
        }
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "shm.create() Created head %#lx size %d\n",
                       shmem->head, head_size);

    }
    else {
        shmem->slotSize = shmem->head->slotSize;
        shmem->slotMaxCount = shmem->head->slotMaxCount;
    }
    shmem->image = ((char *)apr_shm_baseaddr_get(shm)) + shmem->head->structSize;
#endif
    return JK_OK;
}


/* Create or reinit an existing scoreboard. The MPM can control whether
 * the scoreboard is shared across multiple processes or not
 */
static int JK_METHOD jk2_shm_init(struct jk_env *env, jk_shm_t *shm) {
    
    int rv=JK_OK;
    
    /* In case the shm has been initialized already
     * for the current process.
     */
    if (shm->head && shm->image)
        return rv;
        
    shm->privateData = NULL;

    if (shm->fname == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "shm.init(): shm file not specified\n");
        return JK_ERR;
    }
    if (!shm->slotMaxCount)
        shm->slotMaxCount = 1;
    shm->size = shm->slotSize * shm->slotMaxCount;
    shm->size += APR_ALIGN(sizeof(jk_shm_head_t) + shm->slotMaxCount, shm->slotSize);
    
    /* The smalles size is 64K */
    shm->size = APR_ALIGN(shm->size, 1024 * 64);

    if  (shm->mbean->debug > 0) {
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "shm.init(): file=%s size=%d\n",
                      shm->fname, shm->size);
    }

    rv = jk2_shm_create(env, shm);
    
    if (rv != JK_OK) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "shm.create(): error creating shm %s\n",
                      shm->fname);
        return rv;
    }

    if (shm->mbean->debug > 0)
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
        "shm.create(): shm created %#lx %#lx %d\n",
        shm->head, shm->image, shm->attached);

    return JK_OK;
}

/** Reset the scoreboard, in case it gets corrupted.
 *  Will remove all slots and set the head in the original state.
 */
static int JK_METHOD jk2_shm_reset(jk_env_t *env, jk_shm_t *shm)
{
    if (shm->head == NULL) {
        return JK_ERR;
    }

    shm->head->slotSize = shm->slotSize;
    shm->head->slotMaxCount = shm->slotMaxCount;
    shm->head->lastSlot = 0;
    memset(shm->head->slots, 0, shm->head->slotMaxCount); 
    if (shm->mbean->debug > 0) {
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "shm.init() Reset %s %#lx\n",
                      shm->fname, shm->image);
    }
    return JK_OK;
}


/* pos starts with 1 ( 0 is the head )
 */
jk_shm_slot_t * JK_METHOD jk2_shm_getSlot(struct jk_env *env,
                                          struct jk_shm *shm, int pos)
{
    jk_shm_slot_t *slot = NULL;

    if (!shm->image) 
        return NULL;
    if (pos >= shm->slotMaxCount)
        return NULL;
    /* Use only allocated slots */
    if (shm->head->slots[pos])
        slot = (jk_shm_slot_t *)((char *)shm->image + pos * shm->slotSize);
    if (slot) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "shm.getSlot() found existing slot %d %s\n",
                      pos * shm->slotSize,
                      slot->name);
    }
    return slot;
}

jk_shm_slot_t * JK_METHOD jk2_shm_createSlot(struct jk_env *env,
                                             struct jk_shm *shm, 
                                             char *name, int size)
{
    /* For now all slots are equal size
     */
    int i;
    jk_shm_slot_t *slot;

    if (shm->head != NULL) { 
        for (i = 0; i < shm->head->lastSlot; i++) {
            slot = shm->getSlot(env, shm, i);
            if (strncmp(slot->name, name, strlen(name)) == 0) {
                env->l->jkLog(env, env->l, JK_LOG_INFO, 
                              "shm.createSlot() found existing slot %s\n",
                              slot->name);
                return slot;
            }
        }
    }
    else {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "shm.createSlot() no shared memory head\n");
        return NULL;
    }
    /* Allocate new slot  
     * TODO: We need the mutex here
     */
    for (i = shm->head->lastSlot; i < shm->head->slotMaxCount; i++) {
        if (!shm->head->slots[i]) {
            slot = (jk_shm_slot_t *)((char *)shm->image + i * shm->head->slotSize);
            /* Mark the slot as used */
            shm->head->slots[i] = 1;
            /* Clear any garbage data */
            memset(slot, 0, shm->head->slotSize);
            ++shm->head->lastSlot;
            break;
        }
    }
    if (slot == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
            "shm.createSlot() create %d returned NULL\n",
            shm->slotSize);
        return NULL;
    }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "shm.createSlot() Created slot %d\n",
                  shm->head->lastSlot);
    strncpy(slot->name, name, 64);
    slot->size = size;
    return slot;
}

/** Get an ID that is unique across processes.
 */
int JK_METHOD jk2_shm_getId(struct jk_env *env, struct jk_shm *shm)
{

    return 0;
}

static void jk2_shm_resetEndpointStats(jk_env_t *env, struct jk_shm *shm)
{
    int i, j;
    
    if (!shm || !shm->head) {
        return;
    }
    
    for (i = 0; i < shm->head->lastSlot; i++) {
        jk_shm_slot_t *slot = shm->getSlot(env, shm, i);
        
        if (slot == NULL)
            continue;
        
        if (strncmp(slot->name, "epStat", 6) == 0) {
            /* This is an endpoint slot */
            void *data = slot->data;

            for (j = 0; j < slot->structCnt; j++) {
                jk_stat_t *statArray = (jk_stat_t *)data;
                jk_stat_t *stat = statArray + j;

                stat->reqCnt = 0;
                stat->errCnt = 0;
                stat->totalTime = 0;
                stat->maxTime = 0;
            }
        }
    }    
}


static char *jk2_shm_setAttributeInfo[] = {"resetEndpointStats",
                                           "file", "size",
                                           "slots", "useMemory",
                                           NULL};

static int JK_METHOD jk2_shm_setAttribute(jk_env_t *env, jk_bean_t *mbean,
                                          char *name, void *valueP ) 
{
    jk_shm_t *shm=(jk_shm_t *)mbean->object;
    char *value=(char *)valueP;
    
    if( strcmp( "file", name ) == 0 ) {
        shm->fname=value;
    } else if( strcmp( "size", name ) == 0 ) {
        shm->size=atoi(value);
    } else if( strcmp( "slots", name ) == 0 ) {
        shm->slotMaxCount=atoi(value);
    } else if( strcmp( "useMemory", name ) == 0 ) {
        shm->inmem=atoi(value);
    } else if( strcmp( "resetEndpointStats", name ) == 0 ) {
        if( strcmp( value, "1" )==0 )
            jk2_shm_resetEndpointStats( env, shm );
    } else {
        return JK_ERR;
    }
    return JK_OK;   

}

static char *jk2_shm_getAttributeInfo[] = {"file", "size", "slots", "useMemory", NULL};

static void * JK_METHOD jk2_shm_getAttribute(jk_env_t *env, jk_bean_t *mbean, char *name )
{
    jk_shm_t *shm = (jk_shm_t *)mbean->object;

    if( strcmp( name, "file" )==0 ) {
        return shm->fname;
    } else if( strcmp( name, "size" ) == 0 ) {
        return jk2_env_itoa( env, shm->size );
    } else if( strcmp( name, "slots" ) == 0 ) {
        return jk2_env_itoa( env, shm->slotMaxCount );
    } else if( strcmp( name, "useMemory" ) == 0 ) {
        return jk2_env_itoa( env, shm->inmem );
    }
    return NULL;
}


/** Copy a chunk of data into a named slot
 */
static int jk2_shm_writeSlot( jk_env_t *env, jk_shm_t *shm,
                              char *instanceName, char *buf, int len )
{
    jk_shm_slot_t *slot;
    
    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "shm.writeSlot() %s %d\n", instanceName, len );
    if ((size_t)len > (shm->slotSize - sizeof(jk_shm_slot_t))) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "shm.writeSlot() Packet too large %d %d\n",
                      shm->slotSize - sizeof(jk_shm_slot_t),
                      len);
        return JK_ERR;
    }
    if (shm->head == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "shm.writeSlot() No head - shm was not initalized\n");
        return JK_ERR;
    }
    
    slot = shm->createSlot(env, shm, instanceName, 0);
    
    /*  Copy the body in the slot */
    memcpy(slot->data, buf, len);
    slot->size = len;
    slot->ver++;
    /* Update the head lb version number - that would triger
       reconf on the next request */
    shm->head->lbVer++;

    return JK_OK;
}

/* ==================== Dispatch messages from java ==================== */
    
/** Called by java. Will call the right shm method.
 */
static int JK_METHOD jk2_shm_invoke(jk_env_t *env, jk_bean_t *bean, jk_endpoint_t *ep, int code,
                                    jk_msg_t *msg, int raw)
{
    jk_shm_t *shm=(jk_shm_t *)bean->object;

    if( shm->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                      "shm.%d() \n", code);
    
    switch( code ) {
    case SHM_WRITE_SLOT: {
        char *instanceName=msg->getString( env, msg );
        char *buf=(char *)msg->buf;
        int len=msg->len;

        return jk2_shm_writeSlot( env, shm, instanceName, buf, len );
    }
    case SHM_RESET: {
        jk2_shm_reset( env, shm );

        return JK_OK;
    }
    case SHM_DUMP: {
#if 0
        char *name=msg->getString( env, msg );
        /* XXX do we realy need that */
        jk2_shm_dump( env, shm, name );
#endif
        return JK_OK;
    }
    }/* switch */
    return JK_ERR;
}

int JK_METHOD jk2_shm_factory( jk_env_t *env ,jk_pool_t *pool,
                               jk_bean_t *result,
                               const char *type, const char *name)
{
    jk_shm_t *shm;

    shm=(jk_shm_t *)pool->calloc(env, pool, sizeof(jk_shm_t));

    if( shm == NULL )
        return JK_ERR;

    shm->pool = pool;
    shm->privateData = NULL;

    shm->slotSize = DEFAULT_SLOT_SIZE;
    shm->slotMaxCount = DEFAULT_SLOT_COUNT;
    
    result->setAttribute = jk2_shm_setAttribute;
    result->setAttributeInfo = jk2_shm_setAttributeInfo;
    /* Add the following to this function - seems someone else */
    /* thought of it based on the 'comment' previously there */
    result->getAttributeInfo = jk2_shm_getAttributeInfo;
    result->getAttribute = jk2_shm_getAttribute;
    result->multiValueInfo = NULL;
    shm->mbean = result; 
    result->object = shm;
    result->invoke=jk2_shm_invoke;
    shm->init = jk2_shm_init;
    shm->destroy = jk2_shm_destroy;
    
    shm->getSlot = jk2_shm_getSlot;
    shm->createSlot = jk2_shm_createSlot;
    shm->reset = jk2_shm_reset;
    
    return JK_OK;
}

