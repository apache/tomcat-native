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

#include "jk_env.h"
#include "jk_global.h"
#include "jk_map.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct jk_map;
struct jk_env;
struct jk_logger;
typedef struct jk_logger jk_logger_t;

/* Logger object.
 *  XXX level should be moved per component ( to control the generation of messages ),
 *  the level param in the param should be used only as information ( to be displayed
 *  in the log ).
 */
struct jk_logger {
    struct jk_bean *mbean;
    char *name;
    void *logger_private;
    int  level;

    int (JK_METHOD *init)( struct jk_env *env, jk_logger_t *_this );

    void (JK_METHOD *close)( struct jk_env *env, jk_logger_t *_this );

    int (JK_METHOD *log)(struct jk_env *env,
                         jk_logger_t *_this,
                         int level,
                         const char *what);

    int (JK_METHOD *jkLog)(struct jk_env *env,
                           jk_logger_t *_this,
                           const char *file,
                           int line,
                           int level,
                           const char *fmt, ...);

    int (JK_METHOD *jkVLog)(struct jk_env *env,
                            jk_logger_t *_this,
                            const char *file,
                            int line,
                            int level,
                            const char *fmt,
                            va_list msg);

};

#define JK_LOG_DEBUG_LEVEL 0
#define JK_LOG_INFO_LEVEL  1
#define JK_LOG_ERROR_LEVEL 2
#define JK_LOG_EMERG_LEVEL 3

#define JK_LOG_DEBUG_VERB   "debug"
#define JK_LOG_INFO_VERB    "info"
#define JK_LOG_ERROR_VERB   "error"
#define JK_LOG_EMERG_VERB   "emerg"

#define JK_LOG_DEBUG __FILE__,__LINE__,JK_LOG_DEBUG_LEVEL
#define JK_LOG_INFO  __FILE__,__LINE__,JK_LOG_INFO_LEVEL
#define JK_LOG_ERROR __FILE__,__LINE__,JK_LOG_ERROR_LEVEL
#define JK_LOG_EMERG __FILE__,__LINE__,JK_LOG_EMERG_LEVEL

int jk2_logger_file_parseLogLevel(struct jk_env *env, const char *level);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_LOGGER_H */
