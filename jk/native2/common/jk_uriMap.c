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

#ifdef HAS_AP_PCRE
#include "httpd.h"
#define REGEXEC ap_regexec
#else
#ifdef HAS_PCRE
#include "pcre.h"
#include "pcreposix.h"
#define REGEXEC regexec
#endif
#endif

static INLINE const char *jk2_findExtension(jk_env_t *env, const char *uri);

static int jk2_uriMap_checkUri(jk_env_t *env, jk_uriMap_t *uriMap, 
                               const char *uri);

#ifdef WIN32                        
static int jk2_uri_icase = 1;
#else
static int jk2_uri_icase = 0;
#endif

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

/* Match = 0, NoMatch = 1, Abort = -1
 * Based loosely on sections of wildmat.c by Rich Salz
 */
int jk2_strcmp_match(const char *str, const char *exp, int icase)
{
    int x, y;

    for (x = 0, y = 0; exp[y]; ++y, ++x) {
        if (!str[x] && exp[y] != '*')
            return -1;
        if (exp[y] == '*') {
            while (exp[++y] == '*');
            if (!exp[y])
                return 0;
            while (str[x]) {
                int ret;
                if ((ret = jk2_strcmp_match(&str[x++], &exp[y], icase)) != 1)
                    return ret;
            }
            return -1;
        }
        else if (exp[y] != '?') {
            if (icase && tolower(str[x]) != tolower(exp[y]))
                return 1;
            else if (!icase && str[x] != exp[y])
                return 1;
        }
    }
    return (str[x] != '\0');
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
        /* XXX: the maps are already sorted by length. Should we skip that ??? */
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

        if (!jk2_strcmp_match(suffix, uwr->suffix, jk2_uri_icase))
            return uwr;
    }
    return NULL;
}

#define MAX_HOST_LENGTH			1024

/* Find the vhost */
static jk_uriEnv_t *jk2_uriMap_hostMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                       const char *vhost, int port)
{
    int i, j, n;
    char *name;
    char hostname[MAX_HOST_LENGTH] = {0};
    char portSuffix[32];
    int vhostLen;

    if (port) {
        if (vhost) {
            vhostLen = strlen(vhost);
            if (strchr(vhost, ':')) {
               strncpy(hostname, vhost, MAX_HOST_LENGTH);
            } else {
               strncpy(hostname, vhost, MAX_HOST_LENGTH);
               if (strlen(vhost) < MAX_HOST_LENGTH - 1) {
                  sprintf(portSuffix,":%d", port);
                  strncat(hostname+vhostLen, portSuffix, MAX_HOST_LENGTH-vhostLen);
               }
            }
        }
        else
            sprintf(hostname, "*:%d", port);
    }
    else if (vhost)
        strncpy(hostname, vhost, MAX_HOST_LENGTH);
    else /* Return default host if vhost and port wasn't suplied */
        return uriMap->vhosts->get(env, uriMap->vhosts, "*");
    
    hostname[MAX_HOST_LENGTH - 1] = 0;
    
    n = uriMap->vhosts->size(env, uriMap->vhosts);
    /* Check the exact hostname:port first */
    for (i = 0 ; i < n ; i++) {
        jk_uriEnv_t *uriEnv = uriMap->vhosts->valueAt(env, uriMap->vhosts, i);
        name = uriMap->vhosts->nameAt(env, uriMap->vhosts, i);
        /* Host name is not case sensitive */
        if (strcasecmp(name, hostname) == 0 && port == uriEnv->port)
            return uriEnv;
    }

    if (vhost) {
        /* Check the hostname */
        for (i = 0 ; i < n ; i++) {
            jk_uriEnv_t *uriEnv = uriMap->vhosts->valueAt(env, uriMap->vhosts, i);
            name = uriMap->vhosts->nameAt(env, uriMap->vhosts, i);
            /* Host name is not case sensitive */
            if (strcasecmp(name, vhost) == 0)
                return uriEnv;
        }
        /* Then for each vhost, check the aliases */
        for (i = 0 ; i < n ; i++) {
            jk_uriEnv_t *uriEnv = uriMap->vhosts->valueAt(env, uriMap->vhosts, i);
            if (uriEnv->aliases) {
                int m = uriEnv->aliases->size(env, uriEnv->aliases);
                for (j = 0; j < m; j++) {
                    name = uriEnv->aliases->nameAt(env, uriEnv->aliases, j);
                    if (strcasecmp(name, hostname) == 0)
                        return uriEnv;
                }
            }
        }
    }
    /* Finally, check aginst *:port hostname */
    if (port) {
        for (i = 0 ; i < n ; i++) {
            jk_uriEnv_t *uriEnv = uriMap->vhosts->valueAt(env, uriMap->vhosts, i);
            name = uriMap->vhosts->nameAt(env, uriMap->vhosts, i);
            if ((strncmp(name, "*:", 2) == 0) && uriEnv->port == port) {
                return uriEnv;
            }
        }
    }
    /* Return default host if none found */
    return uriMap->vhosts->get(env, uriMap->vhosts, "*");
}

