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
 * Description: Logger implementation using AOLserver's native logging.
 *
 * Author: Alexander Leykekh, aolserver@aol.net
 *
 * $Revision$
 *
 */ 

#include "jk_ns.h"
#include <stdio.h>

static int JK_METHOD jk2_logger_ns_log(jk_env_t *env, jk_logger_t *l,                                 
				       int level,
				       const char *what)
{
    return JK_OK;
}


static int JK_METHOD jk2_logger_ns_init(jk_env_t *env, jk_logger_t *_this)
{
    return JK_OK;
}

static int JK_METHOD jk2_logger_ns_close(jk_env_t *env, jk_logger_t *_this)
{
    return JK_OK;
}

static int JK_METHOD jk2_logger_ns_jkVLog(jk_env_t *env, jk_logger_t *l,
					  const char *file,
					  int line,
					  int level,
					  const char *fmt,
					  va_list args)
{
    apr_pool_t *aprPool;
    int rc;
    char *buf, *buf2, *buf3;

    if (env==NULL || env->tmpPool==NULL || l==NULL || fmt==NULL)
        return JK_ERR;

    aprPool=env->tmpPool->_private;
    if (aprPool == NULL)
        return JK_ERR;

    if( level < l->level )
        return JK_OK;

    buf=apr_pvsprintf( aprPool, fmt, args );
    if (buf == NULL)
        return JK_ERR;
    
    rc=strlen( buf );
    /* Remove trailing \n. */
    if( buf[rc-1] == '\n' )
        buf[rc-1]='\0';

    if (file != NULL) {
        buf2 = apr_psprintf (aprPool, "%s:%d:", file, line);
	if (buf2 == NULL)
	    return JK_ERR;

	buf3 = apr_pstrcat (aprPool, buf2, buf, NULL);
	if (buf3 == NULL)
	    return JK_ERR;

    } else {
        buf3 = buf;
    }
    
    if( level == JK_LOG_DEBUG_LEVEL ) {
        Ns_Log (Debug, "nsjk2: %s", buf3);
    } else if( level == JK_LOG_INFO_LEVEL ) {
        Ns_Log (Notice, "nsjk2: %s", buf3);
    } else {
        Ns_Log (Error, "nsjk2: %s", buf3);
    }

    return JK_OK;
}

static int jk2_logger_ns_jkLog(jk_env_t *env, jk_logger_t *l,
                               const char *file,
			       int line,
			       int level,
			       const char *fmt, ...)
{
    va_list args;
    int rc;

    if (env==NULL || l==NULL || fmt==NULL)
        return JK_ERR;    

    va_start(args, fmt);
    rc=jk2_logger_ns_jkVLog( env, l, file, line, level, fmt, args );
    va_end(args);

    return rc;
}


static int JK_METHOD
jk2_logger_file_setProperty(jk_env_t *env, jk_bean_t *mbean, 
                            char *name,  void *valueP )
{
    jk_logger_t *_this;
    char *value=valueP;

    if (env==NULL || mbean==NULL || name==NULL)
        return JK_ERR;

    _this=mbean->object;
    if (_this == NULL)
        return JK_ERR;

    if( strcmp( name, "level" )==0 ) {
        _this->level = jk2_logger_file_parseLogLevel(env, value);
        if( _this->level == JK_LOG_DEBUG_LEVEL ) {
            env->debug = 1;
            /*             _this->jkLog( env, _this, JK_LOG_ERROR, */
            /*                           "Level %s %d \n", value, _this->level ); */
        }
        return JK_OK;
    }
    return JK_ERR;
}



int JK_METHOD 
jk2_logger_ns_factory(jk_env_t *env, jk_pool_t *pool, jk_bean_t *result,
		      const char *type, const char *name)
{
    jk_logger_t *l;

    if (env==NULL || pool==NULL || result==NULL)
        return JK_ERR;

    l = (jk_logger_t *)pool->calloc(env, pool, sizeof(jk_logger_t));
    if(l==NULL ) {
        return JK_ERR;
    }
    
    l->log = jk2_logger_ns_log;
    l->logger_private = NULL;
    l->init =jk2_logger_ns_init;
    l->jkLog = jk2_logger_ns_jkLog;
    l->jkVLog = jk2_logger_ns_jkVLog;

    l->level=JK_LOG_ERROR_LEVEL;
    
    result->object=(void *)l;
    l->mbean=result;
    result->setAttribute = jk2_logger_file_setProperty;

    return JK_OK;
}

