/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2002 The Apache Software Foundation.          *
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

#ifdef HAS_APR
#include "apr_uri.h"
/** Parse the name:
       VHOST/PATH

    If VHOST is empty, we map to the default host.

    The PATH will be further split in CONTEXT/LOCALPATH during init ( after
    we have all uris, including the context paths ).
*/
static int jk2_uriEnv_parseName( jk_env_t *env, jk_uriEnv_t *uriEnv,
                                 char *name)
{

    apr_uri_t uri;
    char s[1024];

    /* If it uri starts with / then it is simple 
     * default host uri
     */
    if (*name == '/')
        strcpy(s, name);
    else {
        strcpy(s, "http://");
        strcat(s, name);
    }
    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "uriEnv.parseName() uri %s name %s real %s\n", 
                  uriEnv->name, name, s);
    
    if (apr_uri_parse(uriEnv->pool->_private, s, &uri) == APR_SUCCESS) {
        
        uriEnv->port = uri.port;
        if (uri.hostname) {
            if (!uriEnv->port)
                uriEnv->virtual = uri.hostname;
            else
                uriEnv->virtual = apr_pstrcat(uriEnv->pool->_private,
                                    uri.hostname, ":", uri.port_str, NULL);
        }
        else
            uriEnv->virtual = "*";
        uriEnv->uri = uri.path;
        if (!uri.path)
            uriEnv->match_type = MATCH_TYPE_HOST;
        return JK_OK;
    }
    return JK_ERR;
}
#else
static int jk2_uriEnv_parseName( jk_env_t *env, jk_uriEnv_t *uriEnv,
                                 char *name)
{
    char *uri = NULL;
    char *colon;
    char host[1024];
    char path[1024];

    strcpy(host, name);
    colon = strchr(host, ':');
    if (colon != NULL) {
        ++colon;
        uri = strchr(colon, '/');
    }
    else
        uri = strchr(host, '/');
            
    if (!uri) {
        /* That's a virtual host definition ( no actual mapping, just global
         * settings like aliases, etc
         */
        
        uriEnv->match_type = MATCH_TYPE_HOST;
        if (colon)
            uriEnv->port = atoi(colon);
        uriEnv->virtual = uriEnv->pool->pstrdup(env, uriEnv->pool, host);
        return JK_OK;
    }
    strcpy(path, uri);
    if (colon) {
        *uri = '\0';
        uriEnv->port = atoi(colon);
    }
    /* If it doesn't start with /, it must have a vhost */
    if (strlen(host) && uri != host) {
        uriEnv->virtual = uriEnv->pool->calloc( env, uriEnv->pool, strlen(host) + 1 );
        strncpy(uriEnv->virtual, name, strlen(host));
    }
    else
        uriEnv->virtual = "*";
   uriEnv->uri = uriEnv->pool->pstrdup(env, uriEnv->pool, path);
    
    return JK_OK;
}
#endif /* HAS_APR */


static void * JK_METHOD jk2_uriEnv_getAttribute(jk_env_t *env, jk_bean_t *bean,
                                     char *name )
{
    jk_uriEnv_t *uriEnv=(jk_uriEnv_t *)bean->object;
    
    if( strcmp( name, "host" )==0 ) {
        return  (uriEnv->virtual==NULL) ? "*" : uriEnv->virtual;
    } else if( strcmp( name, "uri" )==0 ) {
        return uriEnv->uri;
    } else if( strcmp( name, "group" )==0 ) {
        return uriEnv->workerName;
    }
    return NULL;
}

static int JK_METHOD jk2_uriEnv_setAttribute(jk_env_t *env,
                                  jk_bean_t *mbean,
                                  char *nameParam,
                                  void *valueP)
{
    jk_uriEnv_t *uriEnv=mbean->object;
    char *valueParam=valueP;
    
    char *name = uriEnv->pool->pstrdup(env,uriEnv->pool, nameParam);
    char *val = uriEnv->pool->pstrdup(env,uriEnv->pool, valueParam);

    uriEnv->properties->add(env ,uriEnv->properties,
                            name, val);

    if (strcmp("group", name) == 0) {
        uriEnv->workerName = val;
        return JK_OK;
    } 
    else if (strcmp("context", name) == 0) {        
        uriEnv->contextPath=val;
        uriEnv->ctxt_len = strlen(val);

        if (strcmp(val, uriEnv->uri) == 0) {
            uriEnv->match_type = MATCH_TYPE_CONTEXT;
        }
        return JK_OK;
    } 
    else if (strcmp("servlet", name) == 0) {
        uriEnv->servlet=val;
    } 
    else if (strcmp("timing", name) == 0) {
        uriEnv->timing = atoi(val);
    }
    else if (strcmp("alias", name) == 0) {
        if (uriEnv->match_type == MATCH_TYPE_HOST) {
            if (uriEnv->aliases == NULL) {
                jk2_map_default_create(env, &uriEnv->aliases, mbean->pool);
            }
            uriEnv->aliases->put(env, uriEnv->aliases, val, uriEnv, NULL);
        }
    }
    else {
        /* OLD - DEPRECATED */
        int d = 1;
        if (strcmp("worker", name) == 0) {
            d = 1;
            uriEnv->workerName = val;
        } 
        else if (strcmp("path", name) == 0) {
            if (val == NULL)
                uriEnv->uri = NULL;
            else
                uriEnv->uri = uriEnv->pool->pstrdup(env, uriEnv->pool, val);
        }
        else if (strcmp("uri", name) == 0) {
            jk2_uriEnv_parseName(env, uriEnv, val);
        } 
        else if (strcmp("name", name) == 0) {
            jk2_uriEnv_parseName(env, uriEnv, val);
        }
        else if (strcmp("vhost", name) == 0) {
            if (val == NULL)
                uriEnv->virtual = NULL;
            else
                uriEnv->virtual = uriEnv->pool->pstrdup(env, uriEnv->pool, val);
        }
        else
            d = 0;
        if (d)
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                    "uriEnv.setAttribute() the %s directive is depriciated\n",
                    name);
        
    }
    return JK_OK;
}


