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

#ifdef HAS_AP_PCRE
#include "httpd.h"
#else
#ifdef HAS_PCRE
#include "pcre.h"
#include "pcreposix.h"
#endif
#endif

/* return non-zero if pattern has any glob chars in it */

static int jk2_is_wildmatch(const char *pattern)
{
    while (*pattern) {
        switch (*pattern) {
        case '?':
        case '*':
            return 1;
        }
        ++pattern;
    }
    return 0;
}

/** Parse the name:
       VHOST/PATH

    If VHOST is empty, we map to the default host.

    The PATH will be further split in CONTEXT/LOCALPATH during init ( after
    we have all uris, including the context paths ).
*/
static int jk2_uriEnv_parseName(jk_env_t *env, jk_uriEnv_t *uriEnv,
                                char *name)
{
    char *uri = NULL;
    char *colon;
    char host[1024];
    int pcre = 0;

    if (*name == '$') {
#if defined(HAS_PCRE) || defined(HAS_AP_PCRE)
        ++name;
        uriEnv->uri = uriEnv->pool->pstrdup(env, uriEnv->pool, name);
        uriEnv->match_type = MATCH_TYPE_REGEXP;
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriEnv.parseName() parsing %s regexp\n", name);
        {
#ifdef HAS_AP_PCRE
            regex_t *preg =
                ap_pregcomp((apr_pool_t *) uriEnv->pool->_private,
                            uriEnv->uri, REG_EXTENDED);
            if (!preg) {
#else
            regex_t *preg =
                (regex_t *) uriEnv->pool->calloc(env, uriEnv->pool,
                                                 sizeof(regex_t));
            if (regcomp(preg, uriEnv->uri, REG_EXTENDED)) {
#endif
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriEnv.parseName() error compiling regexp %s\n",
                              uri);
                return JK_ERR;
            }
            uriEnv->regexp = preg;
        }
        return JK_OK;
#else
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriEnv.parseName() parsing regexp %s not supported\n",
                      uri);
        return JK_ERR;
#endif
    }

    strcpy(host, name);
    colon = strchr(host, ':');
    uri = strchr(host, '$');
    if (uri)
        pcre = 1;
    if (colon != NULL) {
        ++colon;
        if (!uri)
            uri = strchr(colon, '/');
    }
    else if (!uri)
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

    *uri = '\0';
    if (colon)
        uriEnv->port = atoi(colon);

    /* If it doesn't start with /, it must have a vhost */
    if (strlen(host) && uri != host) {
        uriEnv->virtual =
            uriEnv->pool->calloc(env, uriEnv->pool, strlen(host) + 1);
        strncpy(uriEnv->virtual, name, strlen(host));
    }
    else
        uriEnv->virtual = "*";

    *uri = '/';
    if (pcre) {
        ++uri;
        uriEnv->match_type = MATCH_TYPE_REGEXP;
#if defined(HAS_PCRE) || defined(HAS_AP_PCRE)
        uriEnv->uri = uriEnv->pool->pstrdup(env, uriEnv->pool, uri);
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriEnv.parseName() parsing regexp %s\n", uri);
        {
#ifdef HAS_AP_PCRE
            regex_t *preg =
                ap_pregcomp((apr_pool_t *) uriEnv->pool->_private,
                            uriEnv->uri, REG_EXTENDED);
            if (!preg) {
#else
            regex_t *preg =
                (regex_t *) uriEnv->pool->calloc(env, uriEnv->pool,
                                                 sizeof(regex_t));
            if (regcomp(preg, uriEnv->uri, REG_EXTENDED)) {
#endif
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriEnv.parseName() error compiling regexp %s\n",
                              uri);
                return JK_ERR;
            }
            uriEnv->regexp = preg;
        }
#else
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriEnv.parseName() parsing regexp %s not supported\n",
                      uri);
#endif
    }
    else
        uriEnv->uri = uriEnv->pool->pstrdup(env, uriEnv->pool, uri);

    return JK_OK;
}

static char *getAttInfo[] =
    { "host", "uri", "group", "context", "inheritGlobals",
    "match_type", "servlet", "timing", "aliases", "debug", "disabled",
    NULL
};
static char *setAttInfo[] =
    { "host", "uri", "group", "context", "inheritGlobals",
    "servlet", "timing", "alias", "path", "debug", "disabled", 
    NULL
};

static char *matchTypes[] = {
    "exact",
    "prefix",
    "suffix",
    "gensuffix",
    "contextpath",
    "host",
    "context",
    "regexp"
};

