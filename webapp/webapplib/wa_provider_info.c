/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *         Copyright (c) 1999, 2000  The Apache Software Foundation.         *
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
 * 4. The names  "The  Jakarta  Project",  "Tomcat",  and  "Apache  Software *
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

// CVS $Id$
// Author: Pier Fumagalli <mailto:pier.fumagalli@eng.sun.com>

#include <wa.h>

/**
 * Configure a connection with the parameter from the web server configuration
 * file.
 *
 * @param conn The connection to configure.
 * @param param The extra parameter from web server configuration.
 * @return An error message or NULL.
 */
static const char *wa_info_configure(wa_connection *conn, char *param) {
    if(conn==NULL) return("Connection not specified");
    if (param==NULL) conn->conf=NULL;
    else conn->conf=strdup(param);
    return(NULL);
}

/**
 * Initialize a connection.
 *
 * @param conn The connection to initialize.
 */
static void wa_info_init(wa_connection *conn) {
    wa_callback_debug(WA_LOG,NULL,"Initializing connection %s",conn->name);
}

/**
 * Destroys a connection.
 *
 * @param conn The connection to destroy.
 */
static void wa_info_destroy(wa_connection *conn) {
    wa_callback_debug(WA_LOG,NULL,"Destroying connection %s",conn->name);
}

/**
 * Describe the configuration member found in a connection.
 *
 * @param conn The connection for wich a description must be produced.
 * @param buf The buffer where the description must be stored.
 * @param len The buffer length.
 * @return The number of bytes written to the buffer (terminator included).
 */
static int wa_info_conninfo(wa_connection *conn, char *buf, int len) {
    int x=0;
    char *msg="Null connection specified\0";
    return(0);

    if ((buf==NULL)||(len==0)) return(0);
    if(conn!=NULL) msg=(char *)conn->conf;
    if (msg==NULL) msg="No extra parameters specified for this connection\0";

    // Copy the message string in the buffer and return
    for (x=0; x<len; x++) {
        buf[x]=msg[x];
        if (msg[x]=='\0') return(x+1);
    }
    buf[x-1]='\0';
    return(x);
}

/**
 * Describe the configuration member found in a web application.
 *
 * @param appl The application for wich a description must be produced.
 * @param buf The buffer where the description must be stored.
 * @param len The buffer length.
 * @return The number of bytes written to the buffer (terminator included).
 */
static int wa_info_applinfo(wa_application *conn, char *buf, int len) {
    return(0);
}

/**
 * Handle a connection from the web server.
 *
 * @param req The request data.
 */
