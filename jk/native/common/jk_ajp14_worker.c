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
 * Description: AJP14 next generation Bi-directional protocol.             *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_context.h"
#include "jk_ajp14_worker.h"


/*
 * AJP14 Autoconf Phase
 *
 * CONTEXT QUERY / REPLY
 */

#define MAX_URI_SIZE    512

static int handle_discovery(ajp_endpoint_t  *ae,
                            jk_worker_env_t *we,
                            jk_msg_buf_t    *msg,
                            jk_logger_t     *l)
{
    int                 cmd;
    int                 i,j;
    jk_login_service_t  *jl = ae->worker->login;
    jk_context_item_t   *ci;
    jk_context_t        *c;  
    char                *old;
    char                *buf;

#ifndef TESTME

    ajp14_marshal_context_query_into_msgb(msg, we->virtual, l);

    jk_log(l, JK_LOG_DEBUG, "Into ajp14:discovery - send query\n");

    if (ajp_connection_tcp_send_message(ae, msg, l) != JK_TRUE)
        return JK_FALSE;

    jk_log(l, JK_LOG_DEBUG, "Into ajp14:discovery - wait context reply\n");

    jk_b_reset(msg);

    if (ajp_connection_tcp_get_message(ae, msg, l) != JK_TRUE)
        return JK_FALSE;

    if ((cmd = jk_b_get_byte(msg)) != AJP14_CONTEXT_INFO_CMD) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14:discovery - awaited command %d, received %d\n", AJP14_CONTEXT_INFO_CMD, cmd);
        return JK_FALSE;
    }

    if (context_alloc(&c, we->virtual) != JK_TRUE) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14:discovery - can't allocate context room\n");
        return JK_FALSE;
    }
 
    if (ajp14_unmarshal_context_info(msg, c, l) != JK_TRUE) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14:discovery - can't get context reply\n");
        return JK_FALSE;
    }

    jk_log(l, JK_LOG_DEBUG, "Into ajp14:discovery - received context\n");

    buf = malloc(MAX_URI_SIZE);      /* Really a very long URI */

    if (! buf) {
        jk_log(l, JK_LOG_ERROR, "Error ajp14:discovery - can't alloc buf\n");
        return JK_FALSE;
    }

    for (i = 0; i < c->size; i++) {
        ci = c->contexts[i];
        for (j = 0; j < ci->size; j++) {

#ifndef USE_SPRINTF
            snprintf(buf, MAX_URI_SIZE - 1, "/%s/%s", ci->cbase, ci->uris[j]);
#else
            sprintf(buf, "/%s/%s", ci->cbase, ci->uris[j]);
#endif

            jk_log(l, JK_LOG_INFO, "Into ajp14:discovery - worker %s will handle uri %s in context %s [%s]\n",
                    ae->worker->name, ci->uris[j], ci->cbase, buf);

            uri_worker_map_add(we->uri_to_worker, buf, ae->worker->name, l);
        }
    }

    free(buf);
    context_free(&c);

#else 

    uri_worker_map_add(we->uri_to_worker, "/examples/servlet/*", ae->worker->name, l);
    uri_worker_map_add(we->uri_to_worker, "/examples/*.jsp", ae->worker->name, l);
    uri_worker_map_add(we->uri_to_worker, "/examples/*.gif", ae->worker->name, l);

#endif 

    return JK_TRUE;
}

/* 
 * AJP14 Logon Phase 
 *
 * INIT + REPLY / NEGO + REPLY 
 */

static int handle_logon(ajp_endpoint_t *ae,
					   jk_msg_buf_t	   *msg,
					   jk_logger_t     *l)
{
	int	cmd;

	jk_login_service_t *jl = ae->worker->login;

	ajp14_marshal_login_init_into_msgb(msg, jl, l);

	jk_log(l, JK_LOG_DEBUG, "Into ajp14:logon - send init\n");

	if (ajp_connection_tcp_send_message(ae, msg, l) != JK_TRUE)	
		return JK_FALSE;
 
	jk_log(l, JK_LOG_DEBUG, "Into ajp14:logon - wait init reply\n");

	jk_b_reset(msg);

	if (ajp_connection_tcp_get_message(ae, msg, l) != JK_TRUE)
		return JK_FALSE;

	if ((cmd = jk_b_get_byte(msg)) != AJP14_LOGSEED_CMD) {
		jk_log(l, JK_LOG_ERROR, "Error ajp14:logon: awaited command %d, received %d\n", AJP14_LOGSEED_CMD, cmd);
		return JK_FALSE;
	}

	if (ajp14_unmarshal_login_seed(msg, jl, l) != JK_TRUE)
		return JK_FALSE;
				
	jk_log(l, JK_LOG_DEBUG, "Into ajp14:logon - received entropy %s\n", jl->entropy);

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
			jk_log(l, JK_LOG_DEBUG, "Successfully connected to servlet-engine %s\n", jl->servlet_engine_name);
			return JK_TRUE;
		}
		break;
		
	case AJP14_LOGNOK_CMD :
		ajp14_unmarshal_log_nok(msg, l);
		break;
	}

	return JK_FALSE;
}