#if defined(HAS_PCRE) || defined(HAS_AP_PCRE)
static jk_uriEnv_t *jk2_uriMap_regexpMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                         jk_map_t *mapTable, const char *uri) 
{
    int i;    
    int sz = mapTable->size(env, mapTable);
    
    for (i = 0; i < sz; i++) {
        jk_uriEnv_t *uwr = mapTable->valueAt(env, mapTable, i);

        if (uwr->regexp) {
            regex_t *r = (regex_t *)uwr->regexp;
            regmatch_t regm[10];
            if (!REGEXEC(r, uri, r->re_nsub + 1, regm, 0)) {
                return uwr;
            }
        }
    }
    return NULL;

}
#else
static jk_uriEnv_t *jk2_uriMap_regexpMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                         jk_map_t *mapTable, const char *uri) 
{
    return NULL;
}
#endif

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
                    /* Actually create the bean */
                    jk_uriEnv_t *hostEnv = env->getByName2(env, "uri",
                                                           uriEnv->virtual);
                    if (hostEnv == NULL) {
                        env->createBean2(env, uriMap->mbean->pool, "uri",
                                         uriEnv->virtual);
                        hostEnv = env->getByName2(env, "uri",
                                                  uriEnv->virtual);
                        if (!hostEnv) {
                            /* XXX this is a error. */
                            continue;
                        }
                        if (uriMap->mbean->debug > 0) 
                            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                                "uriMap.init() Create missing host %s\n", uriEnv->virtual);
                    }
                    jk2_map_default_create(env, &hostEnv->webapps, uriMap->pool);
                    uriMap->vhosts->put(env, uriMap->vhosts,
                                        uriEnv->virtual, hostEnv, NULL);

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
            uriPath=env->tmpPool->pstrcat(env, env->tmpPool,
                                          hostEnv->virtual, "/", NULL);
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
            jk2_map_default_create(env, &uriEnv->regexpMatch, uriMap->pool);
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
                      "uriMap: fix uri %s context %s host %s\n",
                       uriEnv->uri==NULL ? "null":uriEnv->uri,
                       uriEnv->contextPath==NULL ? "null":uriEnv->contextPath,
                       hostEnv->virtual);

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

            ctxname = uriEnv->pool->pstrcat(env, uriEnv->pool, vhost,
                                            context, NULL);

            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "uriMap: creating context %s\n", ctxname);
            mbean = env->getBean2(env, "uri", ctxname);
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
            ctxEnv->contextPath = context;
            ctxEnv->ctxt_len = strlen(context);
            hostEnv->webapps->put(env, hostEnv->webapps, context, ctxEnv, NULL);
            jk2_map_default_create(env, &ctxEnv->exactMatch, uriMap->pool);
            jk2_map_default_create(env, &ctxEnv->prefixMatch, uriMap->pool);
            jk2_map_default_create(env, &ctxEnv->suffixMatch, uriMap->pool);
            jk2_map_default_create(env, &ctxEnv->regexpMatch, uriMap->pool);
        }
    } 

}

