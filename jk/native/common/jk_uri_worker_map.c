/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/***************************************************************************
 * Description: URI to worker map object.                                  *
 *                                                                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Mladen Turk <mturk@apache.org>                             *
 * Version:     $Revision$                                          *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_util.h"
#include "jk_map.h"
#include "jk_mt.h"
#include "jk_uri_worker_map.h"

#ifdef WIN32
#define JK_STRCMP   strcasecmp
#define JK_STRNCMP  strnicmp
#else
#define JK_STRCMP   strcmp
#define JK_STRNCMP  strncmp
#endif


static const char *uri_worker_map_source_type[] = {
    "unknown",
    SOURCE_TYPE_TEXT_WORKERDEF,
    SOURCE_TYPE_TEXT_JKMOUNT,
    SOURCE_TYPE_TEXT_URIMAP,
    SOURCE_TYPE_TEXT_DISCOVER,
    NULL
};


/* Return the string representation of the uwr source */
const char *uri_worker_map_get_source(uri_worker_record_t *uwr, jk_logger_t *l)
{
    return uri_worker_map_source_type[uwr->source_type];
}

/* Return the string representation of the uwr match type */
char *uri_worker_map_get_match(uri_worker_record_t *uwr, char *buf, jk_logger_t *l)
{
    unsigned int match;

    buf[0] = '\0';
    match = uwr->match_type;

    if (match & MATCH_TYPE_DISABLED)
        strcat(buf, "Disabled ");
/* deprecated
    if (match & MATCH_TYPE_STOPPED)
        strcat(buf, "Stopped ");
 */
    if (match & MATCH_TYPE_NO_MATCH)
        strcat(buf, "Unmount ");
    if (match & MATCH_TYPE_EXACT)
        strcat(buf, "Exact");
    else if (match & MATCH_TYPE_WILDCHAR_PATH)
        strcat(buf, "Wildchar");
/* deprecated
    else if (match & MATCH_TYPE_CONTEXT)
        strcat(buf, "Context");
    else if (match & MATCH_TYPE_CONTEXT_PATH)
        strcat(buf, "Context Path");
    else if (match & MATCH_TYPE_SUFFIX)
        strcat(buf, "Suffix");
    else if (match & MATCH_TYPE_GENERAL_SUFFIX)
        return "General Suffix";
 */
    else
        strcat(buf, "Unknown");
    return buf;
}

/*
 * Given context uri, count the number of path tokens.
 *
 * Servlet specification 2.4, SRV.11.1 says

 *   The container will recursively try tomatch the longest
 *   path-prefix. This is done by stepping down the path tree a
 *   directory at a time, using the / character as a path
 *   separator. The longest match determines the servlet selected.
 *
 * The implication seems to be `most uri path elements is most exact'.
 * This is a little helper function to count uri tokens, so we can
 * keep the worker map sorted with most specific first.
 */
static int worker_count_context_uri_tokens(const char * context)
{
    const char * c = context;
    int count = 0;
    while (c && *c) {
        if ('/' == *c++)
            count++;
    }
    return count;
}

static int worker_compare(const void *elem1, const void *elem2)
{
    uri_worker_record_t *e1 = *(uri_worker_record_t **)elem1;
    uri_worker_record_t *e2 = *(uri_worker_record_t **)elem2;
    int e1_tokens = 0;
    int e2_tokens = 0;

    e1_tokens = worker_count_context_uri_tokens(e1->context);
    e2_tokens = worker_count_context_uri_tokens(e2->context);

    if (e1_tokens != e2_tokens) {
        return (e2_tokens - e1_tokens);
    }
    /* given the same number of URI tokens, use character
     * length as a tie breaker
     */
    if(e2->context_len != e1->context_len)
        return ((int)e2->context_len - (int)e1->context_len);

    return ((int)e2->source_type - (int)e1->source_type);
}

static void worker_qsort(jk_uri_worker_map_t *uw_map)
{

   /* Sort remaining args using Quicksort algorithm: */
   qsort((void *)uw_map->maps, uw_map->size,
         sizeof(uri_worker_record_t *), worker_compare );

}

