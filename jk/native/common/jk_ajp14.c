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
 * Compute the MD5 with ENTROPY / SECRET KEY
 */

void ajp14_compute_md5(jk_login_service_t *s, 
                       jk_logger_t        *l)
{
	jk_md5((const unsigned char *)s->entropy, (const unsigned char *)s->secret_key, s->computed_key);

	jk_log(l, JK_LOG_DEBUG, "Into ajp14_compute_md5 (%s/%s) -> (%s)\n", s->entropy, s->secret_key, s->computed_key);
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
     if (jk_b_append_string(msg, s->web_server_name)) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_login_init_into_msgb - Error appending the web_server_name string\n");
        return JK_FALSE;
    }

    return JK_TRUE;
}


/*
 * Decode the Login Seed Command
 *
 * +-------------------------+---------------------------+
 * | LOGIN SEED CMD (1 byte) | MD5 of entropy (32 chars) |
 * +-------------------------+---------------------------+
 *
 */

int ajp14_unmarshal_login_seed(jk_msg_buf_t       *msg,
                               jk_login_service_t *s,
                               jk_logger_t        *l)
{
    if (jk_b_get_bytes(msg, (unsigned char *)s->entropy, AJP14_ENTROPY_SEED_LEN) < 0) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_login_seed - can't get seed\n");
        return JK_FALSE;
    }

	s->entropy[AJP14_ENTROPY_SEED_LEN] = 0;	/* Just to have a CString */
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
     if (jk_b_append_bytes(msg, (const unsigned char *)s->computed_key, AJP14_COMPUTED_KEY_LEN)) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_login_comp_into_msgb - Error appending the COMPUTED MD5 bytes\n");
        return JK_FALSE;
    }

    return JK_TRUE;
}


/*
 * Decode the LogOk Command
 *
 * +--------------------+------------------------+-------------------------------+
 * | LOGOK CMD (1 byte) | NEGOCIED DATA (32bits) | SERVLET ENGINE INFO (CString) |
 * +--------------------+------------------------+-------------------------------+
 *
 */

int ajp14_unmarshal_log_ok(jk_msg_buf_t       *msg,
                           jk_login_service_t *s,
                           jk_logger_t        *l)
{
	unsigned long 	nego;
	char *			sname;

	nego = jk_b_get_long(msg);

	if (nego == 0xFFFFFFFF) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_log_ok - can't get negociated data\n");
        return JK_FALSE;
    }

	sname = (char *)jk_b_get_string(msg);

	if (! sname) {
		jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_log_ok - can't get servlet engine name\n");
		return JK_FALSE;
	}
	
	if (s->servlet_engine_name)			/* take care of removing previously allocated data */
		free(s->servlet_engine_name);

	s->servlet_engine_name = strdup(sname);

	if (! s->servlet_engine_name) {
		jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_log_ok - can't malloc servlet engine name\n");
		return JK_FALSE;
	}

	return JK_TRUE;
}


/*
 * Decode the Log Nok Command 
 *
 * +---------------------+-----------------------+
 * | LOGNOK CMD (1 byte) | FAILURE CODE (32bits) |
 * +---------------------+-----------------------+
 *
 */

int ajp14_unmarshal_log_nok(jk_msg_buf_t *msg,
                            jk_logger_t  *l)
{
	unsigned long   status;

    jk_log(l, JK_LOG_DEBUG, "Into ajp14_unmarshal_log_nok\n");

	status = jk_b_get_long(msg);

    if (status == 0xFFFFFFFF) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_log_nok - can't get failure code\n");
        return JK_FALSE;
    }

	jk_log(l, JK_LOG_INFO, "Can't Log with servlet engine - code %08lx", status);
	return JK_TRUE;
}


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

/*
 * Build the Context Query Cmd (autoconf)
 *
 * +--------------------------+---------------------------------+
 * | CONTEXT QRY CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) |
 * +--------------------------+---------------------------------+
 *
 */

int ajp14_marshal_context_query_into_msgb(jk_msg_buf_t *msg,
										  char         *virtual,
										  jk_logger_t  *l)
{
	jk_log(l, JK_LOG_DEBUG, "Into ajp14_marshal_context_query_into_msgb\n");

	/* To be on the safe side */
	jk_b_reset(msg);

    /*
     * CONTEXT QUERY CMD
     */
    if (jk_b_append_byte(msg, AJP14_CONTEXT_QRY_CMD))
        return JK_FALSE;

    /*
     * VIRTUAL HOST CSTRING
     */
     if (jk_b_append_string(msg, virtual)) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_context_query_into_msgb - Error appending the virtual host string\n");
        return JK_FALSE;
    }

	return JK_TRUE;
}


/*
 * Decode the Context Info Cmd (Autoconf)
 *
 * The Autoconf feature of AJP14, let us know which URL/URI could
 * be handled by the servlet-engine
 *
 * +---------------------------+---------------------------------+----------------------------+-------------------------------+
 * | CONTEXT INFO CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) | CONTEXT NAME (CString (*)) | URL1 [\n] URL2 [\n] URL3 [\n] |
 * +---------------------------+---------------------------------+----------------------------+-------------------------------+
 *
 */

