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
 * Description: General purpose config object                                 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_config.h"

#define CAPACITY_INC_SIZE (50)
#define LENGTH_OF_LINE    (1024)

static void jk2_trim_prp_comment(char *prp);
static int  jk2_trim(char *s);
static char **jk2_config_getValues(jk_env_t *env, jk_config_t *m,
                                   struct jk_pool *resultPool,
                                   char *name,
                                   char *sep,
                                   int *countP);


/* ==================== ==================== */

/* Set the config file, read it. The property will also be
   used for storing modified properties.
 */
static int jk2_config_setConfigFile( jk_env_t *env,
                                     jk_config_t *cfg,
                                     jk_workerEnv_t *wEnv,
                                     char *workerFile)
{
    struct stat statbuf;
    int err;
    jk_map_t *props;
    int i;
    
    if (stat(workerFile, &statbuf) == -1) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.setConfig(): Can't find config file %s", workerFile );
        return JK_FALSE;
    }
    
    /** Read worker files
     */
    env->l->jkLog(env, env->l, JK_LOG_INFO, "config.setConfig(): Reading config %s %d\n",
                  workerFile, cfg->map->size(env, cfg->map) );
    
    jk2_map_default_create(env, &props, wEnv->pool);
    
    err=jk2_config_read(env, cfg, props, workerFile );
    
    if( err==JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "config.setConfig():  Reading properties %s %d\n",
                      workerFile, props->size( env, props ) );
    } else {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.setConfig(): Error reading properties %s\n",
                      workerFile );
        return JK_FALSE;
    }

    for( i=0; i<props->size( env, props); i++ ) {
        char *name=props->nameAt(env, props, i);
        char *val=props->valueAt(env, props, i);

        cfg->setPropertyString( env, cfg, name, val );
    }
    
    return JK_TRUE;
}

/* Experimental. Dangerous. The file param will go away, for security
   reasons - it can save only in the original file.
 */
static int jk2_config_saveConfig( jk_env_t *env,
                                  jk_config_t *cfg,
                                  jk_workerEnv_t *wEnv,
                                  char *workerFile)
{
    FILE *fp;
    char buf[LENGTH_OF_LINE + 1];            
    int i,j;
        
    fp= fopen(workerFile, "w");
        
    if(fp==NULL)
        return JK_FALSE;

    /* We'll save only the objects/properties that were set
       via config, and to the original 'string'. That keeps the config
       small ( withou redundant ) and close to the original. Comments
       will be saved/loaded later.
    */
    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );

        if( mbean==NULL || mbean->settings==NULL ) 
            continue;
        
        fprintf( fp, "[%s]\n", mbean->name );

        for( j=0; j < mbean->settings->size( env, mbean->settings ); j++ ) {
            char *pname=mbean->settings->nameAt( env, mbean->settings, j);
            /* Don't save redundant information */
            if( strcmp( pname, "name" ) != 0 ) {
                fprintf( fp, "%s=%s\n",
                         pname,
                         mbean->settings->valueAt( env, mbean->settings, j));
            }
        }
        fprintf( fp, "\n" );
    }
    
    fclose(fp);

    return JK_TRUE;
}


/** Interpret the 'name' as [OBJECT].[PROPERTY].
    If the [OBJECT] is not found, construct it using
    the prefix ( i.e. we'll search for a factory that matches
    name - XXX make it longest match ? ).

    Then set the property on the object that is found or
    constructed.

    No replacement or saving is done on the val - this is
    a private method
*/


static int jk2_config_processBeanPropertyString( jk_env_t *env,
                                                 jk_config_t *cfg,
                                                 char *propertyString,
                                                 char **objName, char **propertyName )
{
    jk_bean_t *w = NULL;
    char *type=NULL;
    char *dot=0;
    int i;
    char **comp;
    int nrComp;
    char *lastDot;
    char *lastDot1;
    
    propertyString=cfg->pool->pstrdup( env, cfg->pool, propertyString );
    
    lastDot= rindex( propertyString, (int)'.' );
    lastDot1= rindex( propertyString, (int)':' );

    if( lastDot1==NULL )
        lastDot1=lastDot;
    
    if( lastDot==NULL || lastDot < lastDot1 )
        lastDot=lastDot1;
    
    if( lastDot==NULL || *lastDot=='\0' ) return JK_FALSE;

    *lastDot='\0';
    lastDot++;

    *objName=propertyString;
    *propertyName=lastDot;

    /*     fprintf(stderr, "ProcessBeanProperty string %s %s\n", *objName, *propertyName); */
    
    return JK_TRUE;
}

