/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/***************************************************************************
 * Description: Logger object definitions                                  *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_LOGGER_H
#define JK_LOGGER_H

#include "jk_global.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct jk_logger jk_logger_t;
struct jk_logger
{
    void *logger_private;
    int level;
    const char *log_fmt;        /* the configured timestamp format for logging */
    const char *log_fmt_subsec; /* like log_fmt, but milli/micro seconds
                                   marker replaced, because strftime() doesn't handle those */
    int    log_fmt_type;        /* do we want milli or microseconds */
    size_t log_fmt_offset;      /* at which position should we insert */
    size_t log_fmt_size;        /* how long is this format string */

    int (JK_METHOD * log) (jk_logger_t *l, int level, int used, char *what);

};

typedef struct jk_file_logger_t jk_file_logger_t;
struct jk_file_logger_t
{
    FILE *logfile;
    /* For Apache 2 APR piped logging */
    void *jklogfp;
    /* For Apache 1.3 piped logging */
    int log_fd;
};

/* Level like Java tracing, but available only
   at compile time on DEBUG preproc define.
 */
#define JK_LOG_TRACE_LEVEL   0
#define JK_LOG_DEBUG_LEVEL   1
#define JK_LOG_INFO_LEVEL    2
#define JK_LOG_WARNING_LEVEL 3
#define JK_LOG_ERROR_LEVEL   4
#define JK_LOG_EMERG_LEVEL   5
#define JK_LOG_REQUEST_LEVEL 6
#define JK_LOG_DEF_LEVEL     JK_LOG_INFO_LEVEL

#define JK_LOG_TRACE_VERB   "trace"
#define JK_LOG_DEBUG_VERB   "debug"
#define JK_LOG_INFO_VERB    "info"
#define JK_LOG_WARN_VERB    "warn"
#define JK_LOG_ERROR_VERB   "error"
#define JK_LOG_EMERG_VERB   "emerg"
#define JK_LOG_DEF_VERB     JK_LOG_INFO_VERB

#if defined(__GNUC__) || (defined(_MSC_VER) && (_MSC_VER > 1200))
#define JK_LOG_TRACE   __FILE__,__LINE__,__FUNCTION__,JK_LOG_TRACE_LEVEL
#define JK_LOG_DEBUG   __FILE__,__LINE__,__FUNCTION__,JK_LOG_DEBUG_LEVEL
#define JK_LOG_ERROR   __FILE__,__LINE__,__FUNCTION__,JK_LOG_ERROR_LEVEL
#define JK_LOG_EMERG   __FILE__,__LINE__,__FUNCTION__,JK_LOG_EMERG_LEVEL
#define JK_LOG_INFO    __FILE__,__LINE__,__FUNCTION__,JK_LOG_INFO_LEVEL
#define JK_LOG_WARNING __FILE__,__LINE__,__FUNCTION__,JK_LOG_WARNING_LEVEL
#else
#define JK_LOG_TRACE   __FILE__,__LINE__,NULL,JK_LOG_TRACE_LEVEL
#define JK_LOG_DEBUG   __FILE__,__LINE__,NULL,JK_LOG_DEBUG_LEVEL
#define JK_LOG_ERROR   __FILE__,__LINE__,NULL,JK_LOG_ERROR_LEVEL
#define JK_LOG_EMERG   __FILE__,__LINE__,NULL,JK_LOG_EMERG_LEVEL
#define JK_LOG_INFO    __FILE__,__LINE__,NULL,JK_LOG_INFO_LEVEL
#define JK_LOG_WARNING __FILE__,__LINE__,NULL,JK_LOG_WARNING_LEVEL
#endif

#define JK_LOG_REQUEST __FILE__,0,NULL,JK_LOG_REQUEST_LEVEL

#if defined(JK_PRODUCTION)
/* TODO: all DEBUG messages should be compiled out
 * when this define is in place.
 */
#define JK_IS_PRODUCTION    1
#define JK_TRACE_ENTER(l)
#define JK_TRACE_EXIT(l)
#else
#define JK_IS_PRODUCTION    0
#define JK_TRACE_ENTER(l)                               \
    do {                                                \
        if ((l) && (l)->level == JK_LOG_TRACE_LEVEL) {  \
            jk_log((l), JK_LOG_TRACE, "enter");         \
    } } while (0)

#define JK_TRACE_EXIT(l)                                \
    do {                                                \
        if ((l) && (l)->level == JK_LOG_TRACE_LEVEL) {  \
            jk_log((l), JK_LOG_TRACE, "exit");          \
    } } while (0)

#endif  /* JK_PRODUCTION */

#define JK_LOG_NULL_PARAMS(l) jk_log((l), JK_LOG_ERROR, "NULL parameters")

/* Debug level macro
 * It is more efficient to check the level prior
 * calling function that will not execute anyhow because of level
 */
#define JK_IS_DEBUG_LEVEL(l)  ((l) && (l)->level <  JK_LOG_INFO_LEVEL)


#ifdef __cplusplus
}
#endif      /* __cplusplus */
#endif      /* JK_LOGGER_H */
