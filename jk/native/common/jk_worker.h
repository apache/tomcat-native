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
 * Description: Workers controller header file                             *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           * 
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_WORKER_H
#define JK_WORKER_H

#include "jk_logger.h"
#include "jk_service.h"
#include "jk_map.h"
#include "jk_uri_worker_map.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

    int wc_open(jk_map_t *init_data, jk_worker_env_t *we, jk_logger_t *l);

    void wc_close(jk_logger_t *l);

    jk_worker_t *wc_get_worker_for_name(const char *name, jk_logger_t *l);

    int wc_create_worker(const char *name,
                         jk_map_t *init_data,
                         jk_worker_t **rc,
                         jk_worker_env_t *we, jk_logger_t *l);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* JK_WORKER_H */
