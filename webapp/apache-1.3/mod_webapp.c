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
module MODULE_VAR_EXPORT webapp_module;
/* Wether the WebApp Library has been initialized or not */
static wa_boolean wam_initialized=FALSE;
/* The list of configured connections */
static wa_chain *wam_connections=NULL;

/* ************************************************************************* */
/* MODULE AND LIBRARY INITIALIZATION AND DESTRUCTION                         */
/* ************************************************************************* */

/* Destroy the module and the WebApp Library */
static void wam_destroy(void *nil) {
    if (!wam_initialized) return;
    wa_destroy();
    wam_initialized=FALSE;
}

/* Startup the module and the WebApp Library */
static void wam_startup(server_rec *s, pool *p) {
    if (!wam_initialized) return;
    wa_startup();
}

/* Initialize the module and the WebApp Library */
static const char *wam_init(pool *p) {
    const char *ret=NULL;

    if(wam_initialized) return(NULL);
    if ((ret=wa_init())!=NULL) return(ret);
    ap_register_cleanup(p,NULL,wam_destroy,NULL);
    wam_initialized=TRUE;
    return(NULL);
}

/* ************************************************************************* */
/* CONFIGURATION DIRECTIVES HANDLING                                         */
/* ************************************************************************* */

/* Retrieve or create a wa_virtualhost structure for an Apache server_rec
   and store it as the per-server module configuration */
const char *wam_server(server_rec *svr, wa_virtualhost **h) {
    wa_virtualhost *host=NULL;
    char *name=svr->server_hostname;
    int port=(int)svr->port;
    const char *ret=NULL;

    /* Attempt to retrieve the wa_virtualhost structure and create it
       if necessary, storing it into the server_rec structure. */
    host=ap_get_module_config(svr->module_config,&webapp_module);

    /* If we already configured the wa_virtualhost, simply return it */
    if (host!=NULL) {
        *h=host;
        return(NULL);
    }

    /* The wa_virtualhost was not found in the per-server module configuration
       so we'll try to create it. */
    ret=wa_cvirtualhost(&host,name,port);
    if (ret!=NULL) {
        *h=NULL;
        return(ret);
    }

    /* We successfully created a wa_virtualhost structure, store it in the
       per-server configuration member and return it. */
    ap_set_module_config(svr->module_config,&webapp_module,host);
    *h=host;
    return(NULL);
}

/* Process the WebAppConnection directive. */
static const char *wam_directive_connection(cmd_parms *cmd, void *mconfig,
                                            char *name, char *prov, char *p) {
    wa_connection *conn=NULL;
    const char *ret=NULL;
    wa_chain *elem=NULL;

    /* Initialize the library */
    if ((ret=wam_init(cmd->pool))!=NULL) return(ret);

    /* Attempt to create a new wa_connection structure */
    if ((ret=wa_cconnection(&conn,name,prov,p))!=NULL) return(ret);

    /* Check if we have a duplicate connection with this name */
    elem=wam_connections;
    while (elem!=NULL) {
        wa_connection *curr=(wa_connection *)elem->curr;
        if (strcasecmp(conn->name,curr->name)==0)
            return("Duplicate connection name");
        elem=elem->next;
    }

    /* We don't have a duplicate connection, store it locally */
    elem=apr_palloc(wa_pool,sizeof(wa_chain));
    elem->curr=conn;
    elem->next=wam_connections;
    wam_connections=elem;
    return(NULL);
}

/* Process the WebAppDeploy directive */
static const char *wam_directive_deploy(cmd_parms *cmd, void *mconfig,
                                      char *name, char *cnam, char *path) {
    wa_virtualhost *host=NULL;
    wa_application *appl=NULL;
    wa_connection *conn=NULL;
    wa_chain *elem=NULL;
    const char *ret=NULL;

    /* Initialize the library and retrieve/create the host structure */
    if ((ret=wam_init(cmd->pool))!=NULL) return(ret);
    if ((ret=wam_server(cmd->server,&host))!=NULL) return(ret);

    /* Retrieve the connection */
    elem=wam_connections;
    while(elem!=NULL) {
        wa_connection *curr=(wa_connection *)elem->curr;
        if (strcasecmp(curr->name,cnam)==0) {
            conn=curr;
            break;
        }
        elem=elem->next;
    }
    if (conn==NULL) return("Specified connection not configured");

    /* Create a new wa_application member */
    if ((ret=wa_capplication(&appl,name,path))!=NULL) return(ret);

    /* Deploy the web application */
    if ((ret=wa_deploy(appl,host,conn))!=NULL) return(ret);

    /* Done */
    return(NULL);
}

