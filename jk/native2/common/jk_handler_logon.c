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
#include "jk_msg.h"
#include "jk_md5.h"
#include "jk_logger.h"
#include "jk_service.h"
#include "jk_env.h"
#include "jk_handler.h"
#include "jk_registry.h"

/* Private definitions */

/*
 * Third Login Phase (web server -> servlet engine), md5 of seed + secret is sent
 */
#define AJP14_LOGCOMP_CMD	(unsigned char)0x12

/* web-server want context info after login */
#define AJP14_CONTEXT_INFO_NEG      0x80000000 

/* web-server want context updates */
#define AJP14_CONTEXT_UPDATE_NEG    0x40000000 

/* communication could use AJP14 */
#define AJP14_PROTO_SUPPORT_AJP14_NEG   0x00010000 

#define AJP14_ENTROPY_SEED_LEN		32  /* we're using MD5 => 32 chars */
#define AJP14_COMPUTED_KEY_LEN		32  /* we're using MD5 also */

/*
 * Decode the Login Command
 *
 * +-------------------------+---------------------------+
 * | LOGIN SEED CMD (1 byte) | MD5 of entropy (String) |
 * +-------------------------+---------------------------+
 *
 * Build the reply: 
 *   byte    LOGCOMP
 *   String  MD5 of random + Secret key
 *   long    negotiation
 *   String  serverName
 *
 */
static int JK_METHOD jk2_handler_login(jk_env_t *env, void *target, 
                                       jk_endpoint_t *ae, jk_msg_t   *msg )
{
    int rc;
    char *entropy;
    char computedKey[AJP14_COMPUTED_KEY_LEN];
    char *secret=ae->worker->secret;
    long negociation;
    
    entropy= msg->getString( env, msg );
    if( entropy==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "Error ajp14_unmarshal_login_seed - can't get seed\n");
        return JK_HANDLER_FATAL;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "handle.logseed() received entropy %s\n", entropy);
    
    jk2_md5((const unsigned char *)entropy,
           (const unsigned char *)secret, computedKey);

    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "Into ajp14_compute_md5 (%s/%s) -> (%s)\n",
                  entropy, secret, computedKey);
    
    msg->reset( env, msg );

    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "Into ajp14_marshal_login_comp_into_msgb\n");

    rc=msg->appendByte( env, msg, AJP14_LOGCOMP_CMD);
    if (rc!=JK_OK )
        return JK_HANDLER_FATAL;

    /* COMPUTED-SEED */
    rc= msg->appendString( env, msg, (const unsigned char *)computedKey);
    if( rc!=JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                 "handler.loginSecret() error serializing computed secret\n");
        return JK_HANDLER_FATAL;
    }
    negociation=
        (AJP14_CONTEXT_INFO_NEG | AJP14_PROTO_SUPPORT_AJP14_NEG);
    msg->appendLong(env, msg, negociation);
    
    rc=msg->appendString(env, msg, ae->worker->workerEnv->server_name);

    if ( rc != JK_OK)
        return JK_HANDLER_FATAL;
    
    return JK_HANDLER_RESPONSE;
}


/*
 * Decode the LogOk Command. After that we're done, the connection is
 * perfect and ready.
 *
 * +--------------------+------------------------+---------------------------
 * | LOGOK CMD (1 byte) | NEGOCIED DATA (32bits) | SERVLET ENGINE INFO(CString)
 * +--------------------+------------------------+---------------------------
 *
 */
static int JK_METHOD jk2_handler_logok(jk_env_t *env, void *target, 
                                       jk_endpoint_t *ae, jk_msg_t   *msg )
{
    unsigned long nego;
    char *sname;
//    int rc;

    nego = msg->getLong(env, msg);
    
    if (nego == 0xFFFFFFFF) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "handler.log_ok()  can't get negociated data\n");
        return JK_HANDLER_FATAL;
    }
    
    sname = (char *)msg->getString(env, msg);
    
    if (! sname) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "handler.logon() Error getting servlet engine name\n");
        return JK_HANDLER_FATAL;
    }
    
    /* take care of removing previously allocated data */
    /* XXXXXXXXX NEED A SUB POOL !!!! */
    if (ae->servletContainerName == NULL || 
        strcmp( sname, ae->servletContainerName) != 0 )  {
        ae->servletContainerName=
            (char *)ae->pool->pstrdup( env, ae->pool,sname );
    }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "handler.logok() Successfully connected to %s\n",
                  ae->servletContainerName);

    return JK_HANDLER_LAST;
}


/*
 * Decode the Log Nok Command 
 *
 * +---------------------+-----------------------+
 * | LOGNOK CMD (1 byte) | FAILURE CODE (32bits) |
 * +---------------------+-----------------------+
 *
 */
static int JK_METHOD jk2_handler_lognok(jk_env_t *env, void *target, 
                                        jk_endpoint_t *ae, jk_msg_t   *msg )
{
    unsigned long   status;
    
    status = msg->getLong(env, msg);
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "handler.logonFailure() code %08lx", status);
    
    return JK_HANDLER_FATAL;
}

int JK_METHOD jk2_handler_logon_init( jk_env_t *env, jk_handler_t *_this,
                                      jk_workerEnv_t *wEnv) 
{
    wEnv->registerHandler( env, wEnv, "handler.logon",
                           "login", JK_HANDLE_LOGON_SEED,
                           jk2_handler_login, NULL );
    
    wEnv->registerHandler( env, wEnv, "handler.logon",
                           "logOk", JK_HANDLE_LOGON_OK, 
                           jk2_handler_logok, NULL );
    
    wEnv->registerHandler( env, wEnv, "handler.logon",
                           "logNok", JK_HANDLE_LOGON_ERR,
                           jk2_handler_lognok, NULL );
    return JK_OK;
}


/** Register handlers
 */
int JK_METHOD jk2_handler_logon_factory( jk_env_t *env, jk_pool_t *pool,
                                         jk_bean_t *result,
                                         const char *type, const char *name)
{
    jk_handler_t *h;

    h=(jk_handler_t *)pool->calloc( env, pool, sizeof( jk_handler_t));

    h->init=jk2_handler_logon_init;

    result->object=h;
    
    return JK_OK;
}
