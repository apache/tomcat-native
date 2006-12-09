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
 * Description: General purpose map object                                 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Mladen Turk <mturk@apache.org>                             *
 * Version:     $Revision$                                          *
 ***************************************************************************/
#ifdef AS400
#include "apr_xlate.h"
#endif

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_util.h"
#include "jk_map.h"

#define CAPACITY_INC_SIZE   (50)
#define LENGTH_OF_LINE      (8192)
#define JK_MAP_RECURSION    (20)
#define JK_MAP_REFERENCE    (".reference")
#define JK_MAP_REFERENCE_SZ (strlen(JK_MAP_REFERENCE))

/* Compute the "checksum" for a key, consisting of the first
 * 4 bytes, packed into an int.
 * This checksum allows us to do a single integer
 * comparison as a fast check to determine whether we can
 * skip a strcmp
 */
#define COMPUTE_KEY_CHECKSUM(key, checksum)    \
{                                              \
    const char *k = (key);                     \
    unsigned int c = (unsigned int)*k;         \
    (checksum) = c;                            \
    (checksum) <<= 8;                          \
    if (c) {                                   \
        c = (unsigned int)*++k;                \
        checksum |= c;                         \
    }                                          \
    (checksum) <<= 8;                          \
    if (c) {                                   \
        c = (unsigned int)*++k;                \
        checksum |= c;                         \
    }                                          \
    (checksum) <<= 8;                          \
    if (c) {                                   \
        c = (unsigned int)*++k;                \
        checksum |= c;                         \
    }                                          \
}

struct jk_map
{
    jk_pool_t p;
    jk_pool_atom_t buf[SMALL_POOL_SIZE];

    const char **names;
    const void **values;
    unsigned int *keys;

    unsigned int capacity;
    unsigned int size;
};

static void trim_prp_comment(char *prp);
static size_t trim(char *s);
static int map_realloc(jk_map_t *m);

int jk_map_alloc(jk_map_t **m)
{
    if (m) {
        return jk_map_open(*m = (jk_map_t *)malloc(sizeof(jk_map_t)));
    }

    return JK_FALSE;
}

int jk_map_free(jk_map_t **m)
{
    int rc = JK_FALSE;

    if (m && *m) {
        jk_map_close(*m);
        free(*m);
        *m = NULL;
    }

    return rc;
}

int jk_map_open(jk_map_t *m)
{
    int rc = JK_FALSE;

    if (m) {
        jk_open_pool(&m->p, m->buf, sizeof(jk_pool_atom_t) * SMALL_POOL_SIZE);
        m->capacity = 0;
        m->size = 0;
        m->keys  = NULL;
        m->names = NULL;
        m->values = NULL;
        rc = JK_TRUE;
    }

    return rc;
}

int jk_map_close(jk_map_t *m)
{
    int rc = JK_FALSE;

    if (m) {
        jk_close_pool(&m->p);
        rc = JK_TRUE;
    }

    return rc;
}

void *jk_map_get(jk_map_t *m, const char *name, const void *def)
{
    const void *rc = (void *)def;

    if (m && name) {
        unsigned int i;
        unsigned int key;
        COMPUTE_KEY_CHECKSUM(name, key)
        for (i = 0; i < m->size; i++) {
            if (m->keys[i] == key && strcmp(m->names[i], name) == 0) {
                rc = m->values[i];
                break;
            }
        }
    }

    return (void *)rc;          /* DIRTY */
}

int jk_map_get_id(jk_map_t *m, const char *name)
{
    int rc = -1;
    if (m && name) {
        unsigned int i;
        unsigned int key;
        COMPUTE_KEY_CHECKSUM(name, key)
        for (i = 0; i < m->size; i++) {
            if (m->keys[i] == key && strcmp(m->names[i], name) == 0) {
                rc = i;
                break;
            }
        }
    }

    return rc;
}

const char *jk_map_get_string(jk_map_t *m, const char *name, const char *def)
{
    const char *rc = def;

    if (m && name) {
        unsigned int i;
        unsigned int key;
        COMPUTE_KEY_CHECKSUM(name, key)
        for (i = 0; i < m->size; i++) {
            if (m->keys[i] == key && strcmp(m->names[i], name) == 0) {
                rc = m->values[i];
                break;
            }
        }
    }

    return rc;
}


