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

/**
 * Scoreboard - used for communication on multi-process servers.
 *
 * This is an optional component - will be enabled only if APR is used. 
 * The code is cut&pasted from apache and mod_jserv.
 *
 * 
 * 
 * @author Jserv and Apache people
 */

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"

#ifdef HAS_APR

#include "apr.h"
#include "apr_strings.h"
#include "apr_portable.h"
#include "apr_lib.h"

#define APR_WANT_STRFUNC
#include "apr_want.h"

#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if APR_HAS_SHARED_MEMORY

#include "apr_shm.h"

/*
  Shared memory support for jk.

  Will be used for interprocess communication ( as a channel - eventually with a single copy ), and
  for configuration/monitoring.

  The model will be very similar with the apache scoreboard and jserv's load-balancing shmem.

  However we try to make it more flexible in order to support run-time configuration and 
  IPC.

  The shmem will consist of 'slots'. Each slot will be owned by a process.

  The tricky part is avoiding inter-process synchronization - that's done by a complex set of
  rules ( similar with the scoreboard and jserv).

  ( XXX The first impl. will just provide the jserv-style support for lb workers )
*/

struct jk_shm_buffer {
    /** Incremented after each modification */
    int generation;
    /** 1 if the buffer is in an unstable state.
     *  XXX Shouldn't happen
     */
    int busy;

    /** Pid of the owning process */
    int owner;
    
    int id;
    char name[64];
    
    char *data;
};

struct jk_shm_head {
    int generation;

    int bufferSize;
    int bufferCount;
    struct jk_shm_buffer *buffers;
};

typedef struct jk_shm {
    char *fname;
    apr_shm_t *aprShm;
    apr_size_t size;
    apr_pool_t *aprPool;
    
    int objCount;
    int lastId;
    

    /* XXX writelock */
    
    struct jk_shm_head *image;
} jk_shm_t;


static apr_pool_t *globalShmPool;

static int jk_shm_destroy(jk_env_t *env, jk_shm_t *shm)
{
    return apr_shm_destroy(shm->aprShm);
}

static int jk_shm_detach(jk_env_t *env, jk_shm_t *shm)
{
    return apr_shm_detach(shm->aprShm);
}

static int jk_shm_attach(jk_env_t *env, jk_shm_t *shm)
{

}


/* Create or reinit an existing scoreboard. The MPM can control whether
 * the scoreboard is shared across multiple processes or not
 */
static int jk_shm_createScoreboard(jk_env_t *env, jk_shm_t *shm)
{
    apr_status_t rv;

    /* We don't want to have to recreate the scoreboard after
     * restarts, so we'll create a global pool and never clean it.
     */
    rv = apr_pool_create(&globalShmPool, NULL);

    if (rv != APR_SUCCESS) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "Unable to create global pool for jk_shm\n");
        return rv;
    }

    /* The config says to create a name-based shmem */
    if ( shm->fname == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "No name for jk_shm\n");
        return JK_FALSE;
    }
    
    /* make sure it's an absolute pathname */
    /*  fname = ap_server_root_relative(pconf, ap_scoreboard_fname); */

    /* The shared memory file must not exist before we create the
     * segment. */
    apr_file_remove(shm->fname, globalShmPool ); /* ignore errors */

    rv = apr_shm_create(&shm->aprShm, shm->size, shm->fname, globalShmPool);
    
    if (rv) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "shm.create(): error creating named scoreboard %s %d\n",
                      shm->fname, rv);
        return rv;
    }

    shm->image = apr_shm_baseaddr_get( shm->aprShm);
    if( shm->image==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "shm.create(): No memory allocated %s\n",
                      shm->fname);
        return JK_FALSE;
    }
    
    memset(shm->image, 0, shm->size);

    return JK_TRUE;
}


int jk2_shm_factory( jk_env_t *env, jk_pool_t *pool,
                     jk_bean_t *result,
                     const char *type, const char *name)
{
    jk_shm_t *_this;

    _this=(jk_shm_t *)pool->alloc(env, pool, sizeof(jk_shm_t));
    if( _this == NULL )
        return JK_FALSE;

    /* result->setAttribute=jk2_scoreboard_setAttribute; */
    /* result->getAttribute=jk2_scoreboard_getAttribute; */
    /*     _this->mbean=result; */
    result->object=_this;

    /* result->aprPool=(apr_pool_t *)p->_private; */
        
    return JK_TRUE;
}

#endif /* APR_HAS_SHARED_MEMORY */
#endif /* HAS_APR */

