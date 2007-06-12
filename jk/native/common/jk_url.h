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
 * Description: URL manupilation subroutines header file. (mod_proxy)      *
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

/* for proxy_canonenc() */
enum enctype {
    enc_path, enc_search, enc_user, enc_fpath, enc_parm
};

#define JK_PROXYREQ_NONE 0         /**< No proxy */
#define JK_PROXYREQ_PROXY 1        /**< Standard proxy */
#define JK_PROXYREQ_REVERSE 2      /**< Reverse proxy */
#define JK_PROXYREQ_RESPONSE 3     /**< Origin response */

/*
 * Do a canonical encoding of the x url y contains the result
 * and should have a size of at least 3 * len + 1 bytes.
 */
char * jk_canonenc(char *y, const char *x, int len,
                   enum enctype t, int forcedec, int proxyreq);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* _JK_URL_H */