int jk_map_get_int(jk_map_t *m, const char *name, int def)
{
    char buf[100];
    const char *rc;
    size_t len;
    int int_res;
    int multit = 1;

    sprintf(buf, "%d", def);
    rc = jk_map_get_string(m, name, buf);

    len = strlen(rc);
    if (len) {
        char *lastchar = &buf[0] + len - 1;
        strcpy(buf, rc);
        if ('m' == *lastchar || 'M' == *lastchar) {
            *lastchar = '\0';
            multit = 1024 * 1024;
        }
        else if ('k' == *lastchar || 'K' == *lastchar) {
            *lastchar = '\0';
            multit = 1024;
        }
        int_res = atoi(buf);
    }
    else
        int_res = def;

    return int_res * multit;
}

double jk_map_get_double(jk_map_t *m, const char *name, double def)
{
    char buf[100];
    const char *rc;

    sprintf(buf, "%f", def);
    rc = jk_map_get_string(m, name, buf);

    return atof(rc);
}

int jk_map_get_bool(jk_map_t *m, const char *name, int def)
{
    char buf[100];
    const char *rc;

    sprintf(buf, "%d", def);
    rc = jk_map_get_string(m, name, buf);

    return jk_get_bool_code(rc, def);
}

char **jk_map_get_string_list(jk_map_t *m,
                              const char *name,
                              unsigned *list_len, const char *def)
{
    const char *l = jk_map_get_string(m, name, def);
    char **ar = NULL;

#if defined(AS400) || defined(_REENTRANT)
    char *lasts;
#endif

    *list_len = 0;

    if (l) {
        unsigned capacity = 0;
        unsigned idex = 0;
        char *p;
        char *v = jk_pool_strdup(&m->p, l);

        if (!v) {
            return NULL;
        }

        /*
         * GS, in addition to VG's patch, we now need to
         * strtok also by a "*"
         */
#if defined(AS400) || defined(_REENTRANT)
        for (p = strtok_r(v, " \t,", &lasts);
             p; p = strtok_r(NULL, " \t,", &lasts))
#else
        for (p = strtok(v, " \t,"); p; p = strtok(NULL, " \t,"))
#endif

        {

            if (idex == capacity) {
                ar = jk_pool_realloc(&m->p,
                                     sizeof(char *) * (capacity + 5),
                                     ar, sizeof(char *) * capacity);
                if (!ar) {
                    return JK_FALSE;
                }
                capacity += 5;
            }
            ar[idex] = jk_pool_strdup(&m->p, p);
            idex++;
        }

        *list_len = idex;
    }

    return ar;
}

int jk_map_add(jk_map_t *m, const char *name, const void *value)
{
    int rc = JK_FALSE;

    if (m && name) {
        unsigned int key;
        COMPUTE_KEY_CHECKSUM(name, key)
        map_realloc(m);

        if (m->size < m->capacity) {
            m->values[m->size] = value;
            m->names[m->size] = jk_pool_strdup(&m->p, name);
            m->keys[m->size] = key;
            m->size++;
            rc = JK_TRUE;
        }
    }

    return rc;
}

int jk_map_put(jk_map_t *m, const char *name, const void *value, void **old)
{
    int rc = JK_FALSE;

    if (m && name) {
        unsigned int i;
        unsigned int key;
        COMPUTE_KEY_CHECKSUM(name, key)
        for (i = 0; i < m->size; i++) {
            if (m->keys[i] == key && strcmp(m->names[i], name) == 0) {
                break;
            }
        }

        if (i < m->size) {
            if (old)
                *old = (void *)m->values[i];        /* DIRTY */
            m->values[i] = value;
            rc = JK_TRUE;
        }
        else {
            rc = jk_map_add(m, name, value);
        }
    }

    return rc;
}

int jk_map_read_property(jk_map_t *m, const char *str, jk_logger_t *l)
{
    int rc = JK_TRUE;
    char buf[LENGTH_OF_LINE + 1];
    char *prp = &buf[0];

    if (strlen(str) > LENGTH_OF_LINE)
        return JK_FALSE;

    strcpy(prp, str);
    if (trim(prp)) {
        char *v = strchr(prp, '=');
        if (v) {
            *v = '\0';
            v++;
            trim(prp);
            trim(v);
            if (strlen(v) && strlen(prp)) {
                const char *oldv = jk_map_get_string(m, prp, NULL);
                v = jk_map_replace_properties(m, v);
                if (oldv && jk_is_unique_property(prp) == JK_FALSE) {
                    char *tmpv = jk_pool_alloc(&m->p,
                                       strlen(v) + strlen(oldv) + 3);
                    if (tmpv) {
                        char sep = '*';
                        if (jk_is_path_property(prp))
                            sep = PATH_SEPERATOR;
                        else if (jk_is_cmd_line_property(prp))
                            sep = ' ';
                        else if (jk_is_list_property(prp))
                            sep = ',';
                        sprintf(tmpv, "%s%c%s", oldv, sep, v);
                    }
                    v = tmpv;
                }
                else {
                    if (jk_is_deprecated_property(prp)) {
                        jk_log(l, JK_LOG_WARNING,
                               "The attribute %s is deprecated - please check"
                               " the documentation for the correct replacement.",
                               prp);
                    }
                    v = jk_pool_strdup(&m->p, v);
                }
                if (v) {
                    jk_map_put(m, prp, v, NULL);
                }
                else {
                    rc = JK_FALSE;
                }
            }
        }
    }
    return rc;
}