static int jk2_uriEnv_init(jk_env_t *env, jk_uriEnv_t *uriEnv)
{
/*    int err; */
    char *asterisk;
    char *uri=uriEnv->pool->pstrdup( env, uriEnv->pool, uriEnv->uri);

    /* Set the worker */
    char *wname=uriEnv->workerName;

    if( uriEnv->workerEnv->timing == JK_TRUE ) {
        uriEnv->timing=JK_TRUE;
    }
    if( uriEnv->workerName == NULL ) {
        /* The default worker */
        uriEnv->workerName=uriEnv->uriMap->workerEnv->defaultWorker->mbean->name;;
        uriEnv->worker=uriEnv->uriMap->workerEnv->defaultWorker;

        if( uriEnv->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriEnv.init() map %s %s\n",
                          uriEnv->uri, uriEnv->uriMap->workerEnv->defaultWorker->mbean->name);
    }

    /* No further init - will be called by uriMap.init() */

    if( uriEnv->workerName != NULL && uriEnv->worker==NULL ) {
        uriEnv->worker= env->getByName( env, wname );
        if( uriEnv->worker==NULL ) {
            uriEnv->worker= env->getByName2( env, "lb", wname );
            if( uriEnv->worker==NULL ) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "uriEnv.init() map to invalid worker %s %s\n",
                              uriEnv->uri, uriEnv->workerName);
                /* XXX that's allways a 'lb' worker, create it */
            }
        }
    } 
    
    if( uri==NULL ) 
        return JK_ERR;
    
    if ('/' != uri[0]) {
        /*
         * JFC: please check...
         * Not sure what to do, but I try to prevent problems.
         * I have fixed jk_mount_context() in apaches/mod_jk2.c so we should
         * not arrive here when using Apache.
         */
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriEnv.init() context must start with '/' in %s\n",
                      uri);
        return JK_ERR;
    }

    asterisk = strchr(uri, '*');

    /* set the mapping type */
    if (!asterisk) {
        /* Something like:  JkMount /login/j_security_check ajp13 */
        uriEnv->prefix      = uri;
        uriEnv->prefix_len  =strlen( uriEnv->prefix );
        uriEnv->suffix      = NULL;
        if( uriEnv->match_type != MATCH_TYPE_CONTEXT &&
            uriEnv->match_type != MATCH_TYPE_HOST ) {
            /* Context and host maps do not have ASTERISK */
            uriEnv->match_type  = MATCH_TYPE_EXACT;
        }
        if( uriEnv->mbean->debug > 0 ) {
            if( uriEnv->match_type == MATCH_TYPE_CONTEXT ) {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriEnv.init() context mapping %s=%s \n",
                              uriEnv->prefix, uriEnv->workerName);
                
            } else if( uriEnv->match_type == MATCH_TYPE_HOST ) {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriEnv.init() host mapping %s=%s \n",
                              uriEnv->virtual, uriEnv->workerName);
            } else {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriEnv.init() exact mapping %s=%s \n",
                              uriEnv->prefix, uriEnv->workerName);
            }
        }
        return JK_OK;
    }

    /*
     * We have an * in the uri. Check the type.
     *  - /context/ASTERISK.suffix
     *  - /context/PREFIX/ASTERISK
     *
     *  Unsupported:
     *  - context path:       /ASTERISK/prefix
     *  - general suffix rule /context/prefix/ASTERISKsuffix
     */
    asterisk--;
    if ('/' == asterisk[0]) {
        if ( 0 == strncmp("/*/",uri,3) ) {
            /* general context path */
            asterisk[1] = '\0';
            uriEnv->suffix  = asterisk + 2;
            uriEnv->prefix = uri;
            uriEnv->prefix_len  =strlen( uriEnv->prefix );
            uriEnv->match_type = MATCH_TYPE_CONTEXT_PATH;

            if( uriEnv->mbean->debug > 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "Into jk_uri_worker_map_t::uri_worker_map_open, "
                              "general context path rule %s * %s -> %s was added\n",
                              uri, asterisk + 2, uriEnv->workerName);
            }
        } else if ('.' == asterisk[2]) {
            /* suffix rule: /context/ASTERISK.extension */
            asterisk[1]      = '\0';
            asterisk[2]      = '\0';
            uriEnv->prefix      = uri;
            uriEnv->prefix_len  =strlen( uriEnv->prefix );
            uriEnv->suffix      = asterisk + 3;
            uriEnv->match_type  = MATCH_TYPE_SUFFIX;
            if( uriEnv->mbean->debug > 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriEnv.init() suffix mapping %s .%s=%s was added\n",
                              uriEnv->prefix, uriEnv->suffix, uriEnv->workerName);
            }
        } else if ('\0' != asterisk[2]) {
            /* general suffix rule /context/prefix/ASTERISKextraData */
            asterisk[1] = '\0';
            uriEnv->suffix  = asterisk + 2;
            uriEnv->prefix  = uri;
            uriEnv->prefix_len  =strlen( uriEnv->prefix );
            uriEnv->match_type = MATCH_TYPE_GENERAL_SUFFIX;
            if( uriEnv->mbean->debug > 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriEnv.init() general suffix mapping %s.%s=%s\n",
                              uri, asterisk + 2, uriEnv->workerName);
            }
        } else {
            /* context based /context/prefix/ASTERISK  */
            asterisk[1]      = '\0';
            asterisk[0]      = '\0'; /* Remove the extra '/' */
            uriEnv->suffix      = NULL;
            uriEnv->prefix      = uri;
            uriEnv->prefix_len  =strlen( uriEnv->prefix );
            uriEnv->match_type  = MATCH_TYPE_PREFIX;
            if( uriEnv->mbean->debug > 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriEnv.init() prefix mapping %s=%s\n",
                              uriEnv->prefix, uriEnv->workerName);
            }
        }
    } else {
        /* Something like : JkMount /servlets/exampl* ajp13 */
        /* Is this valid ??? */
        uriEnv->prefix      = uri;
        uriEnv->prefix_len  =strlen( uriEnv->prefix );
        uriEnv->suffix      = NULL;
        uriEnv->match_type  = MATCH_TYPE_PREFIX;
        if( uriEnv->mbean->debug > 0 ) {
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriEnv.init() prefix mapping2 %s=%s\n",
                          uri, uriEnv->workerName);
        }
    }
    if( uriEnv->mbean->debug > 0 )
        env->l->jkLog( env, env->l, JK_LOG_DEBUG,
                       "uriEnv.init()  %s  host=%s  uri=%s type=%d ctx=%s prefix=%s suffix=%s\n",
                       uriEnv->mbean->name, uriEnv->virtual, uriEnv->uri,
                       uriEnv->match_type, uriEnv->contextPath, uriEnv->prefix, uriEnv->suffix );
    
    return JK_OK;
}

