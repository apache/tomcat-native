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
 * Description: AJP14 next generation Bi-directional protocol.
 *              Backward compatible with Ajp13
 * Author:      Henri Gomez <hgomez@slib.fr>
 * Author:      Costin <costin@costin.dnt.ro>                              
 * Author:      Gal Shachor <shachor@il.ibm.com>                           
 */

#include "jk_global.h"
#include "jk_context.h"
#include "jk_pool.h"
#include "jk_util.h"
#include "jk_msg_buff.h"
#include "jk_ajp_common.h"
#include "jk_ajp14.h" 
#include "jk_logger.h"
#include "jk_service.h"

int JK_METHOD ajp14_worker_factory(jk_worker_t **w,
                                   const char *name,
                                   jk_logger_t *l);


/* Ajp14 methods - XXX move to handler abstraction */
int logon(ajp_endpoint_t *ae,
          jk_logger_t    *l);

int discovery(ajp_endpoint_t *ae,
              jk_worker_env_t *we,
              jk_logger_t    *l);

/* -------------------- Method -------------------- */
static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t    *props,
                              jk_worker_env_t *we,
                              jk_logger_t *l)
{   
    ajp_worker_t *aw;
    char * secret_key;
    int proto=AJP14_PROTO;

    aw = pThis->worker_private;
    secret_key = jk_get_worker_secret_key(props, aw->name);
    
    if ((!secret_key) || (!strlen(secret_key))) {
        jk_log(l, JK_LOG_ERROR,
               "No secretkey, defaulting to unauthenticated AJP13\n");
        proto=AJP13_PROTO;
        aw->proto= AJP13_PROTO;
        aw->logon= NULL; 
    }
    
    if (ajp_validate(pThis, props, we, l, proto) == JK_FALSE)
        return JK_FALSE;
    
    /* jk_log(l, JK_LOG_DEBUG,
       "Into ajp14:validate - secret_key=%s\n", secret_key); */
    return JK_TRUE;
}

static int JK_METHOD get_endpoint(jk_worker_t    *pThis,
                                  jk_endpoint_t **pend,
                                  jk_logger_t    *l)
{
    ajp_worker_t *aw=pThis->worker_private;

    if( aw->login->secret_key ==NULL ) {
        return (ajp_get_endpoint(pThis, pend, l, AJP13_PROTO));
    } else {
        return (ajp_get_endpoint(pThis, pend, l, AJP14_PROTO));
    }
}

static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t    *props, 
                          jk_worker_env_t *we,
                          jk_logger_t *l)
{
    ajp_worker_t   *aw=pThis->worker_private;
    ajp_endpoint_t *ae;
    jk_endpoint_t  *je;
    int             rc;
    char * secret_key;
    int proto=AJP14_PROTO;
    
    secret_key = jk_get_worker_secret_key(props, aw->name);
    
    if( secret_key==NULL ) {
        proto=AJP13_PROTO;
        aw->proto= AJP13_PROTO;
        aw->logon= NULL; 
    } else {
        /* Set Secret Key (used at logon time) */	
        aw->login->secret_key = strdup(secret_key);
    }

    if (ajp_init(pThis, props, we, l, proto) == JK_FALSE)
        return JK_FALSE;
    
    if (aw->login->secret_key == NULL) {
        /* No extra initialization for AJP13 */
        return JK_TRUE;
    }

    /* -------------------- Ajp14 discovery -------------------- */
    
    /* Set WebServerName (used at logon time) */
    aw->login->web_server_name = strdup(we->server_name);
    
    if (aw->login->web_server_name == NULL) {
        jk_log(l, JK_LOG_ERROR, "can't malloc web_server_name\n");
        return JK_FALSE;
    }
    
    if (get_endpoint(pThis, &je, l) == JK_FALSE)
        return JK_FALSE;
    
    ae = je->endpoint_private;
    
    if (ajp_connect_to_endpoint(ae, l) == JK_TRUE) {
        
	/* connection stage passed - try to get context info
	 * this is the long awaited autoconf feature :)
	 */
        rc = discovery(ae, we, l);
        ajp_close_endpoint(ae, l);
        return rc;
    }
    
    return JK_TRUE;
}


static int JK_METHOD destroy(jk_worker_t **pThis,
                             jk_logger_t *l)
{
    ajp_worker_t *aw = (*pThis)->worker_private;
    
    if (aw->login != NULL &&
        aw->login->secret_key != NULL ) {
        /* Ajp14-specific initialization */
        if (aw->login->web_server_name) {
            free(aw->login->web_server_name);
            aw->login->web_server_name = NULL;
        }
        
        if (aw->login->secret_key) {
            free(aw->login->secret_key);
            aw->login->secret_key = NULL;
        }
        
        free(aw->login);
        aw->login = NULL;
        return (ajp_destroy(pThis, l, AJP14_PROTO));
    } else {
        return (ajp_destroy(pThis, l, AJP13_PROTO));
    }    
}

int JK_METHOD ajp14_worker_factory(jk_worker_t **w,
                                   const char   *name,
                                   jk_logger_t  *l)
{
    ajp_worker_t *aw = (ajp_worker_t *)malloc(sizeof(ajp_worker_t));
   
    jk_log(l, JK_LOG_DEBUG, "Into ajp14_worker_factory\n");

    if (name == NULL || w == NULL) {
        jk_log(l, JK_LOG_ERROR, "In ajp14_worker_factory, NULL parameters\n");
        return JK_FALSE;
    }

    if (! aw) {
        jk_log(l, JK_LOG_ERROR,
               "In ajp14_worker_factory, malloc of private data failed\n");
        return JK_FALSE;
    }

    aw->name = strdup(name);
    
    if (! aw->name) {
        free(aw);
        jk_log(l, JK_LOG_ERROR,
               "In ajp14_worker_factory, malloc failed for name\n");
        return JK_FALSE;
    }

    aw->proto= AJP14_PROTO;

    aw->login= (jk_login_service_t *)malloc(sizeof(jk_login_service_t));

    if (aw->login == NULL) {
        jk_log(l, JK_LOG_ERROR,
               "In ajp14_worker_factory, malloc failed for login area\n");
        return JK_FALSE;
    }
	
    memset(aw->login, 0, sizeof(jk_login_service_t));
    
    aw->login->negociation=
        (AJP14_CONTEXT_INFO_NEG | AJP14_PROTO_SUPPORT_AJP14_NEG);
    aw->login->web_server_name=NULL; /* must be set in init */
    
    aw->ep_cache_sz= 0;
    aw->ep_cache= NULL;
    aw->connect_retry_attempts= AJP_DEF_RETRY_ATTEMPTS;
    aw->worker.worker_private= aw;
   
    aw->worker.validate= validate;
    aw->worker.init= init;
    aw->worker.get_endpoint= get_endpoint;
    aw->worker.destroy=destroy;
    aw->logon= logon; 

    *w = &aw->worker;
    return JK_TRUE;
}
