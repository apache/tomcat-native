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
 * Description: ajpv1.3 worker header file                                 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_AJP13_WORKER_H
#define JK_AJP13_WORKER_H

#include "jk_pool.h"
#include "jk_connect.h"
#include "jk_util.h"
#include "jk_msg_buff.h"
#include "jk_ajp_common.h"
#include "jk_ajp13.h"
#include "jk_logger.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

#define JK_AJP13_WORKER_NAME ("ajp13")
#define JK_AJP13_WORKER_TYPE (2)

int JK_METHOD ajp13_worker_factory(jk_worker_t **w,
                                   const char *name, jk_logger_t *l);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* JK_AJP13_WORKER_H */
