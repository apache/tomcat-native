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
 * Description: AJP14 Discovery handler
 * Author:      Henri Gomez <hgomez@slib.fr>
 * Version:     $Revision$                                          
 */

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_env.h"
#include "jk_msg_buff.h"
#include "jk_logger.h"
#include "jk_service.h"
#include "jk_handler.h"
#include "jk_webapp.h"
#include "jk_workerEnv.h"

int JK_METHOD jk_handler_discovery_factory( jk_env_t *env, jk_pool_t *pool, void **result,
                                            const char *type, const char *name);

int jk_handler_discovery_sendDiscovery(jk_endpoint_t *ae,
                                       jk_workerEnv_t *we,
                                       jk_logger_t    *l);

int jk_handler_discovery_handleContextList(jk_endpoint_t  *ae,
                                           jk_workerEnv_t *we,
                                           jk_msg_buf_t    *msg,
                                           jk_logger_t     *l);

int jk_handler_discovery_sendGetContextaState(jk_msg_buf_t *msg,
                                              jk_workerEnv_t *c,
                                              char *vhost,
                                              char *cname,
                                              jk_logger_t  *l);

int jk_handler_discovery_handleContextState(jk_msg_buf_t *msg,
                                            jk_workerEnv_t *c,
                                            jk_logger_t  *l);

/*
 * Context Query (web server -> servlet engine), which URI are handled by servlet engine ?
 */
#define AJP14_CONTEXT_QRY_CMD	(unsigned char)0x15

/*
 * Context Info (servlet engine -> web server), URI handled response
 */
#define AJP14_CONTEXT_INFO_CMD	(unsigned char)0x16

/* 
 * Context Update (servlet engine -> web server), status of context changed
 */
#define AJP14_CONTEXT_UPDATE_CMD (unsigned char)0x17

/*
 * Context Status (web server -> servlet engine), what's
 * the status of the context ?
 */
#define AJP14_CONTEXT_STATE_CMD		(unsigned char)0x1C

/*
 * Context Status Reply (servlet engine -> web server), status of context
 */
#define AJP14_CONTEXT_STATE_REP_CMD	(unsigned char)0x1D

#define MAX_URI_SIZE    512

static int jk_handler_discovery_init( jk_worker_t *w );

/* ==================== Constructor and impl. ==================== */

int JK_METHOD jk_handler_discovery_factory( jk_env_t *env, jk_pool_t *pool,
                                            void **result,
                                            const char *type, const char *name)
{
    jk_handler_t *h=(jk_handler_t *)pool->alloc( pool, sizeof( jk_handler_t));

    h->init=jk_handler_discovery_init;
    *result=h;
    return JK_TRUE;
}


static int jk_handler_discovery_init( jk_worker_t *w ) {
    return JK_TRUE;
}


/** Send a 'discovery' message. This is a request for tomcat to
 *  report all configured webapps and mappings.
 *
 * Build the Context Query Cmd (autoconf)
 *
 * +--------------------------+-
 * | CONTEXT QRY CMD (1 byte) | 
 * +--------------------------+-
 *
 */
int jk_handler_discovery_sendDiscovery(jk_endpoint_t *ae,
                                       jk_workerEnv_t *we,
                                       jk_logger_t    *l)
{
    jk_pool_t     *p = ae->pool;
    jk_msg_buf_t  *msg;
    int           rc=JK_FALSE;
    int                 cmd;
    int                 i,j;
    jk_login_service_t  *jl = ae->worker->login;
    jk_webapp_t   *ci;
    jk_webapp_t         *webapp;  
    char                *buf;


    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14:discovery\n");
    
    msg = jk_b_new(p);
    jk_b_set_buffer_size(msg, DEF_BUFFER_SZ);
    jk_b_reset(msg);
    
    /*
     * CONTEXT QUERY CMD
     */
    if (jk_b_append_byte(msg, AJP14_CONTEXT_QRY_CMD))
        return JK_FALSE;
    
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14:discovery - send query\n");

    if (ajp_connection_tcp_send_message(ae, msg, l) != JK_TRUE)
        return JK_FALSE;
    
    jk_b_reset(msg);
    
    return rc;
}

/* -------------------- private utils/marshaling -------------------- */

/*
 * Decode the ContextList Cmd (Autoconf)
 *
 * The Autoconf feature of AJP14, let us know which URL/URI could
 * be handled by the servlet-engine
 *
 * +---------------------------+---------------------------------+---------
 * | CONTEXT INFO CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) | 
 * +---------------------------+---------------------------------+---------
 *
 *-------------------+-------------------------------+-----------+
 *CONTEXT NAME (CString (*)) | URL1 [\n] URL2 [\n] URL3 [\n] | NEXT CTX. |
 *-------------------+-------------------------------+-----------+
 */
