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

/* Counter for requests */
static int k=0;

/* Initialize this provider. */
static const char *info_init(void) {
    wa_debug(WA_MARK,"INFO provider initialized");
    return(NULL);
}

/* Notify this provider of its imminent startup. */
static void info_startup(void) {
    wa_debug(WA_MARK,"INFO provider started");
}

/* Cleans up all resources allocated by this provider. */
static void info_shutdown(void) {
    wa_debug(WA_MARK,"INFO provider shut down");
}

/* Configure a connection with the parameter from the web server
   configuration file. */
static const char *info_connect(wa_connection *conn, const char *param) {
    wa_debug(WA_MARK,"Provider is configuring \"%s\" with parameter \"%s\"",
             conn->name,param);
    conn->conf=NULL;
    return(NULL);
}

/* Receive notification of the deployment of an application. */
static const char *info_deploy(wa_application *appl) {
    wa_debug(WA_MARK,"Provider is deploying %s for http://%s:%d%s (Conn: %s)",
             appl->name,appl->host->name,appl->host->port,appl->rpth,
             appl->conn->name);
    appl->conf=NULL;
    appl->depl=wa_true;
    return(NULL);
}

/* Describe the configuration member found in a connection. */
static char *info_conninfo(wa_connection *conn, apr_pool_t *pool) {
    return(NULL);
}

/* Describe the configuration member found in a web application. */
static char *info_applinfo(wa_application *appl, apr_pool_t *pool) {
    return(NULL);
}

/* Display informations regarding an application */
static void info_handle_application(wa_request *r, wa_application *a) {
    char *desc;

    wa_rprintf(r,"   <tr>\n");
    wa_rprintf(r,"    <td width=\"10%%\" valign=\"top\" align=\"right\">\n");
    wa_rprintf(r,"     <font size=\"-1\">\n");
    wa_rprintf(r,"      Application&nbsp;Name<br>\n");
    wa_rprintf(r,"      Root&nbsp;URL&nbsp;Path<br>\n");
    wa_rprintf(r,"      Local&nbsp;Deployment&nbsp;Path<br>\n");
    wa_rprintf(r,"      Configuration&nbsp;Details<br>\n");
    wa_rprintf(r,"      Connection<br>\n");
    wa_rprintf(r,"      Deployed\n");
    wa_rprintf(r,"     </font>\n");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"    <td width=\"90%%\" valign=\"top\" align=\"left\">\n");
    wa_rprintf(r,"     <font size=\"-1\">");
    wa_rprintf(r,"      <b>&quot;%s&quot;</b><br>\n",a->name);
    wa_rprintf(r,"      <b>&quot;%s&quot;</b><br>\n",a->rpth);

    if (a->lpth==NULL) wa_rprintf(r,"      <i>No local deployment path</i>");
    else               wa_rprintf(r,"      <b>&quot;%s&quot;</b>",a->lpth);
    wa_rprintf(r,"<br>\n");

    desc=a->conn->prov->applinfo(a,r->pool);
    if (desc==NULL) wa_rprintf(r,"      <i>No configuration information</i>");
    else            wa_rprintf(r,"      <b>&quot;%s&quot;</b>",desc);
    wa_rprintf(r,"<br>\n");

    wa_rprintf(r,"      <b>&quot;%s&quot;</b>",a->conn->name);
    wa_rprintf(r," <i><a href=\"#%s\">(details)</a></i><br>\n",a->conn->name);
    wa_rprintf(r,"      <b>%s</b><br>\n",a->depl?"TRUE":"FALSE");
    wa_rprintf(r,"     </font>\n");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"   </tr>\n");
    wa_rflush(r);
}

/* Display informations regarding a virtual host and its children applications.
   At the same time add all connections to the 'c' chain for later */
