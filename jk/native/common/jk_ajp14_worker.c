/*
 * Copyright (c) 1997-2001 The Java Apache Project.  All rights reserved.
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
 * Description: AJP14 next generation Bi-directional protocol.             *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_context.h"
#include "jk_ajp14_worker.h"

/* -------------------- Method -------------------- */
static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t    *props,
                              jk_logger_t *l)
{   
	ajp_worker_t *aw;
	char * secret_key;

    if (ajp_validate(pThis, props, l, AJP14_PROTO) == JK_FALSE)
		return JK_FALSE;

   aw = pThis->worker_private;

   secret_key = jk_get_worker_secret_key(props, aw->name);

   if ((!secret_key) || (!strlen(secret_key))) {
		jk_log(l, JK_LOG_ERROR, "validate error, empty or missing secretkey\n");
		return JK_FALSE;
	}

	return JK_TRUE;
}

static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t    *props, 
                          jk_logger_t *l)
{
   ajp_worker_t *aw;
   char         *secret_key;

   if (ajp_init(pThis, props, l, AJP14_PROTO) == JK_FALSE)
		return JK_FALSE;

   aw = pThis->worker_private;

   aw->login->secret_key = jk_get_worker_secret_key(props, aw->name);
   return JK_TRUE;
}
    

static int JK_METHOD destroy(jk_worker_t **pThis,
                             jk_logger_t *l)
{
	ajp_worker_t *aw = (*pThis)->worker_private;

	if (aw->login)
		free(aw->login);

    return (ajp_destroy(pThis, l, AJP14_PROTO));
}

static int JK_METHOD get_endpoint(jk_worker_t    *pThis,
                                  jk_endpoint_t **pend,
                                  jk_logger_t    *l)
{
    return (ajp_get_endpoint(pThis, pend, l, AJP14_PROTO));
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
        jk_log(l, JK_LOG_ERROR, "In ajp14_worker_factory, malloc of private data failed\n");
        return JK_FALSE;
    }

    aw->name = strdup(name);
   
    if (! aw->name) {
        free(aw);
        jk_log(l, JK_LOG_ERROR, "In ajp14_worker_factory, malloc failed for name\n");
        return JK_FALSE;
    }

    aw->proto                  = AJP14_PROTO;

    aw->login                  = (jk_login_service_t *)malloc(sizeof(jk_login_service_t));

	if (aw->login == NULL) {
		jk_log(l, JK_LOG_ERROR, "In ajp14_worker_factory, malloc failed for login area\n");
		return JK_FALSE;
	}
	
	memset(aw->login, 0, sizeof(jk_login_service_t));

	aw->login->negociation	    = (AJP14_CONTEXT_INFO_NEG | AJP14_PROTO_SUPPORT_AJP14_NEG);
	aw->login->web_server_name  = "MyApache";

    aw->ep_cache_sz            = 0;
    aw->ep_cache               = NULL;
    aw->connect_retry_attempts = AJP_DEF_RETRY_ATTEMPTS;
    aw->worker.worker_private  = aw;
   
    aw->worker.validate        = validate;
    aw->worker.init            = init;
    aw->worker.get_endpoint    = get_endpoint;
    aw->worker.destroy         = destroy;
   
    *w = &aw->worker;
    return JK_TRUE;
}