void wa_info_handle(wa_request *req) {
    int x=0;
    int ret=0;
    char *buf;
    time_t t=0;
    char *ts=NULL;
    wa_connection *conn=wa_connections;
    wa_host *host=wa_hosts;

    time(&t);
    ts=ctime(&t);
    if (ts==NULL) ts=strdup("[Unknown generation time]");
    else ts[24]='\0';

    wa_callback_setstatus(req,200);
    wa_callback_settype(req,"text/html");
    wa_callback_commit(req);
    wa_callback_printf(req,"<html>\n");
    wa_callback_printf(req," <head>\n");
    wa_callback_printf(req,"  <title>mod_webapp: status</title>\n");
    wa_callback_printf(req," </head>\n");
    wa_callback_printf(req," <body>\n");
    wa_callback_printf(req,"  <form action=\"%s://%s:%d%s\"",req->schm,
                       req->name,req->port,req->ruri);
    wa_callback_printf(req," method=\"post\">\n");
    wa_callback_printf(req,"   <input type=\"submit\" value=\"Refresh\">\n");
    wa_callback_printf(req,"   <input type=\"hidden\" name=\"lastcall\"");
    wa_callback_printf(req," value=\"%s\">\n",ts);
    wa_callback_printf(req,"   Generated on %s<br>\n",ts);
    wa_callback_printf(req,"  </form>\n");
    free(ts);
    wa_callback_flush(req);

    wa_callback_printf(req,"  <dl>\n");
    wa_callback_printf(req,"   <dt><b>Connections:</b></dt>\n");
    // Dump configured connections
    while (conn!=NULL) {
        char desc[1024];

        wa_callback_printf(req,"   <dd>\n");
        wa_callback_printf(req,"    Connection &quot;%s&quot;\n",conn->name);
        wa_callback_printf(req,"    Prov. &quot;%s&quot;\n",conn->prov->name);
        if ((*conn->prov->conninfo)(conn,desc,1024)>0)
            wa_callback_printf(req,"    (%s)\n",desc);
        else
            wa_callback_printf(req,"    [No description available]\n");
        wa_callback_printf(req,"   </dd>\n");
        wa_callback_flush(req);
        conn=conn->next;
    }
    wa_callback_printf(req,"  </dl>\n");
    wa_callback_flush(req);

    // Dump configured hosts and applications
    while (host!=NULL) {
        wa_application *appl=host->apps;

        wa_callback_printf(req,"  <dl>\n");
        wa_callback_printf(req,"   <dt><b>Host: %s:%d</b></dt>\n",
                           host->name,host->port);
        wa_callback_printf(req,"   <dd>\n");
        while (appl!=NULL) {
            char d[1024];

            wa_callback_printf(req,"    Application &quot;%s&quot;\n",
                               appl->name);
            wa_callback_printf(req,"    mounted under &quot;%s&quot;\n",
                               appl->path);
            wa_callback_printf(req,"    using connection &quot;%s&quot;\n",
                               appl->conn->name);
            wa_callback_flush(req);

            // Get provider specific description of the application
            if ((*appl->conn->prov->applinfo)(appl,d,1024)>0)
                wa_callback_printf(req,"    (%s)<br>\n",d);
            else
                wa_callback_printf(req,"    [No description available]<br>\n");

            wa_callback_printf(req,"   </dd>\n");
            wa_callback_flush(req);
            appl=appl->next;
        }
        wa_callback_printf(req,"  </dl>\n");
        wa_callback_flush(req);
        host=host->next;
    }
    wa_callback_flush(req);

    // Dump the first line of the request
    wa_callback_printf(req,"  <dl>\n");
    wa_callback_printf(req,"   <dt><b>This Request (%d bytes):</b></dt>\n",
                       req->clen);
    wa_callback_printf(req,"   <dd>\n");
    wa_callback_printf(req,"    Request URI: &quot;%s://%s:%d%s",
                       req->schm,req->name,req->port,req->ruri);
    if (req->args==NULL) wa_callback_printf(req,"&quot;<br>\n");
    else wa_callback_printf(req,"?%s&quot;<br>\n",req->args);
    wa_callback_printf(req,"    Configured Host: &quot;%s:%d&quot;<br>\n",
                       req->host->name,req->host->port);
    wa_callback_printf(req,"    Requested Host: &quot;%s:%d&quot;<br>\n",
                       req->name,req->port);
    wa_callback_printf(req,"    Remote Host: &quot;%s&quot;<br>\n",
                       req->rhst==NULL?"[NULL]":req->rhst);
    wa_callback_printf(req,"    Remote Address: &quot;%s&quot;<br>\n",
                       req->radr==NULL?"[NULL]":req->radr);
    wa_callback_printf(req,"    Remote User: &quot;%s&quot;<br>\n",
                       req->user==NULL?"[NULL]":req->user);
    wa_callback_printf(req,"    Authentication Method: &quot;%s&quot;<br>\n",
                       req->auth==NULL?"[NULL]":req->auth);
    wa_callback_printf(req,"    <br>\n");
    wa_callback_printf(req,"    <code>\n");
    wa_callback_printf(req,"     %s",req->meth);
    wa_callback_printf(req," %s",req->ruri);
    if (req->args!=NULL) wa_callback_printf(req,"?%s",req->args);
    wa_callback_printf(req," %s<br>\n",req->prot);
    wa_callback_printf(req,"    <br>\n");
    wa_callback_flush(req);

    // Dump the request headers
    for (x=0; x<req->hnum; x++) {
        wa_callback_printf(req,"     %s: %s<br>\n",req->hnam[x],
                                                    req->hval[x]);
    }
    wa_callback_flush(req);

    // Dump the request body
    wa_callback_printf(req,"    </code>\n");
    if (req->clen>0) {
        wa_callback_printf(req,"<pre>\n");
        buf=(char *)wa_callback_alloc(req,1024*sizeof(char));
        ret=1;
        while (ret>0) {
            ret=wa_callback_read(req,buf,1024);
            if (ret>0) {
                wa_callback_write(req,buf,ret);
                wa_callback_flush(req);
            } else if (ret<0) {
                wa_callback_printf(req,">\n<b>TRANSFER INTERRUPTED</b>\n");
            }
        }
        wa_callback_printf(req,"\n</pre>\n");
    }

    // Finish the request dump
    wa_callback_printf(req,"   </dd>\n");
    wa_callback_printf(req,"  </dl>\n");

    // Finish the page
    wa_callback_printf(req," </body>\n");
    wa_callback_printf(req,"<html>\n");
    wa_callback_flush(req);
}

/** WebAppLib plugin description. */
wa_provider wa_provider_info = {
    "info",
    wa_info_configure,
    wa_info_init,
    wa_info_destroy,
    wa_info_conninfo,
    wa_info_applinfo,
    wa_info_handle,
};