/* Match = 0, NoMatch = 1, Abort = -1
 * Based loosely on sections of wildmat.c by Rich Salz
 */
static int wildchar_match(const char *str, const char *exp, int icase)
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
                if ((ret = wildchar_match(&str[x++], &exp[y], icase)) != 1)
                    return ret;
            }
            return -1;
        }
        else if (exp[y] != '?') {
            if (icase && (tolower(str[x]) != tolower(exp[y])))
                return 1;
            else if (!icase && str[x] != exp[y])
                return 1;
        }
    }
    return (str[x] != '\0');
}

int uri_worker_map_alloc(jk_uri_worker_map_t **uw_map_p,
                         jk_map_t *init_data, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (uw_map_p) {
        int rc;
        jk_uri_worker_map_t *uw_map;
        *uw_map_p = (jk_uri_worker_map_t *)calloc(1, sizeof(jk_uri_worker_map_t));
        uw_map = *uw_map_p;

        JK_INIT_CS(&(uw_map->cs), rc);
        if (rc == JK_FALSE) {
            jk_log(l, JK_LOG_ERROR,
                   "creating thread lock (errno=%d)",
                   errno);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        jk_open_pool(&(uw_map->p),
                     uw_map->buf, sizeof(jk_pool_atom_t) * BIG_POOL_SIZE);
        uw_map->size = 0;
        uw_map->nosize = 0;
        uw_map->capacity = 0;
        uw_map->maps = NULL;
        uw_map->fname = NULL;
        uw_map->reject_unsafe = 0;
        uw_map->reload = JK_URIMAP_DEF_RELOAD;
        uw_map->modified = 0;
        uw_map->checked = 0;

        if (init_data)
            rc = uri_worker_map_open(uw_map, init_data, l);
        JK_TRACE_EXIT(l);
        return rc;
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);

    return JK_FALSE;
}

static int uri_worker_map_close(jk_uri_worker_map_t *uw_map, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (uw_map) {
        int i;
        JK_DELETE_CS(&(uw_map->cs), i);
        jk_close_pool(&uw_map->p);
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

int uri_worker_map_free(jk_uri_worker_map_t **uw_map, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (uw_map && *uw_map) {
        uri_worker_map_close(*uw_map, l);
        free(*uw_map);
        *uw_map = NULL;
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }
    else
        JK_LOG_NULL_PARAMS(l);

    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

/*
 * Ensure there will be memory in context info to store Context Bases
 */

#define UW_INC_SIZE 4           /* 4 URI->WORKER STEP */

static int uri_worker_map_realloc(jk_uri_worker_map_t *uw_map)
{
    if (uw_map->size == uw_map->capacity) {
        uri_worker_record_t **uwr;
        int capacity = uw_map->capacity + UW_INC_SIZE;

        uwr =
            (uri_worker_record_t **) jk_pool_alloc(&uw_map->p,
                                                   sizeof(uri_worker_record_t
                                                          *) * capacity);

        if (!uwr)
            return JK_FALSE;

        if (uw_map->capacity && uw_map->maps)
            memcpy(uwr, uw_map->maps,
                   sizeof(uri_worker_record_t *) * uw_map->capacity);

        uw_map->maps = uwr;
        uw_map->capacity = capacity;
    }

    return JK_TRUE;
}


/*
 * Delete all entries of a given source type
 */

static int uri_worker_map_clear(jk_uri_worker_map_t *uw_map,
                         unsigned int source_type, jk_logger_t *l)
{
    uri_worker_record_t *uwr = NULL;
    unsigned int i;
    unsigned int j;

    JK_TRACE_ENTER(l);

    for (i = 0; i < uw_map->size; i++) {
        uwr = uw_map->maps[i];
        if (uwr->source_type == source_type) {
            jk_log(l, JK_LOG_DEBUG,
                   "deleting map rule '%s=%s' source '%s'",
                   uwr->context, uwr->worker_name, uri_worker_map_get_source(uwr, l));
            for (j = i; j < uw_map->size-1; j++)
                uw_map->maps[j] = uw_map->maps[j+1];
            uw_map->size--;
            i--;
        }
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}



int uri_worker_map_add(jk_uri_worker_map_t *uw_map,
                       const char *puri, const char *worker,
                       unsigned int source_type, jk_logger_t *l)
{
    uri_worker_record_t *uwr = NULL;
    char *uri;
    unsigned int match_type = 0;

    JK_TRACE_ENTER(l);

    if (*puri == '-') {
        /* Disable urimap.
         * This way you can disable already mounted
         * context.
         */
        match_type = MATCH_TYPE_DISABLED;
        puri++;
    }
    if (*puri == '!') {
        match_type |= MATCH_TYPE_NO_MATCH;
        puri++;
    }

    if (uri_worker_map_realloc(uw_map) == JK_FALSE) {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    uwr = (uri_worker_record_t *)jk_pool_alloc(&uw_map->p,
                                    sizeof(uri_worker_record_t));
    if (!uwr) {
        jk_log(l, JK_LOG_ERROR,
               "can't alloc map entry");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    uri = jk_pool_strdup(&uw_map->p, puri);
    if (!uri || !worker) {
        jk_log(l, JK_LOG_ERROR,
               "can't alloc uri/worker strings");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    if (*uri == '/') {
        uwr->uri = uri;
        uwr->context = uri;
        uwr->worker_name = jk_pool_strdup(&uw_map->p, worker);
        uwr->context_len = strlen(uwr->context);
        uwr->source_type = source_type;
        if (strchr(uri, '*') ||
            strchr(uri, '?')) {
            /* Something like
             * /context/ * /user/ *
             * /context/ *.suffix
             */
            match_type |= MATCH_TYPE_WILDCHAR_PATH;
            jk_log(l, JK_LOG_DEBUG,
                   "wildchar rule '%s=%s' source '%s' was added",
                   uwr->context, uwr->worker_name, uri_worker_map_get_source(uwr, l));

        }
        else {
            /* Something like:  JkMount /login/j_security_check ajp13 */
            match_type |= MATCH_TYPE_EXACT;
            jk_log(l, JK_LOG_DEBUG,
                   "exact rule '%s=%s' source '%s' was added",
                   uwr->context, uwr->worker_name, uri_worker_map_get_source(uwr, l));
        }
    }
    else {
        /*
         * JFC: please check...
         * Not sure what to do, but I try to prevent problems.
         * I have fixed jk_mount_context() in apaches/mod_jk.c so we should
         * not arrive here when using Apache.
         */
        jk_log(l, JK_LOG_ERROR,
               "invalid context '%s': does not begin with '/'",
               uri);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    uwr->match_type = match_type;
    uw_map->maps[uw_map->size] = uwr;
    uw_map->size++;
    if (match_type & MATCH_TYPE_NO_MATCH) {
        /* If we split the mappings this one will be calculated */
        uw_map->nosize++;
    }
    worker_qsort(uw_map);
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

int uri_worker_map_open(jk_uri_worker_map_t *uw_map,
                        jk_map_t *init_data, jk_logger_t *l)
{
    int rc = JK_TRUE;

    JK_TRACE_ENTER(l);

    if (uw_map) {
        int sz = jk_map_size(init_data);

        jk_log(l, JK_LOG_DEBUG,
               "rule map size is %d",
               sz);

        if (sz > 0) {
            int i;
            for (i = 0; i < sz; i++) {
                const char *u = jk_map_name_at(init_data, i);
                const char *w = jk_map_value_at(init_data, i);
                /* Multiple mappings like :
                 * /servlets-examples|/ *
                 * will create two mappings:
                 * /servlets-examples
                 * and:
                 * /servlets-examples/ *
                 */
                if (strchr(u, '|')) {
                    char *s, *r = strdup(u);
                    s = strchr(r, '|');
                    *(s++) = '\0';
                    /* Add first mapping */
                    if (!uri_worker_map_add(uw_map, r, w, SOURCE_TYPE_JKMOUNT, l)) {
                        jk_log(l, JK_LOG_ERROR,
                        "invalid mapping rule %s->%s", r, w);
                        rc = JK_FALSE;
                    }
                    for (; *s; s++)
                        *(s - 1) = *s;
                    *(s - 1) = '\0';
                    /* add second mapping */
                    if (!uri_worker_map_add(uw_map, r, w, SOURCE_TYPE_JKMOUNT, l)) {
                        jk_log(l, JK_LOG_ERROR,
                               "invalid mapping rule %s->%s", r, w);
                        rc = JK_FALSE;
                    }
                    free(r);
                }
                else if (!uri_worker_map_add(uw_map, u, w, SOURCE_TYPE_JKMOUNT, l)) {
                    jk_log(l, JK_LOG_ERROR,
                           "invalid mapping rule %s->%s",
                           u, w);
                    rc = JK_FALSE;
                    break;
                }
                if (rc == JK_FALSE)
                    break;
            }
        }

        if (rc == JK_FALSE) {
            jk_log(l, JK_LOG_ERROR,
                   "there was an error, freing buf");
            jk_close_pool(&uw_map->p);
        }
    }

    JK_TRACE_EXIT(l);
    return rc;
}

static const char *find_match(jk_uri_worker_map_t *uw_map,
                        const char *url, jk_logger_t *l)
{
    unsigned int i;

    JK_TRACE_ENTER(l);

    for (i = 0; i < uw_map->size; i++) {
        uri_worker_record_t *uwr = uw_map->maps[i];

        /* Check for match types */
        if ((uwr->match_type & MATCH_TYPE_DISABLED) ||
            (uwr->match_type & MATCH_TYPE_NO_MATCH))
            continue;

        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG, "Attempting to map context URI '%s=%s' source '%s'",
                   uwr->context, uwr->worker_name, uri_worker_map_get_source(uwr, l));

        if (uwr->match_type & MATCH_TYPE_WILDCHAR_PATH) {
            /* Map is already sorted by context_len */
            if (wildchar_match(url, uwr->context,
#ifdef WIN32
                               0
#else
                               0
#endif
                               ) == 0) {
                    if (JK_IS_DEBUG_LEVEL(l))
                        jk_log(l, JK_LOG_DEBUG,
                               "Found a wildchar match '%s=%s'",
                               uwr->context, uwr->worker_name);
                    JK_TRACE_EXIT(l);
                    return uwr->worker_name;
             }
        }
        else if (JK_STRNCMP(uwr->context, url, uwr->context_len) == 0) {
            if (strlen(url) == uwr->context_len) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Found an exact match '%s=%s'",
                           uwr->context, uwr->worker_name);
                JK_TRACE_EXIT(l);
                return uwr->worker_name;
            }
        }
    }

    JK_TRACE_EXIT(l);
    return NULL;
}

static int is_nomatch(jk_uri_worker_map_t *uw_map,
                      const char *uri, const char* worker,
                      jk_logger_t *l)
{
    unsigned int i;

    JK_TRACE_ENTER(l);

    for (i = 0; i < uw_map->size; i++) {
        uri_worker_record_t *uwr = uw_map->maps[i];

        /* Check only nomatch mappings */
        if (!(uwr->match_type & MATCH_TYPE_NO_MATCH) ||
            (uwr->match_type & MATCH_TYPE_DISABLED))
            continue;
        /* Check only matching workers */
        if (*uwr->worker_name != '*' && strcmp(uwr->worker_name, worker))
            continue;
        if (uwr->match_type & MATCH_TYPE_WILDCHAR_PATH) {
            /* Map is already sorted by context_len */
            if (wildchar_match(uri, uwr->context,
#ifdef WIN32
                               0
#else
                               0
#endif
                               ) == 0) {
                    jk_log(l, JK_LOG_DEBUG,
                           "Found a wildchar no match '%s=%s' source '%s'",
                           uwr->context, uwr->worker_name, uri_worker_map_get_source(uwr, l));
                    JK_TRACE_EXIT(l);
                    return JK_TRUE;
             }
        }
        else if (JK_STRNCMP(uwr->context, uri, uwr->context_len) == 0) {
            if (strlen(uri) == uwr->context_len) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Found an exact no match '%s=%s' source '%s'",
                           uwr->context, uwr->worker_name, uri_worker_map_get_source(uwr, l));
                JK_TRACE_EXIT(l);
                return JK_TRUE;
            }
        }
    }

    JK_TRACE_EXIT(l);
    return JK_FALSE;
}


const char *map_uri_to_worker(jk_uri_worker_map_t *uw_map,
                              const char *uri, const char *vhost,
                              jk_logger_t *l)
{
    unsigned int i;
    unsigned int vhost_len;
    int reject_unsafe;
    const char *rv = NULL;
    char  url[JK_MAX_URI_LEN+1];

    JK_TRACE_ENTER(l);

    if (!uw_map || !uri) {
        JK_LOG_NULL_PARAMS(l);
        JK_TRACE_EXIT(l);
        return NULL;
    }
    if (*uri != '/') {
        jk_log(l, JK_LOG_WARNING,
                "Uri %s is invalid. Uri must start with /", uri);
        JK_TRACE_EXIT(l);
        return NULL;
    }
    if (uw_map->fname) {
        uri_worker_map_update(uw_map, 0, l);
        if (!uw_map->size) {
            jk_log(l, JK_LOG_INFO,
                   "No worker maps defined for %s.",
                   uw_map->fname);
            JK_TRACE_EXIT(l);
            return NULL;
        }
    }
    /* Make the copy of the provided uri and strip
     * everything after the first ';' char.
     */
    reject_unsafe = uw_map->reject_unsafe;
    vhost_len = 0;
/*
 * In case we got a vhost, we prepend a slash
 * and the vhost to the url in order to enable
 * vhost mapping rules especially for IIS.
 */
    if (vhost) {
/* Size including leading slash. */
        vhost_len = strlen(vhost) + 1;
        if (vhost_len >= JK_MAX_URI_LEN) {
            vhost_len = 0;
            jk_log(l, JK_LOG_WARNING,
                   "Host prefix %s for URI %s is invalid and will be ignored."
                   " It must be smaller than %d chars",
                   vhost, JK_MAX_URI_LEN-1);
        }
        else {
            url[0] = '/';
            strncpy(&url[1], vhost, vhost_len);
        }
    }
    for (i = 0; i < strlen(uri); i++) {
        if (i == JK_MAX_URI_LEN) {
            jk_log(l, JK_LOG_WARNING,
                   "URI %s is invalid. URI must be smaller than %d chars",
                   uri, JK_MAX_URI_LEN);
            JK_TRACE_EXIT(l);
            return NULL;
        }
        if (uri[i] == ';')
            break;
        else {
            url[i + vhost_len] = uri[i];
            if (reject_unsafe && (uri[i] == '%' || uri[i] == '\\')) {
                jk_log(l, JK_LOG_INFO, "Potentially unsafe request url '%s' rejected", uri);
                JK_TRACE_EXIT(l);
                return NULL;
            }
        }
    }
    url[i + vhost_len] = '\0';

    if (JK_IS_DEBUG_LEVEL(l)) {
        char *url_rewrite = strstr(uri, JK_PATH_SESSION_IDENTIFIER);
        if (url_rewrite)
            jk_log(l, JK_LOG_DEBUG, "Found session identifier '%s' in url '%s'",
                   url_rewrite, uri);
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG, "Attempting to map URI '%s' from %d maps",
               url, uw_map->size);
    rv = find_match(uw_map, url, l);
/* If this doesn't find a match, try without the vhost. */
    if (! rv && vhost_len) {
        rv = find_match(uw_map, &url[vhost_len], l);
    }

/* In case we found a match, check for the unmounts. */
    if (rv && uw_map->nosize) {
/* Again first including vhost. */
        int rc = is_nomatch(uw_map, url, rv, l);
/* If no unmount was find, try without vhost. */
        if (!rc && vhost_len)
            rc = is_nomatch(uw_map, &url[vhost_len], rv, l);
        if (rc) {
            if (JK_IS_DEBUG_LEVEL(l)) {
                jk_log(l, JK_LOG_DEBUG,
                       "Denying match for worker %s by nomatch rule",
                       rv);
            }
            rv = NULL;
        }
    }

    JK_TRACE_EXIT(l);
    return rv;
}

int uri_worker_map_load(jk_uri_worker_map_t *uw_map,
                        jk_logger_t *l)
{
    int rc = JK_FALSE;
    jk_map_t *map;

    jk_map_alloc(&map);
    if (jk_map_read_properties(map, uw_map->fname, &uw_map->modified,
                               JK_MAP_HANDLE_NORMAL, l)) {
        int i;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Loading urimaps from %s with reload check interval %d seconds",
                   uw_map->fname, uw_map->reload);
        uri_worker_map_clear(uw_map, SOURCE_TYPE_URIMAP, l);
        for (i = 0; i < jk_map_size(map); i++) {
            const char *u = jk_map_name_at(map, i);
            const char *w = jk_map_value_at(map, i);
            /* Multiple mappings like :
             * /servlets-examples|/ *
             * will create two mappings:
             * /servlets-examples
             * and:
             * /servlets-examples/ *
             */
            if (strchr(u, '|')) {
                char *s, *r = strdup(u);
                s = strchr(r, '|');
                *(s++) = '\0';
                /* Add first mapping */
                if (!uri_worker_map_add(uw_map, r, w, SOURCE_TYPE_URIMAP, l)) {
                    jk_log(l, JK_LOG_ERROR,
                    "invalid mapping rule %s->%s", r, w);
                }
                for (; *s; s++)
                   *(s - 1) = *s;
                *(s - 1) = '\0';
                /* add second mapping */
                if (!uri_worker_map_add(uw_map, r, w, SOURCE_TYPE_URIMAP, l)) {
                    jk_log(l, JK_LOG_ERROR,
                    "invalid mapping rule %s->%s", r, w);
                }
                free(r);
            }
            else if (!uri_worker_map_add(uw_map, u, w, SOURCE_TYPE_URIMAP, l)) {
                jk_log(l, JK_LOG_ERROR,
                       "invalid mapping rule %s->%s",
                       u, w);
            }
        }
        uw_map->checked = time(NULL);
        rc = JK_TRUE;
    }
    jk_map_free(&map);
    return rc;
}

int uri_worker_map_update(jk_uri_worker_map_t *uw_map,
                          int force, jk_logger_t *l)
{
    int rc = JK_TRUE;
    time_t now = time(NULL);

    if ((uw_map->reload > 0 && difftime(now, uw_map->checked) > uw_map->reload) ||
        force) {
        struct stat statbuf;
        uw_map->checked = now;
        if ((rc = jk_stat(uw_map->fname, &statbuf)) == -1) {
            jk_log(l, JK_LOG_ERROR,
                   "Unable to stat the %s (errno=%d)",
                   uw_map->fname, errno);
            return JK_FALSE;
        }
        if (statbuf.st_mtime == uw_map->modified) {
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "File %s  is not modified",
                       uw_map->fname);
            return JK_TRUE;
        }
        JK_ENTER_CS(&(uw_map->cs), rc);
        /* Check if some other thread updated status */
        if (statbuf.st_mtime == uw_map->modified) {
            JK_LEAVE_CS(&(uw_map->cs), rc);
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "File %s  is not modified",
                       uw_map->fname);
            return JK_TRUE;
        }
        rc = uri_worker_map_load(uw_map, l);
        JK_LEAVE_CS(&(uw_map->cs), rc);
        jk_log(l, JK_LOG_INFO,
               "Reloaded urimaps from %s", uw_map->fname);
    }
    return JK_TRUE;
}