static int jk2_uriMap_createMappings(jk_env_t *env, jk_uriMap_t *uriMap)
{
    int i;

    if (uriMap->mbean->debug > 5) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.init() processing mappings\n");
    
    /* XXX We should also sort prefix mappings and maybe use binary search - but
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
        else if (uriEnv->match_type == MATCH_TYPE_REGEXP) {
            ctxEnv = hostEnv->webapps->get(env, hostEnv->webapps, "/");
        }
        /* Next find by uri prefix */
        if (ctxEnv == NULL)
            ctxEnv = jk2_uriMap_prefixMap(env, uriMap, hostEnv->webapps, uri,
                                          strlen(uri));

        if (ctxEnv == NULL) {
            env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                           "uriMap.init() no context for %s\n", uri);
            /* Normal case if Location is used */
            continue;
        }

        /* Correct the context path if needed */
        uriEnv->contextPath = ctxEnv->prefix;
        uriEnv->ctxt_len = ctxEnv->prefix_len;

        if (uriMap->mbean->debug > 5) 
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                          "uriMap.init() adding context %s:%s for %s\n", vhost, ctxEnv->prefix, uri); 

        switch (uriEnv->match_type) {
            case MATCH_TYPE_EXACT:
                ctxEnv->exactMatch->add(env, ctxEnv->exactMatch, uri, uriEnv);
                ctxEnv->exactMatch->sort(env, ctxEnv->exactMatch);
                break;
            case MATCH_TYPE_SUFFIX:
                ctxEnv->suffixMatch->add(env, ctxEnv->suffixMatch, uri, uriEnv);
                ctxEnv->suffixMatch->sort(env, ctxEnv->suffixMatch);
                break;
            case MATCH_TYPE_PREFIX:
                ctxEnv->prefixMatch->add(env, ctxEnv->prefixMatch, uri, uriEnv);
                ctxEnv->suffixMatch->sort(env, ctxEnv->prefixMatch);
                break;
            case MATCH_TYPE_REGEXP:
                if (uriEnv->regexp)
                    ctxEnv->regexpMatch->add(env, ctxEnv->regexpMatch, uri, uriEnv);
                break;
        }
    }
    return JK_OK;
}

static jk_uriEnv_t* jk2_uriMap_duplicateUri(jk_env_t *env, jk_uriMap_t *uriMap,
                                            jk_uriEnv_t *uriEnv, jk_uriEnv_t *mapEnv)
{
    char *uriname;
    jk_uriEnv_t *newEnv;
    jk_bean_t *mbean;

    uriname = uriEnv->pool->pstrcat(env, uriEnv->pool, uriEnv->name, 
                                    mapEnv->uri, NULL);

    env->l->jkLog(env, env->l, JK_LOG_INFO,
        "uriMap: creating duplicate of  uri %s\n", uriname);
    mbean = env->getBean2(env, "uri", uriname);
    if (mbean == NULL)
        mbean = env->createBean2(env, uriMap->pool,"uri", uriname);
    if (mbean == NULL || mbean->object == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
            "uriMap: can't create uri object %s\n", uriname);
        return NULL;
    }
    newEnv = mbean->object;
    newEnv->match_type = mapEnv->match_type;
    newEnv->prefix = mapEnv->prefix;
    newEnv->prefix_len = mapEnv->prefix_len;
    newEnv->contextPath = mapEnv->contextPath;
    newEnv->ctxt_len = mapEnv->ctxt_len;
    newEnv->worker = mapEnv->worker;
    newEnv->workerName = mapEnv->workerName;
    newEnv->workerEnv = mapEnv->workerEnv;
    newEnv->regexp = mapEnv->regexp;

    return newEnv;
}

static jk_uriEnv_t* jk2_uriMap_duplicateContext(jk_env_t *env, jk_uriMap_t *uriMap,
                                            jk_uriEnv_t *uriEnv, jk_uriEnv_t *mapEnv)
{
    char *uriname;
    jk_uriEnv_t *newEnv;
    jk_bean_t *mbean;

    uriname = uriEnv->pool->pstrcat(env, uriEnv->pool, uriEnv->name, 
                                    mapEnv->contextPath, NULL);

    env->l->jkLog(env, env->l, JK_LOG_INFO,
        "uriMap: creating duplicate of context %s\n", uriname);
    mbean = env->getBean2(env, "uri", uriname);
    if (mbean == NULL)
        mbean = env->createBean2(env, uriMap->pool,"uri", uriname);
    if (mbean == NULL || mbean->object == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
            "uriMap: can't create uri object %s\n", uriname);
        return NULL;
    }
    newEnv = mbean->object;
    newEnv->match_type = mapEnv->match_type;
    newEnv->prefix = mapEnv->prefix;
    newEnv->prefix_len = mapEnv->prefix_len;
    newEnv->contextPath = mapEnv->contextPath;
    newEnv->ctxt_len = mapEnv->ctxt_len;
    newEnv->worker = mapEnv->worker;
    newEnv->workerName = mapEnv->workerName;
    newEnv->workerEnv = mapEnv->workerEnv;

    jk2_map_default_create(env, &newEnv->exactMatch, uriMap->pool);
    jk2_map_default_create(env, &newEnv->prefixMatch, uriMap->pool);
    jk2_map_default_create(env, &newEnv->suffixMatch, uriMap->pool);
    jk2_map_default_create(env, &newEnv->regexpMatch, uriMap->pool);

    return newEnv;
}

