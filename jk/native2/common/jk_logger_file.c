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

/***************************************************************************
 * Description: Utility functions (mainly configuration)                   *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/


#include "jk_env.h"
#include "jk_map.h"
#include "jk_logger.h"
#include <stdio.h>
#include <fcntl.h>

#include "jk_registry.h"

#define LOG_FORMAT          ("log_format")

#define HUGE_BUFFER_SIZE (8*1024)
#define LOG_LINE_SIZE    (1024)


/* 
 * define the log format, we're using by default the one from error.log 
 *
 * [Mon Mar 26 19:44:48 2001] [jk_uri_worker_map.c (155)]: Into jk_uri_worker_map_t::uri_worker_map_alloc
 * log format used by apache in error.log
 */
#ifndef JK_TIME_FORMAT 
#define JK_TIME_FORMAT "[%a %b %d %H:%M:%S %Y] "
#endif

static const char * jk2_logger_file_logFmt = JK_TIME_FORMAT;

static void jk2_logger_file_setTimeStr(jk_env_t *env, char *str, int len)
{
    apr_time_exp_t gmt;
    apr_size_t     l;

    apr_time_exp_gmt(&gmt, apr_time_now());
    apr_strftime(str, &l, len, jk2_logger_file_logFmt, &gmt);
}

static int JK_METHOD jk2_logger_file_log(jk_env_t *env, jk_logger_t *l,                                 
                               int level,
                               const char *what)
{
    apr_status_t rv = APR_SUCCESS;
    apr_file_t *f = (apr_file_t *)l->logger_private;

    if (f == NULL) {
        /* This is usefull to debug what happens before logger is set.
           On apache you need -X option ( no detach, single process ) */
        if (what != NULL)
            fprintf(stderr, what);
        return JK_OK;
    }
    if(l && l->level <= level && l->logger_private && what) {       
        rv = apr_file_puts(what, f);
        /* flush the log for each entry only for debug level */
        if (rv == APR_SUCCESS && l->level == JK_LOG_DEBUG_LEVEL)
            rv = apr_file_flush(f);
        return JK_OK;
    }

    return JK_ERR;
}

int jk2_logger_file_parseLogLevel(jk_env_t *env, const char *level)
{
    if (!level)
        return JK_LOG_INFO_LEVEL;
    
    if (!strcasecmp(level, JK_LOG_INFO_VERB))
        return JK_LOG_INFO_LEVEL;

    if (!strcasecmp(level, JK_LOG_ERROR_VERB))
        return JK_LOG_ERROR_LEVEL;

    if (!strcasecmp(level, JK_LOG_EMERG_VERB))
        return JK_LOG_EMERG_LEVEL;

    return JK_LOG_DEBUG_LEVEL;
}

static int JK_METHOD jk2_logger_file_init(jk_env_t *env, jk_logger_t *_this )
{
    apr_status_t rv;
    apr_file_t *oldF = (apr_file_t *)_this->logger_private;

    apr_file_t *f = NULL;
    jk_workerEnv_t *workerEnv = env->getByName(env, "workerEnv");
    if (!_this->name) {
        _this->name="${serverRoot}/logs/jk2.log";
    }
    _this->name = jk2_config_replaceProperties(env, workerEnv->initData,
                                               _this->mbean->pool, _this->name);
    if (!_this->name || !strcmp("stderr", _this->name)) {
         if ((rv = apr_file_open_stderr(&f, env->globalPool->_private)) !=
                                        APR_SUCCESS) {
            _this->jkLog(env, _this,JK_LOG_ERROR,
                         "Can't open stderr file\n");
            return JK_ERR;
        }
        _this->logger_private = f;
    } 
    else {
        if ((rv = apr_file_open(&f, _this->name,  
                                APR_APPEND | APR_READ | APR_WRITE | APR_CREATE,
                                APR_OS_DEFAULT,
                                env->globalPool->_private)) != APR_SUCCESS) {
            _this->jkLog(env, _this,JK_LOG_ERROR,
                         "Can't open log file %s\n", _this->name );
            return JK_ERR;
        }
        _this->logger_private = f;
    } 
    _this->jkLog(env, _this,JK_LOG_INFO,
                 "Initializing log file %s\n", _this->name );
    if (oldF) {
        apr_file_close(oldF);
    }
    return JK_OK;
}