/* Process the WebAppInfo directive */
static const char *wam_directive_info(cmd_parms *cmd, void *mconfig,
                                      char *path) {
    const char *ret;

    /* We simply divert this call to a WebAppConnection and a WebAppDeploy
       calls */
    if ((ret=wam_directive_connection(cmd,mconfig,"_INFO_","info",""))!=NULL)
        return(ret);
    if ((ret=wam_directive_deploy(cmd,mconfig,"_INFO_","_INFO_",path))!=NULL)
        return(ret);

    return(NULL);
}

/* The list of Directives for the WebApp module */
static const command_rec wam_directives[] = {
    {
        "WebAppInfo",               /* directive name */
        wam_directive_info,         /* config action routine */
        NULL,                       /* argument to include in call */
        OR_OPTIONS,                 /* where available */
        TAKE1,                      /* arguments */
        "<uri-path>"
    },{
        "WebAppConnection",         /* directive name */
        wam_directive_connection,   /* config action routine */
        NULL,                       /* argument to include in call */
        RSRC_CONF,                  /* where available */
        TAKE23,                     /* arguments */
        "<name> <provider> [optional parameter]"
    }, {
        "WebAppDeploy",             /* directive name */
        wam_directive_deploy,       /* config action routine */
        NULL,                       /* argument to include in call */
        RSRC_CONF,                  /* where available */
        TAKE3,                      /* arguments */
        "<name> <connection> <uri-path>"
    }, {NULL}

};

/* ************************************************************************* */
/* CALLBACKS FROM WEB SERVER                                                 */
/* ************************************************************************* */

/* Log a message associated with a request */
void wam_handler_log(wa_request *r, const char *f, const int l, char *msg) {
    request_rec *req=(request_rec *)r->data;
    server_rec *svr=req->server;

    ap_log_error(f,l,APLOG_NOERRNO|APLOG_ERR,svr,"%s",msg);
}

/* Set the HTTP status of the response. */
void wam_handler_setstatus(wa_request *r, int status) {
    request_rec *req=(request_rec *)r->data;

    req->status=status;
}

/* Set the MIME Content-Type of the response. */
void wam_handler_setctype(wa_request *r, char *type) {
    request_rec *req=(request_rec *)r->data;

    if (type==NULL) return;

    req->content_type=ap_pstrdup(req->pool,type);
    ap_table_add(req->headers_out,"Content-Type",ap_pstrdup(req->pool,type));
}

/* Set a header in the HTTP response. */
void wam_handler_setheader(wa_request *r, char *name, char *value) {
    request_rec *req=(request_rec *)r->data;

    if (name==NULL) return;
    if (value==NULL) value="";

    ap_table_add(req->headers_out,ap_pstrdup(req->pool,name),
                 ap_pstrdup(req->pool,value));
}

/* Commit the first part of the response (status and headers) */
void wam_handler_commit(wa_request *r) {
    request_rec *req=(request_rec *)r->data;

    ap_send_http_header(req);
    ap_rflush(req);
}

/* Flush all data in the response buffer */
void wam_handler_flush(wa_request *r) {
    request_rec *req=(request_rec *)r->data;

    ap_rflush(req);
}

/* Read a chunk of text from the request body */
int wam_handler_read(wa_request *r, char *buf, int len) {
    request_rec *req=(request_rec *)r->data;
    long ret=0;

    /* Check if we have something to read. */
    if (r->clen==0) return(0);

    /* Check if we had an error previously. */
    if (r->rlen==-1) return(-1);

    /* Send HTTP_CONTINUE to client when we're ready to read for the first
       time. */
    if (r->rlen==0) {
        if (ap_should_client_block(req)==0) return(0);
    }

    /* Read some data from the client and fill the buffer. */
    ret=ap_get_client_block(req,buf,len);
    if (ret==-1) {
        r->rlen=-1;
        return(-1);
    }

    /* We did read some bytes, increment the current rlen and return. */
    r->rlen+=ret;
    return((int)ret);
}

/* Write a chunk of text into the response body. */
int wam_handler_write(wa_request *r, char *buf, int len) {
    request_rec *req=(request_rec *)r->data;

    return(ap_rwrite(buf, len, req));
}

/* The structure holding all callback handling functions for the library */
static wa_handler wam_handler = {
    wam_handler_log,
    wam_handler_setstatus,
    wam_handler_setctype,
    wam_handler_setheader,
    wam_handler_commit,
    wam_handler_flush,
    wam_handler_read,
    wam_handler_write,
};

/* ************************************************************************* */
/* REQUEST HANDLING                                                          */
/* ************************************************************************* */

