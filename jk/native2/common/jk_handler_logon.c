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

/**
 * Description: AJP14 Login handler
 * Author:      Henri Gomez <hgomez@apache.org>
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
    rc= msg->appendString( env, msg, (const char *)computedKey);
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
/*    int rc; */

    nego = msg->getLong(env, msg);
    
    if (nego == 0xFFFFFFFF) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "handler.logok()  can't get negociated data\n");
        return JK_HANDLER_FATAL;
    }
    
    sname = (char *)msg->getString(env, msg);
    
    if (! sname) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "handler.logok() Error getting servlet engine name\n");
        return JK_HANDLER_FATAL;
    }
    
    /* take care of removing previously allocated data */
    /* XXXXXXXXX NEED A SUB POOL !!!! */
    /*
      if (ae->servletContainerName == NULL || 
      strcmp( sname, ae->servletContainerName) != 0 )  {
      ae->servletContainerName=
      (char *)ae->pool->pstrdup( env, ae->pool,sname );
      }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "handler.logok() Successfully connected to %s\n",
                  ae->servletContainerName);
    */
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