static void *JK_METHOD jk2_uriEnv_getAttribute(jk_env_t *env, jk_bean_t *bean,
                                               char *name)
{
    jk_uriEnv_t *uriEnv = (jk_uriEnv_t *)bean->object;

    if (strcmp(name, "host") == 0) {
        return uriEnv->virtual;
    }
    else if (strcmp(name, "uri") == 0) {
        return uriEnv->uri;
    }
    else if (strcmp(name, "group") == 0) {
        return uriEnv->workerName;
    }
    else if (strcmp(name, "context") == 0) {
        return uriEnv->contextPath;
    }
    else if (strcmp(name, "servlet") == 0) {
        return uriEnv->servlet;
    }
    else if (strcmp(name, "prefix") == 0) {
        return uriEnv->prefix;
    }
    else if (strcmp(name, "suffix") == 0) {
        return uriEnv->suffix;
    }
    else if (strcmp(name, "match_type") == 0) {
        return matchTypes[uriEnv->match_type];
    }
    else if (strcmp("timing", name) == 0) {
        return jk2_env_itoa(env, uriEnv->timing);
    }
    else if (strcmp("aliases", name) == 0 && uriEnv->aliases) {
        return jk2_map_concatKeys(env, uriEnv->aliases, ":");
    }
    else if (strcmp("path", name) == 0) {
        return uriEnv->uri;
    }
    else if (strcmp("inheritGlobals", name) == 0) {
        return jk2_env_itoa(env, uriEnv->inherit_globals);
    }
    else if (strcmp(name, "debug") == 0) {
        return jk2_env_itoa(env, bean->debug);
    }
    else if (strcmp(name, "disabled") == 0) {
        return jk2_env_itoa(env, bean->disabled);
    }    return NULL;
}


static int JK_METHOD jk2_uriEnv_setAttribute(jk_env_t *env,
                                             jk_bean_t *mbean,
                                             char *nameParam, void *valueP)
{
    jk_uriEnv_t *uriEnv = mbean->object;
    char *valueParam = valueP;

    char *name = uriEnv->pool->pstrdup(env, uriEnv->pool, nameParam);
    char *val = uriEnv->pool->pstrdup(env, uriEnv->pool, valueParam);

    uriEnv->properties->add(env, uriEnv->properties, name, val);

    if (strcmp("group", name) == 0) {
        uriEnv->workerName = val;
        return JK_OK;
    }
    else if (strcmp("context", name) == 0) {
        uriEnv->contextPath = val;
        uriEnv->ctxt_len = strlen(val);

        if (strcmp(val, uriEnv->uri) == 0) {
            uriEnv->match_type = MATCH_TYPE_CONTEXT;
        }
        return JK_OK;
    }
    else if (strcmp("servlet", name) == 0) {
        uriEnv->servlet = val;
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
    else if (strcmp("path", name) == 0) {
        /** This is called from Location in jk2, it has the same effect
         *    as using the constructor.
         */
        if (val == NULL)
            uriEnv->uri = NULL;
        else
            uriEnv->uri = uriEnv->pool->pstrdup(env, uriEnv->pool, val);
    }
    else if (strcmp("inheritGlobals", name) == 0) {
        uriEnv->inherit_globals = atoi(val);
    }
    else if (strcmp("worker", name) == 0) {
        /* OLD - DEPRECATED */
       
		uriEnv->workerName = val;
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriEnv.setAttribute() the %s directive is deprecated. Use 'group' instead.\n",
                      name);
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
    else if (strcmp(name, "debug") == 0) {
        mbean->debug = atoi(val);
    }
    else if (strcmp(name, "disabled") == 0) {
        mbean->disabled = atoi(val);
    }
    return JK_OK;
}


static int JK_METHOD jk2_uriEnv_beanInit(jk_env_t *env, jk_bean_t *bean)
{
    jk_uriEnv_t *uriEnv = bean->object;
    int res = JK_OK;

    if (bean->state == JK_STATE_INIT)
        return JK_OK;

    if (uriEnv->init) {
        res = uriEnv->init(env, uriEnv);
    }
    if (res == JK_OK) {
        bean->state = JK_STATE_INIT;
    }
    return res;
}

static int jk2_uriEnv_init(jk_env_t *env, jk_uriEnv_t *uriEnv)
{
    char *uri = uriEnv->pool->pstrdup(env, uriEnv->pool, uriEnv->uri);

    /* Set the worker */
    char *wname = uriEnv->workerName;

    if (uriEnv->workerEnv->timing == JK_TRUE) {
        uriEnv->timing = JK_TRUE;
    }
    if (uriEnv->workerName == NULL) {
        /* The default worker */
        uriEnv->workerName =
            uriEnv->uriMap->workerEnv->defaultWorker->mbean->name;
        wname = uriEnv->workerName;
        uriEnv->worker = uriEnv->uriMap->workerEnv->defaultWorker;

        if (uriEnv->mbean->debug > 0)
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriEnv.init() map %s %s %s\n",
                          uriEnv->uri,
                          uriEnv->uriMap->workerEnv->defaultWorker->mbean->
                          name, uriEnv->workerName);
        if (uriEnv->workerName == NULL) {
            uriEnv->workerName = "lb:lb";
        }
    }

    /* No further init - will be called by uriMap.init() */

    if (uriEnv->workerName != NULL && uriEnv->worker == NULL) {
        uriEnv->worker = env->getByName(env, wname);
        if (uriEnv->worker == NULL) {
            uriEnv->worker = env->getByName2(env, "lb", wname);
            if (uriEnv->worker == NULL) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "uriEnv.init() map to invalid worker %s %s\n",
                              uriEnv->uri, uriEnv->workerName);
                /* XXX that's allways a 'lb' worker, create it */
            }
        }
    }

    if (uri == NULL)
        return JK_ERR;

    if (uriEnv->match_type == MATCH_TYPE_REGEXP) {
        uriEnv->prefix = uri;
        uriEnv->prefix_len = strlen(uriEnv->prefix);
        uriEnv->suffix = NULL;
        if (uriEnv->mbean->debug > 0) {
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriEnv.init() regexp mapping %s=%s \n",
                          uriEnv->prefix, uriEnv->workerName);

        }
        return JK_OK;
    }
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

    /* set the mapping type */
    if (!jk2_is_wildmatch(uri)) {
        /* Something like:  JkMount /login/j_security_check ajp13 */
        uriEnv->prefix = uri;
        uriEnv->prefix_len = strlen(uriEnv->prefix);
        uriEnv->suffix = NULL;
        if (uriEnv->match_type != MATCH_TYPE_CONTEXT &&
            uriEnv->match_type != MATCH_TYPE_HOST) {
            /* Context and host maps do not have ASTERISK */
            uriEnv->match_type = MATCH_TYPE_EXACT;
        }
        if (uriEnv->mbean->debug > 0) {
            if (uriEnv->match_type == MATCH_TYPE_CONTEXT) {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriEnv.init() context mapping %s=%s \n",
                              uriEnv->prefix, uriEnv->workerName);

            }
            else if (uriEnv->match_type == MATCH_TYPE_HOST) {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriEnv.init() host mapping %s=%s \n",
                              uriEnv->virtual, uriEnv->workerName);
            }
            else {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriEnv.init() exact mapping %s=%s \n",
                              uriEnv->prefix, uriEnv->workerName);
            }
        }
        return JK_OK;
    }

    if (uri[strlen(uri) - 1] == '*') {
        /* context based /context/prefix/ASTERISK  */
        uri[strlen(uri) - 1] = '\0';
        uriEnv->suffix = NULL;
        uriEnv->prefix = uri;
        uriEnv->prefix_len = strlen(uriEnv->prefix);
        uriEnv->match_type = MATCH_TYPE_PREFIX;
        if (uriEnv->mbean->debug > 0) {
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriEnv.init() prefix mapping %s=%s\n",
                          uriEnv->prefix, uriEnv->workerName);
        }
    }
    else {
        /*
         * We have an * or ? in the uri.
         */
        uriEnv->suffix = uri;
        uriEnv->prefix = NULL;
        uriEnv->prefix_len = 0;
        uriEnv->suffix_len = strlen(uri);
        uriEnv->match_type = MATCH_TYPE_SUFFIX;
        if (uriEnv->mbean->debug > 0) {
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriEnv.init() suffix mapping %s=%s\n",
                          uriEnv->prefix, uriEnv->workerName);
        }
    }
    if (uriEnv->mbean->debug > 0)
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriEnv.init()  %s  host=%s  uri=%s type=%d ctx=%s prefix=%s suffix=%s\n",
                      uriEnv->mbean->name, uriEnv->virtual, uriEnv->uri,
                      uriEnv->match_type, uriEnv->contextPath, uriEnv->prefix,
                      uriEnv->suffix);

    uriEnv->mbean->state = JK_STATE_INIT;
    return JK_OK;
}