static char *myAttInfo[]={ "host", "uri", "group", NULL };
    
int JK_METHOD jk2_uriEnv_factory(jk_env_t *env, jk_pool_t *pool,
                                 jk_bean_t *result,
                                 const char *type, const char *name)
{
    jk_pool_t *uriPool;
    jk_uriEnv_t *uriEnv;

    uriPool=(jk_pool_t *)pool->create( env, pool,
                                       HUGE_POOL_SIZE);

    uriEnv=(jk_uriEnv_t *)pool->calloc(env, uriPool,
                                       sizeof(jk_uriEnv_t));
    
    uriEnv->pool = uriPool;
    
    jk2_map_default_create(env, &uriEnv->properties, uriPool);

    uriEnv->init = jk2_uriEnv_init;

    result->setAttribute = jk2_uriEnv_setAttribute;
    result->getAttribute = jk2_uriEnv_getAttribute;
    uriEnv->mbean = result;
    result->object = uriEnv;
    result->getAttributeInfo = myAttInfo;

    uriEnv->name = result->localName;
    if (jk2_uriEnv_parseName(env, uriEnv, result->localName) != JK_OK) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriEnv.factory() error parsing %s\n",
                      uriEnv->name);
        return JK_ERR;
    }
    uriEnv->workerEnv = env->getByName(env, "workerEnv");
    uriEnv->workerEnv->uriMap->addUriEnv(env, uriEnv->workerEnv->uriMap,
                                         uriEnv);
    uriEnv->uriMap = uriEnv->workerEnv->uriMap;

    return JK_OK;
}
