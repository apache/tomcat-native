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
            if (icase && tolower(str[x]) != tolower(exp[y]))
                return 1;
            else if (!icase && str[x] != exp[y])
                return 1;
        }
    }
    return (str[x] != '\0');
} 

/*
 * We are now in a security nightmare, it maybe that somebody sent 
 * us a uri that looks like /top-secret.jsp. and the web server will 
 * fumble and return the jsp content. 
 *
 * To solve that we will check for path info following the suffix, we 
 * will also check that the end of the uri is not ".suffix.",
 * ".suffix/", or ".suffix ".
 */
static int check_security_fraud(jk_uri_worker_map_t *uw_map, const char *uri)
{
    unsigned i;

    for (i = 0; i < uw_map->size; i++) {
        if (uw_map->maps[i]->s->match_type == MATCH_TYPE_SUFFIX) {
            char *suffix_start;
            for (suffix_start = strstr(uri, uw_map->maps[i]->suffix);
                 suffix_start;
                 suffix_start =
                 strstr(suffix_start + 1, uw_map->maps[i]->suffix)) {

                if ('.' != *(suffix_start - 1)) {
                    continue;
                }
                else {
                    char *after_suffix =
                        suffix_start + strlen(uw_map->maps[i]->suffix);

                    if ((('.' == *after_suffix) || ('/' == *after_suffix)
                         || (' ' == *after_suffix))
                        && (0 ==
                            JK_STRNCMP(uw_map->maps[i]->context, uri,
                                       uw_map->maps[i]->context_len))) {
                        /* 
                         * Security violation !!!
                         * this is a fraud.
                         */
                        return i;
                    }
                }
            }
        }
    }

    return -1;
}


