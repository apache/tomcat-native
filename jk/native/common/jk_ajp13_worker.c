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
 * Description: Bi-directional protocol.                                   *
 * Author:      Costin <costin@costin.dnt.ro>                              *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/

#include "jk_ajp13_worker.h"


/* -------------------- Method -------------------- */
static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t *props,                            
                              jk_worker_env_t *we,
                              jk_logger_t *l)
{
	return (ajp_validate(pThis, props, we, l, AJP13_PROTO));
}


static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t *props, 
                          jk_worker_env_t *we,
                          jk_logger_t *l)
{
	return (ajp_init(pThis, props, we, l, AJP13_PROTO));
}


static int JK_METHOD destroy(jk_worker_t **pThis,
                             jk_logger_t *l)
{
	return (ajp_destroy(pThis, l, AJP13_PROTO));
}


static int JK_METHOD get_endpoint(jk_worker_t *pThis,
                                  jk_endpoint_t **pend,
                                  jk_logger_t *l)
{
    return (ajp_get_endpoint(pThis, pend, l, AJP13_PROTO));
}

int JK_METHOD ajp13_worker_factory(jk_worker_t **w,
                                   const char   *name,
                                   jk_logger_t  *l)
{
    ajp_worker_t *aw = (ajp_worker_t *)malloc(sizeof(ajp_worker_t));
    
    jk_log(l, JK_LOG_DEBUG, "Into ajp13_worker_factory\n");

    if (name == NULL || w == NULL) {
        jk_log(l, JK_LOG_ERROR, "In ajp13_worker_factory, NULL parameters\n");
	    return JK_FALSE;
    }
        
    if (! aw) {
        jk_log(l, JK_LOG_ERROR, "In ajp13_worker_factory, malloc of private_data failed\n");
	    return JK_FALSE;
    }

    aw->name = strdup(name);          
    
    if (! aw->name) {
	    free(aw);
	    jk_log(l, JK_LOG_ERROR, "In ajp13_worker_factory, malloc failed\n");
	    return JK_FALSE;
    } 

	aw->proto					 = AJP13_PROTO;
	aw->login					 = NULL;

    aw->ep_cache_sz            = 0;
    aw->ep_cache               = NULL;
    aw->connect_retry_attempts = AJP_DEF_RETRY_ATTEMPTS;
    aw->worker.worker_private  = aw;
    
    aw->worker.validate        = validate;
    aw->worker.init            = init;
    aw->worker.get_endpoint    = get_endpoint;
    aw->worker.destroy         = destroy;
    
	aw->logon				   = NULL;	/* No Logon on AJP13 */

    *w = &aw->worker;
    return JK_TRUE;
}