static int jk2_uriMap_createGlobals(jk_env_t *env, jk_uriMap_t *uriMap)
{
    int i, n;
    jk_uriEnv_t *globalEnv;

    if (uriMap->mbean->debug > 5) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.init() creating global mappings\n");
    globalEnv = uriMap->vhosts->get(env, uriMap->vhosts, "*");

    n = uriMap->vhosts->size(env, uriMap->vhosts);
    for (i = 0 ; i < n ; i++) {
        jk_uriEnv_t *uriEnv = uriMap->vhosts->valueAt(env, uriMap->vhosts, i);
        /* Duplicate globals for each vhost that has inheritGlobals set */
        if (uriEnv != globalEnv && uriEnv->inherit_globals) {
            int j, m;
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                         "uriMap.init() global for %s\n",
                          uriEnv->name);
            m = globalEnv->webapps->size(env, globalEnv->webapps);
            for (j = 0; j < m; j++) {
                jk_uriEnv_t *ctxEnv;
                jk_uriEnv_t *appEnv;
                int k, l;

                ctxEnv = globalEnv->webapps->valueAt(env, globalEnv->webapps, j);
                appEnv = uriEnv->webapps->get(env, uriEnv->webapps, ctxEnv->contextPath);

                /* Create the webapp if it doesn't exists on the selected vhost */
                if (!appEnv) {

                    appEnv = jk2_uriMap_duplicateContext(env, uriMap, uriEnv, ctxEnv);
                    uriEnv->webapps->put(env, uriEnv->webapps, appEnv->contextPath, appEnv, NULL);
                    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                                  "uriMap.init() creating global webapp %s for %s\n",
                                  appEnv->contextPath, uriEnv->name);
                }
                /* Fix the exact matches */
                l = ctxEnv->exactMatch->size(env, ctxEnv->exactMatch);
                for (k = 0; k < l; k++) {
                    jk_uriEnv_t *mapEnv;
                    jk_uriEnv_t *newEnv;
                    
                    mapEnv = ctxEnv->exactMatch->valueAt(env, ctxEnv->exactMatch, k);
                    newEnv = appEnv->exactMatch->get(env, appEnv->exactMatch, mapEnv->uri);
                    /* Create the new exact match uri */
                    if (!newEnv) {
                        newEnv = jk2_uriMap_duplicateUri(env, uriMap, uriEnv, mapEnv);
                        appEnv->exactMatch->put(env, appEnv->exactMatch, newEnv->name, newEnv, NULL);
                        
                    }
                }
                /* Fix the prefix matches */
                l = ctxEnv->prefixMatch->size(env, ctxEnv->prefixMatch);
                for (k = 0; k < l; k++) {
                    jk_uriEnv_t *mapEnv;
                    jk_uriEnv_t *newEnv;
                    
                    mapEnv = ctxEnv->prefixMatch->valueAt(env, ctxEnv->prefixMatch, k);
                    newEnv = appEnv->prefixMatch->get(env, appEnv->prefixMatch, mapEnv->uri);
                    /* Create the new prefix match uri */
                    if (!newEnv) {
                        newEnv = jk2_uriMap_duplicateUri(env, uriMap, uriEnv, mapEnv);
                        appEnv->prefixMatch->put(env, appEnv->prefixMatch, newEnv->name, newEnv, NULL);                        
                    }
                }
                /* Fix the suffix matches */
                l = ctxEnv->suffixMatch->size(env, ctxEnv->suffixMatch);
                for (k = 0; k < l; k++) {
                    jk_uriEnv_t *mapEnv;
                    jk_uriEnv_t *newEnv;
                    
                    mapEnv = ctxEnv->suffixMatch->valueAt(env, ctxEnv->suffixMatch, k);
                    newEnv = appEnv->suffixMatch->get(env, appEnv->suffixMatch, mapEnv->uri);
                    /* Create the new suffix match uri */
                    if (!newEnv) {
                        newEnv = jk2_uriMap_duplicateUri(env, uriMap, uriEnv, mapEnv);
                        appEnv->suffixMatch->put(env, appEnv->prefixMatch, newEnv->name, newEnv, NULL);                        
                    }
                }
                uriEnv->webapps->put(env, uriEnv->webapps, 
                                     appEnv->contextPath, appEnv, NULL);
            }
        }
    }
    return JK_OK;
}