int jk_map_read_properties(jk_map_t *m, const char *f, time_t *modified, jk_logger_t *l)
{
    int rc = JK_FALSE;

    if (m && f) {
        struct stat statbuf;
        FILE *fp;
        if ((rc = stat(f, &statbuf)) == -1)
            return JK_FALSE;
#ifdef AS400
        fp = fopen(f, "r, o_ccsid=0");
#else
        fp = fopen(f, "r");
#endif

        if (fp) {
            char buf[LENGTH_OF_LINE + 1];
            char *prp;

            rc = JK_TRUE;

            while (NULL != (prp = fgets(buf, LENGTH_OF_LINE, fp))) {
                trim_prp_comment(prp);
                if (*prp) {
                    if ((rc = jk_map_read_property(m, prp, l)) == JK_FALSE)
                        break;
                }
            }
            fclose(fp);
            if (modified)
                *modified = statbuf.st_mtime;
        }
    }

    return rc;
}


int jk_map_size(jk_map_t *m)
{
    if (m) {
        return m->size;
    }

    return -1;
}

const char *jk_map_name_at(jk_map_t *m, int idex)
{
    if (m && idex >= 0) {
        return m->names[idex];  /* DIRTY */
    }

    return NULL;
}

void *jk_map_value_at(jk_map_t *m, int idex)
{
    if (m && idex >= 0) {
        return (void *)m->values[idex]; /* DIRTY */
    }

    return NULL;
}

static void trim_prp_comment(char *prp)
{
#ifdef AS400
    char *comment;
    /* lots of lines that translate a '#' realtime deleted   */
    comment = strchr(prp, *APR_NUMBERSIGN);
#else
    char *comment = strchr(prp, '#');
#endif
    if (comment) {
        *comment = '\0';
    }
}

static size_t trim(char *s)
{
    size_t i;

    /* check for empty strings */
    if (!(i = strlen(s)))
        return 0;
    for (i = i - 1; (i >= 0) &&
         isspace((int)((unsigned char)s[i])); i--);

    s[i + 1] = '\0';

    for (i = 0; ('\0' != s[i]) &&
         isspace((int)((unsigned char)s[i])); i++);

    if (i > 0) {
        strcpy(s, &s[i]);
    }

    return strlen(s);
}