/* Match an Apache request */
static int wam_match(request_rec *r) {
    wa_virtualhost *host=NULL;
    wa_application *appl=NULL;
    wa_chain *elem=NULL;

    /* Paranoid check */
    if (!wam_initialized) return(DECLINED);

    /* Check if this host was recognized */
    host=ap_get_module_config(r->server->module_config,&webapp_module);
    if (host==NULL) return(DECLINED);

    /* Check if the uri is contained in one of our applications root path */
    elem=host->apps;
    while(elem!=NULL) {
        appl=(wa_application *)elem->curr;
        if (strncmp(appl->rpth,r->uri,strlen(appl->rpth))==0) break;

        appl=NULL;
        elem=elem->next;
    }
    if (appl==NULL) return(DECLINED);

    /* The uri path is matched: set the handler and return */
    r->handler=ap_pstrdup(r->pool,"webapp-handler");

    /* Set the webapp request structure into Apache's request structure */
    ap_set_module_config(r->request_config, &webapp_module, appl);
    return(OK);
}

/* Handle the current request */
static int wam_invoke(request_rec *r) {
    server_rec *svr=r->server;
    conn_rec *con=r->connection;
    wa_application *appl=NULL;
    wa_request *req=NULL;
    const char *msg=NULL;
    char *stmp=NULL;
    char *ctmp=NULL;
    int ret=0;

    /* Paranoid check */
    if (!wam_initialized) return(DECLINED);

    /* Try to get a hold on the webapp request structure */
    appl=(wa_application *)ap_get_module_config(r->request_config,
                                                &webapp_module);
    if (appl==NULL) return(DECLINED);

    /* Allocate the webapp request structure */
    if ((msg=wa_ralloc(&req, &wam_handler, r))!=NULL) {
        ap_log_error(APLOG_MARK,APLOG_NOERRNO|APLOG_ERR,svr,"%s",msg);
        return(HTTP_INTERNAL_SERVER_ERROR);
    }

    /* Set up the WebApp Library request structure client and server host
       data (from the connection */
    stmp=(char *)r->hostname;
    ctmp=(char *)ap_get_remote_host(con,r->per_dir_config, REMOTE_HOST);
    req->serv->host=apr_pstrdup(req->pool,stmp);
    req->clnt->host=apr_pstrdup(req->pool,ctmp);
    req->serv->addr=apr_pstrdup(req->pool,con->local_ip);
    req->clnt->addr=apr_pstrdup(req->pool,con->remote_ip);
    req->serv->port=con->local_addr.sin_port;
    req->clnt->port=con->remote_addr.sin_port;

    /* Set up all other members of the request structure */
    req->meth=apr_pstrdup(req->pool,(char *)r->method);
    req->ruri=apr_pstrdup(req->pool,r->uri);
    req->args=apr_pstrdup(req->pool,r->args);
    req->prot=apr_pstrdup(req->pool,r->protocol);
    req->schm=apr_pstrdup(req->pool,ap_http_method(r));
    req->user=apr_pstrdup(req->pool,con->user);
    req->auth=apr_pstrdup(req->pool,con->ap_auth_type);
    req->clen=0;
    req->rlen=0;

    /* Copy headers into webapp request structure */
    if (r->headers_in!=NULL) {
        array_header *arr=ap_table_elts(r->headers_in);
        table_entry *ele=(table_entry *)arr->elts;
        int x=0;

        /* Copy headers one by one */
        for (x=0; x<arr->nelts;x++) {
            if (ele[x].key==NULL) continue;
            if (ele[x].val==NULL) continue;
            apr_table_add(req->hdrs,apr_pstrdup(req->pool,ele[x].key),
                                    apr_pstrdup(req->pool,ele[x].val));
            if (strcasecmp(ele[x].key,"Content-Length")==0)
                req->clen=atol(ele[x].val);
        }
    }

    /* Check if we can read something from the request */
    ret=ap_setup_client_block(r,REQUEST_CHUNKED_DECHUNK);
    if (ret!=OK) return(ret);

    /* Invoke the request */
    ret=wa_rinvoke(req,appl);

    /* Destroy the request member */
    wa_rfree(req);
    ap_rflush(r);

    return(OK);
}

/* List of all available Apache handlers */
static const handler_rec wam_handlers[] = {
    {"webapp-handler", wam_invoke},
    {NULL}
};

/* Apache module declaration */
module MODULE_VAR_EXPORT webapp_module = {
    STANDARD_MODULE_STUFF,
    NULL,                               /* module initializer */
    NULL,                               /* per-directory config creator */
    NULL,                               /* dir config merger */
    NULL,                               /* server config creator */
    NULL,                               /* server config merger */
    wam_directives,                     /* command table */
    wam_handlers,                       /* [9] list of handlers */
    wam_match,                          /* [2] filename-to-URI translation */
    NULL,                               /* [5] check/validate user_id */
    NULL,                               /* [6] check user_id is valid *here* */
    NULL,                               /* [4] check access by host address */
    NULL,                               /* [7] MIME type checker/setter */
    NULL,                               /* [8] fixups */
    NULL,                               /* [10] logger */
    NULL,                               /* [3] header parser */
    wam_startup,                        /* child initializer */
    NULL,                               /* child exit/cleanup */
    NULL                                /* [1] post read_request handling */
};
