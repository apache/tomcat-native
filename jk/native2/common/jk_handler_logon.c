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

/**
 * Description: AJP14 Login handler
 * Author:      Henri Gomez <hgomez@slib.fr>
 * Version:     $Revision$                                          
 */

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_msg_buff.h"
#include "jk_md5.h"
#include "jk_logger.h"
#include "jk_service.h"
#include "jk_env.h"
#include "jk_handler.h"

/* Private definitions */

/*
 * Initial Login Phase (web server -> servlet engine)
 */
#define AJP14_LOGINIT_CMD	(unsigned char)0x10

/*
 * Second Login Phase (servlet engine -> web server), md5 seed is received
 */
#define AJP14_LOGSEED_CMD	(unsigned char)0x11

/*
 * Third Login Phase (web server -> servlet engine), md5 of seed + secret is sent
 */
#define AJP14_LOGCOMP_CMD	(unsigned char)0x12

/*
 * Login Accepted (servlet engine -> web server)
 */
#define AJP14_LOGOK_CMD		(unsigned char)0x13

/*
 * Login Rejected (servlet engine -> web server), will be logged
 */
#define AJP14_LOGNOK_CMD	(unsigned char)0x14

int JK_METHOD jk_handler_logon_factory( jk_env_t *env, void **result,
                                        char *type, char *name);


/* Private functions */

static void ajp14_compute_md5(jk_login_service_t *s, 
                              jk_logger_t *l);

static int ajp14_marshal_login_init_into_msgb(jk_msg_buf_t *msg, 
                                              jk_login_service_t *s, 
                                              jk_logger_t *l);

static int ajp14_unmarshal_login_seed(jk_msg_buf_t *msg, 
                                      jk_login_service_t *s, 
                                      jk_logger_t *l);

static  int ajp14_marshal_login_comp_into_msgb(jk_msg_buf_t *msg, 
                                               jk_login_service_t *s, 
                                               jk_logger_t *l);

static int ajp14_unmarshal_log_ok(jk_msg_buf_t *msg, 
                                  jk_login_service_t *s, 
                                  jk_logger_t *l);

static int ajp14_unmarshal_log_nok(jk_msg_buf_t *msg, 
                                   jk_logger_t *l);


static int jk_handler_logon_logon(jk_endpoint_t *ae, jk_logger_t    *l);
static int jk_handler_logon_init( jk_worker_t *w );

/* ==================== Impl =================== */

int JK_METHOD jk_handler_logon_factory( jk_env_t *env, void **result,
                                       char *type, char *name)
{
    jk_handler_t *h=(jk_handler_t *)malloc( sizeof( jk_handler_t));

    h->init=jk_handler_logon_init;
    *result=h;
    return JK_TRUE;
}


static int jk_handler_logon_init( jk_worker_t *w ) {
    w->logon= jk_handler_logon_logon; 
    return JK_TRUE;
}


/* 
 * AJP14 Logon Phase 
 *
 * INIT + REPLY / NEGO + REPLY 
 */
static int handle_logon(jk_endpoint_t *ae,
                        jk_msg_buf_t	   *msg,
                        jk_logger_t     *l)
{
    int	cmd;
    
    jk_login_service_t *jl = ae->worker->login;
    
    ajp14_marshal_login_init_into_msgb(msg, jl, l);
    
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14:logon - send init\n");
    
    if (ajp_connection_tcp_send_message(ae, msg, l) != JK_TRUE)	
        return JK_FALSE;
    
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14:logon - wait init reply\n");
    
    jk_b_reset(msg);
    
    if (ajp_connection_tcp_get_message(ae, msg, l) != JK_TRUE)
        return JK_FALSE;
    
    if ((cmd = jk_b_get_byte(msg)) != AJP14_LOGSEED_CMD) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14:logon: awaited command %d, received %d\n",
               AJP14_LOGSEED_CMD, cmd);
        return JK_FALSE;
    }
    
    if (ajp14_unmarshal_login_seed(msg, jl, l) != JK_TRUE)
        return JK_FALSE;
    
    l->jkLog(l, JK_LOG_DEBUG,
           "Into ajp14:logon - received entropy %s\n", jl->entropy);
    
    ajp14_compute_md5(jl, l);
    
    if (ajp14_marshal_login_comp_into_msgb(msg, jl, l) != JK_TRUE)
        return JK_FALSE;
    
    if (ajp_connection_tcp_send_message(ae, msg, l) != JK_TRUE) 
        return JK_FALSE;
    
    jk_b_reset(msg);
    
    if (ajp_connection_tcp_get_message(ae, msg, l) != JK_TRUE)
        return JK_FALSE;
    
    switch (jk_b_get_byte(msg)) {
        
    case AJP14_LOGOK_CMD  :	
        if (ajp14_unmarshal_log_ok(msg, jl, l) == JK_TRUE) {
            l->jkLog(l, JK_LOG_DEBUG,
                   "Successfully connected to servlet-engine %s\n",
                   jl->servlet_engine_name);
            return JK_TRUE;
        }
        break;
        
    case AJP14_LOGNOK_CMD :
        ajp14_unmarshal_log_nok(msg, l);
        break;
    }
    
    return JK_FALSE;
}


