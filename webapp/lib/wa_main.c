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
#if APR_HAS_THREADS
        if (apr_atomic_init(wa_pool)!=APR_SUCCESS)
            return("Cannot initialize atomic integer library");
#endif
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
