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
 * Maps can be                                                             *
 *                                                                         *
 * Exact Context -> /exact/uri=worker e.g. /examples/do*=ajp12             *
 * Context Based -> /context/*=worker e.g. /examples/*=ajp12               *
 * Context and suffix ->/context/*.suffix=worker e.g. /examples/*.jsp=ajp12*
 *                                                                         *
 * This lets us either partition the work among the web server and the     *
 * servlet container.                                                      *
 *                                                                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_util.h"
#include "jk_uri_worker_map.h"

#define MATCH_TYPE_EXACT    (0)
#define MATCH_TYPE_CONTEXT  (1)
#define MATCH_TYPE_SUFFIX   (2)
#define MATCH_TYPE_GENERAL_SUFFIX (3)   /* match all URIs of the form *ext */
/* match all context path URIs with a path component suffix */
#define MATCH_TYPE_CONTEXT_PATH (4)
/* match multiple wild characters (*) and (?) */
#define MATCH_TYPE_WILDCHAR_PATH (5)

struct uri_worker_record
{
    /* Original uri for logging */
    char *uri;

    /* Name of worker mapped */
    char *worker_name;

    /* Suffix of uri */
    char *suffix;

    /* Base context */
    char *context;

    /* char length of the context */
    unsigned ctxt_len;

    int match_type;
};
typedef struct uri_worker_record uri_worker_record_t;

struct jk_uri_worker_map
{
    /* Memory Pool */
    jk_pool_t p;
    jk_pool_atom_t buf[SMALL_POOL_SIZE];

    /* Temp Pool */
    jk_pool_t tp;
    jk_pool_atom_t tbuf[SMALL_POOL_SIZE];

    /* map URI->WORKER */
    uri_worker_record_t **maps;

    /* Map Number */
    unsigned size;

    /* Map Capacity */
    unsigned capacity;
};

