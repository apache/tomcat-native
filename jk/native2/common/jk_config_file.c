/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2003 The Apache Software Foundation.          *
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

/**
 * Description: File based configuration.
 *
 * @author: Gal Shachor <shachor@il.ibm.com>                           
 * @author: Costin Manolache
 */

#ifdef AS400
#include "apr_xlate.h"    
#endif

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_config.h"

#define LENGTH_OF_LINE    (1024)

/* 
 */
static int jk2_config_file_saveConfig( jk_env_t *env,
                                       jk_config_t *cfg, char *workerFile )
{
    FILE *fp;
    int i,j;
    
    if( workerFile==NULL )
        workerFile=cfg->file;

    if( workerFile== NULL )
        return JK_ERR;
    
#ifdef AS400
     fp = fopen(workerFile, "w, o_ccsid=0");
#else
     fp = fopen(workerFile, "w");
#endif        
        
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

/* ==================== */
/*  Reading / parsing. 
 */

static void jk2_trim_prp_comment(char *prp)
{
#ifdef AS400
    char *comment;
  /* lots of lines that translate a '#' realtime deleted   */
    comment = strchr(prp, *APR_NUMBERSIGN); 
#else
    char *comment = strchr(prp, '#');
#endif
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



int jk2_config_file_parseProperty(jk_env_t *env, jk_map_t *m, char **section, char *prp )
{
    int rc = JK_ERR;
    char *v;
    jk_map_t *prefNode=NULL;

    jk2_trim_prp_comment(prp);
    
    if( jk2_trim(prp)==0 )
        return JK_OK;

    /* Support windows-style 'sections' - for cleaner config
     */
    if( (prp[0] == '[') ) {
        v=strchr(prp, ']' );
        *v='\0';
        jk2_trim( v );
        prp++;
        
        *section=m->pool->pstrdup(env, m->pool, prp);

        jk2_map_default_create( env, &prefNode, m->pool );

        m->add( env, m, *section, prefNode);

        return JK_OK;
    }
    
    v = strchr(prp, '=');
    if(v==NULL)
        return JK_OK;
        
    *v = '\0';
    v++;                        

    if(strlen(v)==0 || strlen(prp)==0)
        return JK_OK;

    if (*section!=NULL){
        prefNode=m->get( env, m, *section);
    }else{
        prefNode=m;
    }
    
    if( prefNode==NULL )
        return JK_ERR;

    /* fprintf(stderr, "Adding [%s] %s=%s\n", cfg->section, prp, v ); */
    prefNode->add( env, prefNode, m->pool->pstrdup(env, m->pool, prp),
                   m->pool->pstrdup(env, m->pool, v));

    return JK_OK;
}



/** Read the config file
 */
int jk2_config_file_read(jk_env_t *env, jk_map_t *m,const char *file)
{
    FILE *fp;
    char buf[LENGTH_OF_LINE + 1];            
    char *prp;
    char *section=NULL;
    if(m==NULL || file==NULL )
        return JK_ERR;

#ifdef AS400
    fp = fopen(file, "r, o_ccsid=0");
#else
    fp = fopen(file, "r");
#endif        

    if(fp==NULL)
        return JK_ERR;

    while(NULL != (prp = fgets(buf, LENGTH_OF_LINE, fp))) {
        jk2_config_file_parseProperty( env, m, &section, prp );
    }

    fclose(fp);
    return JK_OK;
}


static int jk2_config_file_readFile(jk_env_t *env,
                                    jk_config_t *cfg,
                                    int *didReload, int firstTime)
{
    int rc;
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
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "config.update(): No reload needed %s %ld %ld\n", cfg->file,
                          cfg->mtime, statbuf.st_mtime );
        return JK_OK;
    }

    if( cfg->cs == NULL ) {
        jk_bean_t *jkb=env->createBean2(env, cfg->mbean->pool,"threadMutex", NULL);
        if( jkb != NULL && jkb->object != NULL ) {
            cfg->cs=jkb->object;
            jkb->init(env, jkb );
        }
    }

    if( cfg->cs != NULL ) 
        cfg->cs->lock( env, cfg->cs );

    /* Check if another thread has updated the config */

    rc=stat(cfg->file, &statbuf);
    if (rc == -1) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.update(): Can't find config file %s", cfg->file );
        if( cfg->cs != NULL ) 
            cfg->cs->unLock( env, cfg->cs );
        return JK_ERR;
    }
    
    if( ! firstTime && statbuf.st_mtime <= cfg->mtime ) {
        if( cfg->cs != NULL ) 
            cfg->cs->unLock( env, cfg->cs );
        return JK_OK;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "cfg.update() Updating config %s %d %d\n",
                  cfg->file, cfg->mtime, statbuf.st_mtime);

    /** The map will be lost - all objects must be saved !
     */
    jk2_map_default_create(env, &cfg->cfgData, env->tmpPool);

    rc=jk2_config_file_read(env, cfg->cfgData , cfg->file );
    
    if( rc==JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "config.setConfig():  Reading properties %s %d\n",
                      cfg->file, cfg->cfgData->size( env, cfg->cfgData ) );
    } else {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.setConfig(): Error reading properties %s\n",
                      cfg->file );
        if( cfg->cs != NULL ) 
            cfg->cs->unLock( env, cfg->cs );
        return JK_ERR;
    }
    
    rc=jk2_config_processConfigData( env, cfg, firstTime );

    if( didReload!=NULL )
        *didReload=JK_TRUE;
    
    cfg->mtime= statbuf.st_mtime;

    if( cfg->cs != NULL ) 
        cfg->cs->unLock( env, cfg->cs );

    return rc;
}


static int jk2_config_file_update(jk_env_t *env,
                             jk_config_t *cfg, int *didReload)
{
    return jk2_config_file_readFile( env, cfg, didReload, JK_FALSE );
}

/** Set a property for this config object
 */
static int JK_METHOD jk2_config_file_setAttribute( struct jk_env *env, struct jk_bean *mbean,
                                              char *name, void *valueP)
{
    jk_config_t *cfg=mbean->object;
    char *value=valueP;
    
    if( strcmp( name, "file" )==0 ) {
        cfg->file=value;
        return jk2_config_file_readFile( env, cfg, NULL, JK_TRUE );
    } else if( strcmp( name, "debugEnv" )==0 ) {
        env->debug=atoi( value );
    } else if( strcmp( name, "save" )==0 ) {
        return jk2_config_file_saveConfig(env, cfg, value);
    } else {
        return JK_ERR;
    }
    return JK_OK;
}


int JK_METHOD jk2_config_file_factory( jk_env_t *env, jk_pool_t *pool,
                        jk_bean_t *result,
                        const char *type, const char *name)
{
    jk_config_t *_this;

    _this=(jk_config_t *)pool->alloc(env, pool, sizeof(jk_config_t));
    if( _this == NULL )
        return JK_ERR;

    _this->pool = pool;
    _this->ver=0;

    _this->setPropertyString=jk2_config_setPropertyString;
    _this->setProperty=jk2_config_setProperty;
    _this->processNode=jk2_config_processNode;

    _this->cs=NULL;
    
    _this->update=jk2_config_file_update;
    _this->save=jk2_config_file_saveConfig;

    result->object=_this;
    result->setAttribute=jk2_config_file_setAttribute;
    _this->mbean=result;

    return JK_OK;
}