static int logon(ajp_endpoint_t *ae,
                jk_logger_t    *l)
{
	jk_pool_t          *p = &ae->pool;
	jk_msg_buf_t	   *msg;
	int					rc;

	jk_log(l, JK_LOG_DEBUG, "Into ajp14:logon\n");

	msg = jk_b_new(p);
	jk_b_set_buffer_size(msg, DEF_BUFFER_SZ);
	
	if ((rc = handle_logon(ae, msg, l)) == JK_FALSE) 
		ajp_close_endpoint(ae, l);

	return rc;
}

static int discovery(ajp_endpoint_t *ae,
                     jk_worker_env_t *we,
                     jk_logger_t    *l)
{
    jk_pool_t          *p = &ae->pool;
    jk_msg_buf_t       *msg;
    int                 rc;

    jk_log(l, JK_LOG_DEBUG, "Into ajp14:discovery\n");

    msg = jk_b_new(p);
    jk_b_set_buffer_size(msg, DEF_BUFFER_SZ);

    if ((rc = handle_discovery(ae, we, msg, l)) == JK_FALSE)
        ajp_close_endpoint(ae, l);

    return rc;
}

/* -------------------- Method -------------------- */
static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t    *props,
                              jk_worker_env_t *we,
                              jk_logger_t *l)
{   
	ajp_worker_t *aw;
	char * secret_key;

    if (ajp_validate(pThis, props, we, l, AJP14_PROTO) == JK_FALSE)
		return JK_FALSE;

   aw = pThis->worker_private;

   secret_key = jk_get_worker_secret_key(props, aw->name);

   if ((!secret_key) || (!strlen(secret_key))) {
		jk_log(l, JK_LOG_ERROR, "validate error, empty or missing secretkey\n");
		return JK_FALSE;
	}

	/* jk_log(l, JK_LOG_DEBUG, "Into ajp14:validate - secret_key=%s\n", secret_key); */
	return JK_TRUE;
}

static int JK_METHOD get_endpoint(jk_worker_t    *pThis,
                                  jk_endpoint_t **pend,
                                  jk_logger_t    *l)
{
    return (ajp_get_endpoint(pThis, pend, l, AJP14_PROTO));
}

static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t    *props, 
                          jk_worker_env_t *we,
                          jk_logger_t *l)
{
	ajp_worker_t   *aw;
	ajp_endpoint_t *ae;
	jk_endpoint_t  *je;
    int             rc;

   	if (ajp_init(pThis, props, we, l, AJP14_PROTO) == JK_FALSE)
		return JK_FALSE;

   	aw = pThis->worker_private;

	/* Set Secret Key (used at logon time) */	
	aw->login->secret_key = strdup(jk_get_worker_secret_key(props, aw->name));

	if (aw->login->secret_key == NULL) {
		jk_log(l, JK_LOG_ERROR, "can't malloc secret_key\n");
		return JK_FALSE;
	}

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

	if (aw->login) {

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
	}

    return (ajp_destroy(pThis, l, AJP14_PROTO));
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
	aw->login->web_server_name  = NULL; /* must be set in init */

    aw->ep_cache_sz            = 0;
    aw->ep_cache               = NULL;
    aw->connect_retry_attempts = AJP_DEF_RETRY_ATTEMPTS;
    aw->worker.worker_private  = aw;
   
    aw->worker.validate        = validate;
    aw->worker.init            = init;
    aw->worker.get_endpoint    = get_endpoint;
    aw->worker.destroy         = destroy;

	aw->logon				   = logon; /* LogOn Handler for AJP14 */
    *w = &aw->worker;
    return JK_TRUE;
}
