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
/* Callback structure declaration */
static wa_callbacks webapp_callbacks;


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


/* ************************************************************************* */
/* WEB-APPLICATION INITIALIZATION AND REQUEST HANDLING                       */
/* ************************************************************************* */

/**
 * "Translate" (or better, match) an HTTP request into a webapp request.
 *
 * @param r The Apache request structure.
 */
static int webapp_translate(request_rec *r) {
    wa_host *host=NULL;
    wa_application *appl=NULL;
    wa_request *req=NULL;

    // Check if this host was recognized
    host=wa_host_get(r->server->server_hostname, r->server->port);
    if (host==NULL) return(DECLINED);

    // Check if the uri is contained in one of our web applications root path
    appl=wa_host_findapp(host, r->uri);
    if (appl==NULL) return(DECLINED);

    // The uri path is matched by the application, set the handler and return
    r->handler=ap_pstrdup(r->pool,"webapp-handler");

    // Create a new request structure
    req=(wa_request *)ap_palloc(r->pool,sizeof(wa_request));
    req->host=host;
    req->application=appl;

    // Set the webapp request structure into Apache's request structure
    ap_set_module_config(r->request_config, &webapp_module, req);
    return(OK);
}

/**
 * Handle a request thru a configured web application.
 *
 * @param r The Apache request structure.
 */
static int webapp_handler(request_rec *r) {
    const char *message=NULL;
    wa_request *req=NULL;

    // Try to get a hold on the webapp request structure
    req=(wa_request *)ap_get_module_config(r->request_config, &webapp_module);
    if (req==NULL) return(DECLINED);

    // Set up basic parameters in the request structure
    req->data=r;
    req->method=(char *)r->method;
    req->uri=r->uri;
    req->arguments=r->args;
    req->protocol=r->protocol;

    // Copy headers into webapp request structure
    if (!r->headers_in) {
        array_header *arr=ap_table_elts(r->headers_in);
        table_entry *ele=(table_entry *)arr->elts;
        int count=arr->nelts;
        int x=0;

        // Allocate memory for pointers
        req->header_names=(char **)ap_palloc(r->pool,count*sizeof(char *));
        req->header_values=(char **)ap_palloc(r->pool,count*sizeof(char *));

        // Copy header pointers one by one
        for (x=0; x<count;x++) {
            if (ele[x].key==NULL) continue;
            req->header_names[x]=ele[x].key;
            req->header_values[x]=ele[x].val;
        }
    }

    // Try to handle the request
    message=wa_request_handle(req,&webapp_callbacks);

    // We got an error message (critical error, not from providers)
    if (message!=NULL) {
        ap_log_error(APLOG_MARK,APLOG_CRIT,r->server,"%s",message);
        return(HTTP_INTERNAL_SERVER_ERROR);
    }

    return OK;
}

/**
 * Destroy webapp connections.
 */
static void webapp_destroy(void *k) {
    wa_connection_destroy();
}

/**
 * Initialize webapp connections.
 */
static void webapp_init(server_rec *s, pool *p) {
    // Register our cleanup function
#ifdef WIN32
    // Under Win32 we clean up when the process exits, since web server
    // children are threads (sockets, connections and all the rest resides
    // in the same memory space.
    ap_register_cleanup(p, NULL, webapp_destroy, ap_null_cleanup);
#else
    // Under UNIX we clean up when a child exists, since web server children
    // are processes, and not threads.
    ap_register_cleanup(p, NULL, ap_null_cleanup, webapp_destroy);
#endif

    // Initialize connections
    wa_connection_init();
}

/* ************************************************************************* */
/* WEBAPP LIBRARY CALLBACK FUNCTIONS                                         */
/* ************************************************************************* */

/**
 * Log data on the web server log file.
 *
 * @param file The source file of this log entry.
 * @param line The line number within the source file of this log entry.
 * @param data The web-server specific data (wa_request->data).
 * @param fmt The format string (printf style).
 * @param ... All other parameters (if any) depending on the format.
 */
static void webapp_callback_log(void *data, const char *file, int line,
                                const char *fmt, ...) {
    request_rec *r=(request_rec *)data;
    va_list ap;

    va_start (ap,fmt);
    if (r==NULL) {
        fprintf(stderr,"[%s:%d] ",file,line);
        vfprintf(stderr,fmt,ap);
        fprintf(stderr,"\n");
    } else {
        char *message=ap_pvsprintf(r->pool,fmt,ap);
        ap_log_error(file,line,APLOG_ERR,r->server,"%s",message);
    }
}

/**
 * Allocate memory while processing a request.
 *
 * @param data The web-server specific data (wa_request->data).
 * @param size The size in bytes of the memory to allocate.
 * @return A pointer to the allocated memory or NULL.
 */
static void *webapp_callback_alloc(void *data, int size) {
    request_rec *r=(request_rec *)data;

    if (r==NULL) {
        webapp_callback_log(r,WA_LOG,"Apache request_rec pointer is NULL");
        return(NULL);
    }
    return(ap_palloc(r->pool,size));
}

/**
 * Read part of the request content.
 *
 * @param data The web-server specific data (wa_request->data).
 * @param buf The buffer that will hold the data.
 * @param len The buffer length.
 * @return The number of bytes read, 0 on end of file or -1 on error.
 */
static int webapp_callback_read(void *data, char *buf, int len) {
    request_rec *r=(request_rec *)data;

    if (r==NULL) {
        webapp_callback_log(r,WA_LOG,"Apache request_rec pointer is NULL");
        return(-1);
    }

    return((int)ap_get_client_block(r,buf,len));
}

