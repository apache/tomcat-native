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
        return JK_ERR;
    }

    cfg->file=workerFile;
    
    /** Read worker files
     */
    env->l->jkLog(env, env->l, JK_LOG_INFO, "config.setConfig(): Reading config %s %d\n",
                  workerFile, cfg->map->size(env, cfg->map) );
    
    jk2_map_default_create(env, &props, wEnv->pool);
    
    err=jk2_config_read(env, cfg, props, workerFile );
    
    if( err==JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "config.setConfig():  Reading properties %s %d\n",
                      workerFile, props->size( env, props ) );
    } else {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.setConfig(): Error reading properties %s\n",
                      workerFile );
        return JK_ERR;
    }

    for( i=0; i<props->size( env, props); i++ ) {
        char *name=props->nameAt(env, props, i);
        char *val=props->valueAt(env, props, i);

        cfg->setPropertyString( env, cfg, name, val );
    }
    
    return JK_OK;
}

/* Experimental. Dangerous. The file param will go away, for security
   reasons - it can save only in the original file.
 */
static int jk2_config_saveConfig( jk_env_t *env,
                                  jk_config_t *cfg,
                                  char *workerFile)
{
    FILE *fp;
//    char buf[LENGTH_OF_LINE + 1];            
    int i,j;

    if( workerFile==NULL )
        workerFile=cfg->file;

    if( workerFile== NULL )
        return JK_ERR;
    
    fp= fopen(workerFile, "w");
        
    if(fp==NULL)
        return JK_ERR;

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "config.save(): Saving %s\n", workerFile );
    
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

        if( strcmp( name, mbean->name ) != 0 ) {
            /* It's an alias. */
            continue;
        }
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

    return JK_OK;
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
//    int i;
//    char **comp;
//    int nrComp;
    char *lastDot;
    char *lastDot1;
    
    propertyString=cfg->pool->pstrdup( env, cfg->pool, propertyString );
    
    lastDot= strrchr( propertyString, (int)'.' );
    lastDot1= strrchr( propertyString, (int)':' );

    if( lastDot1==NULL )
        lastDot1=lastDot;
    
    if( lastDot==NULL || lastDot < lastDot1 )
        lastDot=lastDot1;
    
    if( lastDot==NULL || *lastDot=='\0' ) return JK_ERR;

    *lastDot='\0';
    lastDot++;

    *objName=propertyString;
    *propertyName=lastDot;

    /*     fprintf(stderr, "ProcessBeanProperty string %s %s\n", *objName, *propertyName); */
    
    return JK_OK;
}


/** Set a property on a bean. Call this when you know the bean.
    The name and values will be saved in the config tables.
    
    @param mbean coresponds to the object beeing configured
    @param name the property to be set ( can't have '.' or ':' in it )
    @param val the value, $(property) will be replaced.
 */
int jk2_config_setProperty(jk_env_t *env, jk_config_t *cfg,
                           jk_bean_t *mbean, char *name, char *val)
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

    /* fprintf( stderr, "config.setProperty2 %s %s %s \n", mbean->name, name, val ); */
    
    /** Used for future replacements
     */
    cfg->map->add( env, cfg->map, pname, val );

    /*     env->l->jkLog( env, env->l, JK_LOG_INFO, "config: set %s / %s / %s=%s\n", */
    /*                    mbean->name, name, pname, val); */
    if( strcmp( name, "name" ) == 0 ) {
        return JK_OK;
    }
    if( strcmp( name, "ver " ) == 0 ) {
        mbean->ver=atol(val);
        return JK_OK;
    }
    if( strcmp( name, "debug" ) == 0 ) {
        mbean->debug=atoi( val );
        return JK_OK;
    }
    if( strcmp( name, "disabled" ) == 0 ) {
        mbean->disabled=atoi( val );
        return JK_OK;
    }
    if( strcmp( name, "info" ) == 0 ) {
        /* do nothing, this is a comment */
        return JK_OK;
    }

    if( (mbean == cfg->mbean) &&
        (strcmp( name, "file" ) == 0 ) &&
        cfg->file != NULL ) {
        /* 'file' property on ourself, avoid rec.
         */
        if( cfg->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "config.setAttribute() ignore %s %s %s\n", mbean->name, name, val );
        
        return JK_OK;
    }
    
    if(mbean->setAttribute) {
        int rc= mbean->setAttribute( env, mbean, name, val );
        if( rc != JK_OK ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "config.setAttribute() Error setting %s %s %s\n", mbean->name, name, val );
        }
        if( cfg->mbean->debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "config.setAttribute() %d setting %s %s %s\n",
                          cfg->mbean->debug, mbean->name, name, val );
        return rc;
    }
    return JK_ERR;
}

