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
 * Description: Logger implementation using win32's native logger,
 * 
 * 
 * @author Costin Manolache
 * @author Ignacio J. Ortega
 */ 

#include "jk_env.h"
#include "jk_map.h"
#include "jk_logger.h"
#include <stdio.h>


#define HUGE_BUFFER_SIZE (8*1024)
#define JAKARTA_EVENT_SOURCE "Apache Jakarta Connector2"

#ifdef WIN32

#include "jk_logger_win32_message.h"

static int JK_METHOD jk2_logger_win32_log(jk_env_t *env, jk_logger_t *l,                                 
                                 int level,
                                 const char *what)
{
    HANDLE h=RegisterEventSource(NULL,JAKARTA_EVENT_SOURCE);
    LPCTSTR *Buffer;
    Buffer=&what;
    if( h==NULL ) {
        return JK_ERR;
    }
    if(l && l->level <= level && what) {       
        if( level == JK_LOG_DEBUG_LEVEL ) {
            ReportEvent(h,EVENTLOG_SUCCESS,0,MSG_DEBUG,NULL,1,0,Buffer,NULL);
        } else if( level == JK_LOG_INFO_LEVEL ) {
            ReportEvent(h,EVENTLOG_INFORMATION_TYPE,0,MSG_INFO,NULL,1,0,Buffer,NULL);
        } else if( level == JK_LOG_ERROR_LEVEL ){
            ReportEvent(h,EVENTLOG_WARNING_TYPE,0,MSG_ERROR,NULL,1,0,Buffer,NULL);
        } else if( level == JK_LOG_EMERG_LEVEL ){
            ReportEvent(h,EVENTLOG_ERROR_TYPE,0,MSG_EMERG,NULL,1,0,Buffer,NULL);
        }
    }
    DeregisterEventSource(h);
    
    return JK_OK;
}


static int JK_METHOD jk2_logger_win32_init(jk_env_t *env, jk_logger_t *_this)
{
    HKEY hk; 
    DWORD dwData; 
    char event_key[100]="SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\";

    if( !RegCreateKey(HKEY_LOCAL_MACHINE, 
            strcat(event_key,JAKARTA_EVENT_SOURCE), 
            &hk)){
       
        RegSetValueEx(hk, "EventMessageFile", 0, REG_SZ, (LPBYTE) env->soName, strlen(env->soName) + 1);
        dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE; 
 
        RegSetValueEx(hk, "TypesSupported", 0, REG_DWORD, (LPBYTE) &dwData, sizeof(DWORD));
 
        RegCloseKey(hk); 
    }
    return JK_OK;
} 


static int JK_METHOD jk2_logger_win32_close(jk_env_t *env, jk_logger_t *_this)
{
    
    return JK_OK;
}

static int JK_METHOD jk2_logger_win32_jkVLog(jk_env_t *env, jk_logger_t *l,
                                     const char *file,
                                     int line,
                                     int level,
                                     const char *fmt,
                                     va_list args)
{
    int rc = 0;
    if(l->level <= level) {
        char buf[HUGE_BUFFER_SIZE];
        char *f = (char *)(file + strlen(file) - 1);
        int used = 0;

        while(f != file && '\\' != *f && '/' != *f) {
            f--;
        }
        if(f != file) {
            f++;
        }

        if( level >= JK_LOG_DEBUG_LEVEL ) {
            used += _snprintf(&buf[used], HUGE_BUFFER_SIZE, " [%s (%d)]: ", f, line);
        }
        if(used < 0) {
            return 0; /* [V] not sure what to return... */
        }
    

        rc = _vsnprintf(buf + used, HUGE_BUFFER_SIZE - used, fmt, args);

        l->log(env, l, level, buf);
    }
    return rc ;
}

static int jk2_logger_win32_jkLog(jk_env_t *env, jk_logger_t *l,
                                 const char *file,
                                 int line,
                                 int level,
                                 const char *fmt, ...)
{
    va_list args;
    int rc;
    
    va_start(args, fmt);
    rc=jk2_logger_win32_jkVLog( env, l, file, line, level, fmt, args );
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
            /*             _this->jkLog( env, _this, JK_LOG_ERROR, */
            /*                           "Level %s %d \n", value, _this->level ); */
        }
        return JK_OK;
    }
    return JK_ERR;
}



int JK_METHOD 
jk2_logger_win32_factory(jk_env_t *env, jk_pool_t *pool, jk_bean_t *result,
                           const char *type, const char *name)
{
    jk_logger_t *l = (jk_logger_t *)pool->calloc(env, pool,
                                                 sizeof(jk_logger_t));

    if(l==NULL ) {
        return JK_ERR;
    }
    
    l->log = jk2_logger_win32_log;
    l->logger_private = NULL;
    l->init =jk2_logger_win32_init;
    l->jkLog = jk2_logger_win32_jkLog;
    l->jkVLog = jk2_logger_win32_jkVLog;

    l->level=JK_LOG_ERROR_LEVEL;
    
    result->object=(void *)l;
    l->mbean=result;
    result->setAttribute = jk2_logger_file_setProperty;

    return JK_OK;
}

#else
jk2_logger_win32_factory(jk_env_t *env, jk_pool_t *pool, jk_bean_t *result,
                           const char *type, const char *name)
{
    env->l->jkLog( env, env->l, JK_LOG_ERROR,
                   "win32logger.factory(): Support for win32 logger is disabled.");
    env->l->jkLog( env, env->l, JK_LOG_ERROR,
                   "win32logger.factory(): Needs WINNT > 4.0 ");
    result->disabled=1;
    return JK_FALSE;
}
#endif