int jk_handler_discovery_handleContextList(jk_endpoint_t  *ae,
                                           jk_workerEnv_t *we,
                                           jk_msg_buf_t    *msg,
                                           jk_logger_t     *l)
{
    char *vname;
    char *cname;
    char *uri;
    int                 cmd;
    int                 i,j;
    jk_login_service_t  *jl = ae->worker->login;
    jk_webapp_t   *ci;
    jk_webapp_t         *webapp;  
    char                *buf;

    /*     if (ajp_connection_tcp_get_message(ae, msg, l) != JK_TRUE) */
    /*         return JK_FALSE; */
    
    if ((cmd = jk_b_get_byte(msg)) != AJP14_CONTEXT_INFO_CMD) {
        l->jkLog(l, JK_LOG_ERROR, "Error ajp14:discovery - wrong cmd %d:%d\n",
                 AJP14_CONTEXT_INFO_CMD, cmd);
        return JK_FALSE;
    }

    for (;;) {
        vname  = (char *)jk_b_get_string(msg);
        cname  = (char *)jk_b_get_string(msg); 

        if (cname==NULL || strlen( cname ) == 0 ) {
            l->jkLog(l, JK_LOG_DEBUG, "End of contest list\n");
            return JK_TRUE;
        }   
        
        l->jkLog(l, JK_LOG_DEBUG,
                 "contextList - Context: %s:%s \n", vname, cname);

        webapp = we->createWebapp( we, vname, cname, NULL );
        if( webapp==NULL ) {
            l->jkLog(l, JK_LOG_ERROR,
                     "discoveryHandler: can't create webapp\n");
            return JK_FALSE;
        }
         
        for (;;) {
            uri  = (char *)jk_b_get_string(msg);
            
            if (uri==NULL || strlen( uri ) == 0 ) {
                l->jkLog(l, JK_LOG_DEBUG, "No more URI for context %s", cname);
                break;
            }
            
            l->jkLog(l, JK_LOG_INFO,
                   "Got URI %s for %s:%s\n", uri, vname, cname);
            
/*             if (context_add_uri(c, cname, uri) == JK_FALSE) { */
/*                 l->jkLog(l, JK_LOG_ERROR, */
/*                        "Error ajp14_unmarshal_context_info - " */
/*                        "can't add/set uri (%s) for context %s\n", uri, cname); */
/*                 return JK_FALSE; */
/*             }  */
        }
    }
    return JK_TRUE;
}


/*
 * Build the Context State Query Cmd
 *
 * We send the list of contexts where we want to know state,
 * empty string end context list*
 * If cname is set, only ask about THIS context
 *
 * +----------------------------+----------------------------------+----------
 * | CONTEXT STATE CMD (1 byte) |  VIRTUAL HOST NAME  | CONTEXT NAME
 *                              |   (CString (*))     |  (CString (*)) 
 * +----------------------------+----------------------------------+----------
 *
 */
int jk_handler_discovery_sendGetContextaState(jk_msg_buf_t *msg,
                                              jk_workerEnv_t *we,
                                              char *vhost, 
                                              char  *cname,
                                              jk_logger_t  *l)
{
    jk_webapp_t *ci;
    int                i;
    
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14_marshal_context_state_into_msgb\n");
    
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
     if (jk_b_append_string(msg, vhost)) {
        l->jkLog(l, JK_LOG_ERROR, "Error ajp14_marshal_context_state_into_msgb"
               "- Error appending the virtual host string\n");
        return JK_FALSE;
     }
     
     /*
      * CONTEXT CSTRING
      */
     if (jk_b_append_string(msg, cname )) {
         l->jkLog(l, JK_LOG_ERROR,
                  "Error ajp14_marshal_context_state_into_msgb"
                  "- Error appending the context string %s\n", cname);
         return JK_FALSE;
     }
     
     return JK_TRUE;
}


/*
 * Decode the Context State Reply Cmd
 *
 * We get update of contexts list, empty string end context list*
 *
 * +----------------------------------+---------------------------------+----
 * | CONTEXT STATE REPLY CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) | 
 * +----------------------------------+---------------------------------+----
 *
 *------------------------+------------------+----+
 *CONTEXT NAME (CString (*)) | UP/DOWN (1 byte) | .. |
 * ------------------------+------------------+----+
 */
int jk_handler_discovery_handleContextState(jk_msg_buf_t *msg,
                                            jk_workerEnv_t *c,
                                            jk_logger_t  *l)
{
    char                *vname;
    char                *cname;
    jk_webapp_t   *ci = NULL;

    /* get virtual name */
    vname  = (char *)jk_b_get_string(msg);
    
    /* get context name */
    cname  = (char *)jk_b_get_string(msg);
        
    if (! cname || ! strlen(cname)) {
        return JK_TRUE;
    }
        
    /* ci = context_find_base(c, cname); */
        
    if (! ci) {
        l->jkLog(l, JK_LOG_ERROR,
                 "Error ajp14_unmarshal_context_state_reply "
                 "- unknow context %s for virtual %s\n", 
                 cname, vname);
        return JK_FALSE;
    }
        
    ci->status = jk_b_get_int(msg);
        
    l->jkLog(l, JK_LOG_DEBUG, "ajp14_unmarshal_context_state_reply "
             "- updated context %s to state %d\n", cname, ci->status);
    return JK_TRUE;
}







