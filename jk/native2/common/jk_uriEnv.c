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
 * Location properties. UriEnv can be:
 *                                                                 
 * Exact Context -> /exact/uri=worker e.g. /examples/do[STAR]=ajp12
 * Context Based -> /context/[STAR]=worker e.g. /examples/[STAR]=ajp12
 * Context and suffix ->/context/[STAR].suffix=worker e.g. /examples/[STAR].jsp=ajp12
 *                                                                         
 */

#include "jk_pool.h"
#include "jk_env.h"
#include "jk_uriMap.h"
#include "jk_registry.h"

static int jk2_uriEnv_init(jk_env_t *env, jk_uriEnv_t *uriEnv);

static int jk2_uriEnv_parseUri( jk_env_t *env, jk_uriEnv_t *uriEnv,
                                char *name)
{
    char *n=name;
    char *slash=strchr( n, '/' );

    if( slash==NULL ) {
        env->l->jkLog( env, env->l, JK_LOG_ERROR,
                       "uriEnv.parseUri(): At least a '/' must be included %s\n", name);
        return JK_FALSE;
    }
    
    /* Cut the "uri." prefix, if any */
    if( (strncmp( name, "uri.", 4 ) == 0 )||
        (strncmp( name, "uri:", 4 ) == 0 )) {
        n=name+4;
    }
    /* Cut the " */
    if( *n == '"' ) n++;
    /* If it doesn't start with /, it must be a vhost */
    if( *n != '/' ) {
        char *portIdx=strchr( n, ':' );
        char *vhost=NULL;

        /* XXX Extract and set the vhost */
    }
    
    n=slash;
    
    uriEnv->uri=uriEnv->pool->pstrdup(env, uriEnv->pool, n);
    jk2_uriEnv_init(env, uriEnv);
    /*     env->l->jkLog( env, env->l, JK_LOG_INFO, */
    /*                    "uriEnv.parseUri() Setting path %s for %s\n", n, name); */
    return JK_TRUE;
}

static void *jk2_uriEnv_getAttribute(jk_env_t *env, jk_bean_t *bean,
                                     char *name )
{
    if( strcmp( name, "worker" ) ) {

    } else if( strcmp( name, "reqCount" ) ) {

    }
    return NULL;
}

static int jk2_uriEnv_setProperty(jk_env_t *env,
                                  jk_bean_t *mbean,
                                  char *nameParam,
                                  void *valueP)
{
    jk_uriEnv_t *uriEnv=mbean->object;
    char *valueParam=valueP;
    
    char *name=uriEnv->pool->pstrdup(env,uriEnv->pool, nameParam);
    char *val=uriEnv->pool->pstrdup(env,uriEnv->pool, valueParam);

    uriEnv->properties->add( env ,uriEnv->properties,
                             name, val );
    if( strcmp("worker", name) == 0 ) {
        uriEnv->workerName=val;
    }
    if( strcmp("debug", name) == 0 ) {
        uriEnv->debug=atoi( val );
    }
    if( strcmp("path", name) == 0 ) {
        if( val==NULL )
            uriEnv->uri=NULL;
        else
            uriEnv->uri=uriEnv->pool->pstrdup(env, uriEnv->pool, val);
    }
    if( strcmp("uri", name) == 0 ) {
        jk2_uriEnv_parseUri( env, uriEnv, val);
    }
    if( strcmp("vhost", name) == 0 ) {
        if( val==NULL )
            uriEnv->virtual=NULL;
        else
            uriEnv->virtual=uriEnv->pool->pstrdup(env, uriEnv->pool, val);
    }

    return JK_TRUE;
}