/**
 * Set the HTTP response status code.
 *
 * @param data The web-server specific data (wa_request->data).
 * @param status The HTTP status code for the response.
 * @return TRUE on success, FALSE otherwise
 */
static boolean webapp_callback_setstatus(void *data, int status) {
    request_rec *r=(request_rec *)data;

    if (r==NULL) {
        webapp_callback_log(r,WA_LOG,"Apache request_rec pointer is NULL");
        return(FALSE);
    }

    r->status=status;
    return(TRUE);
}

/**
 * Set the HTTP response mime content type.
 *
 * @param data The web-server specific data (wa_request->data).
 * @param type The mime content type of the HTTP response.
 * @return TRUE on success, FALSE otherwise
 */
static boolean webapp_callback_settype(void *data, char *type) {
    request_rec *r=(request_rec *)data;

    if (r==NULL) {
        webapp_callback_log(r,WA_LOG,"Apache request_rec pointer is NULL");
        return(FALSE);
    }

    r->content_type=type;
    return(TRUE);
}

/**
 * Set an HTTP mime header.
 *
 * @param data The web-server specific data (wa_request->data).
 * @param name The mime header name.
 * @param value The mime header value.
 * @return TRUE on success, FALSE otherwise
 */
static boolean webapp_callback_setheader(void *data, char *name, char *value) {
    request_rec *r=(request_rec *)data;

    if (r==NULL) {
        webapp_callback_log(r,WA_LOG,"Apache request_rec pointer is NULL");
        return(FALSE);
    }

    ap_table_add(r->headers_out, name, value);
    return(TRUE);
}

/**
 * Commit the first part of the response (status and headers).
 *
 * @param data The web-server specific data (wa_request->data).
 * @return TRUE on success, FALSE otherwise
 */
static boolean webapp_callback_commit(void *data) {
    request_rec *r=(request_rec *)data;

    if (r==NULL) {
        webapp_callback_log(r,WA_LOG,"Apache request_rec pointer is NULL");
        return(FALSE);
    }

    ap_send_http_header(r);
    return(TRUE);
}

/**
 * Write part of the response data back to the client.
 *
 * @param buf The buffer holding the data to be written.
 * @param len The number of characters to be written.
 * @return The number of characters written to the client or -1 on error.
 */
static int webapp_callback_write(void *data, char *buf, int len) {
    request_rec *r=(request_rec *)data;

    if (r==NULL) {
        webapp_callback_log(r,WA_LOG,"Apache request_rec pointer is NULL");
        return(FALSE);
    }

    return(ap_rwrite(buf, len, r));
}

/**
 * Flush any unwritten response data to the client.
 *
 * @param data The web-server specific data (wa_request->data).
 * @return TRUE on success, FALSE otherwise
 */
static boolean webapp_callback_flush(void *data) {
    request_rec *r=(request_rec *)data;

    if (r==NULL) {
        webapp_callback_log(r,WA_LOG,"Apache request_rec pointer is NULL");
        return(FALSE);
    }

    ap_rflush(r);
    return(TRUE);
}


/* ************************************************************************* */
/* STRUCTURES, FUNCTIONS AND DATA POINTERS                                   */
/* ************************************************************************* */

/* Our callback functions structure */
static wa_callbacks webapp_callbacks = {
    webapp_callback_log,
    webapp_callback_alloc,
    webapp_callback_read,
    webapp_callback_setstatus,
    webapp_callback_settype,
    webapp_callback_setheader,
    webapp_callback_commit,
    webapp_callback_write,
    webapp_callback_flush,
};

/* List of all available configuration directives */
static const command_rec webapp_commands[] = {
    {
        "WebAppConnection",         // directive name
        webapp_config_connection,   // config action routine
        NULL,                       // argument to include in call
        RSRC_CONF,                  // where available
        TAKE23,                     // arguments
        "<name> <connector> [optional parameter]"
    }, {
        "WebAppMount",              // directive name
        webapp_config_mount,        // config action routine
        NULL,                       // argument to include in call
        RSRC_CONF,                  // where available
        TAKE3,                      // arguments
        "<name> <connection> <uri-path>"
    }, {NULL}
};

/* List of all available Apache handlers */
static const handler_rec webapp_handlers[] = {
    {"webapp-handler", webapp_handler},
    {NULL}
};

/* Apache module declaration */
module webapp_module = {
    STANDARD_MODULE_STUFF,
#ifdef WIN32
    webapp_init,                        /* module initializer */
#else
    NULL,                               /* module initializer */
#endif
    NULL,                               /* per-directory config creator */
    NULL,                               /* dir config merger */
    NULL,                               /* server config creator */
    NULL,                               /* server config merger */
    webapp_commands,                    /* command table */
    webapp_handlers,                    /* [9] list of handlers */
    webapp_translate,                   /* [2] filename-to-URI translation */
    NULL,                               /* [5] check/validate user_id */
    NULL,                               /* [6] check user_id is valid *here* */
    NULL,                               /* [4] check access by host address */
    NULL,                               /* [7] MIME type checker/setter */
    NULL,                               /* [8] fixups */
    NULL,                               /* [10] logger */
    NULL,                               /* [3] header parser */
#ifdef WIN32
    NULL,                               /* child initializer */
#else
    webapp_init,                        /* child initializer */
#endif
    NULL,                               /* child exit/cleanup */
    NULL                                /* [1] post read_request handling */
};