static void info_handle_host(wa_request *r, wa_virtualhost *h, wa_chain *c) {
    wa_chain *elem=NULL;

    wa_rprintf(r,"  <table width=\"80%%\" border=\"1\" cellspacing=\"0\">\n");
    wa_rprintf(r,"   <tr>\n");
    wa_rprintf(r,"    <td bgcolor=\"#ccccff\" colspan=\"2\">\n");
    wa_rprintf(r,"     <b>Host %s:%d</b>\n",h->name,h->port);
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"   </tr>\n");
    wa_rflush(r);

    elem=h->apps;
    while(elem!=NULL) {
        wa_application *curr=(wa_application *)elem->curr;
        wa_chain *orig=c;

        info_handle_application(r,curr);

        /* Check out if this application connection is already stored in the
           connections chain */
        c=orig;
        while(c->next!=NULL) {
            wa_connection *conn=(wa_connection *)c->next->curr;
            if (strcmp(conn->name,curr->conn->name)==0) break;
            c=c->next;
        }
        /* If we didn't find the connection, add it to the chain */
        if (c->next==NULL) {
            c->next=(wa_chain *)apr_palloc(r->pool,sizeof(wa_chain));
            c->next->curr=curr->conn;
            c->next->next=NULL;
        }

        /* Process next application in host */
        elem=elem->next;
    }

    wa_rprintf(r,"  </table>\n");
    wa_rprintf(r,"  <br>\n");
    wa_rflush(r);
}

/* Display informations regarding a connection */
static void info_handle_connection(wa_request *r, wa_connection *c) {
    char *desc;

    wa_rprintf(r,"   <tr>\n");
    wa_rprintf(r,"    <td width=\"10%%\" valign=\"top\" align=\"right\">\n");
    wa_rprintf(r,"     <a name=\"%s\">\n",c->name);
    wa_rprintf(r,"     <font size=\"-1\">\n");
    wa_rprintf(r,"      Connection&nbsp;Name<br>\n");
    wa_rprintf(r,"      Connection&nbsp;Parameters<br>\n");
    wa_rprintf(r,"      Provider<br>\n");
    wa_rprintf(r,"      Configuration&nbsp;Details\n");
    wa_rprintf(r,"     </font>\n");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"    <td width=\"90%%\" valign=\"top\" align=\"left\">\n");
    wa_rprintf(r,"     <font size=\"-1\">");
    wa_rprintf(r,"      <b>&quot;%s&quot;</b><br>\n",c->name);
    wa_rprintf(r,"      <b>&quot;%s&quot;</b><br>\n",c->parm);
    wa_rprintf(r,"      <b>&quot;%s&quot;</b><br>\n",c->prov->name);

    desc=c->prov->conninfo(c,r->pool);
    if (desc==NULL) wa_rprintf(r,"      <i>No configuration information</i>\n");
    else            wa_rprintf(r,"      <b>&quot;%s&quot;</b>\n",desc);

    wa_rprintf(r,"     </font>\n");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"   </tr>\n");
    wa_rflush(r);
}

/* Display informations for a header name */
static int info_handle_hdrname(void *d, const char *n, const char *v) {
    wa_request *r=(wa_request *)d;

    wa_rprintf(r,"       <nobr>%s</nobr><br>\n",n);
    return(TRUE);
}

/* Display informations for a header velue */
static int info_handle_hdrvalue(void *d, const char *n, const char *v) {
    wa_request *r=(wa_request *)d;
    char *b=(char *)v;

    if (strlen(b)>64) {
        b=apr_pstrndup(r->pool,b,64);
        b=apr_pstrcat(r->pool,b," ....",NULL);
    }
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",b);
    return(TRUE);
}

/* Handle a connection from the web server. */
static int info_handle(wa_request *r, wa_application *a) {
    wa_chain *conn=(wa_chain *)apr_palloc(r->pool,sizeof(wa_chain));
    wa_chain *elem=NULL;

    wa_rsetstatus(r,200,NULL);
    wa_rsetctype(r,"text/html");
    wa_rcommit(r);

    wa_rprintf(r,"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">");
    wa_rprintf(r,"\n\n");
    wa_rprintf(r,"<html>\n");
    wa_rprintf(r," <head>\n");
    wa_rprintf(r,"  <title>WebApp Library Configuration</title>");
    wa_rprintf(r," </head>\n");
    wa_rprintf(r," <body>\n");
    wa_rprintf(r,"  <div align=\"center\">\n");
    wa_rprintf(r,"  <table width=\"90%%\" border=\"1\" cellspacing=\"0\">\n");
    wa_rprintf(r,"   <tr>\n");
    wa_rprintf(r,"    <td align=\"center\" bgcolor=\"#ffcccc\">\n");
    wa_rprintf(r,"     <font size=\"+1\">\n");
    wa_rprintf(r,"      <b>WebApp Library Configuration</b>\n");
    wa_rprintf(r,"     </font>\n");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"   </tr>\n");
    wa_rprintf(r,"  </table>\n");
    wa_rprintf(r,"  <br>\n");
    wa_rflush(r);

    /* Process all virtual hosts and related applications (this will also
       add connections to the conn chain) */
    elem=wa_configuration;
    conn->curr=NULL;
    conn->next=NULL;
    while(elem!=NULL) {
        wa_virtualhost *curr=(wa_virtualhost *)elem->curr;
        info_handle_host(r,curr,conn);
        elem=elem->next;
    }

    /* Process all connections */
    wa_rprintf(r,"  <table width=\"80%%\" border=\"1\" cellspacing=\"0\">\n");
    wa_rprintf(r,"   <tr>\n");
    wa_rprintf(r,"    <td bgcolor=\"#ccffcc\" colspan=\"2\">\n");
    wa_rprintf(r,"     <b>Connections</b>");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"   </tr>\n");

    elem=conn->next;
    while(elem!=NULL) {
        wa_connection *curr=(wa_connection *)elem->curr;
        info_handle_connection(r,curr);
        elem=elem->next;
    }

    wa_rprintf(r,"  </table>\n");
    wa_rprintf(r,"  <br>\n");

