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
 * Description: Utility functions (mainly configuration)                   *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_env.h"
#include "jk_map.h"
#include "jk_logger.h"
#include <stdio.h>

#include "jk_registry.h"

#define LOG_FORMAT		    ("log_format")

#define HUGE_BUFFER_SIZE (8*1024)
#define LOG_LINE_SIZE    (1024)

int JK_METHOD jk_logger_file_factory(jk_env_t *env, jk_pool_t *pool, void **result,
                                     const char *type, const char *name);


/* 
 * define the log format, we're using by default the one from error.log 
 *
 * [Mon Mar 26 19:44:48 2001] [jk_uri_worker_map.c (155)]: Into jk_uri_worker_map_t::uri_worker_map_alloc
 * log format used by apache in error.log
 */
#ifndef JK_TIME_FORMAT 
#define JK_TIME_FORMAT "[%a %b %d %H:%M:%S %Y] "
#endif

const char * jk_logger_file_logFmt = JK_TIME_FORMAT;

static void jk_logger_file_setTimeStr(char * str, int len)
{
	time_t		t = time(NULL);
    	struct tm 	*tms;

    	tms = gmtime(&t);
        if( tms==NULL ) return;
	strftime(str, len, jk_logger_file_logFmt, tms);
}

static int jk_logger_file_log(jk_logger_t *l,                                 
                              int level,
                              const char *what)
{
    FILE *f = (FILE *)l->logger_private;

    if( f==NULL ) {
        /* This is usefull to debug what happens before logger is set.
           On apache you need -X option ( no detach, single process ) */
        printf("%s", what );
        return JK_TRUE;
    }
    if(l && l->level <= level && l->logger_private && what) {       
        unsigned sz = strlen(what);
        if(sz) {
            fwrite(what, 1, sz, f);
            /* [V] Flush the dam' thing! */
            fflush(f);
        }

        return JK_TRUE;
    }

    return JK_FALSE;
}

static int jk_logger_file_parseLogLevel(const char *level)
{
    if( level == NULL ) return JK_LOG_ERROR_LEVEL;
    
    if(0 == strcasecmp(level, JK_LOG_INFO_VERB)) {
        return JK_LOG_INFO_LEVEL;
    }

    if(0 == strcasecmp(level, JK_LOG_ERROR_VERB)) {
        return JK_LOG_ERROR_LEVEL;
    }

    if(0 == strcasecmp(level, JK_LOG_EMERG_VERB)) {
        return JK_LOG_EMERG_LEVEL;
    }

    return JK_LOG_DEBUG_LEVEL;
}

static int jk_logger_file_open(jk_logger_t *_this,
                               jk_map_t *properties )
{
    char *file=jk_map_getStrProp(NULL, properties,"logger","file",
                                 "name","mod_jk.log");
    int level;
    char *levelS=jk_map_getStrProp(NULL, properties,"logger","file",
                                   "level", "ERROR");
    char *logformat=jk_map_getStrProp(NULL, properties,"logger","file",
                                   "timeFormat", JK_TIME_FORMAT);
    FILE *f;

    _this->level = jk_logger_file_parseLogLevel(levelS);
    if( _this->level == 0 )
        _this->jkLog( _this, JK_LOG_ERROR, "Level %s %d \n", levelS, _this->level ); 
    
    if( logformat==NULL ) {
        logformat=JK_TIME_FORMAT;
    }
    jk_logger_file_logFmt = logformat;

    f = fopen(file, "a+");
    if(f==NULL) {
        _this->jkLog(_this,JK_LOG_ERROR,"Can't open log file %s\n", file );
        return JK_FALSE;
    }
    _this->logger_private = f;
    return JK_TRUE;
}

static int jk_logger_file_close(jk_logger_t *_this)
{
    FILE *f = _this->logger_private;
    if( f==NULL ) return JK_TRUE;
    
    fflush(f);
    fclose(f);
    _this->logger_private=NULL;

    /*     free(_this); */
    return JK_TRUE;
}

static int jk_logger_file_jkLog(jk_logger_t *l,
                                const char *file,
                                int line,
                                int level,
                                const char *fmt, ...)
{
    int rc = 0;
    
    if( !file || !fmt) {
        return -1;
    }

    if(l->logger_private==NULL ||
       l->level <= level) {
#ifdef NETWARE
/* On NetWare, this can get called on a thread that has a limited stack so */
/* we will allocate and free the temporary buffer in this function         */
        char *buf;
#else
        char buf[HUGE_BUFFER_SIZE];
#endif
        char *f = (char *)(file + strlen(file) - 1);
        va_list args;
        int used = 0;

        while(f != file && '\\' != *f && '/' != *f) {
            f--;
        }
        if(f != file) {
            f++;
        }

#ifdef WIN32
	set_time_str(buf, HUGE_BUFFER_SIZE);
	used = strlen(buf);
        if( level >= JK_LOG_DEBUG_LEVEL )
            used += _snprintf(&buf[used], HUGE_BUFFER_SIZE, " [%s (%d)]: ", f, line);        
#elif defined(NETWARE) /* until we get a snprintf function */
        buf = (char *) malloc(HUGE_BUFFER_SIZE);
        if (NULL == buf)
           return -1;

	jk_logger_file_setTimeStr(buf, HUGE_BUFFER_SIZE);
	used = strlen(buf);
        if( level >= JK_LOG_DEBUG_LEVEL )
            used += sprintf(&buf[used], " [%s (%d)]: ", f, line);
#else 
	jk_logger_file_setTimeStr(buf, HUGE_BUFFER_SIZE);
	used = strlen(buf);
        if( level >= JK_LOG_DEBUG_LEVEL )
            used += snprintf(&buf[used], HUGE_BUFFER_SIZE, " [%s (%d)]: ", f, line);        
#endif
        if(used < 0) {
            return 0; /* [V] not sure what to return... */
        }
    
        va_start(args, fmt);
#ifdef WIN32
        rc = _vsnprintf(buf + used, HUGE_BUFFER_SIZE - used, fmt, args);
#elif defined(NETWARE) /* until we get a vsnprintf function */
        rc = vsprintf(buf + used, fmt, args);
#else 
        rc = vsnprintf(buf + used, HUGE_BUFFER_SIZE - used, fmt, args);
#endif
        va_end(args);

        l->log(l, level, buf);
#ifdef NETWARE
        free(buf);
#endif
    }
    
    return rc;
}


int jk_logger_file_factory(jk_env_t *env,
                           jk_pool_t *pool, 
                           void **result,
                           const char *type,
                           const char *name)
{
    jk_logger_t *l = (jk_logger_t *)pool->alloc(pool, sizeof(jk_logger_t));

    if(l==NULL ) {
        return JK_FALSE;
    }

    l->log = jk_logger_file_log;
    l->logger_private = NULL;
    l->open =jk_logger_file_open;
    l->jkLog = jk_logger_file_jkLog;

    *result=(void *)l;

    return JK_TRUE;
}

