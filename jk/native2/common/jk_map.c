/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
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

/***************************************************************************
 * Description: General purpose map object                                 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_map.h"

#define CAPACITY_INC_SIZE (50)
#define LENGTH_OF_LINE    (1024)

struct jk_map {
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
        m->p.close(&m->p);
        rc = JK_TRUE;
    }
    
    return rc;
}

void *map_get(jk_map_t *m,
              const char *name,
              const void *def)
{
    const void *rc = (void *)def;
    
    if(m && name) {
        unsigned i;
        for(i = 0 ; i < m->size ; i++) {
            if(0 == strcmp(m->names[i], name)) {
                rc = m->values[i];
                break;
            }
        }
    }

    return (void *)rc; /* DIRTY */
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
        char *v = m->p.pstrdup(&m->p, l);
        
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
                ar = m->p.realloc(&m->p, 
                                     sizeof(char *) * (capacity + 5),
                                     ar,
                                     sizeof(char *) * capacity);
                if(!ar) {
                    return JK_FALSE;
                }
                capacity += 5;
            }
            ar[idex] = m->p.pstrdup(&m->p, l);
            idex ++;
        }
        
        *list_len = idex;
    }

    return ar;
}

int map_put(jk_map_t *m,
            const char *name,
            const void *value,
            void **old)
{
    int rc = JK_FALSE;

    if(m && name ) {
        unsigned i;
        for(i = 0 ; i < m->size ; i++) {
            if(0 == strcmp(m->names[i], name)) {
                break;
            }
        }

        if(i < m->size) {
            if( old!=NULL )
                *old = (void *) m->values[i]; /* DIRTY */
            m->values[i] = value;
            rc = JK_TRUE;
        } else {
            map_realloc(m);

            if(m->size < m->capacity) {
                m->values[m->size] = value;
                m->names[m->size] = m->p.pstrdup(&m->p, name);
                m->size ++;
                rc = JK_TRUE;
            }
        }       
    }

    return rc;
}


/* XXX Very strange hack to deal with special properties
 */
int jk_is_some_property(const char *prp_name, const char *suffix)
{
    if (prp_name && suffix) {
		size_t prp_name_len = strlen(prp_name);
		size_t suffix_len = strlen(suffix);
		if (prp_name_len >= suffix_len) {
			const char *prp_suffix = prp_name + prp_name_len - suffix_len;
			if(0 == strcmp(suffix, prp_suffix)) {
		        return JK_TRUE;
			}
        }
    }

    return JK_FALSE;
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
                            v = map_replace_properties(v, m);

                            if(oldv) {
                                char *tmpv = m->p.alloc(&m->p, 
                                                        strlen(v) + strlen(oldv) + 3);
                                if(tmpv) {
                                    char sep = '*';
                                    if(jk_is_some_property(prp, "path")) {
                                        sep = PATH_SEPERATOR;
                                    } else if(jk_is_some_property(prp, "cmd_line")) {
                                        sep = ' ';
                                    }

                                    sprintf(tmpv, "%s%c%s", 
                                            oldv, sep, v);
                                }                                
                                v = tmpv;
                            } else {
                                v = m->p.pstrdup(&m->p, v);
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
        return (char *)m->names[idex]; /* DIRTY */
    }

    return NULL;
}

void *map_value_at(jk_map_t *m,
                   int idex)
{
    if(m && idex >= 0) {
        return (void *) m->values[idex]; /* DIRTY */
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

        names = (char **)m->p.alloc(&m->p, sizeof(char *) * capacity);
        values = (void **)m->p.alloc(&m->p, sizeof(void *) * capacity);
        
        if(values && names) {
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

    while(env_start = strstr(env_start, "$(")) {
        char *env_end = strstr(env_start, ")");
        if( rec++ > 20 ) return rc;
        if(env_end) {
            char env_name[LENGTH_OF_LINE + 1] = ""; 
            char *env_value;

            *env_end = '\0';
            strcpy(env_name, env_start + 2);
            *env_end = ')';

            env_value = map_get_string(m, env_name, NULL);
	    if(!env_value) {
	      env_value=getenv( env_name );
	    }
            if(env_value) {
                int offset=0;
                char *new_value = m->p.alloc(&m->p, 
                                                (sizeof(char) * (strlen(rc) + strlen(env_value))));
                if(!new_value) {
                    break;
                }
                *env_start = '\0';
                strcpy(new_value, rc);
                strcat(new_value, env_value);
                strcat(new_value, env_end + 1);
		offset= env_start - rc + strlen( env_value );
                rc = new_value;
		/* Avoid recursive subst */
                env_start = rc + offset; 
            } else {
                env_start = env_end;
            }
        } else {
            break;
        }
    }

    return rc;
}


/** Get a string property, using the worker's style
    for properties.
    Example worker.ajp13.host=localhost.
*/
char *map_getStrProp(jk_map_t *m,
                     const char *objType,
                     const char *objName,
                     const char *pname,
                     char *def)
{
    char buf[1024];

    if( m==NULL ||
        objType==NULL ||
        objName==NULL ||
        pname==NULL ) {
        return def;
    }

    sprintf(buf, "%s.%s.%s", objType, objName, pname);
    return map_get_string(m, buf, NULL);
}

int map_getIntProp(jk_map_t *m,
                   const char *objType,
                   const char *objName,
                   const char *pname,
                   const int def)
{
    char buf[1024];

    if( m==NULL ||
        objType==NULL ||
        objName==NULL ||
        pname==NULL ) {
        return def;
    }

    sprintf(buf, "%s.%s.%s", objType, objName, pname);
    return map_get_int(m, buf, def);
}

double map_getDoubleProp(jk_map_t *m,
                         const char *objType,
                         const char *objName,
                         const char *pname,
                         const double def)
{
    char buf[1024];

    if( m==NULL ||
        objType==NULL ||
        objName==NULL ||
        pname==NULL ) {
        return def;
    }

    sprintf(buf, "%s.%s.%s", objType, objName, pname);
    return map_get_double(m, buf, def);
}

char **map_getListProp(jk_map_t *m,
                    const char *objType,
                    const char *objName,
                    const char *pname, 
                    unsigned *size)
{
    char buf[1024];

    if( m==NULL ||
        objType==NULL ||
        objName==NULL ||
        pname==NULL ) {
        return NULL;
    }

    sprintf(buf, "%s.%s.%s", objType, objName, pname);

    return map_get_string_list(m, buf, size, NULL);
}