#ifdef DEBUG
    /* See the request */
    wa_rprintf(r,"  <table width=\"80%%\" border=\"1\" cellspacing=\"0\">\n");
    wa_rprintf(r,"   <tr>\n");
    wa_rprintf(r,"    <td bgcolor=\"#cccccc\" colspan=\"2\">\n");
    wa_rprintf(r,"     <b>Your request</b>");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"   </tr>\n");

    /* A couple of forms for testing */
    k++;
    wa_rprintf(r,"   <tr>\n");
    wa_rprintf(r,"    <td width=\"100%%\" colspan=\"2\">\n");
    wa_rprintf(r,"     <form action=\"%s\" method=\"GET\">\n",r->ruri);
    wa_rprintf(r,"      <input type=\"hidden\" name=\"a\" value=\"%d.%d\">\n",
               getpid(),k);
    wa_rprintf(r,"      <input type=\"text\" name=\"b\" value=\"%s\">\n",
               "some random data...");
    wa_rprintf(r,"      <input type=\"submit\" value=\"Submit\">\n");
    wa_rprintf(r,"      Resubmit via GET\n");
    wa_rprintf(r,"     </form>\n");
    wa_rprintf(r,"     <form action=\"%s\" method=\"POST\">\n",r->ruri);
    wa_rprintf(r,"      <input type=\"hidden\" name=\"a\" value=\"%d.%d\">\n",
               getpid(),k);
    wa_rprintf(r,"      <input type=\"text\" name=\"b\" value=\"%s\">\n",
               "some random data...");
    wa_rprintf(r,"      <input type=\"submit\" value=\"Submit\">\n");
    wa_rprintf(r,"      Resubmit via POST\n");
    wa_rprintf(r,"     </form>\n");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"   </tr>\n");

    /* The request parameters */
    wa_rprintf(r,"   <tr>\n");
    wa_rprintf(r,"    <td width=\"10%%\" valign=\"top\" align=\"right\">\n");
    wa_rprintf(r,"     <font size=\"-1\">\n");
    wa_rprintf(r,"       Server&nbsp;Host<br>\n",r->serv->host);
    wa_rprintf(r,"       Server&nbsp;Address<br>\n",r->serv->addr);
    wa_rprintf(r,"       Server&nbsp;Port<br>\n",r->serv->port);
    wa_rprintf(r,"       Client&nbsp;Host<br>\n",r->clnt->host);
    wa_rprintf(r,"       Client&nbsp;Address<br>\n",r->clnt->addr);
    wa_rprintf(r,"       Client&nbsp;Port<br>\n",r->clnt->port);
    wa_rprintf(r,"       Request&nbsp;Method<br>\n",r->meth);
    wa_rprintf(r,"       Request&nbsp;URI<br>\n",r->ruri);
    wa_rprintf(r,"       Request&nbsp;Arguments<br>\n",r->args);
    wa_rprintf(r,"       Request&nbsp;Protocol<br>\n",r->prot);
    wa_rprintf(r,"       Request&nbsp;Scheme<br>\n",r->schm);
    wa_rprintf(r,"       Authenticated&nbsp;User<br>\n",r->user);
    wa_rprintf(r,"       Authentication&nbsp;Mechanism<br>\n",r->auth);
    wa_rprintf(r,"       Request&nbsp;Content&nbsp;Length<br>\n",r->clen);
    wa_rprintf(r,"     </font>\n");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"    <td width=\"90%%\" valign=\"top\" align=\"left\">\n");
    wa_rprintf(r,"     <font size=\"-1\"><nobr>\n");
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",r->serv->host);
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",r->serv->addr);
    wa_rprintf(r,"      <b><nobr>&quot;%d&quot;</nobr></b><br>\n",r->serv->port);
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",r->clnt->host);
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",r->clnt->addr);
    wa_rprintf(r,"      <b><nobr>&quot;%d&quot;</nobr></b><br>\n",r->clnt->port);
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",r->meth);
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",r->ruri);
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",r->args);
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",r->prot);
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",r->schm);
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",r->user);
    wa_rprintf(r,"      <b><nobr>&quot;%s&quot;</nobr></b><br>\n",r->auth);
    wa_rprintf(r,"      <b><nobr>&quot;%d&quot;</nobr></b>\n",r->clen);
    wa_rprintf(r,"     </font>\n");
    wa_rprintf(r,"    </td>\n");

    /* See the request headers */
    wa_rprintf(r,"   <tr>\n");
    wa_rprintf(r,"    <td width=\"100%%\" colspan=\"2\">\n");
    wa_rprintf(r,"     <font size=\"-1\">Headers</font>\n");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"   </tr>\n");
    wa_rprintf(r,"   <tr>\n");
    wa_rprintf(r,"    <td width=\"10%%\" valign=\"top\" align=\"right\">\n");
    wa_rprintf(r,"     <font size=\"-1\">\n");
    apr_table_do(info_handle_hdrname,r,r->hdrs,NULL);
    wa_rprintf(r,"     </font>\n");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"    <td width=\"90%%\" valign=\"top\" align=\"left\">\n");
    wa_rprintf(r,"     <font size=\"-1\">\n");
    apr_table_do(info_handle_hdrvalue,r,r->hdrs,NULL);
    wa_rprintf(r,"     </font>\n");
    wa_rprintf(r,"    </td>\n");
    wa_rprintf(r,"   </tr>\n");
    wa_rflush(r);

    /* Dump the request body */
    if (r->clen>0) {
        char *buf=(char *)apr_palloc(r->pool,1024*sizeof(char));
        int ret=1;

        wa_rprintf(r,"   <tr>\n");
        wa_rprintf(r,"    <td width=\"100%%\" colspan=\"2\">\n");
        wa_rprintf(r,"     <font size=\"-1\">Request Body</font>\n");
        wa_rprintf(r,"    </td>\n");
        wa_rprintf(r,"   </tr>\n");
        wa_rprintf(r,"   <tr>\n");
        wa_rprintf(r,"    <td width=\"100%%\" colspan=\"2\">\n");
        wa_rprintf(r,"     <font size=\"-1\">\n");
        wa_rprintf(r,"      <pre>\n");

        while (ret>0) {
            ret=wa_rread(r,buf,1024);
            if (ret>0) {
                wa_rwrite(r,buf,ret);
                wa_rflush(r);
            } else if (ret<0) {
                wa_rprintf(r,">\n<b>TRANSFER INTERRUPTED</b>\n");
            }
        }

        wa_rprintf(r,"      </pre>\n");
        wa_rprintf(r,"     </font>\n");
        wa_rprintf(r,"    </td>\n");
        wa_rprintf(r,"   </tr>\n");
    } else {
        wa_rprintf(r,"   <tr>\n");
        wa_rprintf(r,"    <td width=\"100%%\" colspan=\"2\">\n");
        wa_rprintf(r,"     <font size=\"-1\"><i>No Request Body</i></font>\n");
        wa_rprintf(r,"    </td>\n");
        wa_rprintf(r,"   </tr>\n");
    }

    wa_rprintf(r,"  </table>\n");
#endif /* ifdef DEBUG */
    wa_rprintf(r,"  <br>\n");

    wa_rprintf(r,"  </div>\n");
    wa_rprintf(r,"  <br>\n");

    wa_rprintf(r," </body>\n");
    wa_rprintf(r,"</html>\n");
    wa_rflush(r);

    return(200);
}

/* The INFO provider structure */
wa_provider wa_provider_info = {
    "info",
    info_init,
    info_startup,
    info_shutdown,
    info_connect,
    info_deploy,
    info_conninfo,
    info_applinfo,
    info_handle,
};