static int jk2_logger_file_close(jk_env_t *env,jk_logger_t *_this)
{
    apr_status_t rv;
    apr_file_t *f = _this->logger_private;
    
    if (!f)
        return JK_OK;
    
    apr_file_flush(f);
    rv = apr_file_close(f);
    _this->logger_private = NULL;

    /*     free(_this); */
    return JK_OK;
}

static int JK_METHOD
jk2_logger_file_setProperty(jk_env_t *env, jk_bean_t *mbean, 
                            char *name,  void *valueP )
{
    jk_logger_t *_this = mbean->object;
    char *value = valueP;
    if (!strcmp(name, "name"))
        _this->name = (char *)value;
    else if (!strcmp(name, "file")) {
        _this->name = (char *)value;
        /* Set the file imediately */
        jk2_logger_file_init(env, (jk_logger_t *)mbean->object);
    } 
    else if (!strcmp(name, "timeFormat"))
        jk2_logger_file_logFmt = value;
    else if (!strcmp(name, "level")) {
        _this->level = jk2_logger_file_parseLogLevel(env, value);
        if( _this->level == 0) {
            _this->jkLog(env, _this, JK_LOG_INFO,
                         "Level %s %d \n", value, _this->level);
        }
    }
    return JK_OK;
}

static int JK_METHOD jk2_logger_file_jkVLog(jk_env_t *env, jk_logger_t *l,
                                  const char *file,
                                  int line,
                                  int level,
                                  const char *fmt,
                                  va_list args)
{
    int rc = 0;
    char *buf;
    char *fmt1;
    apr_pool_t *aprPool = env->tmpPool->_private;
    char rfctime[APR_RFC822_DATE_LEN];
    apr_time_t time = apr_time_now();
    
    if (!file || !args)
        return -1;


    if (l->logger_private == NULL ||
        l->level <= level) {
        char *f = (char *)(file + strlen(file) - 1);
        char *slevel;
        switch (level){
            case JK_LOG_INFO_LEVEL:
                slevel = JK_LOG_INFO_VERB;
                break;
            case JK_LOG_ERROR_LEVEL:
                slevel = JK_LOG_ERROR_VERB;
                break;
            case JK_LOG_EMERG_LEVEL:
                slevel = JK_LOG_EMERG_VERB;
                break;
            case JK_LOG_DEBUG_LEVEL:
            default:
                slevel = JK_LOG_DEBUG_VERB;
                break;
        }
        while (f != file && *f != '\\' && *f != '/')
            f--;
        if (f != file)
            ++f;
        
        /* XXX rfc822_date or apr_ctime ? */
        apr_ctime(rfctime, time);
        fmt1 = apr_psprintf(aprPool, "[%s] (%5s ) [%s (%d)]  %s", rfctime, slevel, f, line, fmt);
        buf = apr_pvsprintf(aprPool, fmt1, args);

        l->log(env, l, level, buf);        
    }
    
    return rc;
}


static int jk2_logger_file_jkLog(jk_env_t *env, jk_logger_t *l,
                                 const char *file,
                                 int line,
                                 int level,
                                 const char *fmt, ...)
{
    va_list args;
    int rc;
    
    va_start(args, fmt);
    rc = jk2_logger_file_jkVLog(env, l, file, line, level, fmt, args);
    va_end(args);

    return rc;
}

int JK_METHOD jk2_logger_file_factory(jk_env_t *env, jk_pool_t *pool, 
                                      jk_bean_t *result,
                                      const char *type, const char *name)
{
    jk_logger_t *log = (jk_logger_t *)pool->calloc(env, pool, sizeof(jk_logger_t));

    if (log == NULL) {
        fprintf(stderr, "loggerFile.factory(): OutOfMemoryException\n"); 
        return JK_ERR;
    }

    log->log = jk2_logger_file_log;
    log->logger_private = NULL;
    log->init =jk2_logger_file_init;
    log->jkLog = jk2_logger_file_jkLog;
    log->jkVLog = jk2_logger_file_jkVLog;
    log->level = JK_LOG_ERROR_LEVEL;
    jk2_logger_file_logFmt = JK_TIME_FORMAT;
    
    result->object = log;
    log->mbean = result;
    
    result->setAttribute = jk2_logger_file_setProperty;

    return JK_OK;
}
