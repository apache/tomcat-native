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

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"

#ifndef WIN32

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
}

#endif
