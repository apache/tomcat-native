/*
 * Copyright (c) 1997-1999 The Java Apache Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * 4. The names "Apache JServ", "Apache JServ Servlet Engine" and 
 *    "Java Apache Project" must not be used to endorse or promote products 
 *    derived from this software without prior written permission.
 *
 * 5. Products derived from this software may not be called "Apache JServ"
 *    nor may "Apache" nor "Apache JServ" appear in their names without 
 *    prior written permission of the Java Apache Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *    
 * THIS SOFTWARE IS PROVIDED BY THE JAVA APACHE PROJECT "AS IS" AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE JAVA APACHE PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Java Apache Group. For more information
 * on the Java Apache Project and the Apache JServ Servlet Engine project,
 * please see <http://java.apache.org/>.
 *
 */

/***************************************************************************
 * Description: General purpose map object                                 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                               *
 ***************************************************************************/

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_map.h"
#include "jk_util.h"

#define CAPACITY_INC_SIZE (50)
#define LENGTH_OF_LINE    (1024)

struct jk_map {
    jk_pool_t p;
    jk_pool_atom_t buf[SMALL_POOL_SIZE];

    char **names;
    void **values;

    unsigned capacity;
    unsigned size;
};

static void trim_prp_comment(char *prp);
static int trim(char *s);
static int map_realloc(jk_map_t *m);
static char *update_env_variables(char *value, jk_map_t *m);

int map_alloc(jk_map_t **m)
{
    if(m) {
        return map_open(*m = (jk_map_t *)malloc(sizeof(jk_map_t)));
    }
    
    return JK_FALSE;
}

int map_free(jk_map_t **m)
{
    int rc = JK_FALSE;

    if(m && *m) {
        map_close(*m);  
        free(*m);
        *m = NULL;
    }
    
    return rc;
}

int map_open(jk_map_t *m)
{
    int rc = JK_FALSE;

    if(m) {
        jk_open_pool(&m->p, m->buf, sizeof(jk_pool_atom_t) * SMALL_POOL_SIZE);
        m->capacity = 0;
        m->size     = 0;
        m->names    = NULL;
        m->values   = NULL;
        rc = JK_TRUE;
    }
    
    return rc;
}

int map_close(jk_map_t *m)
{
    int rc = JK_FALSE;

    if(m) {
        jk_close_pool(&m->p);
        rc = JK_TRUE;
    }
    
    return rc;
}

void *map_get(jk_map_t *m,
              const char *name,
              const void *def)
{
    void *rc = (void *)def;
    
    if(m && name) {
        unsigned i;
        for(i = 0 ; i < m->size ; i++) {
            if(0 == strcmp(m->names[i], name)) {
                rc = m->values[i];
                break;
            }
        }
    }

    return rc;
}

int map_get_int(jk_map_t *m,
                const char *name,
                int def)
{
    char buf[100];
    char *rc;
    int  len;
    int  int_res;
    int  multit = 1;

    sprintf(buf, "%d", def);
    rc = map_get_string(m, name, buf);

    len = strlen(rc);
    if(len) {        
        char *lastchar = rc + len - 1;
        if('m' == *lastchar || 'M' == *lastchar) {
            *lastchar = '\0';
            multit = 1024 * 1024;
        } else if('k' == *lastchar || 'K' == *lastchar) {
            *lastchar = '\0';
            multit = 1024;
        }
    }

    int_res = atoi(rc);

    return int_res * multit;
}

double map_get_double(jk_map_t *m,
                      const char *name,
                      double def)
{
    char buf[100];
    char *rc;

    sprintf(buf, "%f", def);
    rc = map_get_string(m, name, buf);

    return atof(rc);
}

char *map_get_string(jk_map_t *m,
                    const char *name,
                    const char *def)
{
    return map_get(m, name, def);
}

char **map_get_string_list(jk_map_t *m,
                           const char *name,
                           unsigned *list_len,
                           const char *def)
{
    char *l = map_get_string(m, name, def);
    char **ar = NULL;

    *list_len = 0;

    if(l) {
        unsigned capacity = 0;
        unsigned idex = 0;    
        char *v = jk_pool_strdup(&m->p, l);
        
        if(!v) {
            return NULL;
        }

        /*
         * GS, in addition to VG's patch, we now need to 
         * strtok also by a "*"
         */

        for(l = strtok(v, " \t,*") ; 
            l ; 
            l = strtok(NULL, " \t,*")) {
        
            if(idex == capacity) {
                ar = jk_pool_realloc(&m->p, 
                                     sizeof(char *) * (capacity + 5),
                                     ar,
                                     sizeof(char *) * capacity);
                if(!ar) {
                    return JK_FALSE;
                }
                capacity += 5;
            }
            ar[idex] = jk_pool_strdup(&m->p, l);
            idex ++;
        }
        
        *list_len = idex;
    }

    return ar;
}

