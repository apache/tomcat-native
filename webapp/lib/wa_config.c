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
    if (buf[0]=='/' || l==0) {
      appl->rpth=apr_pstrcat(wa_pool,buf,"/",NULL);
    } else {
      appl->rpth=apr_pstrcat(wa_pool,"/",buf,"/",NULL);
    }

    /* Zero all other parameters */
    appl->host=NULL;
    appl->conn=NULL;
    appl->conf=NULL;
    appl->lpth=NULL;
    appl->allw=NULL;
    appl->deny=NULL;
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
    if (p<1) return("Invalid port number (p<1) No \"Port\" statement found");
    if (p>65535) return("Invalid port number (p>65535)");

    /* Allocate some memory */
    host=(wa_virtualhost *)apr_palloc(wa_pool,sizeof(wa_virtualhost));
    if (host==NULL) return("Cannot allocate memory");

    /* Set up parameters */
    host->name=apr_pstrdup(wa_pool,n);
    host->port=p;
    host->apps=NULL;

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
