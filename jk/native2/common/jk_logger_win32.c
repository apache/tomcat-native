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
int JK_METHOD 
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