int map_put(jk_map_t *m,
            const char *name,
            void *value,
            void **old)
{
    int rc = JK_FALSE;

    if(m && name && old) {
        unsigned i;
        for(i = 0 ; i < m->size ; i++) {
            if(0 == strcmp(m->names[i], name)) {
                break;
            }
        }

        if(i < m->size) {
            *old = m->values[i];
            m->values[i] = value;
            rc = JK_TRUE;
        } else {
            map_realloc(m);

            if(m->size < m->capacity) {
                m->values[m->size] = value;
                m->names[m->size] = jk_pool_strdup(&m->p, name);
                m->size ++;
                rc = JK_TRUE;
            }
        }       
    }

    return rc;
}

int map_read_properties(jk_map_t *m,
                        const char *f)
{
    int rc = JK_FALSE;

    if(m && f) {
        FILE *fp = fopen(f, "r");
        
        if(fp) {
            char buf[LENGTH_OF_LINE + 1];            
            char *prp;
            
            rc = JK_TRUE;

            while(NULL != (prp = fgets(buf, LENGTH_OF_LINE, fp))) {
                trim_prp_comment(prp);
                if(trim(prp)) {
                    char *v = strchr(prp, '=');
                    if(v) {
                        *v = '\0';
                        v++;                        
                        if(strlen(v) && strlen(prp)) {
                            char *oldv = map_get_string(m, prp, NULL);
                            v = update_env_variables(v, m);
                            if(oldv) {
                                char *tmpv = jk_pool_alloc(&m->p, 
                                                           strlen(v) + strlen(oldv) + 3);
                                if(tmpv) {
                                    char sep = '*';
                                    if(jk_is_path_poperty(prp)) {
                                        sep = PATH_SEPERATOR;
                                    } else if(jk_is_cmd_line_poperty(prp)) {
                                        sep = ' ';
                                    }

                                    sprintf(tmpv, "%s%c%s", 
                                            oldv, sep, v);
                                }                                
                                v = tmpv;
                            } else {
                                v = jk_pool_strdup(&m->p, v);
                            }
                            if(v) {
                                void *old = NULL;
                                map_put(m, prp, v, &old);
                            } else {
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
    if(m) {
        return m->size;
    }

    return -1;
}

char *map_name_at(jk_map_t *m,
                  int idex)
{
    if(m && idex >= 0) {
        return m->names[idex];
    }

    return NULL;
}

void *map_value_at(jk_map_t *m,
                   int idex)
{
    if(m && idex >= 0) {
        return m->values[idex];
    }

    return NULL;
}

static void trim_prp_comment(char *prp)
{
    char *comment = strchr(prp, '#');
    if(comment) {
        *comment = '\0';
    }
}

static int trim(char *s)
{
    int i;

    for(i = strlen(s) - 1 ; (i >= 0) && isspace(s[i]) ;  i--)
        ;
    
    s[i + 1] = '\0';
    
    for(i = 0 ; ('\0' !=  s[i]) && isspace(s[i]) ; i++)
        ;
    
    if(i > 0) {
        strcpy(s, &s[i]);
    }

    return strlen(s);
}

static int map_realloc(jk_map_t *m)
{
    if(m->size == m->capacity) {
        char **names;
        void **values;
        int  capacity = m->capacity + CAPACITY_INC_SIZE;

        names = (char **)jk_pool_alloc(&m->p, sizeof(char *) * capacity);
        values = (void **)jk_pool_alloc(&m->p, sizeof(void *) * capacity);
        
        if(values && names) {
            memcpy(names, m->names, sizeof(char *) * m->capacity);
            memcpy(values, m->values, sizeof(void *) * m->capacity);

            m->names = names;
            m->values = values;
            m->capacity = capacity;

            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

static char *update_env_variables(char *value, jk_map_t *m)
{
    char *rc = value;
    char *env_start = value;

    while(env_start = strstr(env_start, "$(")) {
        char *env_end = strstr(env_start, ")");
        if(env_end) {
            char env_name[LENGTH_OF_LINE + 1] = ""; 
            char *env_value;

            *env_end = '\0';
            strcpy(env_name, env_start + 2);
            *env_end = ')';

            env_value = map_get_string(m, env_name, NULL);
            if(env_value) {
                char *new_value = jk_pool_alloc(&m->p, 
                                                (sizeof(char) * (strlen(rc) + strlen(env_value))));
                if(!new_value) {
                    break;
                }
                *env_start = '\0';
                strcpy(new_value, rc);
                strcat(new_value, env_value);
                strcat(new_value, env_end + 1);
                rc = new_value;
                env_start = rc;
            } else {
                env_start = env_end;
            }
        } else {
            break;
        }
    }

    return rc;
}