/** Create a jk component using the name prefix
 */
static jk_bean_t *jk2_config_createInstance( jk_env_t *env, jk_config_t *cfg,
                                             char *objName )
{
    jk_pool_t *workerPool;
    jk_bean_t *w=NULL;
    int i;
    char *type=NULL;
        
    /** New object. Create it using the prefix
     */
    for( i=0; i< env->_registry->size( env, env->_registry ) ; i++ ) {
        char *factName=env->_registry->nameAt( env, env->_registry, i );
        int len=strlen(factName );
        
        if( (strncmp( objName, factName, len) == 0) &&
            ( (objName[len] == '.') ||
              (objName[len] == ':') ||
              (objName[len] == '_') ||
              (objName[len] == '\0') )  ) {
            /* We found the factory. */
            type=factName;
            /*             env->l->jkLog(env, env->l, JK_LOG_INFO, */
            /*                               "Found %s  %s %s %d %d\n", type, objName, */
            /*                           factName, len, strncmp( objName, factName, len)); */
            break;
        }
    }
    if( type==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.createInstance(): Can't find type for %s \n", objName);
        return NULL;
    } 
    
    workerPool=cfg->pool->create(env, cfg->pool, HUGE_POOL_SIZE);
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "config.createInstance(): Create [%s] %s\n", type, objName);
    env->createInstance( env, workerPool, type, objName );
    w=env->getMBean( env, objName );
    if( w==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.createInstance(): Error creating  [%s] %s\n", objName, type);
        return NULL;
    }
    if( w->settings == NULL )
        jk2_map_default_create(env, &w->settings, cfg->pool);

    return w;
}


/** Set a property on a bean. Call this when you know the bean.
    The name and values will be saved in the config tables.
    
    @param mbean coresponds to the object beeing configured
    @param name the property to be set ( can't have '.' or ':' in it )
    @param val the value, $(property) will be replaced.
 */
static int jk2_config_setProperty(jk_env_t *env, jk_config_t *cfg,
                                  jk_bean_t *mbean, char *name, void *val)
{
    char *pname;
    if( mbean == cfg->mbean ) {
        pname=name;
    } else {
        pname=cfg->pool->calloc( env, cfg->pool,
                                 strlen( name ) + strlen( mbean->name ) + 4 );
        strcpy( pname, mbean->name );
        strcat( pname, "." );
        strcat( pname, name );
    }

    /* fprintf( stderr, "config.setProperty %s %s %s \n", mbean->name, name, val ); */

    /** Save it on the config. XXX no support for deleting yet */
    /* The _original_ value. Will be saved with $() in it */
    if( mbean->settings == NULL )
        jk2_map_default_create(env, &mbean->settings, cfg->pool);
    
    mbean->settings->add( env, mbean->settings, name, val );

    /* Call the 'active' setter
     */
    val = jk2_config_replaceProperties(env, cfg->map, cfg->map->pool, val);

    /** Used for future replacements
     */
    cfg->map->add( env, cfg->map, pname, val );

    /*     env->l->jkLog( env, env->l, JK_LOG_INFO, "config: set %s / %s / %s=%s\n", */
    /*                    mbean->name, name, pname, val); */
 
    if(mbean->setAttribute)
        return mbean->setAttribute( env, mbean, name, val );
    return JK_FALSE;
}

