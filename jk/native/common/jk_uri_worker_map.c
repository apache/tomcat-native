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

static int worker_compare(const void *elem1, const void *elem2)
{
    uri_worker_record_t *e1 = *(uri_worker_record_t **)elem1;
    uri_worker_record_t *e2 = *(uri_worker_record_t **)elem2;
    return ((int)e2->context_len - (int)e1->context_len);
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

int uri_worker_map_alloc(jk_uri_worker_map_t **uw_map,
                         jk_map_t *init_data, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (uw_map) {
        int rc;
        *uw_map = (jk_uri_worker_map_t *)calloc(1, sizeof(jk_uri_worker_map_t));

        JK_INIT_CS(&((*uw_map)->cs), rc);
        if (rc == JK_FALSE) {
            jk_log(l, JK_LOG_ERROR,
                   "creating thread lock errno=%d",
                   errno);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        if (init_data)
            rc = uri_worker_map_open(*uw_map, init_data, l);
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


int uri_worker_map_add(jk_uri_worker_map_t *uw_map,
                       const char *puri, const char *worker, jk_logger_t *l)
{
    uri_worker_record_t *uwr = NULL;
    char *uri;
    unsigned int match_type = 0;
    unsigned int i;

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

    /* Find if duplicate entry */
    for (i = 0; i < uw_map->size; i++) {
        uwr = uw_map->maps[i];
        if (strcmp(uwr->uri, puri) == 0) {
            /* Update disabled flag */
            if (match_type & MATCH_TYPE_DISABLED)
                uwr->match_type |= MATCH_TYPE_DISABLED;
            else
                uwr->match_type &= ~MATCH_TYPE_DISABLED;
            if (strcmp(uwr->worker_name, worker) == 0) {
                jk_log(l, JK_LOG_DEBUG,
                       "map rule %s=%s already exists",
                       puri, worker);
                JK_TRACE_EXIT(l);
                return JK_TRUE;
            }
            else {
                jk_log(l, JK_LOG_DEBUG,
                       "changing map rule %s=%s ",
                       puri, worker);
                uwr->worker_name = jk_pool_strdup(&uw_map->p, worker);
                JK_TRACE_EXIT(l);
                return JK_TRUE;
            }
        }
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
    uwr->suffix = NULL;

    uri = jk_pool_strdup(&uw_map->p, puri);
    if (!uri || !worker) {
        jk_log(l, JK_LOG_ERROR,
               "can't alloc uri/worker strings");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    if (*uri == '/') {
        if (strchr(uri, '*') ||
            strchr(uri, '?')) {
            /* Something like
             * /context/ * /user/ *
             * /context/ *.suffix
             */
            match_type |= MATCH_TYPE_WILDCHAR_PATH;
            jk_log(l, JK_LOG_DEBUG,
                    "wildchar rule %s=%s was added",
                    uri, worker);

        }
        else {
            /* Something like:  JkMount /login/j_security_check ajp13 */
            match_type |= MATCH_TYPE_EXACT;
            jk_log(l, JK_LOG_DEBUG,
                   "exact rule %s=%s was added",
                   uri, worker);
        }
        uwr->uri = uri;
        uwr->context = uri;
        uwr->worker_name = jk_pool_strdup(&uw_map->p, worker);
        uwr->context_len = strlen(uwr->context);
    }
    else {
        /*
         * JFC: please check...
         * Not sure what to do, but I try to prevent problems.
         * I have fixed jk_mount_context() in apaches/mod_jk.c so we should
         * not arrive here when using Apache.
         */
        jk_log(l, JK_LOG_ERROR,
               "invalid context %s",
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

    uw_map->size = 0;
    uw_map->capacity = 0;

    if (uw_map) {
        int sz;

        rc = JK_TRUE;
        jk_open_pool(&uw_map->p,
                     uw_map->buf, sizeof(jk_pool_atom_t) * SMALL_POOL_SIZE);
        uw_map->size = 0;
        uw_map->maps = NULL;

        sz = jk_map_size(init_data);

        jk_log(l, JK_LOG_DEBUG,
               "rule map size is %d",
               sz);

        if (sz > 0) {
            int i;
            for (i = 0; i < sz; i++) {
                if (uri_worker_map_add
                    (uw_map, jk_map_name_at(init_data, i),
                     jk_map_value_at(init_data, i), l) == JK_FALSE) {
                    rc = JK_FALSE;
                    break;
                }
            }

            if (i == sz) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "there are %d rules",
                           uw_map->size);
            }
            else {
                jk_log(l, JK_LOG_ERROR,
                       "Parsing error");
                rc = JK_FALSE;
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

/* returns the index of the last occurrence of the 'ch' character
   if ch=='\0' returns the length of the string str  */
static int last_index_of(const char *str, char ch)
{
    const char *str_minus_one = str - 1;
    const char *s = str + strlen(str);
    while (s != str_minus_one && ch != *s) {
        --s;
    }
    return (s - str);
}

static int is_nomap_match(jk_uri_worker_map_t *uw_map,
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
        /* Check only mathing workers */
        if (strcmp(uwr->worker_name, worker))
            continue;
        if (uwr->match_type & MATCH_TYPE_WILDCHAR_PATH) {
            /* Map is already sorted by context_len */
            if (wildchar_match(uri, uwr->context,
#ifdef WIN32
                               1
#else
                               0
#endif
                               ) == 0) {
                    jk_log(l, JK_LOG_DEBUG,
                           "Found a no match %s -> %s",
                           uwr->worker_name, uwr->context);
                    JK_TRACE_EXIT(l);
                    return JK_TRUE;
             }
        }
        else if (JK_STRNCMP(uwr->context, uri, uwr->context_len) == 0) {
            if (strlen(uri) == uwr->context_len) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                            "Found an exact no match %s -> %s",
                            uwr->worker_name, uwr->context);
                JK_TRACE_EXIT(l);
                return JK_TRUE;
            }
        }
    }

    JK_TRACE_EXIT(l);
    return JK_FALSE;
}


const char *map_uri_to_worker(jk_uri_worker_map_t *uw_map,
                              char *uri, jk_logger_t *l)
{
    unsigned int i;
    char *url_rewrite;
    char rewrite_char = ';';
    const char *rv = NULL;

    JK_TRACE_ENTER(l);
    if (!uw_map || !uri) {
        JK_LOG_NULL_PARAMS(l);
        JK_TRACE_EXIT(l);
        return NULL;
    }
    if (*uri != '/') {
        jk_log(l, JK_LOG_ERROR,
                "uri must start with /");
        JK_TRACE_EXIT(l);
        return NULL;
    }
    url_rewrite = strstr(uri, JK_PATH_SESSION_IDENTIFIER);
    if (url_rewrite) {
        rewrite_char = *url_rewrite;
        *url_rewrite = '\0';
    }

    if (uw_map->fname)
        uri_worker_map_update(uw_map, l);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG, "Attempting to map URI '%s' from %d maps",
               uri, uw_map->size);

    for (i = 0; i < uw_map->size; i++) {
        uri_worker_record_t *uwr = uw_map->maps[i];

        /* Check for match types */
        if ((uwr->match_type & MATCH_TYPE_DISABLED) ||
            (uwr->match_type & MATCH_TYPE_NO_MATCH))
            continue;

        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG, "Attempting to map context URI '%s'", uwr->uri);

        if (uwr->match_type & MATCH_TYPE_WILDCHAR_PATH) {
            const char *wname;
            /* Map is already sorted by context_len */
            if (wildchar_match(uri, uwr->context,
#ifdef WIN32
                               1
#else
                               0
#endif
                               ) == 0) {
                    wname = uwr->worker_name;
                    if (JK_IS_DEBUG_LEVEL(l))
                        jk_log(l, JK_LOG_DEBUG,
                               "Found a wildchar match %s -> %s",
                               uwr->worker_name, uwr->context);
                    JK_TRACE_EXIT(l);
                    rv = wname;
                    goto cleanup;
             }
        }
        else if (JK_STRNCMP(uwr->context, uri, uwr->context_len) == 0) {
            if (strlen(uri) == uwr->context_len) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Found an exact match %s -> %s",
                           uwr->worker_name, uwr->context);
                JK_TRACE_EXIT(l);
                rv = uwr->worker_name;
                goto cleanup;
            }
        }
    }
    /* No matches found */
    JK_TRACE_EXIT(l);

cleanup:
    if (url_rewrite)
        *url_rewrite = rewrite_char;
    if (rv && uw_map->nosize) {
        if (is_nomap_match(uw_map, uri, rv, l)) {
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Denying matching for worker %s by nomatch rule",
                       rv);
            rv = NULL;
        }
    }
    return rv;
}

int uri_worker_map_load(jk_uri_worker_map_t *uw_map,
                        jk_logger_t *l)
{
    int rc = JK_FALSE;
    jk_map_t *map;

    jk_map_alloc(&map);
    if (jk_map_read_properties(map, uw_map->fname,
                               &uw_map->modified)) {
        int i;
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
                *s = '\0';
                /* Add first mapping */
                if (!uri_worker_map_add(uw_map, r, w, l)) {
                    jk_log(l, JK_LOG_ERROR,
                    "invalid mapping rule %s->%s", r, w);
                }
                s++;
                while (*s)
                   *(s - 1) = *s++;
                *(s - 1) = '\0';
                /* add second mapping */
                if (!uri_worker_map_add(uw_map, r, w, l)) {
                    jk_log(l, JK_LOG_ERROR,
                    "invalid mapping rule %s->%s", r, w);
                }
                free(r);
            }
            else if (!uri_worker_map_add(uw_map, u, w, l)) {
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
                          jk_logger_t *l)
{
    int rc = JK_TRUE;
    time_t now = time(NULL);

    if ((now - uw_map->checked) > JK_URIMAP_RELOAD) {
        struct stat statbuf;
        uw_map->checked = now;
        if ((rc = stat(uw_map->fname, &statbuf)) == -1)
            return JK_FALSE;
        if (statbuf.st_mtime == uw_map->modified)
            return JK_TRUE;
        JK_ENTER_CS(&(uw_map->cs), rc);
        /* Check if some other thread updated status */
        if (statbuf.st_mtime == uw_map->modified) {
            JK_LEAVE_CS(&(uw_map->cs), rc);
            return JK_TRUE;
        }
        rc = uri_worker_map_load(uw_map, l);
        JK_LEAVE_CS(&(uw_map->cs), rc);
        jk_log(l, JK_LOG_INFO,
               "Reloaded urimaps from %s", uw_map->fname);
    }
    return rc;
}