static int jk2_uriMap_init(jk_env_t *env, jk_uriMap_t *uriMap)
{
    int rc = JK_OK;
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

    /* All other mappings are added in the right context leaf. */
    if ((rc = jk2_uriMap_createMappings(env, uriMap)) != JK_OK)
        return rc;

    /* Fix the global mappings for virtual hosts */
    rc = jk2_uriMap_createGlobals(env, uriMap);

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

static jk_uriEnv_t *jk2_uriMap_getHostCache(jk_env_t *env, jk_uriMap_t *uriMap,
                                            const char *vhost, int port)
{
    char key[MAX_HOST_LENGTH];
    char portSuffix[32];
    int vhostLen;
    
    if (!vhost && !port)
        return uriMap->vhosts->get(env, uriMap->vhosts, "*");
    if (!vhost)
        vhost = "*";
    vhostLen = strlen(vhost);
    strncpy(key, vhost, MAX_HOST_LENGTH);
    if (vhostLen < MAX_HOST_LENGTH - 1) {
       sprintf(portSuffix,":%d", port);
       strncat(key+vhostLen, portSuffix, MAX_HOST_LENGTH);
    }
    key[MAX_HOST_LENGTH - 1] = 0;
    return uriMap->vhcache->get(env, uriMap->vhcache, key);
}

static void jk2_uriMap_addHostCache(jk_env_t *env, jk_uriMap_t *uriMap,
                                    const char *vhost, int port,
                                    jk_uriEnv_t *hostEnv)
{
    char *key;
    
    if (!vhost)
        vhost = "*";    
    key = uriMap->pool->calloc(env, uriMap->pool, strlen(vhost) + 8); 

    sprintf(key, "%s:%d", vhost, port);
    uriMap->vhcache->add(env, uriMap->vhcache, key, hostEnv);
}

static jk_uriEnv_t *jk2_uriMap_mapUri(jk_env_t *env, jk_uriMap_t *uriMap,
                                      const char *vhost, int port,
                                      const char *uri)
{
    char *url_rewrite = NULL;
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

    hostEnv = jk2_uriMap_getHostCache(env, uriMap, vhost, port);
    if (!hostEnv) {
        hostEnv = jk2_uriMap_hostMap(env, uriMap, vhost, port);
        if (!hostEnv) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "uriMap.mapUri() cannot find host %s/\n", vhost);
            return NULL;
        }
        if (uriMap->mbean->debug > 1)
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriMap.mapUri() caching host %s\n", hostEnv->virtual);    
        jk2_uriMap_addHostCache(env, uriMap, vhost, port, hostEnv);
    }
    else if (uriMap->mbean->debug > 1)
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
        if (url_rewrite)
            *url_rewrite = origChar;
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriMap.mapUri() no context %s\n", uri);    
        return NULL;
    }
    
    if (uriMap->mbean->debug > 1)
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.mapUri() found ctx %s\n", ctxEnv->uri);    

    match = jk2_uriMap_regexpMap(env, uriMap, ctxEnv->regexpMatch, uri);
    if (match != NULL) {
        /* restore */
        if (url_rewrite)
            *url_rewrite = origChar;
        if (uriMap->mbean->debug > 0)
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriMap.mapUri() regexp match %s %s\n",
                          uri, match->workerName); 
        return match;
    }


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
    
    /* Try to find exact match of /uri and prefix /uri/star (*) */
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

    /* And do a wild char match at the end */
    match = jk2_uriMap_suffixMap(env, uriMap, ctxEnv->suffixMatch, uri, 0);
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
    jk2_map_default_create(env, &uriMap->vhcache, pool);

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

