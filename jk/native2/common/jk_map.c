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

typedef struct jk_map_private {
    char **names;
    void **values;

    int capacity;
    int size;
} jk_map_private_t;

static int jk_map_default_realloc(jk_map_t *m);
static void trim_prp_comment(char *prp);
static int trim(char *s);

static void *jk_map_default_get(jk_env_t *env, jk_map_t *m,
                                const char *name)
{
    int i;
    jk_map_private_t *mPriv;
    
    if(name==NULL )
        return NULL;
    mPriv=(jk_map_private_t *)m->_private;

    for(i = 0 ; i < mPriv->size ; i++) {
        if(0 == strcmp(mPriv->names[i], name)) {
            return  mPriv->values[i];
        }
    }
    return NULL;
}


static int jk_map_default_put(jk_env_t *env, jk_map_t *m,
                              const char *name, void *value,
                              void **old)
{
    int rc = JK_FALSE;
    int i;
    jk_map_private_t *mPriv;

    if( name==NULL ) 
        return JK_FALSE;

    mPriv=(jk_map_private_t *)m->_private;
    
    for(i = 0 ; i < mPriv->size ; i++) {
        if(0 == strcmp(mPriv->names[i], name)) {
            break;
        }
    }

    /* Old value found */
    if(i < mPriv->size) {
        if( old!=NULL )
            *old = (void *) mPriv->values[i]; /* DIRTY */
        mPriv->values[i] = value;
        return JK_TRUE;
    }
    
    jk_map_default_realloc(m);
    
    if(mPriv->size < mPriv->capacity) {
        mPriv->values[mPriv->size] = value;
        /* XXX this is wrong - either we take ownership and copy both
           name and value,
           or none. The caller should do that if he needs !
        */
        /*     mPriv->names[mPriv->size] = m->pool->pstrdup(m->pool, name); */
        mPriv->names[mPriv->size] =  name; 
        mPriv->size ++;
        rc = JK_TRUE;
    }
    return rc;
}

static int jk_map_default_size(jk_env_t *env, jk_map_t *m)
{
    jk_map_private_t *mPriv;

    /* assert(m!=NULL) -- we call it via m->... */
    mPriv=(jk_map_private_t *)m->_private;
    return mPriv->size;
}

static char *jk_map_default_nameAt(jk_env_t *env, jk_map_t *m,
                                   int idex)
{
    jk_map_private_t *mPriv;

    mPriv=(jk_map_private_t *)m->_private;

    if(idex < 0 || idex > mPriv->size ) 
        return NULL;
    
    return (char *)mPriv->names[idex]; 
}

static void *jk_map_default_valueAt(jk_env_t *env, jk_map_t *m,
                                    int idex)
{
    jk_map_private_t *mPriv;

    mPriv=(jk_map_private_t *)m->_private;

    if(idex < 0 || idex > mPriv->size )
        return NULL;
    
    return (void *) mPriv->values[idex]; 
}

static void jk_map_default_clear(jk_env_t *env, jk_map_t *m )
{

}

static void jk_map_default_init(jk_env_t *env, jk_map_t *m, int initialSize,
                                void *wrappedObj)
{

}



/* ==================== */
/* General purpose map utils - independent of the map impl */

int jk_map_append(jk_env_t *env, jk_map_t * dst, jk_map_t * src )
{
    /* This was badly broken in the original ! */
    int sz = src->size(env, src);
    int i;
    for(i = 0 ; i < sz ; i++) {
        char *name = src->nameAt(env, src, i);
        void *value = src->valueAt(env, src, i);

        if( dst->get(env, dst, name ) == NULL) {
            int rc= dst->put(env, dst, name, value, NULL );
            if( rc != JK_TRUE )
                return rc;
        }
    }
    return JK_TRUE;
}



char *jk_map_getString(jk_env_t *env, jk_map_t *m,
                       const char *name, char *def)
{
    char *val= m->get( env, m, name );
    if( val==NULL )
        return def;
    return val;
}


