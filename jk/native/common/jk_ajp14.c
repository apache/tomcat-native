/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil-*- */
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
 * Description: Next generation bi-directional protocol handler.           *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/


#include "jk_global.h"
#include "jk_util.h"
#include "jk_map.h"
#include "jk_ajp_common.h"
#include "jk_ajp14.h"
#include "jk_md5.h"


/* 
 * Build the Shutdown Cmd
 *
 * +-----------------------+---------------------------------------+
 * | SHUTDOWN CMD (1 byte) | MD5 of RANDOM + SECRET KEY (32 chars) |
 * +-----------------------+---------------------------------------+
 *
 */

int ajp14_marshal_shutdown_into_msgb(jk_msg_buf_t       *msg,
                                     jk_login_service_t *s,
                                     jk_logger_t        *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into ajp14_marshal_shutdown_into_msgb\n");

    /* To be on the safe side */
    jk_b_reset(msg);

    /*
     * SHUTDOWN CMD
     */
    if (jk_b_append_byte(msg, AJP14_SHUTDOWN_CMD))
        return JK_FALSE;

    /*
     * COMPUTED-SEED
     */
     if (jk_b_append_bytes(msg, (const unsigned char *)s->computed_key, AJP14_COMPUTED_KEY_LEN)) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_shutdown_into_msgb - Error appending the COMPUTED MD5 bytes\n");
        return JK_FALSE;
    }

	return JK_TRUE;
}

/*
 * Decode the Shutdown Nok Command
 *
 * +----------------------+-----------------------+
 * | SHUTNOK CMD (1 byte) | FAILURE CODE (32bits) |
 * +----------------------+-----------------------+
 *
 */
int ajp14_unmarshal_shutdown_nok(jk_msg_buf_t *msg,
                                 jk_logger_t  *l)
{
    unsigned long   status;

    jk_log(l, JK_LOG_DEBUG, "Into ajp14_unmarshal_shutdown_nok\n");

    status = jk_b_get_long(msg);

    if (status == 0xFFFFFFFF) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_shutdown_nok - can't get failure code\n");
        return JK_FALSE;
    }

    jk_log(l, JK_LOG_INFO, "Can't shutdown servlet engine - code %08lx", status);
    return JK_TRUE;
}

/*
 * Build the Unknown Packet
 *
 * +-----------------------------+---------------------------------+------------------------------+
 * | UNKNOWN PACKET CMD (1 byte) | UNHANDLED MESSAGE SIZE (16bits) | UNHANDLED MESSAGE (bytes...) |
 * +-----------------------------+---------------------------------+------------------------------+
 *
 */

int ajp14_marshal_unknown_packet_into_msgb(jk_msg_buf_t		*msg,
                                           jk_msg_buf_t		*unk,
                                           jk_logger_t  	*l)
{
	jk_log(l, JK_LOG_DEBUG, "Into ajp14_marshal_unknown_packet_into_msgb\n");

	/* To be on the safe side */
	jk_b_reset(msg);

	/*
 	 * UNKNOWN PACKET CMD
	 */
	if (jk_b_append_byte(msg, AJP14_UNKNOW_PACKET_CMD))
		return JK_FALSE;

	/*
	 * UNHANDLED MESSAGE SIZE
	 */
	if (jk_b_append_int(msg, (unsigned short) jk_b_get_len(unk)))
		return JK_FALSE;

	/*
	 * UNHANDLED MESSAGE (Question : Did we have to send all the message or only part of)
	 *					 (           ie: only 1k max								    )
	 */
	if (jk_b_append_bytes(msg, jk_b_get_buff(unk), jk_b_get_len(unk))) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_unknown_packet_into_msgb - Error appending the UNHANDLED MESSAGE\n");
        return JK_FALSE;
    }

    return JK_TRUE;
}

