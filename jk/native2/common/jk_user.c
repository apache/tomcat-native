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

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"

#if !(defined(WIN32) || defined(NETWARE))

#include <unistd.h>
#include <pwd.h>

typedef struct jk_user {
    char *user;
    char *group;
} jk_user_t;

/* -------------------- User related functions -------------------- */
/* XXX move it to jk_user.c */

static int JK_METHOD jk2_user_setAttribute( jk_env_t *env, jk_bean_t *mbean, char *name, void *valueP ) {
    char *value=(char *)valueP;
    jk_user_t *usr=(jk_user_t *)mbean->object;
    
    if( strcmp( "user", name ) == 0 ) {
        usr->user=value;
    } else if( strcmp( "group", name ) == 0 ) {
        usr->group=value;
    } else {
	return JK_ERR;
    }
    return JK_OK;   

}

/** This is ugly, we should use a jk_msg to pass string/int/etc
 */
static void *JK_METHOD jk2_user_getAttribute( jk_env_t *env, jk_bean_t *mbean, char *name ) {
    
    if( strcmp( "pid", name ) == 0 ) {
        char *buf=env->tmpPool->calloc( env, env->tmpPool, 20 );
        sprintf( buf, "%d", getpid());
        return buf;
    }
    return NULL;   
}

static int JK_METHOD jk2_user_init(jk_env_t *env, jk_bean_t *chB )
{
    jk_user_t *usr=chB->object;
    struct passwd *passwd;
    int uid;
    int gid;
    int rc;
    
    if( usr->user == NULL ) return JK_OK;

    passwd = getpwnam(usr->user);
    if (passwd == NULL ) {
        return JK_ERR;
    }

    uid = passwd->pw_uid;
    gid = passwd->pw_gid;

    rc = setuid(uid);

    return rc;
}

int JK_METHOD jk2_user_factory( jk_env_t *env ,jk_pool_t *pool,
                                jk_bean_t *result,
                                const char *type, const char *name)
{
    result->setAttribute=jk2_user_setAttribute;
    result->getAttribute=jk2_user_getAttribute;
    result->object=pool->calloc( env, pool, sizeof( jk_user_t ));
    result->invoke=NULL;
    result->init=jk2_user_init;
    result->destroy=NULL;
    
    return JK_OK;
}



#else /* ! HAVE_SIGNALS */

int JK_METHOD jk2_user_factory( jk_env_t *env ,jk_pool_t *pool,
                                  jk_bean_t *result,
                                  const char *type, const char *name)
{
    result->disabled=JK_TRUE;
    return JK_FALSE;
}

#endif
