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
 * Description: URI to worker map object.                          
 * Maps can be                                                     
 *                                                                 
 * Exact Context -> /exact/uri=worker e.g. /examples/do[STAR]=ajp12
 * Context Based -> /context/[STAR]=worker e.g. /examples/[STAR]=ajp12
 * Context and suffix ->/context/[STAR].suffix=worker e.g. /examples/[STAR].jsp=ajp12
 *                                                                         
 * This lets us either partition the work among the web server and the     
 * servlet container.                                                      
 *                                                                         
 * @author: Gal Shachor <shachor@il.ibm.com>
 * @author: Costin Manolache
 */

#include "jk_pool.h"
#include "jk_env.h"
#include "jk_uriMap.h"
#include "jk_registry.h"

static INLINE const char *jk2_findExtension(jk_env_t *env, const char *uri);

static int jk2_uriMap_checkUri(jk_env_t *env, jk_uriMap_t *uriMap, 
                               const char *uri);

/*
 * We are now in a security nightmare, it maybe that somebody sent 
 * us a uri that looks like /top-secret.jsp. and the web server will 
 * fumble and return the jsp content. 
 *
 * To solve that we will check for path info following the suffix, we 
 * will also check that the end of the uri is not ".suffix.",
 * ".suffix/", or ".suffix ".
 *
 * Was: check_security_fraud
 */
static int jk2_uriMap_checkUri(jk_env_t *env, jk_uriMap_t *uriMap, 
                               const char *uri)
{
    int i;    

    for(i = 0 ; i < uriMap->maps->size( env, uriMap->maps ) ; i++) {
        jk_uriEnv_t *uriEnv=uriMap->maps->valueAt( env, uriMap->maps, i );
        
        if(MATCH_TYPE_SUFFIX == uriEnv->match_type) {
            char *suffix_start;
            for(suffix_start = strstr(uri, uriEnv->suffix) ;
                suffix_start ;
                suffix_start =
                    strstr(suffix_start + 1, uriEnv->suffix)) {
                
                if('.' != *(suffix_start - 1)) {
                    continue;
                } else {
                    char *after_suffix = suffix_start +
                        strlen(uriEnv->suffix);
                
                    if((('.' == *after_suffix) ||
                        ('/' == *after_suffix) ||
                        (' ' == *after_suffix)) &&
                       (0 == strncmp(uriEnv->prefix, uri,
                                     uriEnv->prefix_len))) {
                        /* 
                         * Security violation !!!
                         * this is a fraud.
                         */
                        return JK_ERR;
                    }
                }
            }
        }
    }

    return JK_OK;
}


/** Add a uri mapping. Called during uri: initialization. Will just copy the
    uri in the table ( XXX use a map keyed on name ). In init() we process this
    and set the right structures.
 */
static int jk2_uriMap_addUriEnv(jk_env_t *env, jk_uriMap_t *uriMap,
                                jk_uriEnv_t *uriEnv)
{
    uriMap->maps->put(env, uriMap->maps, uriEnv->name, uriEnv, NULL);
    if (uriMap->mbean->debug > 0) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.addUriEnv() %s %s %s\n", uriEnv->name,
                      uriEnv->virtual, uriEnv->uri);
    return JK_OK;
}

static int JK_METHOD jk2_uriMap_setProperty(jk_env_t *env, jk_bean_t *mbean,
                                            char *name, void *valueP)
{
    jk_uriMap_t *uriMap = mbean->object;
    char *value = valueP;
    
    return JK_OK;
}

static jk_uriEnv_t *jk2_uriMap_prefixMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                         jk_map_t *mapTable, const char *uri, 
                                         int uriLen)
{
    int best_match = 0;
    jk_uriEnv_t *match = NULL;
    int i;
    
    int sz = mapTable->size(env, mapTable);
    for (i = 0; i < sz; i++) {
        jk_uriEnv_t *uwr = mapTable->valueAt(env, mapTable, i);

        if (uriLen < uwr->prefix_len)
            continue;
        if (strncmp(uri, uwr->prefix, uwr->prefix_len) == 0) {
            if (uwr->prefix_len > best_match) {
                best_match=uwr->prefix_len;
                match=uwr;
            }
        }
    }
    return match;
}