int uri_worker_map_alloc(jk_uri_worker_map_t **uw_map,
                         jk_map_t *init_data, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (init_data && uw_map) {
        int rc = uri_worker_map_open(*uw_map =
                                     (jk_uri_worker_map_t *)
                                     malloc(sizeof(jk_uri_worker_map_t)),
                                     init_data, l);
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
    int allocated = 0;

    JK_TRACE_ENTER(l);

    if (*puri == '!') {
        match_type = MATCH_TYPE_NO_MATCH;
        puri++;
    }
    
    /* Find if duplicate entry */    
    for (i = 0; i < uw_map->size; i++) {
        uwr = uw_map->maps[i];
        if (strcmp(uwr->uri, puri) == 0) {
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
                uwr->worker_name = worker;
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

    if (uri[0] == '/') {
        char *asterisk = strchr(uri, '*');
        
        if ((asterisk && strchr(asterisk + 1, '*')) ||
            strchr(uri, '?')) {
            uwr->uri = uri;
            /* Lets check if we have multiple
             * asterixes in the uri like:
             * /context/ * /user/ *
             */
            uwr->context = uri;
            match_type |= MATCH_TYPE_WILDCHAR_PATH;
            jk_log(l, JK_LOG_DEBUG,
                    "wild chars path rule %s=%s was added",
                    uri, worker);

        }
        else if (asterisk) {
            uwr->uri = jk_pool_strdup(&uw_map->p, uri);

            if (!uwr->uri) {
                jk_log(l, JK_LOG_ERROR,
                       "can't alloc uri string");
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }
            /*
             * Now, lets check that the pattern is /context/asterisk.suffix
             * or /context/asterisk
             * we need to have a '/' then a '*' and the a '.' or a
             * '/' then a '*'
             */
            asterisk--;
            if ('/' == asterisk[0]) {
                if (0 == strncmp("/*/", uri, 3)) {
                    /* general context path */
                    asterisk[1] = '\0';
                    uwr->context = uri;
                    uwr->suffix = asterisk + 2;
                    match_type |= MATCH_TYPE_CONTEXT_PATH;
                    jk_log(l, JK_LOG_DEBUG,
                           "general context path rule %s*%s=%s was added",
                           uri, asterisk + 2, worker);
                }
                else if ('.' == asterisk[2]) {
                    /* suffix rule */
                    asterisk[1] = asterisk[2] = '\0';
                    uwr->context = uri;
                    uwr->suffix = asterisk + 3;
                    match_type |= MATCH_TYPE_SUFFIX;
                    jk_log(l, JK_LOG_DEBUG,
                           "suffix rule %s.%s=%s was added",
                           uri, asterisk + 3, worker);
                }
                else if ('\0' != asterisk[2]) {
                    /* general suffix rule */
                    asterisk[1] = '\0';
                    uwr->context = uri;
                    uwr->suffix = asterisk + 2;
                    match_type |= MATCH_TYPE_GENERAL_SUFFIX;
                    jk_log(l, JK_LOG_DEBUG,
                           "general suffix rule %s*%s=%s was added",
                           uri, asterisk + 2, worker);
                }
                else {
                    /* context based */
                    asterisk[1] = '\0';
                    uwr->context = uri;
                    match_type |= MATCH_TYPE_CONTEXT;
                    jk_log(l, JK_LOG_DEBUG,
                           "match rule %s=%s was added", uri, worker);
                }
            }
            else {
                /* Something like : JkMount /servlets/exampl* ajp13 */
                uwr->uri = uri;
                uwr->context = uri;
                match_type |= MATCH_TYPE_EXACT;
                jk_log(l, JK_LOG_DEBUG,
                       "exact rule %s=%s was added",
                       uri, worker);
            }

        }
        else {
            /* Something like:  JkMount /login/j_security_check ajp13 */
            uwr->uri = uri;
            uwr->context = uri;
            match_type |= MATCH_TYPE_EXACT;
            jk_log(l, JK_LOG_DEBUG,
                   "exact rule %s=%s was added",
                   uri, worker);
        }
        uwr->worker_name = worker;
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

    uw_map->maps[uw_map->size] = uwr;
    uw_map->size++;
    if (match_type & MATCH_TYPE_NO_MATCH) {    
        /* If we split the mappings this one will be calculated */
        uw_map->nosize++;
    }
    worker_qsort(uw_map);
    uwr->s = jk_shm_alloc_urimap(&uw_map->p);
    if (!uwr->s) {
        jk_log(l, JK_LOG_ERROR,
               "can't alloc shared memory map entry");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    uwr->s->match_type = match_type;
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

#if 0
/* deprecated */
static void jk_no2slash(char *name)
{
    char *d, *s;

    s = d = name;

#if defined(HAVE_UNC_PATHS)
    /* Check for UNC names.  Leave leading two slashes. */
    if (s[0] == '/' && s[1] == '/')
        *d++ = *s++;
#endif

    while (*s) {
        if ((*d++ = *s) == '/') {
            do {
                ++s;
            } while (*s == '/');
        }
        else {
            ++s;
        }
    }
    *d = '\0';
}
#endif

static int is_nomap_match(jk_uri_worker_map_t *uw_map,
                          const char *uri, const char* worker,
                          jk_logger_t *l)
{
    unsigned int i;

    JK_TRACE_ENTER(l);

    for (i = 0; i < uw_map->size; i++) {
        uri_worker_record_t *uwr = uw_map->maps[i];

        /* Check only nomatch mappings */
        if (!(uwr->s->match_type & MATCH_TYPE_NO_MATCH) ||
            (uwr->s->match_type & MATCH_TYPE_DISABLED))
            continue;        
        /* Check only mathing workers */
        if (strcmp(uwr->worker_name, worker))
            continue;
        if (uwr->s->match_type & MATCH_TYPE_WILDCHAR_PATH) {
            /* Map is already sorted by context_len */
            if (wildchar_match(uri, uwr->context,
#ifdef WIN32
                               1
#else
                               0
#endif
                               ) == 0) {
                    jk_log(l, JK_LOG_DEBUG,
                           "Found a wildchar no match %s -> %s",
                           uwr->worker_name, uwr->context);
                    JK_TRACE_EXIT(l);
                    return JK_TRUE;
             }
        }
        else if (JK_STRNCMP(uwr->context, uri, uwr->context_len) == 0) {
            if (uwr->s->match_type & MATCH_TYPE_EXACT) {
                if (strlen(uri) == uwr->context_len) {
                    if (JK_IS_DEBUG_LEVEL(l))
                        jk_log(l, JK_LOG_DEBUG,
                               "Found an exact no match %s -> %s",
                               uwr->worker_name, uwr->context);
                    JK_TRACE_EXIT(l);
                    return JK_TRUE;
                }
            }
            else if (uwr->s->match_type & MATCH_TYPE_CONTEXT) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Found a context no match %s -> %s",
                           uwr->worker_name, uwr->context);
                JK_TRACE_EXIT(l);
                return JK_TRUE;
            }
            else if (uwr->s->match_type & MATCH_TYPE_GENERAL_SUFFIX) {
                int suffix_start = last_index_of(uri, uwr->suffix[0]);
                if (suffix_start >= 0
                    && 0 == JK_STRCMP(uri + suffix_start, uwr->suffix)) {
                        if (JK_IS_DEBUG_LEVEL(l))
                            jk_log(l, JK_LOG_DEBUG,
                                   "Found a general no suffix match for %s -> %s",
                                   uwr->worker_name, uwr->uri);
                        JK_TRACE_EXIT(l);
                        return JK_TRUE;
                }
            }
            else if (uwr->s->match_type & MATCH_TYPE_CONTEXT_PATH) {
                char *suffix_path = NULL;
                if (strlen(uri) > 1
                    && (suffix_path = strchr(uri + 1, '/')) != NULL) {
                    if (0 ==
                        JK_STRNCMP(suffix_path, uwr->suffix,
                                   strlen(uwr->suffix))) {
                        jk_log(l, JK_LOG_DEBUG,
                               "Found a general context no match %s -> %s",
                               uwr->worker_name, uwr->context);
                        JK_TRACE_EXIT(l);
                        return JK_TRUE;
                    }
                }
            }
            else {          /* suffix match */
                size_t suffix_start = strlen(uri) - 1;

                for ( ; suffix_start > 0 && '.' != uri[suffix_start];
                     suffix_start--) {
                    /* TODO: use while loop */
                }
                if (uri[suffix_start] == '.') {
                    const char *suffix = uri + suffix_start + 1;
                    if (JK_STRCMP(suffix, uwr->suffix) == 0) {
                        if (uwr->s->match_type & MATCH_TYPE_NO_MATCH) {
                            if (JK_IS_DEBUG_LEVEL(l))
                                jk_log(l, JK_LOG_DEBUG,
                                       "Found a no suffix match for %s -> %s",
                                       uwr->worker_name, uwr->uri);
                            JK_TRACE_EXIT(l);
                            return JK_TRUE;
                        }
                    }
                }
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
    int best_match = -1;
    size_t longest_match = 0;
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

#if 0
    /* XXX: Disable checking for two slashes.
     * We should not interfere in the web server
     * request processing.
     * It is up to the web server or Tomcat to decide if
     * the url is valid.
     */
    jk_no2slash(uri);
#endif

    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG, "Attempting to map URI '%s' from %d maps",
               uri, uw_map->size);

    for (i = 0; i < uw_map->size; i++) {
        uri_worker_record_t *uwr = uw_map->maps[i];

        /* Check for match types */
        if ((uwr->s->match_type & MATCH_TYPE_DISABLED) ||
            (uwr->s->match_type & MATCH_TYPE_NO_MATCH) ||
            (uwr->context_len < longest_match)) {
            /* can not be a best match anyway */
            continue;
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG, "Attempting to map context URI '%s'", uwr->uri);

        if (uwr->s->match_type & MATCH_TYPE_WILDCHAR_PATH) {
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
            if (uwr->s->match_type & MATCH_TYPE_EXACT) {
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
            else if (uwr->s->match_type & MATCH_TYPE_CONTEXT) {
                if (uwr->context_len > longest_match) {
                    if (JK_IS_DEBUG_LEVEL(l))
                        jk_log(l, JK_LOG_DEBUG,
                               "Found a context match %s -> %s",
                               uwr->worker_name, uwr->context);
                    longest_match = uwr->context_len;
                    best_match = i;
                }
            }
            else if (uwr->s->match_type & MATCH_TYPE_GENERAL_SUFFIX) {
                int suffix_start = last_index_of(uri, uwr->suffix[0]);
                if (suffix_start >= 0
                    && 0 == strcmp(uri + suffix_start, uwr->suffix)) {
                    if (uwr->context_len >= longest_match) {
                        if (JK_IS_DEBUG_LEVEL(l))
                            jk_log(l, JK_LOG_DEBUG,
                                   "Found a general suffix match %s -> *%s",
                                   uwr->worker_name, uwr->suffix);
                        longest_match = uwr->context_len;
                        best_match = i;
                    }
                }
            }
            else if (uwr->s->match_type & MATCH_TYPE_CONTEXT_PATH) {
                char *suffix_path = NULL;
                if (strlen(uri) > 1
                    && (suffix_path = strchr(uri + 1, '/')) != NULL) {
                    if (0 ==
                        JK_STRNCMP(suffix_path, uwr->suffix,
                                   strlen(uwr->suffix))) {
                        if (uwr->context_len >= longest_match) {
                            if (JK_IS_DEBUG_LEVEL(l))
                                jk_log(l, JK_LOG_DEBUG,
                                       "Found a general context path match %s -> *%s",
                                       uwr->worker_name, uwr->suffix);
                            longest_match = uwr->context_len;
                            best_match = i;
                        }
                    }
                }
            }
            else {          /* suffix match */

                size_t suffix_start = strlen(uri) - 1;

                for ( ; suffix_start > 0 && '.' != uri[suffix_start];
                     suffix_start--) {
                    /* TODO: use while loop */
                }
                if (uri[suffix_start] == '.') {
                    const char *suffix = uri + suffix_start + 1;
                    /* for WinXX, fix the JsP != jsp problems */
                    if (JK_STRCMP(suffix, uwr->suffix) == 0) {
                        if (uwr->context_len >= longest_match) {
                            if (JK_IS_DEBUG_LEVEL(l))
                                jk_log(l, JK_LOG_DEBUG,
                                       "Found a suffix match %s -> *.%s",
                                       uwr->worker_name, uwr->suffix);
                            longest_match = uwr->context_len;
                            best_match = i;
                        }
                    }
                }
            }
        }
    }

    if (best_match != -1) {
        JK_TRACE_EXIT(l);
        rv = uw_map->maps[best_match]->worker_name;
        goto cleanup;
    }
    else {
        /*
        * We are now in a security nightmare, it maybe that somebody sent 
        * us a uri that looks like /top-secret.jsp. and the web server will 
        * fumble and return the jsp content. 
        *
        * To solve that we will check for path info following the suffix, we 
        * will also check that the end of the uri is not .suffix.
        */
        int fraud = check_security_fraud(uw_map, uri);

        if (fraud >= 0) {
            jk_log(l, JK_LOG_EMERG,
                   "Found a security fraud in '%s'",
                   uri);
            JK_TRACE_EXIT(l);
            rv = uw_map->maps[fraud]->worker_name;
            goto cleanup;
        }
    }
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
            JK_TRACE_EXIT(l);
            rv = NULL;
        }
    }
    return rv;
}
