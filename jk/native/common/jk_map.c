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
 * Description: General purpose map object                                 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                          *
 ***************************************************************************/
#ifdef AS400
#include "apr_xlate.h"
#endif

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_map.h"
#include "jk_util.h"

#define CAPACITY_INC_SIZE (50)
#define LENGTH_OF_LINE    (1024)

struct jk_map
{
    jk_pool_t p;
    jk_pool_atom_t buf[SMALL_POOL_SIZE];

    const char **names;
    const void **values;

    unsigned capacity;
    unsigned size;
};

static void trim_prp_comment(char *prp);
static int trim(char *s);
static int map_realloc(jk_map_t *m);

int map_alloc(jk_map_t **m)
{
    if (m) {
        return map_open(*m = (jk_map_t *)malloc(sizeof(jk_map_t)));
    }

    return JK_FALSE;
}

int map_free(jk_map_t **m)
{
    int rc = JK_FALSE;

    if (m && *m) {
        map_close(*m);
        free(*m);
        *m = NULL;
    }

    return rc;
}

int map_open(jk_map_t *m)
{
    int rc = JK_FALSE;

    if (m) {
        jk_open_pool(&m->p, m->buf, sizeof(jk_pool_atom_t) * SMALL_POOL_SIZE);
        m->capacity = 0;
        m->size = 0;
        m->names = NULL;
        m->values = NULL;
        rc = JK_TRUE;
    }

    return rc;
}

int map_close(jk_map_t *m)
{
    int rc = JK_FALSE;

    if (m) {
        jk_close_pool(&m->p);
        rc = JK_TRUE;
    }

    return rc;
}

void *map_get(jk_map_t *m, const char *name, const void *def)
{
    const void *rc = (void *)def;

    if (m && name) {
        unsigned i;
        for (i = 0; i < m->size; i++) {
            if (0 == strcmp(m->names[i], name)) {
                rc = m->values[i];
                break;
            }
        }
    }

    return (void *)rc;          /* DIRTY */
}

int map_get_int(jk_map_t *m, const char *name, int def)
{
    char buf[100];
    char *rc;
    int len;
    int int_res;
    int multit = 1;

    sprintf(buf, "%d", def);
    rc = map_get_string(m, name, buf);

    len = strlen(rc);
    if (len) {
        char *lastchar = rc + len - 1;
        if ('m' == *lastchar || 'M' == *lastchar) {
            *lastchar = '\0';
            multit = 1024 * 1024;
        }
        else if ('k' == *lastchar || 'K' == *lastchar) {
            *lastchar = '\0';
            multit = 1024;
        }
    }

    int_res = atoi(rc);

    return int_res * multit;
}

double map_get_double(jk_map_t *m, const char *name, double def)
{
    char buf[100];
    char *rc;

    sprintf(buf, "%f", def);
    rc = map_get_string(m, name, buf);

    return atof(rc);
}

char *map_get_string(jk_map_t *m, const char *name, const char *def)
{
    return map_get(m, name, def);
}

char **map_get_string_list(jk_map_t *m,
                           const char *name,
                           unsigned *list_len, const char *def)
{
    char *l = map_get_string(m, name, def);
    char **ar = NULL;
#if defined(AS400) || defined(_REENTRANT)
    char *lasts;
#endif

    *list_len = 0;

    if (l) {
        unsigned capacity = 0;
        unsigned idex = 0;
        char *v = jk_pool_strdup(&m->p, l);

        if (!v) {
            return NULL;
        }

        /*
         * GS, in addition to VG's patch, we now need to 
         * strtok also by a "*"
         */
#if defined(AS400) || defined(_REENTRANT)
        for (l = strtok_r(v, " \t,*", &lasts);
             l; l = strtok_r(NULL, " \t,*", &lasts))
#else
        for (l = strtok(v, " \t,*"); l; l = strtok(NULL, " \t,*"))
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
            ar[idex] = jk_pool_strdup(&m->p, l);
            idex++;
        }

        *list_len = idex;
    }

    return ar;
}