static jk_uriEnv_t *jk2_uriMap_contextMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                          jk_map_t *mapTable, const char *uri, 
                                          int uriLen)
{
    int i;    
    int sz = mapTable->size(env, mapTable);

    for (i = 0; i < sz; i++) {
        jk_uriEnv_t *uwr = mapTable->valueAt(env, mapTable, i);
        if (uriLen != uwr->prefix_len - 1)
            continue;
        if (strncmp(uri, uwr->prefix, uriLen) == 0) {
            return uwr;
        }
    }
    return NULL;
}

static jk_uriEnv_t *jk2_uriMap_exactMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                        jk_map_t *mapTable, const char *uri, 
                                        int uriLen)
{
    int i;
    int sz = mapTable->size(env, mapTable);
    jk_uriEnv_t *match=NULL;
    
    for (i = 0; i < sz; i++) {
        jk_uriEnv_t *uwr = mapTable->valueAt( env, mapTable, i);
        
        if (uriLen != uwr->prefix_len)
            continue;
        if (strncmp(uri, uwr->prefix, uriLen) == 0) {
            return uwr;
        }
    }
    return NULL;
}

static jk_uriEnv_t *jk2_uriMap_suffixMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                         jk_map_t *mapTable, const char *suffix, 
                                         int suffixLen)
{
    int i;
    int sz = mapTable->size( env, mapTable);

    for (i = 0; i < sz; i++) {
        jk_uriEnv_t *uwr = mapTable->valueAt(env, mapTable, i);

        /* for WinXX, fix the JsP != jsp problems */
#ifdef WIN32                        
        if (strcasecmp(suffix, uwr->suffix) == 0)  {
#else
            if (strcmp(suffix, uwr->suffix) == 0) {
#endif
                if (uriMap->mbean->debug > 0) {
                    env->l->jkLog(env, env->l,JK_LOG_DEBUG,
                                  "uriMap.mapUri() suffix match %s\n",
                                  uwr->suffix);
                }
                return uwr;
            /* indentation trick */
#ifdef WIN32                        
            }
#else
        }
#endif
    }
    return NULL;
}

/* Find the vhost */
static jk_uriEnv_t *jk2_uriMap_hostMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                       const char *vhost, int port)
{
    int i, j;
    char *name;
    char vs[1024] = {0};
    char vv[1024] = {0};

    int n = uriMap->vhosts->size(env, uriMap->vhosts);
    if (port) {
        if (vhost && strchr(vhost, ':'))
            strcpy(vs, vhost);
        else
            sprintf(vs, "%s:%d", vhost ? vhost : "*", port);
        sprintf(vv, "*:%d", port);
    }
    else
        strcpy(vs, vhost ? vhost : "*"); 

    for (i = 0 ; i < n ; i++) {
        jk_uriEnv_t *uriEnv = uriMap->vhosts->valueAt(env, uriMap->vhosts, i);
        name = uriMap->vhosts->nameAt(env, uriMap->vhosts, i);
        /* Host name is not case sensitive */
        if (strcasecmp(name, vs) == 0) {
            if (port == 0 || port == uriEnv->port)
                return uriEnv;
        }
        else if (uriEnv->aliases) {
            int m = uriEnv->aliases->size(env, uriEnv->aliases);
            for (j = 0; j < m; j++) {
                name = uriEnv->aliases->nameAt(env, uriEnv->aliases, j);
                if (strcasecmp(name, vhost) == 0) {
                    if (port == 0 || port == uriEnv->port)
                        return uriEnv;
                }
            }
        }
        else if (port && strcasecmp(name, vv) == 0) {
            return uriEnv;
        }
    }
    return uriMap->vhosts->get(env, uriMap->vhosts, "*");
}