static int jk2_config_setPropertyString(jk_env_t *env, jk_config_t *cfg,
                                        char *name, char *value)
{
    jk_bean_t *mbean;

    jk_workerEnv_t *wEnv=cfg->workerEnv;
    jk_map_t *initData=cfg->map;
    int status;
    char *objName=NULL;
    char *propName=NULL;

    /*     fprintf( stderr, "setPropertyString %s %s \n", name, value ); */

    status=jk2_config_processBeanPropertyString(env, cfg, name, &objName, &propName );
    if( status!=JK_TRUE ) {
        /* Unknown properties ends up in our config, as 'unclaimed' or global */
        jk2_config_setProperty( env, cfg, cfg->mbean, name, value );
        return status;
    }
    
    mbean=env->getMBean( env, objName );
    if( mbean==NULL ) {
        mbean=jk2_config_createInstance( env, cfg, objName );
    }
    if( mbean == NULL ) {
        /* Can't create it, save the value in our map */
        jk2_config_setProperty( env, cfg, cfg->mbean, name, value );
        return JK_FALSE;
    }

    return jk2_config_setProperty( env, cfg, mbean, propName, value );
}


static char *jk2_config_getString(jk_env_t *env, jk_config_t *conf,
                                  const char *name, char *def)
{
    char *val= conf->map->get( env, conf->map, name );
    if( val==NULL )
        return def;
    return val;
}

static int jk2_config_getBool(jk_env_t *env, jk_config_t *conf,
                              const char *prop, const char *def)
{
    char *val=jk2_config_getString( env, conf, prop, (char *)def );

    if( val==NULL )
        return JK_FALSE;

    if( strcmp( val, "1" ) == 0 ||
        strcasecmp( val, "TRUE" ) == 0 ||
        strcasecmp( val, "ON" ) == 0 ) {
        return JK_TRUE;
    }
    return JK_FALSE;
}

/** Get a string property, using the worker's style
    for properties.
    Example worker.ajp13.host=localhost.
*/
static char *jk2_config_getStrProp(jk_env_t *env, jk_config_t *conf,
                                   const char *objType, const char *objName,
                                   const char *pname, char *def)
{
    char buf[1024];
    char *res;

    if(  objName==NULL || pname==NULL ) {
        return def;
    }
    if( objType==NULL ) 
        sprintf(buf, "%s.%s", objName, pname);
    else
        sprintf(buf, "%s.%s.%s", objType, objName, pname);
    res = jk2_config_getString(env, conf, buf, def );
    return res;
}


static int jk2_config_getIntProp(jk_env_t *env, jk_config_t *conf,
                       const char *objType, const char *objName,
                       const char *pname, int def)
{
    char *val=jk2_config_getStrProp( env, conf, objType, objName, pname, NULL );

    if( val==NULL )
        return def;

    return jk2_config_str2int( env, val );
}

/* ==================== */
/* Conversions */

/* Convert a string to int, using 'M', 'K' suffixes
 */
int jk2_config_str2int(jk_env_t *env, char *val )
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


char **jk2_config_split(jk_env_t *env, jk_pool_t *pool,
                        const char *listStr, const char *sep,
                        unsigned *list_len )
{
    char **ar = NULL;
    unsigned capacity = 0;
    unsigned idex = 0;    
    char *v;
    char *l;

    if( sep==NULL )
        sep=" \t,*";
    
    if( list_len != NULL )
        *list_len = 0;

    if(listStr==NULL)
        return NULL;

    v = pool->pstrdup( env, pool, listStr);
    
    if(v==NULL) {
        return NULL;
    }

    /*
     * GS, in addition to VG's patch, we now need to 
     * strtok also by a "*"
     */

    /* Not thread safe */
    for(l = strtok(v, sep) ; l ; l = strtok(NULL, sep)) {
        /* We want at least one space after idex for the null*/
        if(idex+1 >= capacity) {
            ar = pool->realloc(env, pool, 
                               sizeof(char *) * (capacity + 5),
                               ar,
                               sizeof(char *) * capacity);
            if(!ar) {
                return NULL;
            }
            capacity += 5;
        }
        ar[idex] = pool->pstrdup(env, pool, l);
        idex ++;
    }

    /* Append a NULL, we have space */
    ar[idex]=NULL;

    if( list_len != NULL )
        *list_len = idex;

    return ar;
}