int ajp14_unmarshal_context_info(jk_msg_buf_t *msg,
								 jk_context_t *context,
                                 jk_logger_t  *l)
{
    char *sname;
	/* char *old; unused */
	int	 i;

    sname  = (char *)jk_b_get_string(msg);

    if (! sname) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info - can't get virtual hostname\n");
        return JK_FALSE;
    }

    if (context->virtual)         /* take care of removing previously allocated data */
        free(context->virtual);

    context->virtual = strdup(sname);

    if (! context->virtual) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info - can't malloc virtual hostname\n");
        return JK_FALSE;
    }

    sname  = (char *)jk_b_get_string(msg); 

    if (! sname) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info - can't get context\n");
        return JK_FALSE;
    }   

    if (context->cbase)       /* take care of removing previously allocated data */
        free(context->cbase);

    context->cbase = strdup(sname);
 
    if (! context->cbase) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info - can't malloc context\n");
        return JK_FALSE;
    }

	for (i = 1;; i++) {

		sname  = (char *)jk_b_get_string(msg);

		if (!sname) {
			jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info - can't get URL\n");
			return JK_FALSE;
		}

		if (! strlen(sname)) {
			jk_log(l, JK_LOG_INFO, "No more URI/URL (%d)", i);
			break;
		}

		jk_log(l, JK_LOG_INFO, "Got URL (%s) for virtualhost %s and base context %s", sname, context->virtual, context->cbase);
		context_add_uri(context, sname);
	}
	
    return JK_TRUE;
}


/*
 * Build the Context State Query Cmd
 *
 * +----------------------------+----------------------------------+----------------------------+
 * | CONTEXT STATE CMD (1 byte) |  VIRTUAL HOST NAME (CString (*)) | CONTEXT NAME (CString (*)) |
 * +----------------------------+----------------------------------+----------------------------+
 *
 */

int ajp14_marshal_context_state_into_msgb(jk_msg_buf_t *msg,
                                          jk_context_t *context,
                                          jk_logger_t  *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into ajp14_marshal_context_state_into_msgb\n");

    /* To be on the safe side */
    jk_b_reset(msg);

    /*
     * CONTEXT STATE CMD
     */
    if (jk_b_append_byte(msg, AJP14_CONTEXT_STATE_CMD))
        return JK_FALSE;

    /*
     * VIRTUAL HOST CSTRING
     */
     if (jk_b_append_string(msg, context->virtual)) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_context_state_into_msgb - Error appending the virtual host string\n");
        return JK_FALSE;
    }
    
    /*
     * CONTEXT CSTRING
     */
     if (jk_b_append_string(msg, context->cbase)) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_context_state_into_msgb - Error appending the context string\n");
        return JK_FALSE;
    }
    
    return JK_TRUE;
}


/*
 * Decode the Context State Reply Cmd
 *
 * +----------------------------------+---------------------------------+----------------------------+------------------+
 * | CONTEXT STATE REPLY CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) | CONTEXT NAME (CString (*)) | UP/DOWN (1 byte) |
 * +----------------------------------+---------------------------------+----------------------------+------------------+
 *
 */

int ajp14_unmarshal_context_state_reply(jk_msg_buf_t *msg,
										jk_context_t *context,
                           				jk_logger_t  *l)
{
    char *sname;

	sname  = (char *)jk_b_get_string(msg);

    if (! sname) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_state_reply - can't get virtual hostname\n");
        return JK_FALSE;
    }

    if (context->virtual)         /* take care of removing previously allocated data */
        free(context->virtual);

    context->virtual = strdup(sname);

    if (! context->virtual) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_state_reply - can't malloc virtual hostname\n");
        return JK_FALSE;
    }

	sname  = (char *)jk_b_get_string(msg);

	if (! sname) {
		jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_state_reply - can't get context\n");
		return JK_FALSE;
	}	

	if (context->cbase)		/* take care of removing previously allocated data */
		free(context->cbase);

	context->cbase = strdup(sname);

	if (! context->cbase) {
		jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_state_reply - can't malloc context\n");
		return JK_FALSE;
	}

	context->status = jk_b_get_int(msg);

    return JK_TRUE;
}

/*
 * Decode the Context Update Cmd
 * 
 * +-----------------------------+---------------------------------+----------------------------+------------------+
 * | CONTEXT UPDATE CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) | CONTEXT NAME (CString (*)) | UP/DOWN (1 byte) |
 * +-----------------------------+---------------------------------+----------------------------+------------------+
 * 
 */

int ajp14_unmarshal_context_update_cmd(jk_msg_buf_t *msg,
                                       jk_context_t *context,
                                       jk_logger_t  *l)
{
	return (ajp14_unmarshal_context_state_reply(msg, context, l));
}

