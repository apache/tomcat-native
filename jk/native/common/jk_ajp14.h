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
#ifndef JK_AJP14_H
#define JK_AJP14_H


#include "jk_service.h"
#include "jk_msg_buff.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define AJP14_DEF_HOST            	("localhost")
#define AJP14_DEF_PORT            	(8010)
#define AJP14_READ_BUF_SIZE         (8*1024)
#define AJP14_DEF_RETRY_ATTEMPTS    (1)
#define AJP14_DEF_CACHE_SZ      	(1)
#define AJP14_MAX_SEND_BODY_SZ  	(DEF_BUFFER_SZ - 6)
#define AJP14_HEADER_LEN    		(4)
#define AJP14_HEADER_SZ_LEN 		(2)

/*
 * Initial Login Phase (web server -> servlet engine)
 */
#define AJP14_LOGINIT_CMD			(unsigned char)0x10

/*
 * Second Login Phase (servlet engine -> web server), md5 seed is received
 */
#define AJP14_LOGSEED_CMD			(unsigned char)0x11

/*
 * Third Login Phase (web server -> servlet engine), md5 of seed + secret is sent
 */
#define AJP14_LOGCOMP_CMD			(unsigned char)0x12

/*
 * Login Accepted (servlet engine -> web server)
 */
#define AJP14_LOGOK_CMD				(unsigned char)0x13

/*
 * Login Rejected (servlet engine -> web server), will be logged
 */
#define AJP14_LOGNOK_CMD			(unsigned char)0x14

/*
 * Context Query (web server -> servlet engine), which URI are handled by servlet engine ?
 */
#define AJP14_CONTEXT_QRY_CMD		(unsigned char)0x15

/*
 * Context Info (servlet engine -> web server), URI handled response
 */
#define AJP14_CONTEXT_INFO_CMD		(unsigned char)0x16

/* 
 * Context Update (servlet engine -> web server), status of context changed
 */
#define AJP14_CONTEXT_UPDATE_CMD	(unsigned char)0x17

/*
 * Servlet Engine Status (web server -> servlet engine), what's the status of the servlet engine ?
 */
#define AJP14_STATUS_CMD			(unsigned char)0x18

/*
 * Secure Shutdown command (web server -> servlet engine), please servlet stop yourself.
 */
#define AJP14_SHUTDOWN_CMD			(unsigned char)0x19

/*
 * Secure Shutdown command Accepted (servlet engine -> web server)
 */
#define AJP14_SHUTOK_CMD			(unsigned char)0x1A

/*
 * Secure Shutdown Rejected (servlet engine -> web server)
 */
#define AJP14_SHUTNOK_CMD			(unsigned char)0x1B

/*
 * Context Status (web server -> servlet engine), what's the status of the context ?
 */
#define AJP14_CONTEXT_STATE_CMD		(unsigned char)0x1C

/*
 * Context Status Reply (servlet engine -> web server), status of context
 */
#define AJP14_CONTEXT_STATE_REP_CMD	(unsigned char)0x1D

/*
 * Unknown Packet Reply (web server <-> servlet engine), when a packet couldn't be decoded
 */
#define AJP14_UNKNOW_PACKET_CMD		(unsigned char)0x1E


/*
 * Negociation flags 
 */

/*
 * web-server want context info after login
 */
#define AJP14_CONTEXT_INFO_NEG      0x80000000 

/*
 * web-server want context updates
 */
#define AJP14_CONTEXT_UPDATE_NEG    0x40000000 

/*
 * web-server want compressed stream 
 */
#define AJP14_GZIP_STREAM_NEG       0x20000000 

/*
 * web-server want crypted DES56 stream with secret key
 */
#define AJP14_DES56_STREAM_NEG      0x10000000 

/*
 * Extended info on server SSL vars
 */
#define AJP14_SSL_VSERVER_NEG       0x08000000 

/*
 *Extended info on client SSL vars
 */
#define AJP14_SSL_VCLIENT_NEG       0x04000000 

/*
 * Extended info on crypto SSL vars
 */
#define AJP14_SSL_VCRYPTO_NEG       0x02000000 

/*
 * Extended info on misc SSL vars
 */
#define AJP14_SSL_VMISC_NEG         0x01000000 

/*
 * mask of protocol supported 
 */
#define AJP14_PROTO_SUPPORT_AJPXX_NEG   0x00FF0000 

/* 
 * communication could use AJP14 
 */
#define AJP14_PROTO_SUPPORT_AJP14_NEG   0x00010000 

/*
 * communication could use AJP15 
 */
#define AJP14_PROTO_SUPPORT_AJP15_NEG   0x00020000 

/*
 * communication could use AJP16
 */
#define AJP14_PROTO_SUPPORT_AJP16_NEG   0x00040000 

/*
 * Some failure codes
 */
#define AJP14_BAD_KEY_ERR				0xFFFFFFFF
#define AJP14_ENGINE_DOWN_ERR			0xFFFFFFFE
#define AJP14_RETRY_LATER_ERR			0xFFFFFFFD
#define AJP14_SHUT_AUTHOR_FAILED_ERR    0xFFFFFFFC

/*
 * Some status codes
 */
#define AJP14_CONTEXT_DOWN       0x01
#define AJP14_CONTEXT_UP         0x02
#define AJP14_CONTEXT_OK         0x03

/* 
 * Misc defines
 */
#define AJP14_ENTROPY_SEED_LEN		32	/* we're using MD5 => 32 chars */
#define AJP14_COMPUTED_KEY_LEN		32  /* we're using MD5 also */

/*
 * The login structure
 */
typedef struct jk_login_service jk_login_service_t;

struct jk_login_service {

    /*
	 *  Pointer to web-server name
     */
    char * web_server_name;

	/*
	 * Pointer to servlet-engine name
	 */
	char * servlet_engine_name;

	/*
	 * Pointer to secret key
	 */
	char * secret_key;

	/*
	 * Received entropy seed
	 */
	char entropy[AJP14_ENTROPY_SEED_LEN + 1];

	/*
	 * Computed key
	 */
	char computed_key[AJP14_COMPUTED_KEY_LEN + 1];

    /*
     *  What we want to negociate
     */
    unsigned long negociation;

	/*
	 * What we received from servlet engine 
     */
	unsigned long negociated;
};                                

/*
 * functions defined here 
 */

int ajp14_marshal_login_init_into_msgb(jk_msg_buf_t       *msg,
                                       jk_login_service_t *s,
                                       jk_logger_t        *l);

int ajp14_marshal_login_comp_into_msgb(jk_msg_buf_t       *msg,
                                       jk_login_service_t *s,
                                       jk_logger_t        *l);

int ajp14_marshal_unknown_packet_into_msgb(jk_msg_buf_t     *msg,
                                           jk_msg_buf_t     *unk,
                                           jk_logger_t      *l);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_AJP14_H */