/* ==================== */
/*  Reading / parsing */
int jk2_config_parseProperty(jk_env_t *env, jk_config_t *cfg, jk_map_t *m, char *prp )
{
    int rc = JK_FALSE;
    char *v;
        
    jk2_trim_prp_comment(prp);
    
    if( jk2_trim(prp)==0 )
        return JK_TRUE;

    /* Support windows-style 'sections' - for cleaner config
     */
    if( prp[0] == '[' ) {
        char *dummyProp;
        v=strchr(prp, ']' );
        *v='\0';
        jk2_trim( v );
        prp++;
        cfg->section=cfg->pool->pstrdup(env, cfg->pool, prp);
        /* Add a dummy property, so the object will be created */
        dummyProp=cfg->pool->calloc( env, cfg->pool, strlen( prp ) + 7 );
        strcpy(dummyProp, prp );
        strcat( dummyProp, ".name");
        m->add( env, m, dummyProp, cfg->section);

        return JK_TRUE;
    }
    
    v = strchr(prp, '=');
    if(v==NULL)
        return JK_TRUE;
        
    *v = '\0';
    v++;                        
    
    if(strlen(v)==0 || strlen(prp)==0)
        return JK_TRUE;

    /* [ ] Shortcut */
    if( cfg->section != NULL ) {
        char *newN=cfg->pool->calloc( env, cfg->pool, strlen( prp ) + strlen( cfg->section ) + 4 );
        strcpy( newN, cfg->section );
        strcat( newN, "." );
        strcat( newN, prp );
        prp=newN;
    }

    /* Don't replace now - the caller may want to
       save the file, and it'll replace them anyway for runtime changes
       v = jk2_config_replaceProperties(env, cfg->map, cfg->pool, v); */

    /* We don't contatenate the values - but use multi-value
       fields. This eliminates the ugly hack where readProperties
       tried to 'guess' the separator, and the code is much
       cleaner. If we have multi-valued props, it's better
       to deal with that instead of forcing a single-valued
       model.
    */
    m->add( env, m, cfg->pool->pstrdup(env, cfg->pool, prp),
            cfg->pool->pstrdup(env, cfg->pool, v));

    return JK_TRUE;
}

/** Read a query string into the map
 */
int jk2_config_queryRead(jk_env_t *env, jk_config_t *cfg, jk_map_t *m, const char *query)
{
    char *sep;
    char *value;
    char *qry=cfg->pool->pstrdup( env, cfg->pool, query );

    while( qry != NULL ) {
        sep=strchr( qry, '&');
        if( sep !=NULL ) { 
            *sep='\0';
            sep++;
        }

        value = strchr(qry, '=');
        if(value==NULL) {
            value="";
        } else {
            *value = '\0';
            value++;
        }
        m->add( env, m, cfg->pool->pstrdup( env, cfg->pool, qry ),
                cfg->pool->pstrdup( env, cfg->pool, value ));
        qry=sep;
    }
    return JK_TRUE;
}
     
int jk2_config_read(jk_env_t *env, jk_config_t *cfg, jk_map_t *m, const char *f)
{
    FILE *fp;
    char buf[LENGTH_OF_LINE + 1];            
    char *prp;
    char *v;
        
    if(m==NULL || f==NULL )
        return JK_FALSE;

    fp= fopen(f, "r");
        
    if(fp==NULL)
        return JK_FALSE;

    cfg->section=NULL;
    while(NULL != (prp = fgets(buf, LENGTH_OF_LINE, fp))) {
        jk2_config_parseProperty( env, cfg, m, prp );
    }

    fclose(fp);
    return JK_TRUE;
}

/** For multi-value properties, return the concatenation
 *  of all values.
 *
 * @param sep Separators used to separate multi-values and
 *       when concatenating the values, NULL for none. The first
 *       char will be used on the result, the other will be
 *       used to split. ( i.e. the map may either have multiple
 *       values or values separated by one of the sep's chars )
 *    
 */
