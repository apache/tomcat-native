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
 * Description: Next generation bi-directional protocol handler.           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
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
 * +---------------------------+---------------------------------+----------------------------+-------------------------------+-----------+
 * | CONTEXT INFO CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) | CONTEXT NAME (CString (*)) | URL1 [\n] URL2 [\n] URL3 [\n] | NEXT CTX. |
 * +---------------------------+---------------------------------+----------------------------+-------------------------------+-----------+
 */

int ajp14_unmarshal_context_info(jk_msg_buf_t *msg,
								 jk_context_t *c,
                                 jk_logger_t  *l)
{
    char *vname;
    char *cname;
    char *uri;

    vname  = (char *)jk_b_get_string(msg);

    jk_log(l, JK_LOG_DEBUG, "ajp14_unmarshal_context_info - get virtual %s for virtual %s\n", vname, c->virtual);

    if (! vname) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info - can't get virtual hostname\n");
        return JK_FALSE;
    }

    /* Check if we get the correct virtual host */
    if (c->virtual != NULL && 
	vname != NULL &&
	strcmp(c->virtual, vname)) {
        /* set the virtual name, better to add to a virtual list ? */
        
        if (context_set_virtual(c, vname) == JK_FALSE) {
            jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info - can't malloc virtual hostname\n");
            return JK_FALSE;
        }
    }

    for (;;) {
    
        cname  = (char *)jk_b_get_string(msg); 

        if (! cname) {
            jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info - can't get context\n");
            return JK_FALSE;
        }   

        jk_log(l, JK_LOG_DEBUG, "ajp14_unmarshal_context_info - get context %s for virtual %s\n", cname, vname);

        /* grab all contexts up to empty one which indicate end of contexts */
        if (! strlen(cname)) 
            break;

        /* create new context base (if needed) */

        if (context_add_base(c, cname) == JK_FALSE) {
            jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info - can't add/set context %s\n", cname);
            return JK_FALSE;
        }

	    for (;;) {

            uri  = (char *)jk_b_get_string(msg);

		    if (!uri) {
                jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info - can't get URI\n");
                return JK_FALSE;
		    }

		    if (! strlen(uri)) {
			    jk_log(l, JK_LOG_DEBUG, "No more URI for context %s", cname);
			    break;
		    }

		    jk_log(l, JK_LOG_INFO, "Got URI (%s) for virtualhost %s and context %s\n", uri, vname, cname);

		    if (context_add_uri(c, cname, uri) == JK_FALSE) {
                jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info - can't add/set uri (%s) for context %s\n", uri, cname);
                return JK_FALSE;
            } 
	    }
	}

    return JK_TRUE;
}


/*
 * Build the Context State Query Cmd
 *
 * We send the list of contexts where we want to know state, empty string end context list*
 * If cname is set, only ask about THIS context
 *
 * +----------------------------+----------------------------------+----------------------------+----+
 * | CONTEXT STATE CMD (1 byte) |  VIRTUAL HOST NAME (CString (*)) | CONTEXT NAME (CString (*)) | .. |
 * +----------------------------+----------------------------------+----------------------------+----+
 *
 */

int ajp14_marshal_context_state_into_msgb(jk_msg_buf_t *msg,
                                          jk_context_t *c,
                                          char         *cname,
                                          jk_logger_t  *l)
{
    jk_context_item_t *ci;
    int                i;

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
     if (jk_b_append_string(msg, c->virtual)) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_context_state_into_msgb - Error appending the virtual host string\n");
        return JK_FALSE;
    }
    
    if (cname) {

        ci = context_find_base(c, cname);

        if (! ci) {
            jk_log(l, JK_LOG_ERROR, "Warning ajp14_marshal_context_state_into_msgb - unknown context %s\n", cname);
            return JK_FALSE;
        }

        /*
         * CONTEXT CSTRING
         */

        if (jk_b_append_string(msg, cname )) {
            jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_context_state_into_msgb - Error appending the context string %s\n", cname);
            return JK_FALSE;
        }
    }
    else { /* Grab all contexts name */

        for (i = 0; i < c->size; i++) {
        
            /*
             * CONTEXT CSTRING
             */
            if (jk_b_append_string(msg, c->contexts[i]->cbase )) {
                jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_context_state_into_msgb - Error appending the context string\n");
                return JK_FALSE;
            }
        }
    }

    /* End of context list, an empty string */ 

    if (jk_b_append_string(msg, "")) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_marshal_context_state_into_msgb - Error appending end of contexts\n");
        return JK_FALSE;
    }

    return JK_TRUE;
}


/*
 * Decode the Context State Reply Cmd
 *
 * We get update of contexts list, empty string end context list*
 *
 * +----------------------------------+---------------------------------+----------------------------+------------------+----+
 * | CONTEXT STATE REPLY CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) | CONTEXT NAME (CString (*)) | UP/DOWN (1 byte) | .. |
 * +----------------------------------+---------------------------------+----------------------------+------------------+----+
 *
 */

int ajp14_unmarshal_context_state_reply(jk_msg_buf_t *msg,
										jk_context_t *c,
                           				jk_logger_t  *l)
{
    char                *vname;
    char                *cname;
    jk_context_item_t   *ci;

    /* get virtual name */
	vname  = (char *)jk_b_get_string(msg);

    if (! vname) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_state_reply - can't get virtual hostname\n");
        return JK_FALSE;
    }

    /* Check if we speak about the correct virtual */
    if (strcmp(c->virtual, vname)) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_state_reply - incorrect virtual %s instead of %s\n",
               vname, c->virtual);
        return JK_FALSE;
    }
 
    for (;;) {

        /* get context name */
	    cname  = (char *)jk_b_get_string(msg);

	    if (! cname) {
		    jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_state_reply - can't get context\n");
		    return JK_FALSE;
	    }	

        if (! strlen(cname))
            break;

        ci = context_find_base(c, cname);

        if (! ci) {
            jk_log(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_state_reply - unknow context %s for virtual %s\n", 
                   cname, vname);
            return JK_FALSE;
        }

	    ci->status = jk_b_get_int(msg);

        jk_log(l, JK_LOG_DEBUG, "ajp14_unmarshal_context_state_reply - updated context %s to state %d\n", cname, ci->status);
    }

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
                                       jk_context_t *c,
                                       jk_logger_t  *l)
{
	return (ajp14_unmarshal_context_state_reply(msg, c, l));
}

