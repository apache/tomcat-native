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
static wa_callback webapp_callbacks;
/* Our server (used to log error messages from the library) */
static server_rec *webapp_server=NULL;


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
    req->appl=appl;

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
    conn_rec *c = r->connection;
    const char *msg=NULL;
    wa_request *req=NULL;
    int ret=0;

    // Try to get a hold on the webapp request structure
    req=(wa_request *)ap_get_module_config(r->request_config, &webapp_module);
    if (req==NULL) return(DECLINED);

    // Set up basic parameters in the request structure
    req->data=r;
    req->meth=(char *)r->method;
    req->ruri=r->uri;
    req->args=r->args;
    req->prot=r->protocol;
    req->schm=ap_http_method(r);
    req->name=(char *)r->hostname;
    req->port=ap_get_server_port(r);
    req->rhst=(char *)ap_get_remote_host(c, r->per_dir_config, REMOTE_HOST);
    req->radr=c->remote_ip;
    req->user=c->user;
    req->auth=c->ap_auth_type;
    req->clen=0;
    req->rlen=0;

    // Copy headers into webapp request structure
    if (r->headers_in!=NULL) {
        array_header *arr=ap_table_elts(r->headers_in);
        table_entry *ele=(table_entry *)arr->elts;
        int count=arr->nelts;
        int x=0;

        // Allocate memory for pointers
        req->hnum=count;
        req->hnam=(char **)ap_palloc(r->pool,count*sizeof(char *));
        req->hval=(char **)ap_palloc(r->pool,count*sizeof(char *));

        // Copy header pointers one by one
        for (x=0; x<count;x++) {
            if (ele[x].key==NULL) continue;
            req->hnam[x]=ele[x].key;
            req->hval[x]=ele[x].val;
            if (strcasecmp(ele[x].key,"Content-Length")==0)
                req->clen=atol(req->hval[x]);
        }
    } else {
        req->hnum=0;
        req->hnam=NULL;
        req->hval=NULL;
    }

    // Check if we can read something from the request
    ret=ap_setup_client_block(r,REQUEST_CHUNKED_DECHUNK);
    if (ret!=OK) return(ret);

    // Try to handle the request
    msg=wa_request_handle(req);

    // We got an error message (critical error, not from providers)
    if (msg!=NULL) {
        ap_log_error(APLOG_MARK,APLOG_NOERRNO|APLOG_ERR,r->server,"%s",msg);
        return(HTTP_INTERNAL_SERVER_ERROR);
    }

    return OK;
}

/**
 * Initialize the webapp library.
 *
 * @param s The server_rec structure associated with the main server.
 * @param p The pool for memory allocation (it never gets cleaned).
 */
static void webapp_init(server_rec *s, pool *p) {
    webapp_server=s;
    wa_init(&webapp_callbacks);
}

/**
 * Initialize webapp connections.
 *
 * @param s The server_rec structure associated with the main server.
 * @param p The pool for memory allocation (it never gets cleaned).
 */
static void webapp_exit(server_rec *s, pool *p) {
    wa_destroy();
}

/* ************************************************************************* */
/* WEBAPP LIBRARY CALLBACK FUNCTIONS                                         */
/* ************************************************************************* */

/**
 * Add a component to the the current SERVER_SOFTWARE string and return the
 * new value.
 *
 * @param component The new component to add to the SERVER_SOFTWARE string
 *                  or NULL if no modification is necessary.
 * @return The updated SERVER_SOFTWARE string.
 */
const char *webapp_callback_serverinfo(const char *component) {
    const char *ret=NULL;

    if (component!=NULL) ap_add_version_component(component);

    ret=ap_get_server_version();
    if (ret==NULL) return("Unknown");
    return(ret);
}

/**
 * Print a debug or log message to the apache error log file.
 *
 * @param f The source file of this log entry.
 * @param l The line number within the source file of this log entry.
 * @param r The wa_request structure associated with the current request,
 *          or NULL.
 * @param msg The message to be logged.
 */
static void webapp_callback_log(const char *f,int l,wa_request *r,char *msg) {
    server_rec *s=webapp_server;

    // Try to get the current request server_rec member.
    if (r!=NULL)
        if (r->data!=NULL)
            s=((request_rec *)r->data)->server;

    if (s==NULL) {
        // We don't yet know our server_rec, simply dump to stderr and hope :)
#ifdef DEBUG
        fprintf(stderr,"[%s:%l] %s\n",f,l,msg);
#else
        fprintf(stderr,"%s\n",msg);
#endif
    } else {
#ifdef DEBUG
        // We are debugging, so we want to make sure that file and line are in
        // our error message.
        ap_log_error(f,l,APLOG_NOERRNO|APLOG_ERR,s,"[%s:%l] %s",f,l,msg);
#else
        // We are not debugging, so let Apache handle the file and line
        ap_log_error(f,l,APLOG_NOERRNO|APLOG_ERR,s,"%s",msg);
#endif
    }
}

/**
 * Return the current request_rec structure from a wa_request structure and log
 * if we weren't able to retrieve it.
 *
 * @param f The file from where this function was called.
 * @param l The line from where this function was called.
 * @param r The wa_request structure.
 * @return A request_rec pointer or NULL.
 */