static char *jk2_config_getValuesString(jk_env_t *env, jk_config_t *m,
                                     struct jk_pool *resultPool,
                                     char *name,
                                     char *sep )
{
    char **values;
    int valuesCount;
    int i;
    int len=0;
    int pos=0;
    int sepLen=0;
    char *result;
    char sepStr[2];
    
    if(sep==NULL)
        values=jk2_config_getValues( env, m, resultPool, name," \t,*", &valuesCount );
    else
        values=jk2_config_getValues( env, m, resultPool, name, sep, &valuesCount );

    if( values==NULL ) return NULL;
    if( valuesCount<=0 ) return NULL;

    if( sep!= NULL )
        sepLen=strlen( sep );

    for( i=0; i< valuesCount; i++ ) {
        len+=strlen( values[i] );
        if( sep!= NULL )
            len+=1; /* Separator */
    }

    result=(char *)resultPool->alloc( env, resultPool, len + 1 );

    result[0]='\0';
    if( sep!=NULL ) {
        sepStr[0]=sep[0];
        sepStr[1]='\0';
    }
    
    for( i=0; i< valuesCount; i++ ) {
        strcat( values[i], result );
        if( sep!=NULL )
            strcat( sepStr, result );
    }
    return result;
}

/** For multi-value properties, return the array containing
 * all values.
 *
 * @param sep Optional separator, it'll be used to split existing values.
 *            Curently only single-char separators are supported. 
 */
static char **jk2_config_getValues(jk_env_t *env, jk_config_t *m,
                                struct jk_pool *resultPool,
                                char *name,
                                char *sep,
                                int *countP)
{
    char **result;
    int count=0;
    int capacity=8;
    int mapSz= m->map->size(env, m->map );
    int i;
    char *l;

    *countP=0;
    result=(char **)resultPool->alloc( env, resultPool,
                                       capacity * sizeof( char *));
    for(i=0; i<mapSz; i++ ) {
        char *cName= m->map->nameAt( env, m->map, i );
        char *cVal= m->map->valueAt( env, m->map, i );

        if(0 == strcmp(cName, name)) {
            /* Split the value by sep, and add it to the result list
             */
            for(l = strtok(cVal, sep) ; l ; l = strtok(NULL, sep)) {
                if(count == capacity) {
                    result = resultPool->realloc(env, resultPool, 
                                                 sizeof(char *) * (capacity + 5),
                                                 result,
                                                 sizeof(char *) * capacity);
                    if(result==NULL) 
                        return NULL;
                    capacity += 5;
                }
                result[count] = resultPool->pstrdup(env, resultPool, l);
                count++;
            }
        }
    }
    *countP=count;
    return result;
}
                               

/**
 *  Replace $(property) in value.
 * 
 */
char *jk2_config_replaceProperties(jk_env_t *env, jk_map_t *m,
                                   struct jk_pool *resultPool, 
                                   char *value)
{
    char *rc = value;
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
	    if(env_value == NULL ) {
	      env_value=getenv( env_name );
	    }
            /* fprintf(stderr, "XXXjk_map %s %s \n", env_name, env_value ); */

            if(env_value != NULL ) {
                int offset=0;
                char *new_value = resultPool->alloc(env, resultPool, 
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


/** Set a property for this config object
 */
static int jk2_config_setAttribute( struct jk_env *env, struct jk_bean *mbean,
                                    char *name, void *valueP)
{
    jk_config_t *cfg=mbean->object;
    char *value=valueP;
    
    if( strcmp( name, "file" )==0 ) {
        return jk2_config_setConfigFile(env, cfg, cfg->workerEnv, value);
    } else if( strcmp( name, "save" )==0 ) {
        /* Experimental. Setting save='foo' will save the current config in
           foo
        */
        return jk2_config_saveConfig(env, cfg, cfg->workerEnv, value);
    } else {
        return JK_FALSE;
    }
    return JK_TRUE;
}


static void jk2_trim_prp_comment(char *prp)
{
    char *comment = strchr(prp, '#');
    if(comment) {
        *comment = '\0';
    }
}

static int jk2_trim(char *s)
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

int jk2_config_factory( jk_env_t *env, jk_pool_t *pool,
                        jk_bean_t *result,
                        const char *type, const char *name)
{
    jk_config_t *_this;

    _this=(jk_config_t *)pool->alloc(env, pool, sizeof(jk_config_t));
    if( _this == NULL )
        return JK_FALSE;
    _this->pool = pool;

    _this->setPropertyString=jk2_config_setPropertyString;
    _this->setProperty=jk2_config_setProperty;
    _this->mbean=result;
    
    result->object=_this;
    result->setAttribute=jk2_config_setAttribute;

    return JK_TRUE;
}
