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
#endif                          /* __cplusplus */

    typedef struct jk_logger jk_logger_t;
    struct jk_logger
    {
        void *logger_private;
        int level;

        int (JK_METHOD * log) (jk_logger_t *l, int level, const char *what);

    };

    struct file_logger
    {
        FILE *logfile;
        /* For Apache 2 APR piped logging */
        void *jklogfp;
    };
    typedef struct file_logger file_logger_t;

#define JK_LOG_DEBUG_LEVEL   0
#define JK_LOG_INFO_LEVEL    1
#define JK_LOG_ERROR_LEVEL   2
#define JK_LOG_EMERG_LEVEL   3
#define JK_LOG_REQUEST_LEVEL 4

#define JK_LOG_DEBUG_VERB   "debug"
#define JK_LOG_INFO_VERB    "info"
#define JK_LOG_ERROR_VERB   "error"
#define JK_LOG_EMERG_VERB   "emerg"

#define JK_LOG_DEBUG   __FILE__,__LINE__,JK_LOG_DEBUG_LEVEL
#define JK_LOG_INFO    __FILE__,__LINE__,JK_LOG_INFO_LEVEL
#define JK_LOG_ERROR   __FILE__,__LINE__,JK_LOG_ERROR_LEVEL
#define JK_LOG_EMERG   __FILE__,__LINE__,JK_LOG_EMERG_LEVEL
#define JK_LOG_REQUEST __FILE__,0,JK_LOG_REQUEST_LEVEL

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* JK_LOGGER_H */
