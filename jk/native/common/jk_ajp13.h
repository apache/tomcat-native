/*
 * Copyright (c) 1997-1999 The Java Apache Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * 4. The names "Apache JServ", "Apache JServ Servlet Engine" and 
 *    "Java Apache Project" must not be used to endorse or promote products 
 *    derived from this software without prior written permission.
 *
 * 5. Products derived from this software may not be called "Apache JServ"
 *    nor may "Apache" nor "Apache JServ" appear in their names without 
 *    prior written permission of the Java Apache Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *    
 * THIS SOFTWARE IS PROVIDED BY THE JAVA APACHE PROJECT "AS IS" AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE JAVA APACHE PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Java Apache Group. For more information
 * on the Java Apache Project and the Apache JServ Servlet Engine project,
 * please see <http://java.apache.org/>.
 *
 */

/***************************************************************************
 * Description: Experimental bi-directionl protocol handler.               *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/
#ifndef JK_AJP13_H
#define JK_AJP13_H


#include "jk_service.h"
#include "jk_msg_buff.h"
#include "jk_mt.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define AJP13_DEF_HOST            	("localhost")
#define AJP13_DEF_PORT            	(8008)
#define AJP13_READ_BUF_SIZE         (8*1024)
#define AJP13_DEF_RETRY_ATTEMPTS    (1)
#define AJP13_DEF_CACHE_SZ          (1)
#define JK_INTERNAL_ERROR       	(-2)
#define AJP13_MAX_SEND_BODY_SZ      (DEF_BUFFER_SZ - 6)
#define AJP13_HEADER_LEN    		(4)
#define AJP13_HEADER_SZ_LEN 		(2)

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

struct jk_res_data {
    int         status;
    const char *msg;
    unsigned    num_headers;
    char      **header_names;
    char      **header_values;
};
typedef struct jk_res_data jk_res_data_t;

/*
 * Functions
 */
int ajp13_marshal_into_msgb(jk_msg_buf_t *msg,
                            jk_ws_service_t *s,
                            jk_logger_t *l);

int ajp13_unmarshal_response(jk_msg_buf_t *msg,
                             jk_res_data_t *d,
                             jk_pool_t *p,
                             jk_logger_t *l);


int ajp13_marshal_shutdown_into_msgb(jk_msg_buf_t *msg,
                                     jk_pool_t *p,
                                     jk_logger_t *l);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_AJP13_H */
