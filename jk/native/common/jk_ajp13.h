/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */

/***************************************************************************
 * Description: Experimental bi-directionl protocol handler.               *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/
#ifndef JK_AJP13_H
#define JK_AJP13_H

#include "jk_ajp_common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define AJP13_PROTO					13
#define AJP13_WS_HEADER				0x1234
#define AJP13_SW_HEADER				0x4142	/* 'AB' */

#define AJP13_DEF_HOST            	("localhost")
#define AJP13_DEF_PORT            	(8009)
#define AJP13_READ_BUF_SIZE         (8*1024)
#define AJP13_DEF_CACHE_SZ          (1)
#define JK_INTERNAL_ERROR       	(-2)
#define JK_FATAL_ERROR              (-3)
#define AJP13_MAX_SEND_BODY_SZ      (DEF_BUFFER_SZ - 6)

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
 * Functions
 */

int ajp13_marshal_shutdown_into_msgb(jk_msg_buf_t *msg,
                                     jk_pool_t *p,
                                     jk_logger_t *l);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_AJP13_H */
