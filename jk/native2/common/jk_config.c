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

static int jk2_config_readFile(jk_env_t *env,
                                         jk_config_t *cfg,
                                         int *didReload, int firstTime);

/* ==================== ==================== */

/* Set the config file, read it. The property will also be
   used for storing modified properties.
 */
static int jk2_config_setConfigFile( jk_env_t *env,
                                     jk_config_t *cfg,
                                     jk_workerEnv_t *wEnv,
                                     char *workerFile)
{   
    cfg->file=workerFile;

    return jk2_config_readFile( env, cfg, NULL, JK_TRUE );
}

/* Experimental. Dangerous. The file param will go away, for security
   reasons - it can save only in the original file.
 */
static int jk2_config_saveConfig( jk_env_t *env,
                                  jk_config_t *cfg,
                                  char *workerFile)
{
    FILE *fp;
/*    char buf[LENGTH_OF_LINE + 1];            */
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
/*    int i; */
/*    char **comp; */
/*    int nrComp; */
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
    int multiValue=JK_FALSE;
    
    if( mbean == cfg->mbean ) {
        pname=name;
    } else {
        /* Make substitution work for ${OBJ_NAME.PROP} */
        pname=cfg->pool->calloc( env, cfg->pool,
                                 strlen( name ) + strlen( mbean->name ) + 4 );
        strcpy( pname, mbean->name );
        strcat( pname, "." );
        strcat( pname, name );
    }

    name= cfg->pool->pstrdup( env, cfg->pool, name );
    val= cfg->pool->pstrdup( env, cfg->pool, val );
    
    /** Save it on the config. XXX no support for deleting yet */
    /* The _original_ value. Will be saved with $() in it */
    if( mbean->settings == NULL )
        jk2_map_default_create(env, &mbean->settings, cfg->pool);
    
    if( mbean->multiValueInfo != NULL ) {
        int i;
        for( i=0; i<64; i++ ) {
            if(mbean->multiValueInfo[i]== NULL)
                break;
            if( strcmp( name, mbean->multiValueInfo[i])==0 ) {
                multiValue=JK_TRUE;
                break;
            }
        }
    }

    if( multiValue ) {
        mbean->settings->add( env, mbean->settings, name, val );
    } else {
        mbean->settings->put( env, mbean->settings, name, val, NULL );
    }

    /* Call the 'active' setter
     */
    val = jk2_config_replaceProperties(env, cfg->map, cfg->map->pool, val);

    /* fprintf( stderr, "config.setProperty2 %s %s %s \n", mbean->name, name, val ); */
    
    /** Used for future replacements
     */
    if( multiValue ) {
        cfg->map->add( env, cfg->map, pname, val );
    } else {
        cfg->map->put( env, cfg->map, pname, val, NULL );
    }
    
    if( cfg->mbean->debug > 0 )
        env->l->jkLog( env, env->l, JK_LOG_INFO, "config: set %s / %s / %p / %s = %s\n",
                       mbean->name, name, mbean, pname, val);
    
    if( strcmp( name, "name" ) == 0 ) {
        return JK_OK;
    }
    if( strcmp( name, "ver" ) == 0 ) {
        mbean->ver=atol(val);
        return JK_OK;
    }
    if( strcmp( name, "debug" ) == 0 ) {
        mbean->debug=atoi( val );
        if(mbean->setAttribute) {
            mbean->setAttribute( env, mbean, name, val );
        }
        return JK_OK;
    }
    if( strcmp( name, "disabled" ) == 0 ) {
        mbean->disabled=atoi( val );
        if(mbean->setAttribute) {
            mbean->setAttribute( env, mbean, name, val );
        }
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
    int didReplace=JK_FALSE;
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
                /* tmp allocations in tmpPool */
                char *new_value = env->tmpPool->calloc(env, env->tmpPool, 
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
                didReplace=JK_TRUE;
            } else {
                env_start = env_end;
            }
        } else {
            break;
        }
    }
    
    if( didReplace && resultPool!=NULL && resultPool != env->tmpPool ) {
        /* Make sure the result is allocated in the right mempool.
           tmpPool will be reset for each request.
        */
        rc=resultPool->pstrdup( env, resultPool, rc );
    }

    return rc;
}

/* -------------------- Reconfiguration -------------------- */

/** cfgData has component names as keys and a map of attributes as value.
 *  We'll create the beans and call the setters.
 *  If this is not firstTime, we create new componens and modify those
 *  with a lower 'ver'.
 *
 *  Note that _no_ object can be ever destroyed. You can 'disable' them,
 *  but _never_ remove/destroy it. We work in a multithreaded environment,
 *  and any removal may have disastrous consequences. Using critical
 *  sections would drastically affect the performance.
 */
static int jk2_config_processConfigData(jk_env_t *env, jk_config_t *cfg,int firstTime )
{
    int i;
    int rc;
    
    for( i=0; i<cfg->cfgData->size( env, cfg->cfgData ); i++ ) {
        char *name=cfg->cfgData->nameAt(env, cfg->cfgData, i);
        rc=cfg->processNode(env, cfg , name, firstTime);
    }
    return rc;
}