int map_put(jk_map_t *m, const char *name, const void *value, void **old)
{
    int rc = JK_FALSE;

    if (m && name && old) {
        unsigned i;
        for (i = 0; i < m->size; i++) {
            if (0 == strcmp(m->names[i], name)) {
                break;
            }
        }

        if (i < m->size) {
            *old = (void *)m->values[i];        /* DIRTY */
            m->values[i] = value;
            rc = JK_TRUE;
        }
        else {
            map_realloc(m);

            if (m->size < m->capacity) {
                m->values[m->size] = value;
                m->names[m->size] = jk_pool_strdup(&m->p, name);
                m->size++;
                rc = JK_TRUE;
            }
        }
    }

    return rc;
}

int map_read_properties(jk_map_t *m, const char *f)
{
    int rc = JK_FALSE;

    if (m && f) {
#ifdef AS400
        FILE *fp = fopen(f, "r, o_ccsid=0");
#else
        FILE *fp = fopen(f, "r");
#endif

        if (fp) {
            char buf[LENGTH_OF_LINE + 1];
            char *prp;

            rc = JK_TRUE;

            while (NULL != (prp = fgets(buf, LENGTH_OF_LINE, fp))) {
                trim_prp_comment(prp);
                if (trim(prp)) {
                    char *v = strchr(prp, '=');
                    if (v) {
                        *v = '\0';
                        v++;
                        if (strlen(v) && strlen(prp)) {
                            char *oldv = map_get_string(m, prp, NULL);
                            v = map_replace_properties(v, m);
                            if (oldv) {
                                char *tmpv = jk_pool_alloc(&m->p,
                                                           strlen(v) +
                                                           strlen(oldv) + 3);
                                if (tmpv) {
                                    char sep = '*';
                                    if (jk_is_path_poperty(prp)) {
                                        sep = PATH_SEPERATOR;
                                    }
                                    else if (jk_is_cmd_line_poperty(prp)) {
                                        sep = ' ';
                                    }

                                    sprintf(tmpv, "%s%c%s", oldv, sep, v);
                                }
                                v = tmpv;
                            }
                            else {
                                v = jk_pool_strdup(&m->p, v);
                            }
                            if (v) {
                                void *old = NULL;
                                map_put(m, prp, v, &old);
                            }
                            else {
                                rc = JK_FALSE;
                                break;
                            }
                        }
                    }
                }
            }

            fclose(fp);
        }
    }

    return rc;
}


int map_size(jk_map_t *m)
{
    if (m) {
        return m->size;
    }

    return -1;
}

char *map_name_at(jk_map_t *m, int idex)
{
    if (m && idex >= 0) {
        return (char *)m->names[idex];  /* DIRTY */
    }

    return NULL;
}

void *map_value_at(jk_map_t *m, int idex)
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

static int trim(char *s)
{
    int i;

    for (i = strlen(s) - 1; (i >= 0) && isspace(s[i]); i--);

    s[i + 1] = '\0';

    for (i = 0; ('\0' != s[i]) && isspace(s[i]); i++);

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
        int capacity = m->capacity + CAPACITY_INC_SIZE;

        names = (char **)jk_pool_alloc(&m->p, sizeof(char *) * capacity);
        values = (void **)jk_pool_alloc(&m->p, sizeof(void *) * capacity);

        if (values && names) {
            if (m->capacity && m->names)
                memcpy(names, m->names, sizeof(char *) * m->capacity);

            if (m->capacity && m->values)
                memcpy(values, m->values, sizeof(void *) * m->capacity);

            m->names = (const char **)names;
            m->values = (const void **)values;
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
char *map_replace_properties(const char *value, jk_map_t *m)
{
    char *rc = (char *)value;
    char *env_start = rc;
    int rec = 0;

    while (env_start = strstr(env_start, "$(")) {
        char *env_end = strstr(env_start, ")");
        if (rec++ > 20)
            return rc;
        if (env_end) {
            char env_name[LENGTH_OF_LINE + 1] = "";
            char *env_value;

            *env_end = '\0';
            strcpy(env_name, env_start + 2);
            *env_end = ')';

            env_value = map_get_string(m, env_name, NULL);
            if (!env_value) {
                env_value = getenv(env_name);
            }
            if (env_value) {
                int offset = 0;
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