static void jk2_uriMap_createHosts(jk_env_t *env, jk_uriMap_t *uriMap)
{
    int i;

    for (i = 0; i < uriMap->maps->size(env, uriMap->maps); i++) {
        jk_uriEnv_t *uriEnv = uriMap->maps->valueAt(env, uriMap->maps, i);
        if (uriEnv == NULL) 
            continue;
        if (uriEnv->virtual != NULL && strlen(uriEnv->virtual)) {
            if (uriEnv->match_type == MATCH_TYPE_HOST) {
                jk2_map_default_create(env, &uriEnv->webapps, uriMap->pool);
                uriMap->vhosts->put(env, uriMap->vhosts,
                                    uriEnv->virtual, uriEnv, NULL);
            } 
            else { /* Create the missing vhosts */
                if (!uriMap->vhosts->get(env, uriMap->vhosts,
                                         uriEnv->virtual)) {
                    jk2_map_default_create(env, &uriEnv->webapps, uriMap->pool);
                    uriMap->vhosts->put(env, uriMap->vhosts,
                                        uriEnv->virtual, uriEnv, NULL);

                    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                                  "uriMap.init() Fixing Host %s\n", 
                                  uriEnv->virtual);
                }
            }
        }
    }

    /** Make sure each vhost has a default context
     */
    for(i = 0; i < uriMap->vhosts->size(env, uriMap->vhosts); i++) {
        jk_uriEnv_t *hostEnv = uriMap->vhosts->valueAt(env, uriMap->vhosts, i);
        jk_uriEnv_t *rootCtx;
        char *uriPath;
        
        if (hostEnv->virtual != NULL) {
            uriPath=env->tmpPool->calloc(env, env->tmpPool,
                                         strlen(hostEnv->virtual) + 3);
            strcpy(uriPath, hostEnv->virtual);
            strcat(uriPath, "/");
        } else {
            uriPath = "/";
        }
        
        rootCtx = env->getByName2(env, "uri", uriPath);
        if (rootCtx == NULL) {
            env->createBean2(env, uriMap->mbean->pool, "uri", uriPath);
            rootCtx = env->getByName2(env, "uri", uriPath);
            if (uriMap->mbean->debug > 0) 
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriMap.init() Create default context %s\n", uriPath);
            rootCtx->mbean->setAttribute(env, rootCtx->mbean, "context", "/");
        }
    }
}

static void jk2_uriMap_createWebapps(jk_env_t *env, jk_uriMap_t *uriMap)
{
    int i;

    /* Init all contexts */
    /* For each context, init the local uri maps */
    for (i = 0; i < uriMap->maps->size(env, uriMap->maps); i++) {
        jk_uriEnv_t *uriEnv = uriMap->maps->valueAt(env, uriMap->maps, i);
        char *uri;
        char *context;
        if (uriEnv == NULL) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "uriMap.init() NPE\n");
        }
        uri = uriEnv->uri;
        context = uriEnv->contextPath;

        if (uri != NULL && context != NULL && strcmp(uri, context) == 0) {
            char *vhost = uriEnv->virtual;
            int  port = uriEnv->port;
            jk_uriEnv_t *hostEnv = jk2_uriMap_hostMap(env, uriMap, vhost, port);
            if (!hostEnv)
                continue;
            if (  uriMap->mbean->debug > 5) 
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriMap.init() loaded context %s %s %#lx %#lx %#lx\n",
                              uriEnv->virtual, context, hostEnv, hostEnv->webapps,
                              uriMap->pool); 
            uriEnv->match_type = MATCH_TYPE_CONTEXT;

            uriEnv->prefix = context;
            uriEnv->prefix_len = strlen(context);
            hostEnv->webapps->put(env, hostEnv->webapps, context, uriEnv, NULL);
            jk2_map_default_create(env, &uriEnv->exactMatch, uriMap->pool);
            jk2_map_default_create(env, &uriEnv->prefixMatch, uriMap->pool);
            jk2_map_default_create(env, &uriEnv->suffixMatch, uriMap->pool);
        }
    }
    
    /* Now, fix the webapps finding context from each uri 
     * and create one if not found.
     */
    for (i = 0; i < uriMap->maps->size(env, uriMap->maps ); i++) {
        jk_uriEnv_t *uriEnv = uriMap->maps->valueAt(env, uriMap->maps, i);        
        char *vhost= uriEnv->virtual;
        int  port = uriEnv->port;
        char *context= uriEnv->contextPath;
        jk_uriEnv_t *hostEnv = jk2_uriMap_hostMap(env, uriMap, vhost, port);
        jk_uriEnv_t *ctxEnv;
        
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriMap: fix uri %s context %s\n", uriEnv->uri, uriEnv->contextPath );
        if (context == NULL) {
            if (  uriMap->mbean->debug > 5) 
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriMap: no context %s\n", uriEnv->uri );
            continue;
        }
        
        ctxEnv = jk2_uriMap_exactMap(env, uriMap, hostEnv->webapps, context,
                                      strlen(context));
        /* if not alredy created, create it */
        if (ctxEnv == NULL) {
            jk_bean_t *mbean;
            char *ctxname;

            ctxname = uriEnv->pool->calloc(env, uriEnv->pool, strlen(vhost) + 
                                           strlen(context) + 1 );

            strcpy(ctxname, vhost);
            strcat(ctxname, context);

            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "uriMap: creating context %s\n", ctxname);
            mbean = env->getBean2(env, "uri", context);
            if (mbean == NULL)
                mbean = env->createBean2(env, uriMap->pool,"uri", ctxname);
            if (mbean == NULL || mbean->object == NULL) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "uriMap: can't create context object %s\n",ctxname);
                continue;
            }
            ctxEnv = mbean->object;
            ctxEnv->match_type = MATCH_TYPE_CONTEXT;
            ctxEnv->prefix = context;
            ctxEnv->prefix_len = strlen(context);
            hostEnv->webapps->put(env, hostEnv->webapps, context, ctxEnv, NULL);
            jk2_map_default_create(env, &ctxEnv->exactMatch, uriMap->pool);
            jk2_map_default_create(env, &ctxEnv->prefixMatch, uriMap->pool);
            jk2_map_default_create(env, &ctxEnv->suffixMatch, uriMap->pool);
        }
    } 

}

