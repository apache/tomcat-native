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
 * Description: Utility functions (mainly configuration)                   *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/


#include "jk_env.h"
#include "jk_map.h"
#include "jk_logger.h"

#include <global.h>
#include <addin.h>

static int JK_METHOD jk2_logger_domino_log(jk_env_t *env, jk_logger_t *l,
                                         int level, const char *what) {

    if (l && l->level <= level && NULL != what) {
        AddInLogMessageText("%s", NOERROR, what);
        return JK_OK;
    }

    return JK_ERR;
}

int jk2_logger_domino_parseLogLevel(jk_env_t *env, const char *level) {

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

static int JK_METHOD jk2_logger_domino_init(jk_env_t *env, jk_logger_t *_this) {
    return JK_OK;
}

static int jk2_logger_domino_close(jk_env_t *env, jk_logger_t *_this) {
    return JK_OK;
}

static int JK_METHOD
jk2_logger_domino_setProperty(jk_env_t *env, jk_bean_t *mbean,
                            char *name, void *valueP) {

    jk_logger_t *_this = mbean->object;
    char *value = valueP;
    if (!strcmp(name, "level")) {
        _this->level = jk2_logger_domino_parseLogLevel(env, value);
        if (_this->level == 0) {
            _this->jkLog(env, _this, JK_LOG_INFO,
                         "Level %s %d \n", value, _this->level);
        }
    }
    return JK_OK;
}

static int JK_METHOD jk2_logger_domino_jkVLog(jk_env_t *env, jk_logger_t *l,
                                            const char *file,
                                            int line,
                                            int level,
                                            const char *fmt, va_list args) {

    char *buf;
    char *fmt1;
    apr_pool_t *aprPool = env->tmpPool->_private;

    if (!file || !args) {
        return -1;
    }

    if (l->level <= level) {
        char *f = (char *)(file + strlen(file) - 1);
        char *slevel;
        switch (level) {
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
        while (f != file && *f != '\\' && *f != '/') {
            f--;
        }
        if (f != file) {
            ++f;
        }

        fmt1 = apr_psprintf(aprPool, "(%5s ) [%s (%d)] %s", slevel, f, line, fmt);
        buf = apr_pvsprintf(aprPool, fmt1, args);

        l->log(env, l, level, buf);
    }

    return 0;
}


static int jk2_logger_domino_jkLog(jk_env_t *env, jk_logger_t *l,
                                 const char *file,
                                 int line, int level, const char *fmt, ...) {

    va_list args;
    int rc;

    va_start(args, fmt);
    rc = jk2_logger_domino_jkVLog(env, l, file, line, level, fmt, args);
    va_end(args);

    return rc;
}

int JK_METHOD jk2_logger_domino_factory(jk_env_t *env, jk_pool_t *pool,
                                      jk_bean_t *result,
                                      const char *type, const char *name) {

    jk_logger_t *log =
        (jk_logger_t *)pool->calloc(env, pool, sizeof(jk_logger_t));

    if (log == NULL) {
        fprintf(stderr, "loggerDomino.factory(): OutOfMemoryException\n");
        return JK_ERR;
    }

    log->log = jk2_logger_domino_log;
    log->logger_private = NULL;
    log->init = jk2_logger_domino_init;
    log->jkLog = jk2_logger_domino_jkLog;
    log->jkVLog = jk2_logger_domino_jkVLog;
    log->level = JK_LOG_ERROR_LEVEL;

    result->object = log;
    log->mbean = result;

    result->setAttribute = jk2_logger_domino_setProperty;

    return JK_OK;
}
