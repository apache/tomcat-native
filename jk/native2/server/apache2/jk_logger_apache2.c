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

/**
 * Description: Logger implementation using apache's native logging.
 *
 * This is the result of lazyness - a single log file to watch ( error.log )
 * instead of 2, no need to explain/document/decide where to place mod_jk
 * logging, etc.
 *
 * Normal apache logging rules apply.
 *
 * XXX Jk will use per/compoment logging level. All logs will be WARN level
 * in apache, and the filtering will happen on each component level.
 *
 * XXX Add file/line
 *
 * XXX Use env, use the current request structure ( so we can split the log
 * based on vhost configs ).
 *
 * @author Costin Manolache
 */ 

#include "jk_env.h"
#include "jk_map.h"
#include "jk_logger.h"
#include <stdio.h>

#include "httpd.h"
#include "http_log.h"

#include "jk_apache2.h"

#define HUGE_BUFFER_SIZE (8*1024)


static int JK_METHOD jk2_logger_apache2_log(jk_env_t *env, jk_logger_t *l,                                 
                                 int level,
                                 const char *what)
{
    return JK_OK;
}


static int JK_METHOD jk2_logger_apache2_init(jk_env_t *env, jk_logger_t *_this)
{
    return JK_OK;
}

static int JK_METHOD jk2_logger_apache2_close(jk_env_t *env, jk_logger_t *_this)
{
    return JK_OK;
}

static int JK_METHOD jk2_logger_apache2_jkVLog(jk_env_t *env, jk_logger_t *l,
                                     const char *file,
                                     int line,
                                     int level,
                                     const char *fmt,
                                     va_list args)
{
    /* XXX map jk level to apache level */
    server_rec *s=(server_rec *)l->logger_private;
    int rc;

    /* XXX XXX Change this to "SMALLSTACK" or something, I don't think it's
       netware specific */
#ifdef NETWARE
/* On NetWare, this can get called on a thread that has a limited stack so */
/* we will allocate and free the temporary buffer in this function         */
    char *buf;
#else
    char buf[HUGE_BUFFER_SIZE];
#endif

    if( level < l->level )
        return JK_OK;

    if( s==NULL ) {
        return JK_ERR;
    }
    
#if defined(NETWARE) /* until we get a vsnprintf function */
    /* XXX Can we use a pool ? */
    /* XXX It'll go away with env and per thread data !! */
    buf = (char *) malloc(HUGE_BUFFER_SIZE);
    rc = vsprintf(buf, fmt, args);
#else 
    rc = vsnprintf(buf, HUGE_BUFFER_SIZE, fmt, args);
#endif
    rc=strlen( buf );
    /* Remove trailing \n. XXX need to change the log() to not include \n */
    if( buf[rc-1] == '\n' )
        buf[rc-1]='\0';
    
    if( level == JK_LOG_DEBUG_LEVEL ) {
        ap_log_error( file, line, APLOG_DEBUG | APLOG_NOERRNO, 0, s, buf);
    } else if( level == JK_LOG_INFO_LEVEL ) {
        ap_log_error( file, line, APLOG_WARNING | APLOG_NOERRNO, 0, s, buf);
    } else {
        ap_log_error( file, line, APLOG_ERR | APLOG_NOERRNO, 0, s, buf);
    }

#ifdef NETWARE
    free(buf);
#endif
    return rc ;
}

static int jk2_logger_apache2_jkLog(jk_env_t *env, jk_logger_t *l,
                                 const char *file,
                                 int line,
                                 int level,
                                 const char *fmt, ...)
{
    va_list args;
    int rc;
    
    va_start(args, fmt);
    rc=jk2_logger_apache2_jkVLog( env, l, file, line, level, fmt, args );
    va_end(args);

    return rc;
}


static int JK_METHOD
jk2_logger_file_setProperty(jk_env_t *env, jk_bean_t *mbean, 
                            char *name,  void *valueP )
{
    jk_logger_t *_this=mbean->object;
    char *value=valueP;

    if( strcmp( name, "level" )==0 ) {
        _this->level = jk2_logger_file_parseLogLevel(env, value);
        if( _this->level == 0 ) {
            _this->jkLog( env, _this, JK_LOG_ERROR,
                          "Level %s %d \n", value, _this->level );
        }
        return JK_OK;
    }
    return JK_ERR;
}



int JK_METHOD 
jk2_logger_apache2_factory(jk_env_t *env, jk_pool_t *pool, jk_bean_t *result,
                              char *type, char *name)
{
    jk_logger_t *l = (jk_logger_t *)pool->calloc(env, pool,
                                                 sizeof(jk_logger_t));

    if(l==NULL ) {
        return JK_ERR;
    }
    
    l->log = jk2_logger_apache2_log;
    l->logger_private = NULL;
    l->init =jk2_logger_apache2_init;
    l->jkLog = jk2_logger_apache2_jkLog;
    l->jkVLog = jk2_logger_apache2_jkVLog;

    l->level=JK_LOG_ERROR_LEVEL;
    
    result->object=(void *)l;
    l->mbean=result;
    result->setAttribute = jk2_logger_file_setProperty;

    return JK_OK;
}