static int map_realloc(jk_map_t *m)
{
    if (m->size == m->capacity) {
        char **names;
        void **values;
        unsigned int *keys;
        int capacity = m->capacity + CAPACITY_INC_SIZE;

        names = (char **)jk_pool_alloc(&m->p, sizeof(char *) * capacity);
        values = (void **)jk_pool_alloc(&m->p, sizeof(void *) * capacity);
        keys = (unsigned int *)jk_pool_alloc(&m->p, sizeof(unsigned int) * capacity);

        if (values && names) {
            if (m->capacity && m->names)
                memcpy(names, m->names, sizeof(char *) * m->capacity);

            if (m->capacity && m->values)
                memcpy(values, m->values, sizeof(void *) * m->capacity);

            if (m->capacity && m->keys)
                memcpy(keys, m->keys, sizeof(unsigned int) * m->capacity);

            m->names = (const char **)names;
            m->values = (const void **)values;
            m->keys = keys;
            m->capacity = capacity;

            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

/**
 *  Replace $(property) in value.
 *
 */
char *jk_map_replace_properties(jk_map_t *m, const char *value)
{
    char *rc = (char *)value;
    char *env_start = rc;
    int rec = 0;

    while ((env_start = strstr(env_start, "$(")) != NULL) {
        char *env_end = strstr(env_start, ")");
        if (rec++ > 20)
            return rc;
        if (env_end) {
            char env_name[LENGTH_OF_LINE + 1] = "";
            const char *env_value;
#if defined(WIN32)
            char env_buf[LENGTH_OF_LINE + 1];
#endif
            *env_end = '\0';
            strcpy(env_name, env_start + 2);
            *env_end = ')';

            env_value = jk_map_get_string(m, env_name, NULL);
            if (!env_value) {
                env_value = getenv(env_name);
            }
#if defined(WIN32)
            if (!env_value) {
                /* Try the env block from calling process */
                if (GetEnvironmentVariable(env_name, env_buf,
                                           sizeof(env_buf)))
                    env_value = &env_buf[0];
            }
#endif
            if (env_value) {
                size_t offset = 0;
                char *new_value = jk_pool_alloc(&m->p,
                                                (sizeof(char) *
                                                (strlen(rc) +
                                                strlen(env_value))));
                if (!new_value) {
                    break;
                }
                *env_start = '\0';
                strcpy(new_value, rc);
                strcat(new_value, env_value);
                strcat(new_value, env_end + 1);
                offset = env_start - rc + strlen(env_value);
                rc = new_value;
                /* Avoid recursive subst */
                env_start = rc + offset;
            }
            else {
                env_start = env_end;
            }
        }
        else {
            break;
        }
    }

    return rc;
}

/**
 *  Resolve references
 *
 */
int jk_map_resolve_references(jk_map_t *m, const char *prefix,
                              int wildcard, int depth, jk_logger_t *l)
{
    int rc = JK_FALSE;

    JK_TRACE_ENTER(l);

    if (m && prefix && depth <= JK_MAP_RECURSION) {
        size_t prelen = strlen(prefix);
        unsigned int i;
        rc = JK_TRUE;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Checking for references with prefix %s with%s wildcard (recursion %d)",
                   prefix, wildcard? "" : "out", depth);
        for (i = 0; i < m->size; i++) {
            if (m->values[i] && !strncmp(m->names[i], prefix, prelen)) {
                size_t remain = strlen(m->names[i]) - prelen;
                if ((remain == JK_MAP_REFERENCE_SZ ) || (wildcard && remain > JK_MAP_REFERENCE_SZ)) {
                    remain = strlen(m->names[i]) - JK_MAP_REFERENCE_SZ;
                    if (!strncmp(m->names[i] + remain, JK_MAP_REFERENCE, JK_MAP_REFERENCE_SZ)) {
                        char *from = jk_pool_alloc(&m->p,
                                                   (sizeof(char) *
                                                   (strlen(m->values[i]) + 2)));
                        char *to = jk_pool_alloc(&m->p,
                                                 (sizeof(char) *
                                                 (remain + 2)));
                        if (!from || !to) {
                            rc = JK_FALSE;
                            break;
                        }
                        strcpy(from, m->values[i]);
                        *(from+strlen(m->values[i]))   = '.';
                        *(from+strlen(m->values[i])+1) = '\0';
                        strncpy(to, m->names[i], remain);
                        *(to+remain)   = '.';
                        *(to+remain+1) = '\0';

                        rc = jk_map_resolve_references(m, m->values[i], 0, depth+1, l);
                        if (rc == JK_FALSE) {
                            break;
                        }
                        if (JK_IS_DEBUG_LEVEL(l))
                            jk_log(l, JK_LOG_DEBUG,
                                   "Copying values from %s to %s",
                                   from, to);
                        rc = jk_map_inherit_properties(m, from, to);
                        if (rc == JK_FALSE) {
                            break;
                        }
                        m->values[i] = NULL;
                    }
                }
            }
        }
    }
    JK_TRACE_EXIT(l);
    return rc;
}

/**
 *  Inherit properties
 *
 */
int jk_map_inherit_properties(jk_map_t *m, const char *from, const char *to)
{
    int rc = JK_FALSE;

    if (m && from && to) {
        unsigned int i;
        rc = JK_TRUE;
        for (i = 0; i < m->size; i++) {
            if (!strncmp(m->names[i], from, strlen(from))) {
                const char *prp = m->names[i] + strlen(from);
                char *to_prp = jk_pool_alloc(&m->p,
                                             (sizeof(char) *
                                             (strlen(to) +
                                             strlen(prp) + 1)));
                if (!to_prp) {
                    break;
                }
                strcpy(to_prp, to);
                strcat(to_prp, prp);
                if (jk_map_get_id(m, to_prp) < 0 ) {
                    rc = jk_map_add(m, to_prp, m->values[i]);
                    if (rc == JK_FALSE) {
                        break;
                    }
                }
            }
        }
    }
    return rc;
}