int JK_METHOD jk2_uriEnv_factory(jk_env_t *env, jk_pool_t *pool,
                                 jk_bean_t *result,
                                 const char *type, const char *name)
{
    jk_pool_t *uriPool;
    jk_uriEnv_t *uriEnv;

    uriPool = (jk_pool_t *)pool->create(env, pool, HUGE_POOL_SIZE);

    uriEnv = (jk_uriEnv_t *)pool->calloc(env, uriPool, sizeof(jk_uriEnv_t));

    uriEnv->pool = uriPool;

    jk2_map_default_create(env, &uriEnv->properties, uriPool);

    result->init = jk2_uriEnv_beanInit;
    uriEnv->init = jk2_uriEnv_init;

    result->setAttribute = jk2_uriEnv_setAttribute;
    result->getAttribute = jk2_uriEnv_getAttribute;
    uriEnv->mbean = result;
    result->object = uriEnv;
    result->getAttributeInfo = getAttInfo;
    result->setAttributeInfo = setAttInfo;

    uriEnv->name = result->localName;
    if (jk2_uriEnv_parseName(env, uriEnv, result->localName) != JK_OK) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriEnv.factory() error parsing %s\n", uriEnv->name);
        return JK_ERR;
    }
    uriEnv->workerEnv = env->getByName(env, "workerEnv");
    uriEnv->workerEnv->uriMap->addUriEnv(env, uriEnv->workerEnv->uriMap,
                                         uriEnv);
    uriEnv->uriMap = uriEnv->workerEnv->uriMap;

    /* ??? This may be turned on by default 
     * so that global mappings are always present
     * on each vhost, instead of explicitly defined.
     */
#if 1
    uriEnv->inherit_globals = 1;
#endif

    return JK_OK;
}
