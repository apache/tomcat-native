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
 * Description: Next generation bi-directional protocol handler.           *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/


#include "jk_global.h"
#include "jk_util.h"
#include "jk_ajp14.h"

/*
 * Compute the MD5 with ENTROPY / SECRET KEY
 */

void ajp14_compute_md5(jk_login_service_t *s, jk_logger_t *l)
{
	jk_md5(s->entropy, s->secret_key, s->computed_key);

	jk_log(l, JK_LOG_DEBUG, "Into ajp14_compute_md5 (%s)\n", s->computed_key);
}

/*
 * Build the Login Init Command
 *
 * +-------------------------+---------------------------+---------------------------+
 * | LOGIN INIT CMD (1 byte) | NEGOCIATION DATA (32bits) | WEB SERVER INFO (CString) |
 * +-------------------------+---------------------------+---------------------------+
 *
 */

int ajp14_marshal_login_init_into_msgb(jk_msg_buf_t       *msg,
                                       jk_login_service_t *s,
                                       jk_logger_t        *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into ajp14_marshal_login_init_into_msgb\n");
    
    /* To be on the safe side */
    jk_b_reset(msg);

    /*
     * LOGIN
     */
    if (jk_b_append_byte(msg, AJP14_LOGINIT_CMD)) 
        return JK_FALSE;

	/*
     * NEGOCIATION FLAGS
     */
     if (jk_b_append_long(msg, s->negociation))
		return JK_FALSE;

	/*
	 * WEB-SERVER NAME
  	 */
     if (jk_b_append_string(msg, s->server_name)) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_login_init_into_msgb - Error appending the server_name string\n");
        return JK_FALSE;
    }

    return JK_TRUE;
}

/*
 * Build the Login Computed Command
 *
 * +-------------------------+---------------------------------------+
 * | LOGIN COMP CMD (1 byte) | MD5 of RANDOM + SECRET KEY (32 chars) |
 * +-------------------------+---------------------------------------+
 *
 */

int ajp14_marshal_login_comp_into_msgb(jk_msg_buf_t       *msg,
                                       jk_login_service_t *s,
                                       jk_logger_t        *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into ajp14_marshal_login_comp_into_msgb\n");
    
    /* To be on the safe side */
    jk_b_reset(msg);

    /*
     * LOGIN
     */
    if (jk_b_append_byte(msg, AJP14_LOGCOMP_CMD)) 
        return JK_FALSE;

	/*
	 * COMPUTED-SEED
  	 */
     if (jk_b_append_bytes(msg, s->computed_key, AJP14_COMPUTED_KEY_LEN)) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_login_comp_into_msgb - Error appending the COMPUTED MD5 bytes\n");
        return JK_FALSE;
    }

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
	if (jk_b_append_int(msg, jk_b_get_len(unk)))
		return JK_FALSE;

	/*
	 * UNHANDLED MESSAGE
	 */
	if (jk_b_append_bytes(msg, jk_b_get_buff(unk), jk_b_get_len(unk))) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_unknown_packet_into_msgb - Error appending the UNHANDLED MESSAGE\n");
        return JK_FALSE;
    }

    return JK_TRUE;
}
