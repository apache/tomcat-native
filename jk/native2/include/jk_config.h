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

/** Config object. It's more-or-less independent of the config source
    or representation. 
 */

#ifndef JK_CONFIG_H
#define JK_CONFIG_H

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_env.h"
#include "jk_logger.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

    struct jk_pool;
    struct jk_map;
    struct jk_env;
    struct jk_config;
    typedef struct jk_config jk_config_t;

/**
 *
 */
    struct jk_config
    {
        struct jk_bean *mbean;
        int ver;

        /* Parse and process a property. It'll locate the object and call the
         * setAttribute on it.
         */
        int (*setPropertyString) (struct jk_env * env, struct jk_config * cfg,
                                  char *name, char *value);

        /* Set an attribute for a jk object. This should be the
         * only method called to configure objects. The implementation
         * will update the underlying repository in addition to setting
         * the runtime value. Calling setAttribute on the object directly
         * will only set the runtime value.
         */
        int (*setProperty) (struct jk_env * env, struct jk_config * cfg,
                            struct jk_bean * target, char *name, char *value);

    /** Write the config file. If targetFile is NULL, it'll override the
     *  file that was used for reading
    */
        int (*save) (struct jk_env * env, struct jk_config * cfg,
                     char *targetFile);

    /** Check if the config changed, and update the workers.
     */
        int (*update) (struct jk_env * env, struct jk_config * cfg,
                       int *didReload);

    /** Process a node in config data
     */
        int (*processNode) (struct jk_env * env, struct jk_config * cfg,
                            char *node, int didReload);

        /* Private data */
        struct jk_pool *pool;
        void *_private;
        struct jk_workerEnv *workerEnv;
        struct jk_map *map;

        char *file;

        char *section;
        struct jk_map *cfgData;
        /* Only one thread can update the config
         */
        struct jk_mutex *cs;
        time_t mtime;
    };

    int jk2_config_setProperty(struct jk_env *env, struct jk_config *cfg,
                               struct jk_bean *mbean, char *name, char *val);

    int jk2_config_setPropertyString(struct jk_env *env,
                                     struct jk_config *cfg, char *name,
                                     char *value);

    int jk2_config_processConfigData(struct jk_env *env,
                                     struct jk_config *cfg, int firstTime);


    char *jk2_config_replaceProperties(struct jk_env *env, struct jk_map *m,
                                       struct jk_pool *resultPool,
                                       char *value);

    int jk2_config_file_read(struct jk_env *env, struct jk_map *m,
                             const char *file);

    int jk2_config_processNode(struct jk_env *env, struct jk_config *cfg,
                               char *name, int firstTime);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* JK_CONFIG_H */