static int jk_handler_logon_logon(jk_endpoint_t *ae,
                           jk_logger_t    *l)
{
    jk_pool_t     *p = &ae->pool;
    jk_msg_buf_t  *msg;
    int 	  rc;
    
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14:logon\n");

    msg = jk_b_new(p);
    jk_b_set_buffer_size(msg, DEF_BUFFER_SZ);
    
    if ((rc = handle_logon(ae, msg, l)) == JK_FALSE) 
        ajp_close_endpoint(ae, l);
    
    return rc;
}


/* -------------------- private utils/marshaling -------------------- */

/*
 * Compute the MD5 with ENTROPY / SECRET KEY
 */
static void ajp14_compute_md5(jk_login_service_t *s, 
                              jk_logger_t        *l)
{
    jk_md5((const unsigned char *)s->entropy,
           (const unsigned char *)s->secret_key, s->computed_key);

    l->jkLog(l, JK_LOG_DEBUG,
           "Into ajp14_compute_md5 (%s/%s) -> (%s)\n",
           s->entropy, s->secret_key, s->computed_key);
}

/*
 * Build the Login Init Command
 *
 * +-------------------------+---------------------------+-------------------
 * | LOGIN INIT CMD (1 byte) | NEGOCIATION DATA (32bits) | WEB SERVER INFO
 * |                         |                           |    (CString) 
 * +-------------------------+---------------------------+-------------------
 *
 */
static int ajp14_marshal_login_init_into_msgb(jk_msg_buf_t       *msg,
                                              jk_login_service_t *s,
                                              jk_logger_t        *l)
{
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14_marshal_login_init_into_msgb\n");
    
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
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14_marshal_login_init_into_msgb "
               "- Error appending the web_server_name string\n");
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
static int ajp14_unmarshal_login_seed(jk_msg_buf_t       *msg,
                                      jk_login_service_t *s,
                                      jk_logger_t        *l)
{
    if (jk_b_get_bytes(msg, (unsigned char *)s->entropy,
                       AJP14_ENTROPY_SEED_LEN) < 0) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14_unmarshal_login_seed - can't get seed\n");
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
static int ajp14_marshal_login_comp_into_msgb(jk_msg_buf_t       *msg,
                                              jk_login_service_t *s,
                                              jk_logger_t        *l)
{
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14_marshal_login_comp_into_msgb\n");
    
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
     if (jk_b_append_bytes(msg, (const unsigned char *)s->computed_key,
                           AJP14_COMPUTED_KEY_LEN)) {
         l->jkLog(l, JK_LOG_ERROR,
                "Error ajp14_marshal_login_comp_into_msgb "
                " - Error appending the COMPUTED MD5 bytes\n");
        return JK_FALSE;
    }

    return JK_TRUE;
}


/*
 * Decode the LogOk Command
 *
 * +--------------------+------------------------+---------------------------
 * | LOGOK CMD (1 byte) | NEGOCIED DATA (32bits) | SERVLET ENGINE INFO(CString)
 * +--------------------+------------------------+---------------------------
 *
 */
static int ajp14_unmarshal_log_ok(jk_msg_buf_t       *msg,
                                  jk_login_service_t *s,
                                  jk_logger_t        *l)
{
    unsigned long 	nego;
    char *			sname;
    
    nego = jk_b_get_long(msg);
    
    if (nego == 0xFFFFFFFF) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14_unmarshal_log_ok - can't get negociated data\n");
        return JK_FALSE;
    }
    
    sname = (char *)jk_b_get_string(msg);
    
    if (! sname) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14_unmarshal_log_ok - "
               "can't get servlet engine name\n");
        return JK_FALSE;
    }

    /* take care of removing previously allocated data */
    if (s->servlet_engine_name)			
        free(s->servlet_engine_name);
    
    s->servlet_engine_name = strdup(sname);
    
    if (! s->servlet_engine_name) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14_unmarshal_log_ok - "
               " can't malloc servlet engine name\n");
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

static int ajp14_unmarshal_log_nok(jk_msg_buf_t *msg,
                                   jk_logger_t  *l)
{
    unsigned long   status;
    
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14_unmarshal_log_nok\n");
    
    status = jk_b_get_long(msg);
    
    if (status == 0xFFFFFFFF) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14_unmarshal_log_nok - can't get failure code\n");
        return JK_FALSE;
    }
    
    l->jkLog(l, JK_LOG_INFO,
           "Can't Log with servlet engine - code %08lx", status);
    return JK_TRUE;
}