static int jk2_uriMap_init(jk_env_t *env, jk_uriMap_t *uriMap)
{
    int rc = JK_OK;
    int i;
    jk_workerEnv_t *workerEnv = uriMap->workerEnv;
    jk_bean_t *mbean = env->getBean2(env, "uri", "*");

    /* create the default server */
    if (mbean == NULL) {
        mbean = env->createBean2(env, workerEnv->pool,"uri", "*");
        if (mbean == NULL || mbean->object == NULL) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "uriMap.factory() Fail to create default host\n");
            return JK_ERR;
        }
    }

    /* Create virtual hosts and initialize them */
    jk2_uriMap_createHosts(env, uriMap);

    /* Create webapps and initialize them */
    jk2_uriMap_createWebapps(env, uriMap);

    if (uriMap->mbean->debug > 5) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.init() processing mappings\n");

    /* All other mappings are added in the right context leaf.
       XXX We should also sort prefix mappings and maybe use binary search - but
       it won't have too bigger benefits, the number of mappings per ctx is typically 
       small
     */
    for (i = 0; i < uriMap->maps->size(env, uriMap->maps); i++) {
        jk_uriEnv_t *uriEnv = uriMap->maps->valueAt(env, uriMap->maps, i);

        char *vhost = uriEnv->virtual;
        int port = uriEnv->port;
        jk_uriEnv_t *hostEnv = jk2_uriMap_hostMap(env, uriMap, vhost, port);
        
        char *uri = uriEnv->uri;
        jk_uriEnv_t *ctxEnv = NULL;

        if (hostEnv == NULL)
            continue;
        if (uri == NULL )
            continue;
        uriEnv->uriMap = uriMap;
        uriEnv->init(env, uriEnv);

        if (uri == NULL)
            continue;
        
        /* If the context was specified try to find the exact one */
        if (uriEnv->contextPath != NULL)
            ctxEnv = jk2_uriMap_exactMap(env, uriMap, hostEnv->webapps,
                                         uriEnv->contextPath,
                                         uriEnv->ctxt_len);
        /* Next find by uri prefix */
        if (ctxEnv == NULL)
            ctxEnv = jk2_uriMap_prefixMap(env, uriMap, hostEnv->webapps, uri,
                                          strlen(uri));

        if (ctxEnv == NULL) {
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                           "uriMap.init() no context for %s\n", uri); 
            return JK_ERR;
        }

        /* Correct the context path if needed */
        uriEnv->contextPath = ctxEnv->prefix;
        uriEnv->ctxt_len = ctxEnv->prefix_len;

        if (uriMap->mbean->debug > 5) 
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                          "uriMap.init() adding context %s for %s\n", ctxEnv->prefix, uri); 

        switch (uriEnv->match_type) {
            case MATCH_TYPE_EXACT:
                ctxEnv->exactMatch->add(env, ctxEnv->exactMatch, uri, uriEnv);
                break;
            case MATCH_TYPE_SUFFIX:
                ctxEnv->suffixMatch->add(env, ctxEnv->suffixMatch, uri, uriEnv);
                break;
            case MATCH_TYPE_PREFIX:
                ctxEnv->prefixMatch->add(env, ctxEnv->prefixMatch, uri, uriEnv);
                break;
        }
    }
    return rc;
}