/** Get a string property, using the worker's style
    for properties.
    Example worker.ajp13.host=localhost.
*/
char *jk_map_getStrProp(jk_env_t *env, jk_map_t *m,
                        const char *objType, const char *objName,
                        const char *pname,
                        char *def)
{
    char buf[1024];

    if( m==NULL || objType==NULL || objName==NULL || pname==NULL ) {
        return def;
    }
    sprintf(buf, "%s.%s.%s", objType, objName, pname);
    return m->get(env, m, buf );
}

int jk_map_getIntProp(jk_env_t *env, jk_map_t *m,
                      const char *objType, const char *objName,
                      const char *pname,
                      int def)
{
    char *val=jk_map_getStrProp( env, m, objType, objName, pname, NULL );

    if( val==NULL )
        return def;

    return jk_map_str2int( env, val );
}


/* ==================== */
/* Conversions */

/* Convert a string to int, using 'M', 'K' suffixes
 */
int jk_map_str2int(jk_env_t *env, char *val )
{   /* map2int:
       char *v=getString();
       return (c==NULL) ? def : str2int( v );
    */ 
    int  len;
    int  int_res;
    char org='\0';
    int  multit = 1;
    char *lastchar;

    if( val==NULL ) return 0;
    
    /* sprintf(buf, "%d", def); */
    /* rc = map_get_string(m, name, buf); */

    len = strlen(val);
    if(len==0)
        return 0;
    
    lastchar = val + len - 1;
    if('m' == *lastchar || 'M' == *lastchar) {
        org=*lastchar;
        *lastchar = '\0';
        multit = 1024 * 1024;
    } else if('k' == *lastchar || 'K' == *lastchar) {
        org=*lastchar;
        *lastchar = '\0';
        multit = 1024;
    }

    int_res = atoi(val);
    if( org!='\0' )
        *lastchar=org;

    return int_res * multit;
}

char **jk_map_split(jk_env_t *env, jk_map_t *m,
                    jk_pool_t *pool,
                    const char *listStr,
                    unsigned *list_len )
{
    char **ar = NULL;
    unsigned capacity = 0;
    unsigned idex = 0;    
    char *v;
    char *l;

    if( pool == NULL )
        pool=m->pool;
    
    *list_len = 0;

    if(listStr==NULL)
        return NULL;

    v = pool->pstrdup( pool, listStr);
    
    if(v==NULL) {
        return NULL;
    }

    /*
     * GS, in addition to VG's patch, we now need to 
     * strtok also by a "*"
     */

    for(l = strtok(v, " \t,*") ; l ; l = strtok(NULL, " \t,*")) {
        if(idex == capacity) {
            ar = pool->realloc(pool, 
                               sizeof(char *) * (capacity + 5),
                               ar,
                               sizeof(char *) * capacity);
            if(!ar) {
                return NULL;
            }
            capacity += 5;
        }
        ar[idex] = pool->pstrdup(pool, l);
        idex ++;
    }
        
    *list_len = idex;

    return ar;
}


/* ==================== */
/*  Reading / parsing */

