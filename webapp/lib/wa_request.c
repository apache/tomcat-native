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
 * 4. The names  "The  Jakarta  Project",  "WebApp",  and  "Apache  Software *
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

/* @version $Id$ */
#include <wa.h>

/* Allocate a new request structure. */
const char *WA_AllocRequest(wa_request **r, void *d) {
	apr_pool_t *pool=NULL;
	wa_request *req=NULL;
	
	if(apr_pool_create(&pool,WA_Pool)!=APR_SUCCESS)
		return("Cannot create request memory pool");
	if((req=apr_palloc(pool,sizeof(wa_request)))==NULL) {
		apr_pool_destroy(pool);
		return("Cannot allocate memory for the request structure");
	}

	/* Set up the server host data record */
	if((req->serv=apr_palloc(pool,sizeof(wa_hostdata)))==NULL) {
		apr_pool_destroy(pool);
		return("Cannot allocate memory for server host data structure");
	} else {
		req->serv->host=NULL;
		req->serv->addr=NULL;
		req->serv->port=-1;
	}

	/* Set up the server host data record */
	if((req->clnt=apr_palloc(pool,sizeof(wa_hostdata)))==NULL) {
		apr_pool_destroy(pool);
		return("Cannot allocate memory for client host data structure");
	} else {
		req->clnt->host=NULL;
		req->clnt->addr=NULL;
		req->clnt->port=-1;
	}

	/* Set up the headers table */
//	req->hdrs=NULL;

	/* Set up all other request members */
	req->pool=pool;
	req->data=d;
	req->meth=NULL;
	req->ruri=NULL;
	req->args=NULL;
	req->prot=NULL;
	req->schm=NULL;
	req->user=NULL;
	req->auth=NULL;
	req->clen=0;
	req->rlen=0;
	
	/* All done */
	return(NULL);
}