static void jk2_uriMap_destroy(jk_env_t *env, jk_uriMap_t *uriMap)
{

    if( uriMap->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "uriMap.destroy()\n"); 

    /* this can't be null ( or a NPE would have been generated */
    uriMap->pool->close(env, uriMap->pool);
}


/* returns the index of the last occurrence of the 'ch' character
   if ch=='\0' returns the length of the string str  */
static INLINE int jk2_last_index_of(const char *str,char ch)
{
    const char *str_minus_one=str-1;
    const char *s=str+strlen(str);
    while(s!=str_minus_one && ch!=*s) {
	--s;
    }
    return (s-str);
}

/* find the suffix - only once. We also make sure
   we check only the last component, as required by
   servlet spec
*/
static INLINE const char *jk2_findExtension(jk_env_t *env, const char *uri) {
    int suffix_start;
    const char *suffix;
    
    for(suffix_start = strlen(uri) - 1 ; 
        suffix_start > 0; 
        suffix_start--) {
        if( '.' == uri[suffix_start] ||
            '/' == uri[suffix_start] )
            break;
    }
    if( '.' != uri[suffix_start] ) {
        suffix_start=-1;
        suffix = NULL;
    } else {
        suffix_start++;
        suffix = uri + suffix_start;
    }
    return suffix;
}

#define SAFE_URI_SIZE 8192

