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

/**
 * @author  Pier Fumagalli <mailto:pier.fumagalli@eng.sun.com>
 * @version $Id$
 */

#include <httpd.h>
#include <http_config.h>
#include <http_core.h>
#include <http_log.h>
#include <http_main.h>
#include <http_protocol.h>
#include <util_script.h>
#include <wa.h>

/* ************************************************************************* */
/* GENERIC DECLARATIONS                                                      */
/* ************************************************************************* */

/* Module declaration */
module webapp_module;

/* ************************************************************************* */
/* CONFUGURATION ROUTINES                                                    */
/* ************************************************************************* */

/**
 * Configure a webapp connection.
 *
 * @param name The connection unique name.
 * @param connector The name of the WebApp connector to use.
 * @param parameters Connector-specific parameters (can be NULL).
 */
static const char *webapp_config_connection (cmd_parms *cmd, void *mconfig,
                                             char *name, char *provider,
                                             char *parameter) {
    return(wa_connection_create(name, provider, parameter));
}

/**
 * Configure a webapp application mount point.
 *
 * @param name The web application name.
 * @param cname The connection name.
 * @param path The web application root path.
 */
static const char *webapp_config_mount (cmd_parms *cmd, void *mconfig,
                                        char *name, char *cname, char *path) {
    server_rec *s=cmd->server;
    wa_host *host=NULL;
    wa_connection *conn=NULL;
    const char *mesg=NULL;

    // Retrieve (or create) our host structure
    host=wa_host_get(s->server_hostname,s->port);
    if (host==NULL) {
        mesg=wa_host_create(s->server_hostname,s->port);
        if (mesg!=NULL) return(mesg);
        host=wa_host_get(s->server_hostname,s->port);
        if (host==NULL) return("Cannot retrieve host information");
    }

    // Setup the webapp name, connection and path
    conn=wa_connection_get(cname);
    if (conn==NULL) return("Invalid connection name specified");
    return(wa_host_setapp(host, name, path, conn));

    return (NULL);
}


/* Apache module declaration */
module webapp_module = {
    STANDARD_MODULE_STUFF,
    NULL,                               /* module initializer */
    NULL,                               /* per-directory config creator */
    NULL,                               /* dir config merger */
    NULL,                               /* server config creator */
    NULL,                               /* server config merger */
    NULL, //webapp_commands,                    /* command table */
    NULL, //webapp_handlers,                    /* [9] list of handlers */
    NULL, //webapp_translate,                   /* [2] filename-to-URI translation */
    NULL,                               /* [5] check/validate user_id */
    NULL,                               /* [6] check user_id is valid *here* */
    NULL,                               /* [4] check access by host address */
    NULL,                               /* [7] MIME type checker/setter */
    NULL,                               /* [8] fixups */
    NULL,                               /* [10] logger */
    NULL,                               /* [3] header parser */
    NULL, //webapp_init,                        /* child initializer */
    NULL, //webapp_exit,                        /* child exit/cleanup */
    NULL                                /* [1] post read_request handling */
};
