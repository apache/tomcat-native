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
 * Description: Experimental bi-directionl protocol handler.               *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/
#ifndef JK_AJP13_H
#define JK_AJP13_H

#include "jk_ajp_common.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

#define AJP13_PROTO                 13
#define AJP13_WS_HEADER             0x1234
#define AJP13_SW_HEADER             0x4142      /* 'AB' */

#define AJP13_DEF_HOST              ("localhost")
#define AJP13_DEF_PORT              (8009)
#define AJP13_READ_BUF_SIZE         (8*1024)
#define AJP13_DEF_CACHE_SZ          (1)
#define JK_INTERNAL_ERROR           (-2)
#define JK_FATAL_ERROR              (-3)
#define JK_CLIENT_ERROR             (-4)
#define AJP13_MAX_SEND_BODY_SZ      (DEF_BUFFER_SZ - 6)
#define AJP13_DEF_TIMEOUT           (0) /* Idle timout for pooled connections */

/*
 * Message does not have a response (for example, JK_AJP13_END_RESPONSE)
 */
#define JK_AJP13_ERROR              -1
/*
 * Message does not have a response (for example, JK_AJP13_END_RESPONSE)
 */
#define JK_AJP13_NO_RESPONSE        0
/*
 * Message have a response.
 */
#define JK_AJP13_HAS_RESPONSE       1

/*
 * Forward a request from the web server to the servlet container.
 */
#define JK_AJP13_FORWARD_REQUEST    (unsigned char)2

/*
 * Write a body chunk from the servlet container to the web server
 */
#define JK_AJP13_SEND_BODY_CHUNK    (unsigned char)3

/*
 * Send response headers from the servlet container to the web server.
 */
#define JK_AJP13_SEND_HEADERS       (unsigned char)4

/*
 * Marks the end of response.
 */
#define JK_AJP13_END_RESPONSE       (unsigned char)5

/*
 * Marks the end of response.
 */
#define JK_AJP13_GET_BODY_CHUNK     (unsigned char)6

/*
 * Asks the container to shutdown
 */
#define JK_AJP13_SHUTDOWN           (unsigned char)7

/*
 * Told container to take control (secure login phase)
 */
#define AJP13_PING_REQUEST          (unsigned char)8

/*
 * Check if the container is alive
 */
#define AJP13_CPING_REQUEST          (unsigned char)10

/*
 * Reply from the container to alive request
 */
#define AJP13_CPONG_REPLY            (unsigned char)9



/*
 * Functions
 */

    int ajp13_marshal_shutdown_into_msgb(jk_msg_buf_t *msg,
                                         jk_pool_t *p, jk_logger_t *l);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* JK_AJP13_H */
