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
 * Description: URL manipulation subroutines header file. (mod_proxy)      *
 * Version:     $Revision: 500534 $                                        *
 ***************************************************************************/
#ifndef _JK_URL_H
#define _JK_URL_H

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_util.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

/*
 * Do a canonical encoding of the url x.
 * String y contains the result and is pre-allocated
 * for at least maxlen bytes, including a '\0' terminator.
 */
int jk_canonenc(const char *x, char *y, int maxlen);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* _JK_URL_H */