static jk_uriEnv_t *jk2_uriMap_mapUri(jk_env_t *env, jk_uriMap_t *uriMap,
                                      const char *vhost, int port,
                                      const char *uri)
{
    int best_match = -1;
    int longest_match = 0;
    char *clean_uri = NULL;
    char *url_rewrite = NULL;
    const char *suffix;
    int uriLen;
    jk_uriEnv_t *hostEnv;
    jk_uriEnv_t *ctxEnv;
    jk_uriEnv_t *match;

    /* Ugly hack to avoid using non-thread safe code.
       Modify the uri in place for uri session encoding, then
       restore it to the original. That works since the processing
       happens in a single thred. A better solution is to allocate
       the jk_ws_service and it's pool and pass it as param */
    char origChar = '\0';
    
    /* XXX - need to make sure prefix match take precedence over
       extension match ( now it doesn't )
    */
    
    if (uriMap == NULL || uri==NULL) 
	    return NULL;
    
    if (uriMap->mbean->debug > 1)
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.mapUri() hostname %s port %d uri %s\n", vhost, port, uri);    
    if (uri[0] != '/') {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriMap.mapUri() uri must start with /\n");
        return NULL;
    }
    hostEnv = jk2_uriMap_hostMap(env, uriMap, vhost, port);
    if (!hostEnv) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriMap.mapUri() cannot find host %s/\n", vhost);
        return NULL;
    }
    if (uriMap->mbean->debug > 1)
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.mapUri() found host %s\n", hostEnv->virtual);    

    url_rewrite = strstr(uri, JK_PATH_SESSION_IDENTIFIER);
        
    if (url_rewrite) {
        origChar = *url_rewrite;
        *url_rewrite = '\0';
        if (uriMap->mbean->debug > 0)
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriMap.mapUri() rewrote uri %s \n",uri);
    }

    uriLen = strlen(uri);

    /* Map the context */
    ctxEnv = jk2_uriMap_prefixMap(env, uriMap, hostEnv->webapps, uri, uriLen);

    if (ctxEnv == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriMap.mapUri() no context %s\n", uri);    
        return NULL;
    }
    
    if (uriMap->mbean->debug > 1)
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.mapUri() found ctx %s\n", ctxEnv->uri);    

    /* As per Servlet spec, do exact match first */
    match = jk2_uriMap_exactMap(env, uriMap, ctxEnv->exactMatch, uri, uriLen);
    if (match != NULL) {
        /* restore */
        if (url_rewrite)
            *url_rewrite = origChar;
        if (uriMap->mbean->debug > 0)
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriMap.mapUri() exact match %s %s\n",
                          uri, match->workerName); 
        return match;
    }
    
    /* Then prefix match */
    match = jk2_uriMap_prefixMap(env, uriMap, ctxEnv->prefixMatch, uri, uriLen);
    if (match != NULL) {
        char c = uri[match->prefix_len - 1];
        /* XXX Filter prefix matches to allow only exact 
               matches with an optional path_info or query string at end.
               Fixes Bugzilla#12141, needs review..
        */
        if ((uriLen > match->prefix_len && (c=='/' || c=='?')) ||
             uriLen == match->prefix_len) {
            /* restore */
            if (url_rewrite)
                *url_rewrite = origChar;
            if (uriMap->mbean->debug > 0)
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriMap.mapUri() prefix match %s %s\n",
                              uri, match->workerName); 
            return match;
        }
    }
    
    /* Try to find exact match of /uri and prefix /uri/* */
    match = jk2_uriMap_contextMap(env, uriMap, ctxEnv->prefixMatch, uri, uriLen);
    if (match != NULL) {
        /* restore */
        if (url_rewrite)
            *url_rewrite = origChar;
        if (uriMap->mbean->debug > 0)
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriMap.mapUri() context match %s %s\n",
                          uri, match->workerName); 
        return match;
    }

    /* And extension match at the end */
    /* Only once, no need to compute it for each extension match */
    suffix = jk2_findExtension(env, uri);
    if (suffix != NULL) {
        match = jk2_uriMap_suffixMap(env, uriMap, ctxEnv->suffixMatch,
                                     suffix, strlen(suffix));
        if (match != NULL) {
            /* restore */
            if (url_rewrite)
                *url_rewrite = origChar;
            if (uriMap->mbean->debug > 0)
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriMap.mapUri() extension match %s %s\n",
                              uri, match->workerName); 
            return match;
        }
    }

    /* restore */
    if (url_rewrite)
        *url_rewrite = origChar;
        
    if (uriMap->mbean->debug > 1)
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.mapUri() no match found\n"); 

    return NULL;
}

int JK_METHOD jk2_uriMap_factory(jk_env_t *env, jk_pool_t *pool, jk_bean_t *result,
                                 const char *type, const char *name)
{
    jk_uriMap_t *uriMap;

    uriMap = (jk_uriMap_t *)pool->calloc(env, pool, sizeof(jk_uriMap_t));
    if (!uriMap) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriMap.factory() OutOfMemoryError\n");
        return JK_ERR;
    }

    uriMap->pool = pool;

    jk2_map_default_create(env, &uriMap->maps, pool);
    jk2_map_default_create(env, &uriMap->vhosts, pool);

    uriMap->init = jk2_uriMap_init;
    uriMap->destroy = jk2_uriMap_destroy;
    uriMap->addUriEnv = jk2_uriMap_addUriEnv;
    uriMap->checkUri = jk2_uriMap_checkUri;
    uriMap->mapUri = jk2_uriMap_mapUri;
            
    result->object = uriMap;
    result->setAttribute = jk2_uriMap_setProperty;
    uriMap->mbean = result;

    return JK_OK;
}