static request_rec *webapp_callback_check(const char *f,int l,wa_request *r) {
    if (r==NULL) {
        webapp_callback_log(f,l,r, "Invalid wa_request member (NULL)");
        return(NULL);
    }
    if (r->data==NULL) {
        webapp_callback_log(f,l,r, "Invalid wa_request->data member (NULL)");
        return(NULL);
    }
    return((request_rec *)r->data);
}

/**
 * Allocate memory while processing a request.
 *
 * @param req The request member associated with this call.
 * @param size The size in bytes of the memory to allocate.
 * @return A pointer to the allocated memory or NULL.
 */
static void *webapp_callback_alloc(wa_request *req, int size) {
    request_rec *r=webapp_callback_check(WA_LOG,req);

    if (r==NULL) return(NULL);
    return(ap_palloc(r->pool,size));
}

/**
 * Read part of the request content.
 *
 * @param req The request member associated with this call.
 * @param buf The buffer that will hold the data.
 * @param len The buffer length.
 * @return The number of bytes read, 0 on end of file or -1 on error.
 */
static int webapp_callback_read(wa_request *req, char *buf, int len) {
    request_rec *r=webapp_callback_check(WA_LOG,req);
    long ret=0;

    if (r==NULL) return(-1);

    // Check if we have something to read.
    if (req->clen==0) return(0);

    // Check if we had an error previously.
    if (req->rlen==-1) return(-1);

    // Send HTTP_CONTINUE to client when we're ready to read for the first time.
    if (req->rlen==0)
        if (ap_should_client_block(r)==0) return(0);

    // Read some data from the client and fill the buffer.
    ret=ap_get_client_block(r,buf,len);
    if (ret==-1) {
        req->rlen=-1;
        return(-1);
    }

    // We did read some bytes, increment the current rlen counter and return.
    req->rlen+=ret;
    return((int)ret);
}

/**
 * Set the HTTP response status code.
 *
 * @param req The request member associated with this call.
 * @param status The HTTP status code for the response.
 * @return TRUE on success, FALSE otherwise
 */
static boolean webapp_callback_setstatus(wa_request *req, int status) {
    request_rec *r=webapp_callback_check(WA_LOG,req);

    if (r==NULL) return(FALSE);

    r->status=status;
    return(TRUE);
}

/**
 * Set the HTTP response mime content type.
 *
 * @param req The request member associated with this call.
 * @param type The mime content type of the HTTP response.
 * @return TRUE on success, FALSE otherwise
 */
static boolean webapp_callback_settype(wa_request *req, char *type) {
    request_rec *r=webapp_callback_check(WA_LOG,req);
    char *t="";

    if (r==NULL) return(FALSE);
    if (t!=NULL) t=ap_pstrdup(r->pool,type);

    r->content_type=t;
    ap_table_add(r->headers_out, "Content-Type", t);
    return(TRUE);
}

/**
 * Set an HTTP mime header.
 *
 * @param req The request member associated with this call.
 * @param name The mime header name.
 * @param value The mime header value.
 * @return TRUE on success, FALSE otherwise
 */
static boolean webapp_callback_setheader(wa_request *req, char *name,
                                         char *value) {
    request_rec *r=webapp_callback_check(WA_LOG,req);
    char *n="";
    char *v="";

    if (r==NULL) return(FALSE);
    
    if (n!=NULL) n=ap_pstrdup(r->pool,name);
    if (v!=NULL) v=ap_pstrdup(r->pool,value);
    
    ap_table_add(r->headers_out,n,v);
    return(TRUE);
}

/**
 * Commit the first part of the response (status and headers).
 *
 * @param req The request member associated with this call.
 * @return TRUE on success, FALSE otherwise
 */
static boolean webapp_callback_commit(wa_request *req) {
    request_rec *r=webapp_callback_check(WA_LOG,req);

    if (r==NULL) return(FALSE);

    ap_send_http_header(r);
    return(TRUE);
}

/**
 * Write part of the response data back to the client.
 *
 * @param req The request member associated with this call.
 * @param len The number of characters to be written.
 * @return The number of characters written to the client or -1 on error.
 */
static int webapp_callback_write(wa_request *req, char *buf, int len) {
    request_rec *r=webapp_callback_check(WA_LOG,req);

    if (r==NULL) return(-1);

    return(ap_rwrite(buf, len, r));
}

/**
 * Flush any unwritten response data to the client.
 *
 * @param req The request member associated with this call.
 * @return TRUE on success, FALSE otherwise
 */
static boolean webapp_callback_flush(wa_request *req) {
    request_rec *r=webapp_callback_check(WA_LOG,req);

    if (r==NULL) return(FALSE);

    ap_rflush(r);
    return(TRUE);
}


/* ************************************************************************* */
/* STRUCTURES, FUNCTIONS AND DATA POINTERS                                   */
/* ************************************************************************* */

/* Our callback functions structure */
static wa_callback webapp_callbacks = {
    webapp_callback_serverinfo,
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
    NULL,                               /* module initializer */
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
    webapp_init,                        /* child initializer */
    webapp_exit,                        /* child exit/cleanup */
    NULL                                /* [1] post read_request handling */
};
