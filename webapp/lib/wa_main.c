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

/* The current APR memory pool. */
apr_pool_t *wa_pool=NULL;
/* The list of all deployed applications. */
wa_chain *wa_configuration=NULL;
/* The list of all compiled in providers */
wa_provider *wa_providers[] = {
    &wa_provider_info,
    &wa_provider_warp,
    /*&wa_provider_jni,*/
    NULL,
};

/* Initialize the WebApp Library. */
const char *wa_init(void) {
    int x=0;

    wa_debug(WA_MARK,"WebApp Library initializing");

    /* Check the main APR pool. */
    if (wa_pool==NULL) {
        wa_debug(WA_MARK,"Initializing APR");
        if (apr_initialize()!=APR_SUCCESS)
            return("Cannot initialize APR");
        if (apr_pool_create(&wa_pool,NULL)!=APR_SUCCESS)
            return("Cannot create WebApp Library memory pool");
        if (wa_pool==NULL)
            return("Invalid WebApp Library memory pool created");
    }

    /* Initialize providers */
    while(wa_providers[x]!=NULL) {
        const char *ret=wa_providers[x]->init();
        if (ret!=NULL) {
            wa_shutdown();
            return(ret);
        }
        x++;
    }

    /* Done */
    wa_debug(WA_MARK,"WebApp Library initialized");
    return(NULL);
}

/* Startup all providers. */
void wa_startup(void) {
    int x=0;

    while(wa_providers[x]!=NULL) wa_providers[x++]->startup();

    wa_debug(WA_MARK,"WebApp Library started");
}

/* Clean up the WebApp Library. */
void wa_shutdown(void) {
    int x=0;

    /* Initialization check */
    if (wa_pool==NULL) return;

    /* Shutdown providers */
    while(wa_providers[x]!=NULL) wa_providers[x++]->shutdown();

    /* Clean up this library and APR */
    apr_pool_destroy(wa_pool);
    wa_pool=NULL;
    wa_configuration=NULL;
    apr_terminate();

    wa_debug(WA_MARK,"WebApp Library shut down");
}

/* Deploy a web-application. */
const char *wa_deploy(wa_application *a, wa_virtualhost *h, wa_connection *c) {
    wa_chain *elem=NULL;
    const char *ret=NULL;

    /* Check parameters */
    if (a==NULL) return("Invalid application for deployment");
    if (h==NULL) return("Invalid virtual host for deployment");
    if (c==NULL) return("Invalid connection for deployment");

    /* Check if another application was deployed under the same URI path in
       the same virtual host */
    elem=h->apps;
    while (elem!=NULL) {
        wa_application *curr=(wa_application *)elem->curr;
        if (strcasecmp(curr->rpth,a->rpth)==0)
            return("Duplicate application specified for the same URL path");
        elem=elem->next;
    }

    /* Deploy */
    a->host=h;
    a->conn=c;

    /* Give the opportunity to the provider to be notified of the deployment of
       this application */
    if ((ret=c->prov->deploy(a))!=NULL) return(ret);

    /* Append this application to the list of deployed applications in the
       virtual host */
    elem=(wa_chain *)apr_palloc(wa_pool,sizeof(wa_chain));
    elem->curr=a;
    elem->next=h->apps;
    h->apps=elem;

    /* Check if this virtual host is already present in the configuration */
    if (wa_configuration!=NULL) {
        elem=wa_configuration;
        while (elem!=NULL) {
            /* Compare the pointers to structures, we *MIGHT* allow two
               virtual hosts to have the same name and port. The selection
               of the host is done at the web server level */
            if (elem->curr==h) return(NULL);
            elem=elem->next;
        }
    }

    /* We didn't find this host in our list, we need to add it to the conf. */
    elem=(wa_chain *)apr_palloc(wa_pool,sizeof(wa_chain));
    elem->curr=h;
    elem->next=wa_configuration;
    wa_configuration=elem;

    /* Done */
    wa_debug(WA_MARK,"Application %s deployed for http://%s:%d%s (Conn: %s)",
             a->name,h->name,h->port,a->rpth,c->name);
    return(NULL);
}

/* Dump some debugging information. */
void wa_debug(const char *f, const int l, const char *fmt, ...) {
#ifdef DEBUG
    char hdr[128];
    char dta[640];
    char buf[768];
    apr_time_t at;
    char st[128];
    va_list ap;

    at=apr_time_now();
    apr_ctime(st, at);
    va_start(ap,fmt);
    apr_snprintf(hdr,128,"[%s] %d (%s:%d)",st,getpid(),f,l);
    apr_vsnprintf(dta,640,fmt,ap);
    apr_snprintf(buf,728,"%s %s\n",hdr,dta);
    fprintf(stderr,"%s",buf);
    fflush(stderr);
    va_end(ap);
#endif /* ifdef DEBUG */
}
