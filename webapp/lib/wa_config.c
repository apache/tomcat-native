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

/* Allocate and set up a <code>wa_application</code> member. */
const char *wa_capplication(wa_application **a, const char *n,
                            const char *p) {
    wa_application *appl=NULL;
    char buf[1024];
    int l=0;

    /* Check parameters */
    if (a==NULL) return("Invalid application storage location");
    if (n==NULL) return("Invalid application name");
    if (p==NULL) return("Invalid application path");

    /* Allocate some memory */
    appl=(wa_application *)apr_palloc(wa_pool,sizeof(wa_application));
    if (appl==NULL) return("Cannot allocate memory");

    /* Set up application name */
    appl->name=apr_pstrdup(wa_pool,n);

    /* Normalize the application root URL path */
    strncpy(buf,p,1024);
    l=strlen(buf)-1;
    if (buf[l]=='/') buf[l]='\0';
    if (buf[0]=='/') {
        appl->rpth=apr_pstrcat(wa_pool,buf,"/",NULL);
    } else {
        appl->rpth=apr_pstrcat(wa_pool,"/",buf,"/",NULL);
    }

    /* Zero all other parameters */
    appl->host=NULL;
    appl->conn=NULL;
    appl->conf=NULL;
    appl->lpth=NULL;
    appl->depl=wa_false;

    /* Done */
    wa_debug(WA_MARK,"Created application \"%s\" in path \"%s\"",
             appl->name,appl->rpth);
    *a=appl;
    return(NULL);
}

/* Allocate and set up a virtual host */
const char *wa_cvirtualhost(wa_virtualhost **h, const char *n, int p) {
    wa_virtualhost *host=NULL;

    /* Check parameters */
    if (h==NULL) return("Invalid virtual host storage location");
    if (n==NULL) return("Invalid virtual host name");
    if (p<1) return("Invalid port number (p<1)");
    if (p>65535) return("Invalid port number (p>65535)");

    /* Allocate some memory */
    host=(wa_virtualhost *)apr_palloc(wa_pool,sizeof(wa_virtualhost));
    if (host==NULL) return("Cannot allocate memory");

    /* Set up parameters */
    host->name=apr_pstrdup(wa_pool,n);
    host->port=p;

    /* Done! :) */
    wa_debug(WA_MARK,"Created virtual host \"%s:%d\"",host->name,host->port);
    *h=host;
    return(NULL);
}

/* Allocate and setup a connection */
const char *wa_cconnection(wa_connection **c, const char *n,
                           const char *p, const char *a) {
    wa_connection *conn=NULL;
    const char *ret=NULL;
    int x=0;

    /* Check parameters */
    if (c==NULL) return("Invalid connection storage location");
    if (n==NULL) return("Invalid connection name");
    if (p==NULL) return("Invalid connection provider");

    /* Allocate some memory */
    conn=(wa_connection *)apr_palloc(wa_pool,sizeof(wa_connection));
    if (conn==NULL) return("Cannot allocate memory");

    /* Set up the parameter string */
    conn->name=apr_pstrdup(wa_pool,n);
    if (a==NULL) {
        conn->parm=apr_pstrdup(wa_pool,"\0");
    } else {
        conn->parm=apr_pstrdup(wa_pool,a);
    }

    /* Retrieve the provider and set up the conection */
    conn->conf=NULL;
    while(wa_providers[x]!=NULL) {
        if(strcasecmp(wa_providers[x]->name,p)==0) {
             conn->prov=wa_providers[x];
             break;
        } else x++;
    }
    if (conn->prov==NULL) return("Invalid provider name specified");
    if ((ret=conn->prov->connect(conn,a))!=NULL) return(ret);

    /* Done */
    wa_debug(WA_MARK,"Created connection \"%s\" (Prov: \"%s\" Param: \"%s\")",
             n,p,a);
    *c=conn;
    return(NULL);
}
