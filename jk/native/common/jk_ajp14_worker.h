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
 * Description: ajpv14 worker header file                                  *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_AJP14_WORKER_H
#define JK_AJP14_WORKER_H

#include "jk_pool.h"
#include "jk_connect.h"
#include "jk_util.h"
#include "jk_msg_buff.h"
#include "jk_ajp13.h"
#include "jk_ajp14.h"
#include "jk_logger.h"
#include "jk_service.h"
#include "jk_ajp13_worker.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

#define JK_AJP14_WORKER_NAME ("ajp14")
#define JK_AJP14_WORKER_TYPE (3)

int JK_METHOD ajp14_worker_factory(jk_worker_t **w,
                                   const char *name, jk_logger_t *l);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* JK_AJP14_WORKER_H */
