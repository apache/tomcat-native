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

/**
 * Common methods for processing config data. Independent of the config
 * storage ( this is a sort of base class ).
 *
 * Config is read from the storage as a map, with 'bean names' as keys
 * and a map of attributes as values.
 *
 * We'll process the map creating new objects and calling the setters, similar
 * with what we do on the java side ( see ant or tomcat  ).
 *
 * There is also support for update, based on a version number. New objects
 * or existing objects having a 'ver' attribute will have the setter methods
 * called with the new values. XXX a reconfig method may be needed to notify.
 * Backends that support notifications may update directly the changed attributes,
 * but all components must support the simpler store ( where some attributes
 * may be set multiple times, since we can't detect individual att changes ).
 *
 * Note: The current code assumes a backend that is capable of storing
 * multi-valued attributes. This makes things hard to port to registry and
 * some other repositories. 
 *
 * @author: Gal Shachor <shachor@il.ibm.com>                           
 * @author: Costin Manolache
 */
 
#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_config.h"

#define LENGTH_OF_LINE    (1024)

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
    
    if (strlen(name) && *name == '$') {
        cfg->map->put(env, cfg->map, name + 1, val, NULL);
        return JK_OK;
    }

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
        env->l->jkLog( env, env->l, JK_LOG_DEBUG, "config: set %s / %s / %#lx / %s = %s\n",
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
        int oldDisabled=mbean->disabled;
        
        mbean->disabled=atoi( val );
        if(mbean->setAttribute) {
            mbean->setAttribute( env, mbean, name, val );
        }

        /* State change ... - it needs to be handled at the end*/
        /*         if( oldDisabled != mbean->disabled ) { */
        /*         } */
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
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
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
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
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
int jk2_config_processConfigData(jk_env_t *env, jk_config_t *cfg,int firstTime )
{
    int i;
    int rc;

    /* Set the config
     */
    for( i=0; i<cfg->cfgData->size( env, cfg->cfgData ); i++ ) {
        char *name=cfg->cfgData->nameAt(env, cfg->cfgData, i);
        rc=cfg->processNode(env, cfg , name, firstTime);
    }

    /* Init/stop components that need that. FirstTime will be handled by workerEnv, since
       some components don't support dynamic config and need a specific order.
     */
    if( !firstTime ) {
        for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
            char *name=env->_objects->nameAt( env, env->_objects, i );
            jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        
            if( mbean==NULL ) continue;

            /* New state ( == not initialized ) and disabled==0,
               try to reinit */
            if( mbean->state == JK_STATE_NEW &&
                mbean->disabled== 0 ) {
                int initOk=JK_OK;

                if( mbean->init != NULL ) {
                    initOk=mbean->init(env, mbean);
                    env->l->jkLog(env, env->l, JK_LOG_INFO,
                                  "config.update(): Starting %s %d\n", name, initOk );
                }
                if( initOk==JK_OK ) {
                    mbean->state=JK_STATE_INIT;
                }
            }

            /* Initialized state - and the config changed to disabled */
            if( mbean->state == JK_STATE_INIT &&
                mbean->disabled != 0 ) {
                    int initOk=JK_OK;

                    /* Stop */
                    if( mbean->destroy ) {
                        initOk=mbean->destroy(env, mbean);
                        env->l->jkLog(env, env->l, JK_LOG_INFO,
                                      "config.update(): Stopping %s %d\n", name, initOk );
                    }
                    if( initOk ) {
                        mbean->state=JK_STATE_NEW;
                    }
            }
        }
    }
    return rc;
}

/** This method will process one mbean configuration or reconfiguration.
    The special case for firstTime will be eventually removed - it is needed because some
    components may still depend on a specific startup order.

    The goal is for each component to follow JMX patterns - you should be able to add / remove
    jk components at runtime in any order.
*/
int jk2_config_processNode(jk_env_t *env, jk_config_t *cfg, char *name, int firstTime )
{
    int j;   
    
    jk_map_t *prefNode=cfg->cfgData->get(env, cfg->cfgData, name);
    jk_bean_t *bean;
    long ver=0;
    char *verString;
    int newBean=0;

    if( cfg->mbean->debug > 5 ) 
    env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                  "config.setConfig():  process %s\n", name );
    
    bean=env->getBean( env, name );
    if( bean==NULL ) {
        if( cfg->mbean->debug > 0 ) {
            env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                          "config.setConfig():  Creating %s\n", name );
        }
        bean=env->createBean( env, cfg->pool, name );
        newBean=1;
    }
    
    if( bean == NULL ) {
        /* Can't create it, save the value in our map */
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "config.update(): Can't create %s\n", name );
        return JK_ERR;
    }

    /* Don't call setters on objects that have the same ver.
       This is just a workaround for components that are not reconfigurable.
     */
    verString= prefNode->get( env, prefNode, "ver" );
    if( !firstTime ) {
        /* No ver option - assume it didn't change */
        if( verString == NULL && ! newBean ) {
            return JK_OK;
        }
        if( verString != NULL ) {    
            ver=atol( verString );
        
            if( ver == bean->ver && ! newBean ) {
                /* Object didn't change and is not new
                 */
                return JK_OK;
            }
        }
    }
    
    if( !firstTime )
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "config.update(): Updating %s in %d\n", name, getpid() );
    
    for( j=0; j<prefNode->size( env, prefNode ); j++ ) {
        char *pname=prefNode->nameAt(env, prefNode, j);
        char *pvalue=prefNode->valueAt(env, prefNode, j);

        cfg->setProperty( env, cfg, bean, pname, pvalue );
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "config.update(): done %s\n", name );
        
    return JK_OK;
}
