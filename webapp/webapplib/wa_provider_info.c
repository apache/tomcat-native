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
    if (param==NULL) conn->conf=strdup("[No data supplied]");
    else conn->conf=strdup(param);
    return(NULL);
}

/**
 * Describe the configuration member found in a connection.
 *
 * @param conn The connection for wich a description must be produced.
 * @param buf The buffer where the description must be stored.
 * @param len The buffer length.
 * @return The number of bytes written to the buffer.
 */
static int wa_info_conninfo(wa_connection *conn, char *buf, int len) {
    if ((buf==NULL)||(len==0)) return(0);

    if(conn==NULL) return(strlcpy(buf,"Null connection specified",len));
    return(strlcpy(buf,conn->conf,len));
}

/**
 * Describe the configuration member found in a web application.
 *
 * @param appl The application for wich a description must be produced.
 * @param buf The buffer where the description must be stored.
 * @param len The buffer length.
 * @return The number of bytes written to the buffer.
 */
static int wa_info_applinfo(wa_application *conn, char *buf, int len) {
    return(0);
}

/**
 * Initialize a connection.
 *
 * @param conn The connection to initialize.
 */
static void wa_info_init(wa_connection *conn) {
    return;
}

/**
 * Destroys a connection.
 *
 * @param conn The connection to destroy.
 */
static void wa_info_destroy(wa_connection *conn) {
    return;
}

/**
 * Handle a connection from the web server.
 *
 * @param req The request data.
 * @param cb The web-server callback information.
 */
void wa_info_handle(wa_request *req, wa_callbacks *cb) {
    int x=0;
    wa_connection *conn=wa_connections;
    wa_host *host=wa_hosts;

    wa_callback_setstatus(cb,req,200);
    wa_callback_settype(cb,req,"text/html");
    wa_callback_commit(cb,req);
    wa_callback_printf(cb,req,"<html>\n");
    wa_callback_printf(cb,req," <head>\n");
    wa_callback_printf(cb,req,"  <title>mod_webapp: status</title>\n");
    wa_callback_printf(cb,req," </head>\n");
    wa_callback_printf(cb,req," <body>\n");
    
    // Dump configured connections
    while (conn!=NULL) {
        char desc[1024];

        wa_callback_printf(cb,req,"  <dl>\n");
        wa_callback_printf(cb,req,"   <dt><b>Connection: %s</b></dt>\n",
                           conn->name);
        wa_callback_printf(cb,req,"   <dd>\n");
        wa_callback_printf(cb,req,"    Provider &quot;%s&quot;\n",
                           conn->prov->name);
        if ((*conn->prov->conninfo)(conn,desc,1024)>0)
            wa_callback_printf(cb,req,"    (Descr.: &quot;%s&quot;)\n",desc);
        else
            wa_callback_printf(cb,req,"    (No description available)\n");
        wa_callback_printf(cb,req,"   </dd>\n");
        conn=conn->next;
        wa_callback_printf(cb,req,"  </dl>\n");
    }


    // Dump configured hosts and applications
    while (host!=NULL) {
        wa_application *appl=host->apps;

        wa_callback_printf(cb,req,"  <dl>\n");
        wa_callback_printf(cb,req,"   <dt><b>Host: %s:%d</b></dt>\n",
                           host->name,host->port);
        while (appl!=NULL) {
            char d[1024];

            wa_callback_printf(cb,req,"   <dd>\n");
            wa_callback_printf(cb,req,"    Application &quot;%s&quot;\n",
                               appl->name);
            wa_callback_printf(cb,req,"    mounted under &quot;%s&quot;\n",
                               appl->path);
            wa_callback_printf(cb,req,"    using connection &quot;%s&quot;\n",
                               appl->conn->name);

            // Get provider specific description of the application
            if ((*appl->conn->prov->applinfo)(appl,d,1024)>0)
                wa_callback_printf(cb,req,"    (Descr.: &quot;%s&quot;)\n",d);
            else
                wa_callback_printf(cb,req,"    (No description available)\n");

            wa_callback_printf(cb,req,"   </dd>\n");
            appl=appl->next;
        }
        host=host->next;
        wa_callback_printf(cb,req,"  </dl>\n");
    }

    // Dump the first line of the request
    wa_callback_printf(cb,req,"  <dl>\n");
    wa_callback_printf(cb,req,"   <dt><b>This request:</b></dt>\n");
    wa_callback_printf(cb,req,"   <dd>\n");
    wa_callback_printf(cb,req,"    <code>\n");
    wa_callback_printf(cb,req,"     %s",req->meth);
    wa_callback_printf(cb,req," %s",req->ruri);
    if (req->args!=NULL) wa_callback_printf(cb,req,"?%s",req->args);
    wa_callback_printf(cb,req," %s<br>\n",req->prot);

    // Dump the first line of the request
    for (x=0; x<req->hnum; x++)
        wa_callback_printf(cb,req,"     %s: %s<br>",req->hnam[x],
                                                    req->hval[x]);

    // Finish the request dump
    wa_callback_printf(cb,req,"    </code>\n");
    wa_callback_printf(cb,req,"   </dd>\n");
    wa_callback_printf(cb,req,"  </dl>\n");
    
    // Finish the page
    wa_callback_printf(cb,req," </body>\n");
    wa_callback_printf(cb,req,"<html>\n");
    wa_callback_flush(cb,req);
}

/** WebAppLib plugin description. */
wa_provider wa_provider_info = {
    "info",
    wa_info_configure,
    wa_info_init,
    wa_info_destroy,
    wa_info_handle,
    wa_info_conninfo,
    wa_info_applinfo,
};