static int jk2_config_processNode(jk_env_t *env, jk_config_t *cfg, char *name, int firstTime )
{
    int j;   
    
    jk_map_t *prefNode=cfg->cfgData->get(env, cfg->cfgData, name);
    jk_bean_t *bean;
    int ver;
    char *verString;

    if( cfg->mbean->debug > 5 ) 
    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "config.setConfig():  process %s\n", name );
    
    bean=env->getBean( env, name );
    if( bean==NULL ) {
        if( cfg->mbean->debug > 0 ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                          "config.setConfig():  Creating %s\n", name );
        }
        bean=env->createBean( env, cfg->pool, name );
    }

    if( bean == NULL ) {
        /* Can't create it, save the value in our map */
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.update(): Can't create %s\n", name );
        return JK_ERR;
    }

    verString= prefNode->get( env, prefNode, "ver" );
    if( !firstTime ) {
        if( verString == NULL ) {
            return JK_OK;
        }
        ver=atoi( verString );
        
        if( ver <= bean->ver) {
            /* Object didn't change
             */
            return JK_OK;
        }
    }
    
    if( !firstTime )
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "config.update(): Updating %s\n", name );
    
    /* XXX Maybe we shoud destroy/init ? */
    
    for( j=0; j<prefNode->size( env, prefNode ); j++ ) {
        char *pname=prefNode->nameAt(env, prefNode, j);
        char *pvalue=prefNode->valueAt(env, prefNode, j);

        cfg->setProperty( env, cfg, bean, pname, pvalue );
    }

    return JK_OK;
}

static int jk2_config_readFile(jk_env_t *env,
                               jk_config_t *cfg,
                               int *didReload, int firstTime)
{
    int rc;
    int csOk;
    struct stat statbuf;

    if( didReload!=NULL )
        *didReload=JK_FALSE;

    if( cfg->file==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.update(): No config file\n" );
        return JK_ERR;
    }

    rc=stat(cfg->file, &statbuf);
    if (rc == -1) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.update(): Can't find config file %s\n", cfg->file );
        return JK_ERR;
    }
    
    if( !firstTime && statbuf.st_mtime < cfg->mtime ) {
        if( cfg->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "config.update(): No reload needed %s %ld %ld\n", cfg->file,
                          cfg->mtime, statbuf.st_mtime );
        return JK_OK;
    }
     
    JK_ENTER_CS(&cfg->cs, csOk);
    
    if(csOk !=JK_TRUE) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "cfg.update() Can't enter critical section\n");
        return JK_ERR;
    }

    /* Check if another thread has updated the config */

    rc=stat(cfg->file, &statbuf);
    if (rc == -1) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.update(): Can't find config file %s", cfg->file );
        JK_LEAVE_CS(&cfg->cs, csOk);
        return JK_ERR;
    }
    
    if( ! firstTime && statbuf.st_mtime <= cfg->mtime ) {
        JK_LEAVE_CS(&cfg->cs, csOk);
        return JK_OK;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "cfg.update() Updating config %s %d %d\n",
                  cfg->file, cfg->mtime, statbuf.st_mtime);
    
    jk2_map_default_create(env, &cfg->cfgData, env->tmpPool);

    rc=jk2_map_read(env, cfg->cfgData , cfg->file );
    
    if( rc==JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "config.setConfig():  Reading properties %s %d\n",
                      cfg->file, cfg->cfgData->size( env, cfg->cfgData ) );
    } else {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.setConfig(): Error reading properties %s\n",
                      cfg->file );
        JK_LEAVE_CS(&cfg->cs, csOk);
        return JK_ERR;
    }
    
    rc=jk2_config_processConfigData( env, cfg, firstTime );

    if( didReload!=NULL )
        *didReload=JK_TRUE;
    
    cfg->mtime= statbuf.st_mtime;

    JK_LEAVE_CS(&cfg->cs, csOk);
    return rc;
}


static int jk2_config_update(jk_env_t *env,
                             jk_config_t *cfg, int *didReload)
{
    return jk2_config_readFile( env, cfg, didReload, JK_FALSE );
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


int JK_METHOD jk2_config_factory( jk_env_t *env, jk_pool_t *pool,
                        jk_bean_t *result,
                        const char *type, const char *name)
{
    jk_config_t *_this;
    int i;

    _this=(jk_config_t *)pool->alloc(env, pool, sizeof(jk_config_t));
    if( _this == NULL )
        return JK_ERR;
    _this->pool = pool;

    _this->setPropertyString=jk2_config_setPropertyString;
    _this->update=jk2_config_update;
    _this->setProperty=jk2_config_setProperty;
    _this->save=jk2_config_saveConfig;
    _this->mbean=result;
    _this->processNode=jk2_config_processNode;
    _this->ver=0;

    result->object=_this;
    result->setAttribute=jk2_config_setAttribute;

    JK_INIT_CS(&(_this->cs), i);
    if (!i) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.factory(): Can't init CS\n");
        return JK_ERR;
    }

    
    return JK_OK;
}