static int jk2_uriEnv_init(jk_env_t *env, jk_uriEnv_t *uriEnv)
{
    int err;
    char *asterisk;
    char *uri=uriEnv->pool->pstrdup( env, uriEnv->pool, uriEnv->uri);

    if( uri==NULL ) 
        return JK_FALSE;
    
    if ('/' != uri[0]) {
        /*
         * JFC: please check...
         * Not sure what to do, but I try to prevent problems.
         * I have fixed jk_mount_context() in apaches/mod_jk2.c so we should
         * not arrive here when using Apache.
         */
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriMap.addMapping() context must start with '/' in %s\n",
                      uri);
        return JK_FALSE;
    }

    asterisk = strchr(uri, '*');

    // set the mapping type
    if (!asterisk) {
        /* Something like:  JkMount /login/j_security_check ajp13 */
        uriEnv->prefix      = uri;
        uriEnv->prefix_len  =strlen( uriEnv->prefix );
        uriEnv->suffix      = NULL;
        uriEnv->match_type  = MATCH_TYPE_EXACT;
        if( uriEnv->debug > 0 ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "uriMap.addMapping() exact mapping %s=%s was added\n",
                          uri, uriEnv->workerName);
        }
        return JK_TRUE;
    }

    /*
     * Now, lets check that the pattern is /context/STAR.suffix
     * or /context/STAR
     * we need to have a '/' then a '*' and the a '.' or a
     * '/' then a '*'
     */
    asterisk--;
    if ('/' == asterisk[0]) {
        if ('.' == asterisk[2]) {
            /* suffix rule: /foo/bar/STAR.extension */
            asterisk[1]      = '\0';
            asterisk[2]      = '\0';
            uriEnv->prefix      = uri;
            uriEnv->prefix_len  =strlen( uriEnv->prefix );
            uriEnv->suffix      = asterisk + 3;
            uriEnv->match_type  = MATCH_TYPE_SUFFIX;
            if( uriEnv->debug > 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriMap.addMapping() suffix mapping %s.%s=%s was added\n",
                              uri, asterisk + 3, uriEnv->worker);
            }
        } else if ('\0' != asterisk[2]) {
            /* general suffix rule /foo/bar/STARextraData */
            asterisk[1] = '\0';
            uriEnv->suffix  = asterisk + 2;
            uriEnv->prefix  = uri;
            uriEnv->prefix_len  =strlen( uriEnv->prefix );
            uriEnv->match_type = MATCH_TYPE_GENERAL_SUFFIX;
            if( uriEnv->debug > 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                         "uriMap.addMapping() general suffix mapping %s.%s=%s\n",
                              uri, asterisk + 2, uriEnv->worker);
            }
        } else {
            /* context based /foo/bar/STAR  */
            asterisk[1]      = '\0';
            uriEnv->suffix      = NULL;
            uriEnv->prefix      = uri;
            uriEnv->prefix_len  =strlen( uriEnv->prefix );
            uriEnv->match_type  = MATCH_TYPE_CONTEXT;
            if( uriEnv->debug > 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                              "uriMap.addMapping() prefix mapping %s=%s\n",
                              uri, uriEnv->worker);
            }
        }
    } else {
        /* Something like : JkMount /servlets/exampl* ajp13 */
        /* Is this valid ??? */
        uriEnv->prefix      = uri;
        uriEnv->prefix_len  =strlen( uriEnv->prefix );
        uriEnv->suffix      = NULL;
        uriEnv->match_type  = MATCH_TYPE_EXACT;
        if( uriEnv->debug > 0 ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                     "uriMap.addMapping() prefix mapping2 %s=%s\n",
                          uri, uriEnv->worker);
        }
    }

    return JK_TRUE;
}


int JK_METHOD jk2_uriEnv_factory(jk_env_t *env, jk_pool_t *pool,
                                 jk_bean_t *result,
                                 const char *type, const char *name)
{
    jk_pool_t *uriPool;
    int err;
    jk_uriEnv_t *uriEnv;

    uriPool=(jk_pool_t *)pool->create( env, pool,
                                       HUGE_POOL_SIZE);

    uriEnv=(jk_uriEnv_t *)pool->calloc(env, uriPool,
                                       sizeof(jk_uriEnv_t));
    
    uriEnv->pool=uriPool;
    
    jk2_map_default_create( env, &uriEnv->properties, uriPool );

    uriEnv->init=jk2_uriEnv_init;

    result->setAttribute=&jk2_uriEnv_setProperty;
    result->getAttribute=&jk2_uriEnv_getAttribute;
    uriEnv->mbean=result;
    result->object=uriEnv;

    /* The name is a path */
    if( strchr( name, '/' ) != NULL ) {
        jk2_uriEnv_setProperty( env, result, "uri", (char *)name );
    }

    uriEnv->workerEnv=env->getByName( env, "workerEnv" );
    uriEnv->workerEnv->uriMap->addUriEnv( env, uriEnv->workerEnv->uriMap,
                                          uriEnv );
    return JK_TRUE;
}