static int worker_compare(const void *elem1, const void *elem2)
{
    uri_worker_record_t *e1 = *(uri_worker_record_t **)elem1;
    uri_worker_record_t *e2 = *(uri_worker_record_t **)elem2;
    return ((int)e2->ctxt_len - (int)e1->ctxt_len);
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
        if (MATCH_TYPE_SUFFIX == uw_map->maps[i]->match_type) {
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
                            strncmp(uw_map->maps[i]->context, uri,
                                    uw_map->maps[i]->ctxt_len))) {
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
                       char *puri, char *pworker, jk_logger_t *l)
{
    uri_worker_record_t *uwr;
    char *uri;
    char *worker;

    JK_TRACE_ENTER(l);
    if (uri_worker_map_realloc(uw_map) == JK_FALSE) {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    uwr =
        (uri_worker_record_t *) jk_pool_alloc(&uw_map->p,
                                              sizeof(uri_worker_record_t));

    if (!uwr) {
        jk_log(l, JK_LOG_ERROR,
               "can't alloc map entry\n");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    uri = jk_pool_strdup(&uw_map->p, puri);
    worker = jk_pool_strdup(&uw_map->p, pworker);

    if (!uri || !worker) {
        jk_log(l, JK_LOG_ERROR,
               "can't alloc uri/worker strings\n");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    if ('/' == uri[0]) {
        char *asterisk = strchr(uri, '*');
        
        if (asterisk && strchr(asterisk + 1, '*') ||
            strchr(uri, '?')) {
            uwr->uri = jk_pool_strdup(&uw_map->p, uri);

            if (!uwr->uri) {
                jk_log(l, JK_LOG_ERROR,
                       "can't alloc uri string\n");
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }
            /* Lets check if we have multiple
             * asterixes in the uri like:
             * /context/ * /user/ *
             */
            uwr->worker_name = worker;
            uwr->context = uri;
            uwr->suffix = NULL;
            uwr->match_type = MATCH_TYPE_WILDCHAR_PATH;
            jk_log(l, JK_LOG_DEBUG,
                    "wild chars path rule %s=%s was added\n",
                    uri, worker);

        }
        else if (asterisk) {
            uwr->uri = jk_pool_strdup(&uw_map->p, uri);

            if (!uwr->uri) {
                jk_log(l, JK_LOG_ERROR,
                       "can't alloc uri string\n");
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }
            /*
             * Now, lets check that the pattern is /context/*.suffix
             * or /context/*
             * we need to have a '/' then a '*' and the a '.' or a
             * '/' then a '*'
             */
            asterisk--;
            if ('/' == asterisk[0]) {
                if (0 == strncmp("/*/", uri, 3)) {
                    /* general context path */
                    asterisk[1] = '\0';
                    uwr->worker_name = worker;
                    uwr->context = uri;
                    uwr->suffix = asterisk + 2;
                    uwr->match_type = MATCH_TYPE_CONTEXT_PATH;
                    jk_log(l, JK_LOG_DEBUG,
                           "general context path rule %s*%s=%s was added\n",
                           uri, asterisk + 2, worker);
                }
                else if ('.' == asterisk[2]) {
                    /* suffix rule */
                    asterisk[1] = asterisk[2] = '\0';
                    uwr->worker_name = worker;
                    uwr->context = uri;
                    uwr->suffix = asterisk + 3;
                    uwr->match_type = MATCH_TYPE_SUFFIX;
                    jk_log(l, JK_LOG_DEBUG,
                           "suffix rule %s.%s=%s was added\n",
                           uri, asterisk + 3, worker);
                }
                else if ('\0' != asterisk[2]) {
                    /* general suffix rule */
                    asterisk[1] = '\0';
                    uwr->worker_name = worker;
                    uwr->context = uri;
                    uwr->suffix = asterisk + 2;
                    uwr->match_type = MATCH_TYPE_GENERAL_SUFFIX;
                    jk_log(l, JK_LOG_DEBUG,
                           "general suffix rule %s*%s=%s was added\n",
                           uri, asterisk + 2, worker);
                }
                else {
                    /* context based */
                    asterisk[1] = '\0';
                    uwr->worker_name = worker;
                    uwr->context = uri;
                    uwr->suffix = NULL;
                    uwr->match_type = MATCH_TYPE_CONTEXT;
                    jk_log(l, JK_LOG_DEBUG,
                           "match rule %s=%s was added\n", uri, worker);
                }
            }
            else {
                /* Something like : JkMount /servlets/exampl* ajp13 */
                uwr->uri = uri;
                uwr->worker_name = worker;
                uwr->context = uri;
                uwr->suffix = NULL;
                uwr->match_type = MATCH_TYPE_EXACT;
                jk_log(l, JK_LOG_DEBUG,
                       "exact rule %s=%s was added\n",
                       uri, worker);
            }

        }
        else {
            /* Something like:  JkMount /login/j_security_check ajp13 */
            uwr->uri = uri;
            uwr->worker_name = worker;
            uwr->context = uri;
            uwr->suffix = NULL;
            uwr->match_type = MATCH_TYPE_EXACT;
            jk_log(l, JK_LOG_DEBUG,
                   "exact rule %s=%s was added\n",
                   uri, worker);
        }
        uwr->ctxt_len = strlen(uwr->context);
    }
    else {
        /*
         * JFC: please check...
         * Not sure what to do, but I try to prevent problems.
         * I have fixed jk_mount_context() in apaches/mod_jk.c so we should
         * not arrive here when using Apache.
         */
        jk_log(l, JK_LOG_ERROR,
               "invalid context %s\n",
               uri);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    uw_map->maps[uw_map->size] = uwr;
    uw_map->size++;
    
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
        jk_open_pool(&uw_map->tp,
                     uw_map->tbuf, sizeof(jk_pool_atom_t) * SMALL_POOL_SIZE);

        uw_map->size = 0;
        uw_map->maps = NULL;

        sz = jk_map_size(init_data);

        jk_log(l, JK_LOG_DEBUG,
               "rule map size is %d\n",
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
                jk_log(l, JK_LOG_DEBUG,
                       "there are %d rules\n",
                       uw_map->size);
            }
            else {
                jk_log(l, JK_LOG_ERROR,
                       "Parsing error\n");
                rc = JK_FALSE;
            }
        }

        if (rc == JK_FALSE) {
            jk_log(l, JK_LOG_ERROR,
                   "there was an error, freing buf\n");
            jk_close_pool(&uw_map->p);
            jk_close_pool(&uw_map->tp);
        }
    }

    JK_TRACE_EXIT(l);
    return rc;
}

/* returns the index of the last occurrence of the 'ch' character
   if ch=='\0' returns the length of the string str  */
int last_index_of(const char *str, char ch)
{
    const char *str_minus_one = str - 1;
    const char *s = str + strlen(str);
    while (s != str_minus_one && ch != *s) {
        --s;
    }
    return (s - str);
}

int uri_worker_map_close(jk_uri_worker_map_t *uw_map, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (uw_map) {
        jk_close_pool(&uw_map->p);
        jk_close_pool(&uw_map->tp);
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

void jk_no2slash(char *name)
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


char *map_uri_to_worker(jk_uri_worker_map_t *uw_map,
                        char *uri, jk_logger_t *l)
{
    unsigned int i;
    unsigned int best_match = -1;
    unsigned int longest_match = 0;
    char *url_rewrite;

    JK_TRACE_ENTER(l);
    if (!uw_map || !uri) {
        JK_LOG_NULL_PARAMS(l);
        JK_TRACE_EXIT(l);
        return NULL;
    }
    if (*uri != '/') {
        jk_log(l, JK_LOG_ERROR,
                "uri must start with /\n");
        JK_TRACE_EXIT(l);
        return NULL;
    }
    url_rewrite = strstr(uri, JK_PATH_SESSION_IDENTIFIER);
    if (url_rewrite) {
        *url_rewrite = '\0'; 
    }
    jk_no2slash(uri);

    jk_log(l, JK_LOG_DEBUG, "Attempting to map URI '%s'\n", uri);
    for (i = 0; i < uw_map->size; i++) {
        uri_worker_record_t *uwr = uw_map->maps[i];

        if (uwr->ctxt_len < longest_match) {
            continue;       /* can not be a best match anyway */
        }
        jk_log(l, JK_LOG_DEBUG, "Attempting to map context URI '%s'\n", uwr->uri);

        if (uwr->match_type == MATCH_TYPE_WILDCHAR_PATH) {
            /* Map is already sorted by ctxt_len */
            if (wildchar_match(uri, uwr->context,
#ifdef WIN32
                               1
#else
                               0
#endif
                               ) == 0) {

                    jk_log(l, JK_LOG_DEBUG,
                            "Found an wildchar match %s -> %s\n",
                            uwr->worker_name, uwr->context);
                    JK_TRACE_EXIT(l);
                    return uwr->worker_name;
             }
        }
        else if (strncmp(uwr->context, uri, uwr->ctxt_len) == 0) {
            if (uwr->match_type == MATCH_TYPE_EXACT) {
                if (strlen(uri) == uwr->ctxt_len) {
                    jk_log(l, JK_LOG_DEBUG,
                            "Found an exact match %s -> %s\n",
                            uwr->worker_name, uwr->context);
                    JK_TRACE_EXIT(l);
                    return uwr->worker_name;
                }
            }
            else if (uwr->match_type == MATCH_TYPE_CONTEXT) {
                if (uwr->ctxt_len > longest_match) {
                    jk_log(l, JK_LOG_DEBUG,
                            "Found a context match %s -> %s\n",
                            uwr->worker_name, uwr->context);
                    longest_match = uwr->ctxt_len;
                    best_match = i;
                }
            }
            else if (uwr->match_type == MATCH_TYPE_GENERAL_SUFFIX) {
                int suffix_start = last_index_of(uri, uwr->suffix[0]);
                if (suffix_start >= 0
                    && 0 == strcmp(uri + suffix_start, uwr->suffix)) {
                    if (uwr->ctxt_len >= longest_match) {
                        jk_log(l, JK_LOG_DEBUG,
                                "Found a general suffix match %s -> *%s\n",
                                uwr->worker_name, uwr->suffix);
                        longest_match = uwr->ctxt_len;
                        best_match = i;
                    }
                }
            }
            else if (uwr->match_type == MATCH_TYPE_CONTEXT_PATH) {
                char *suffix_path = NULL;
                if (strlen(uri) > 1
                    && (suffix_path = strchr(uri + 1, '/')) != NULL) {
                    if (0 ==
                        strncmp(suffix_path, uwr->suffix,
                                strlen(uwr->suffix))) {
                        if (uwr->ctxt_len >= longest_match) {
                            jk_log(l, JK_LOG_DEBUG,
                                    "Found a general context path match %s -> *%s\n",
                                    uwr->worker_name, uwr->suffix);
                            longest_match = uwr->ctxt_len;
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
#ifdef WIN32
                    if (strcasecmp(suffix, uwr->suffix) == 0) {
#else
                    if (strcmp(suffix, uwr->suffix) == 0) {
#endif
                        if (uwr->ctxt_len >= longest_match) {
                            jk_log(l, JK_LOG_DEBUG,
                                    "Found a suffix match %s -> *.%s\n",
                                    uwr->worker_name, uwr->suffix);
                            longest_match = uwr->ctxt_len;
                            best_match = i;
                        }
                    }
                }
            }
        }
    }

    if (best_match != -1) {
        JK_TRACE_EXIT(l);
        return uw_map->maps[best_match]->worker_name;
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
                    "Found a security fraud in '%s'\n",
                    uri);
            JK_TRACE_EXIT(l);
            return uw_map->maps[fraud]->worker_name;
        }
    }
    JK_TRACE_EXIT(l);
    return NULL;
}
