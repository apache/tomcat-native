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

/* The webserver specified in init. */
wa_webserver *WA_WebServer=NULL;

/* The current APR memory pool. */
static apr_pool_t *WA_Pool=NULL;

/* Initialize the WebApp Library. */
const char *WA_Init(wa_webserver *w) {
	/* Check the main APR pool. */
    if (WA_Pool==NULL) {
        if (apr_initialize()!=APR_SUCCESS)
            return("Cannot initialize APR");
        if (apr_pool_create(&WA_Pool,NULL)!=APR_SUCCESS)
            return("Cannot create WebApp Library memory pool");
        if (WA_Pool==NULL)
            return("Invalid WebApp Library memory pool created");
    }

	/* Assign the current webserver */
	if (WA_WebServer==NULL) {
	    WA_WebServer=w;
	    if (WA_WebServer==NULL) return("Invalid WebServer member specified");
	}

    return(NULL);
}

/* Clean up the WebApp Library. */
const char *WA_Destroy(void) {
    if (WA_Pool==NULL) return("WebApp Library not initialized");
    apr_pool_destroy(WA_Pool);
    WA_Pool=NULL;
    WA_WebServer=NULL;
    apr_terminate();
    return(NULL);
}

/* Allocate and setup a connection */
const char *WA_Connect(wa_connection **c, const char *p, const char *a) {
    wa_connection *conn=NULL;

    /* Check parameters */
    if (c==NULL) return("Invalid storage location specified");

    /* Allocate some memory */
    conn=(wa_connection *)apr_palloc(WA_Pool,sizeof(wa_connection));
    if (conn==NULL) return("Cannot allocate memory");
    conn->pool=WA_Pool;

    /* Retrieve the provider and set up the conection */
    conn->conf=NULL;
    // if ((conn->prov=wa_provider_get(p))==NULL)
    //  return("Invalid provider name specified");
    // if ((msg=conn->prov->configure(conn,a))!=NULL) return(msg);

    /* Done! :) */
    *c=conn;
    return(NULL);
}

/* Allocate, set up and deploy an application. */
const char *WA_Deploy(wa_application **a, wa_connection *c, const char *n,
                      const char *p) {
    wa_application *appl=NULL;
    char *buf=NULL;
    int l=0;

    /* Check parameters */
    if (a==NULL) return("Invalid storage location specified");
    if (c==NULL) return("Invalid connection specified");
    if (n==NULL) return("Invalid application name");
    if (p==NULL) return("Invalid application path");

    /* Allocate some memory */
    appl=(wa_application *)apr_palloc(WA_Pool,sizeof(wa_application));
    if (appl==NULL) return("Cannot allocate memory");
    appl->pool=WA_Pool;

    /* Set up application structure */
    appl->conn=c;
    appl->name=apr_pstrdup(WA_Pool,n);
    buf=apr_pstrdup(appl->pool,p);
    l=strlen(buf)-1;
    if (buf[l]=='/') buf[l]='\0';
    if (buf[0]=='/') appl->rpth=apr_pstrcat(WA_Pool,buf,"/");
    else appl->rpth=apr_pstrcat(WA_Pool,"/",buf,"/");
    appl->lpth=NULL;

    /* Tell the connector provider we're deploying an application */
    appl->conf=NULL;
    // if ((msg=appl->conn->prov->deploy(a))!=NULL) return(msg);

    /* Done! :) */
    *a=appl;
    return(NULL);
}