int jk_map_readFileProperties(jk_env_t *env, jk_map_t *m,
                              const char *f)
{
    int rc = JK_FALSE;
    FILE *fp;
    char buf[LENGTH_OF_LINE + 1];            
    char *prp;
    char *v;
        
    if(m==NULL || f==NULL )
        return JK_FALSE;

    fp= fopen(f, "r");
        
    if(fp==NULL)
        return JK_FALSE;

    rc = JK_TRUE;

    while(NULL != (prp = fgets(buf, LENGTH_OF_LINE, fp))) {
        char *oldv;
        
        trim_prp_comment(prp);

        if( trim(prp)==0 )
            continue;

        v = strchr(prp, '=');
        if(v==NULL)
            continue;
        
        *v = '\0';
        v++;                        

        if(strlen(v)==0 || strlen(prp)==0)
            continue;

        oldv = m->get(env, m, prp );
        
        v = jk_map_replaceProperties(env, m, m->pool, v);
                
        if(oldv) {
            char *tmpv = m->pool->alloc(m->pool, 
                                        strlen(v) + strlen(oldv) + 3);
            char sep = '*';

            if(tmpv==NULL) {
                rc=JK_FALSE;
                break;
            }

            if(jk_is_some_property(prp, "path")) {
                sep = PATH_SEPERATOR;
            } else if(jk_is_some_property(prp, "cmd_line")) {
                sep = ' ';
            }
                
            sprintf(tmpv, "%s%c%s",  oldv, sep, v);
            v = tmpv;
        } else {
            v = m->pool->pstrdup(m->pool, v);
        }
        
        if(v==NULL) {
            /* Allocation error */
            rc = JK_FALSE;
            break;
        }

        m->put(env, m, prp, v, NULL);
    }

    fclose(fp);
    return rc;
}


/**
 *  Replace $(property) in value.
 * 
 */
char *jk_map_replaceProperties(jk_env_t *env, jk_map_t *m,
                               struct jk_pool *resultPool, 
                               const char *value)
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

            env_value = m->get(env, m, env_name);
            
	    if(env_value != NULL ) {
	      env_value=getenv( env_name );
	    }

            if(env_value != NULL ) {
                int offset=0;
                char *new_value = resultPool->alloc(resultPool, 
                                                    (strlen(rc) + strlen(env_value)));
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


/* ==================== */
/* Internal utils */


int jk_map_default_create(jk_env_t *env, jk_map_t **m, jk_pool_t *pool )
{
    jk_map_t *_this;
    jk_map_private_t *mPriv;

    if( m== NULL )
        return JK_FALSE;
    
    _this=(jk_map_t *)pool->alloc(pool, sizeof(jk_map_t));
    mPriv=(jk_map_private_t *)pool->alloc(pool, sizeof(jk_map_private_t));
    *m=_this;

    if( _this == NULL || mPriv==NULL )
        return JK_FALSE;
    
    _this->pool = pool;
    _this->_private=mPriv;
    
    mPriv->capacity = 0;
    mPriv->size     = 0;
    mPriv->names    = NULL;
    mPriv->values   = NULL;

    _this->get=jk_map_default_get;
    _this->put=jk_map_default_put;
    _this->size=jk_map_default_size;
    _this->nameAt=jk_map_default_nameAt;
    _this->valueAt=jk_map_default_valueAt;
    _this->init=jk_map_default_init;
    _this->clear=jk_map_default_clear;
    

    return JK_TRUE;
}

/* int map_free(jk_map_t **m) */
/* { */
/*     int rc = JK_FALSE; */

/*     if(m && *m) { */
/*         (*m)->pool->close((*m)->pool); */
/*         rc = JK_TRUE; */
/*         *m = NULL; */
/*     } */
/*     return rc; */
/* } */


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

static int jk_map_default_realloc(jk_map_t *m)
{
    jk_map_private_t *mPriv=m->_private;
    
    if(mPriv->size == mPriv->capacity) {
        char **names;
        void **values;
        int  capacity = mPriv->capacity + CAPACITY_INC_SIZE;

        names = (char **)m->pool->alloc(m->pool, sizeof(char *) * capacity);
        values = (void **)m->pool->alloc(m->pool, sizeof(void *) * capacity);
        
        if(values && names) {
            if (mPriv->capacity && mPriv->names) 
                memcpy(names, mPriv->names, sizeof(char *) * mPriv->capacity);

            if (mPriv->capacity && mPriv->values)
                memcpy(values, mPriv->values, sizeof(void *) * mPriv->capacity);

            mPriv->names = ( char **)names;
            mPriv->values = ( void **)values;
            mPriv->capacity = capacity;

            return JK_TRUE;
        }
    }

    return JK_FALSE;
}







