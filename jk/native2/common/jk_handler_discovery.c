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
#include "jk_msg.h"
#include "jk_logger.h"
#include "jk_service.h"
#include "jk_handler.h"
#include "jk_webapp.h"
#include "jk_workerEnv.h"
#include "jk_registry.h"

int JK_METHOD jk_handler_discovery_factory( jk_env_t *env, jk_pool_t *pool,
                                            void **result,
                                            const char *type, const char *name);

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
static int jk_handler_contextList(jk_env_t *env, jk_msg_t *msg,
                                  jk_ws_service_t *s,
                                  jk_endpoint_t *ae )
{
    char *vname;
    char *cname;
    char *uri;
    int                 cmd;
    int                 i,j;
    jk_webapp_t         *ci;
    jk_webapp_t         *webapp;  
    char                *buf;
    jk_workerEnv_t *we=ae->worker->workerEnv;
    
    /*     if (ajp_connection_tcp_get_message(ae, msg, l) != JK_TRUE) */
    /*         return JK_FALSE; */

    cmd=msg->getByte( env, msg );
    if (cmd != AJP14_CONTEXT_INFO_CMD) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "handler.discovery() - wrong cmd %d:%d\n",
                      AJP14_CONTEXT_INFO_CMD, cmd);
        return JK_FALSE;
    }

    for (;;) {
        vname  = (char *)msg->getString(env, msg);
        cname  = (char *)msg->getString(env, msg); 

        if (cname==NULL || strlen( cname ) == 0 ) {
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "End of contest list\n");
            return JK_TRUE;
        }   
        
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "contextList - Context: %s:%s \n",
                      vname, cname);

        webapp = we->createWebapp( env, we, vname, cname, NULL );
        if( webapp==NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                     "discoveryHandler: can't create webapp\n");
            return JK_FALSE;
        }
         
        for (;;) {
            uri  = (char *)msg->getString(env, msg);
            
            if (uri==NULL || strlen( uri ) == 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "No more URI for context %s", cname);
                break;
            }
            
            env->l->jkLog(env, env->l, JK_LOG_INFO,
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
static int jk_handler_contextState(jk_env_t *env, jk_msg_t *msg,
                                   jk_ws_service_t *s,
                                   jk_endpoint_t *ae)
{
    char                *vname;
    char                *cname;
    jk_webapp_t   *ci = NULL;

    /* get virtual name */
    vname  = (char *)msg->getString(env, msg);
    
    /* get context name */
    cname  = (char *)msg->getString(env, msg);
        
    if (! cname || ! strlen(cname)) {
        return JK_TRUE;
    }
        
    /* ci = context_find_base(c, cname); */
        
    if (! ci) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "Error ajp14_unmarshal_context_state_reply "
                      "- unknow context %s for virtual %s\n", 
                      cname, vname);
        return JK_FALSE;
    }
        
    ci->status = msg->getInt(env, msg);
        
    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "handler.contextState() context %s state=%d\n",
                  cname, ci->status);
    return JK_TRUE;
}


int JK_METHOD jk_handler_discovery_factory( jk_env_t *env, jk_pool_t *pool,
                                            void **result,
                                            const char *type, const char *name)
{
    jk_map_t *map;
    jk_handler_t *h;
    
    jk_map_default_create( env, &map, pool );
    *result=map;
    
    h=(jk_handler_t *)pool->calloc( env, pool, sizeof( jk_handler_t));
    h->name="contextInfo";
    h->messageId=AJP14_CONTEXT_INFO_CMD;
    h->callback=jk_handler_contextList;
    map->put( env, map, h->name, h, NULL );

    h=(jk_handler_t *)pool->calloc( env, pool, sizeof( jk_handler_t));
    h->name="contextState";
    h->messageId=AJP14_CONTEXT_STATE_REP_CMD;
    h->callback=jk_handler_contextState;
    map->put( env, map, h->name, h, NULL );

    return JK_TRUE;
}