int jk2_config_setPropertyString(jk_env_t *env, jk_config_t *cfg,
                                 char *name, char *value)
{
    jk_bean_t *mbean;

    jk_workerEnv_t *wEnv=cfg->workerEnv;
    jk_map_t *initData=cfg->map;
    int status;
    char *objName=NULL;
    char *propName=NULL;
    
    /* fprintf( stderr, "setPropertyString %s %s \n", name, value ); */

    status=jk2_config_processBeanPropertyString(env, cfg, name, &objName, &propName );
    if( status!=JK_OK ) {
        /* Unknown properties ends up in our config, as 'unclaimed' or global */
        cfg->setProperty( env, cfg, cfg->mbean, name, value );
        return status;
    }

    if( strncmp( objName, "disabled:", 9) == 0 ) {
        return JK_OK;
    }
    
    /** Replace properties in the object name */
    objName = jk2_config_replaceProperties(env, cfg->map, cfg->map->pool, objName);

    mbean=env->getBean( env, objName );
    if( mbean==NULL ) {
        mbean=env->createBean( env, cfg->pool, objName );
    }

    if( mbean == NULL ) {
        /* Can't create it, save the value in our map */
        cfg->setProperty( env, cfg, cfg->mbean, name, value );
        return JK_ERR;
    }

    if( mbean->settings == NULL )
        jk2_map_default_create(env, &mbean->settings, cfg->pool);
    
    return cfg->setProperty( env, cfg, mbean, propName, value );
}



/* ==================== */
/*  Reading / parsing */
int jk2_config_parseProperty(jk_env_t *env, jk_config_t *cfg, jk_map_t *m, char *prp )
{
    int rc = JK_ERR;
    char *v;

    jk2_trim_prp_comment(prp);
    
    if( jk2_trim(prp)==0 )
        return JK_OK;

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

        return JK_OK;
    }
    
    v = strchr(prp, '=');
    if(v==NULL)
        return JK_OK;
        
    *v = '\0';
    v++;                        
    
    if(strlen(v)==0 || strlen(prp)==0)
        return JK_OK;

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

    return JK_OK;
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
    return JK_OK;
}
     
int jk2_config_read(jk_env_t *env, jk_config_t *cfg, jk_map_t *m, const char *f)
{
    FILE *fp;
    char buf[LENGTH_OF_LINE + 1];            
    char *prp;
//    char *v;
        
    if(m==NULL || f==NULL )
        return JK_ERR;

    fp= fopen(f, "r");
        
    if(fp==NULL)
        return JK_ERR;

    cfg->section=NULL;
    while(NULL != (prp = fgets(buf, LENGTH_OF_LINE, fp))) {
        jk2_config_parseProperty( env, cfg, m, prp );
    }

    fclose(fp);
    return JK_OK;
}



/**
 *  Replace $(property) in value.
 * 
 */
char *jk2_config_replaceProperties(jk_env_t *env, jk_map_t *m,
                                   struct jk_pool *resultPool, 
                                   char *value)
{
    char *rc;
    char *env_start;
    int rec = 0;

    rc = value;
    env_start = value;

    while(env_start = strstr(env_start, "${")) {
        char *env_end = strstr(env_start, "}");
        if( rec++ > 20 ) return rc;
        if(env_end) {
            char env_name[LENGTH_OF_LINE + 1] = ""; 
            char *env_value;

            strncpy(env_name, env_start + 2, (env_end-env_start)-2);

            env_value = m->get(env, m, env_name);
	    if(env_value == NULL ) {
	      env_value=getenv( env_name );
	    }
            /* fprintf(stderr, "XXXjk_map %s %s \n", env_name, env_value ); */

            if(env_value != NULL ) {
                int offset=0;
                char *new_value = resultPool->calloc(env, resultPool, 
                                                    (strlen(rc) + strlen(env_value)));
                if(!new_value) {
                    break;
                }
                if( env_start == rc ) {
                    new_value[0]='\0';
                } else {
                    strncpy(new_value, rc, env_start-rc);
                }
                /* fprintf(stderr, "XXX %s %s %s\n", new_value, env_value, env_end + 1 ); */

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
static int JK_METHOD jk2_config_setAttribute( struct jk_env *env, struct jk_bean *mbean,
                                    char *name, void *valueP)
{
    jk_config_t *cfg=mbean->object;
    char *value=valueP;
    
    if( strcmp( name, "file" )==0 ) {
        return jk2_config_setConfigFile(env, cfg, cfg->workerEnv, value);
    } else if( strcmp( name, "debugEnv" )==0 ) {
        env->debug=atoi( value );
    } else if( strcmp( name, "save" )==0 ) {
        /* Experimental. Setting save='foo' will save the current config in
           foo
        */
        return jk2_config_saveConfig(env, cfg, value);
    } else {
        return JK_ERR;
    }
    return JK_OK;
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

int JK_METHOD jk2_config_factory( jk_env_t *env, jk_pool_t *pool,
                        jk_bean_t *result,
                        const char *type, const char *name)
{
    jk_config_t *_this;

    _this=(jk_config_t *)pool->alloc(env, pool, sizeof(jk_config_t));
    if( _this == NULL )
        return JK_ERR;
    _this->pool = pool;

    _this->setPropertyString=jk2_config_setPropertyString;
    _this->setProperty=jk2_config_setProperty;
    _this->save=jk2_config_saveConfig;
    _this->mbean=result;
    
    result->object=_this;
    result->setAttribute=jk2_config_setAttribute;

    return JK_OK;
}
