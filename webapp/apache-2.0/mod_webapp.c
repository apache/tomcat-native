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
#include <http_request.h>
#include <http_config.h>
#include <http_core.h>
#include <http_log.h>
#include <http_main.h>
#include <http_protocol.h>
#include <util_script.h>
#include <wa.h>
#include <apr_tables.h>

/* ************************************************************************* */
/* GENERIC DECLARATIONS                                                      */
/* ************************************************************************* */

/* Module declaration */
module AP_MODULE_DECLARE_DATA webapp_module;
/* Wether the WebApp Library has been initialized or not */
static wa_boolean wam_initialized=wa_false;
/* The list of configured connections */
static wa_chain *wam_connections=NULL;
/* The main server using for logging error not related to requests */
static server_rec *server=NULL;

/* ************************************************************************* */
/* MODULE AND LIBRARY INITIALIZATION AND DESTRUCTION                         */
/* ************************************************************************* */

/* Destroy the module and the WebApp Library */
static apr_status_t wam_shutdown(void *data) {//void *nil) {
    if (!wam_initialized) return APR_SUCCESS;
    wa_shutdown();
    wam_initialized=wa_false;
    return APR_SUCCESS;
}

/* Startup the module and the WebApp Library */
static void wam_startup(apr_pool_t *p, server_rec *s) {
    if (!wam_initialized) return;
    server=s;
    wa_startup();
    apr_pool_cleanup_register(p, NULL, wam_shutdown, apr_pool_cleanup_null);
}

/* Initialize the module and the WebApp Library */
static const char *wam_init(apr_pool_t *p) {
    const char *ret=NULL;

    if(wam_initialized==wa_true) return(NULL);
    if ((ret=wa_init())!=NULL) return(ret);
    wam_initialized=wa_true;
    return(NULL);
}

/* ************************************************************************* */
/* CONFIGURATION DIRECTIVES HANDLING                                         */
/* ************************************************************************* */

/* Retrieve or create a wa_virtualhost structure for an Apache server_rec
   and store it as the per-server module configuration */
static const char *wam_server(server_rec *svr, wa_virtualhost **h) {
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
                                            const char *name, const char *prov, const char *p) {
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
                                      const char *name, const char *cnam, const char *path) {
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
                                      const char *path) {
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
    AP_INIT_TAKE1(
        "WebAppInfo",               /* directive name */
        wam_directive_info,         /* config action routine */
        NULL,                       /* argument to include in call */
        OR_OPTIONS,                 /* where available */
        "<uri-path>"),
    AP_INIT_TAKE23(
        "WebAppConnection",         /* directive name */
        wam_directive_connection,   /* config action routine */
        NULL,                       /* argument to include in call */
        RSRC_CONF,                  /* where available */
        "<name> <provider> [optional parameter]"),
    AP_INIT_TAKE3(
        "WebAppDeploy",             /* directive name */
        wam_directive_deploy,       /* config action routine */
        NULL,                       /* argument to include in call */
        RSRC_CONF,                  /* where available */
        "<name> <connection> <uri-path>"),
    {NULL}

};

/* ************************************************************************* */
/* CALLBACKS TO WEB SERVER                                                   */
/* ************************************************************************* */

/* Log a generic error */
void wa_log(const char *f, const int l, const char *fmt, ...) {
    va_list ap;
    char buf[1024];
#ifdef DEBUG
    char tmp[1024];
#endif

    va_start(ap,fmt);
#ifdef DEBUG
    apr_vsnprintf(tmp,1024,fmt,ap);
    apr_snprintf(buf,1024,"[%s:%d] %s",f,l,tmp);
#else
    apr_vsnprintf(buf,1024,fmt,ap);
#endif
    va_end(ap);

    ap_log_error(f,l,APLOG_NOERRNO|APLOG_ERR,0,server,"%s",buf);
}

/* Log a message associated with a request */
static void wam_handler_log(wa_request *r, const char *f, const int l, char *msg) {
    request_rec *req=(request_rec *)r->data;
    server_rec *svr=req->server;

    ap_log_error(f,l,APLOG_NOERRNO|APLOG_ERR,0,svr,"%s",msg);
}

/* Set the HTTP status of the response. */
static void wam_handler_setstatus(wa_request *r, int status) {
    request_rec *req=(request_rec *)r->data;

    req->status=status;
}

/* Set the MIME Content-Type of the response. */
static void wam_handler_setctype(wa_request *r, char *type) {
    request_rec *req=(request_rec *)r->data;

    if (type==NULL) return;

    req->content_type=apr_pstrdup(req->pool,type);
    apr_table_add(req->headers_out,"Content-Type",apr_pstrdup(req->pool,type));
}

/* Set a header in the HTTP response. */
static void wam_handler_setheader(wa_request *r, char *name, char *value) {
    request_rec *req=(request_rec *)r->data;

    if (name==NULL) return;
    if (value==NULL) value="";

    apr_table_add(req->headers_out,apr_pstrdup(req->pool,name),
                 apr_pstrdup(req->pool,value));
}

/* Commit the first part of the response (status and headers) */
static void wam_handler_commit(wa_request *r) {
#if 0
/* Modules can completely forget about headers in Apache 2.0, the
 * core server always makes sure they are sent at the correct time. rbb
 */
    request_rec *req=(request_rec *)r->data;

    ap_send_http_header(req);
    ap_rflush(req);
#endif
}

/* Flush all data in the response buffer */
static void wam_handler_flush(wa_request *r) {
    request_rec *req=(request_rec *)r->data;

    ap_rflush(req);
}

/* Read a chunk of text from the request body */
static int wam_handler_read(wa_request *r, char *buf, int len) {
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
static int wam_handler_write(wa_request *r, char *buf, int len) {
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
    r->handler=apr_pstrdup(r->pool,"webapp-handler");

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
    apr_port_t port;

    if (strcmp(r->handler, "webapp-handler")) return(DECLINED);

    /* Paranoid check */
    if (!wam_initialized) return(DECLINED);

    /* Try to get a hold on the webapp request structure */
    appl=(wa_application *)ap_get_module_config(r->request_config,
                                                &webapp_module);
    if (appl==NULL) return(DECLINED);

    /* Allocate the webapp request structure */
    if ((msg=wa_ralloc(&req, &wam_handler, r))!=NULL) {
        ap_log_error(APLOG_MARK,APLOG_NOERRNO|APLOG_ERR,0,svr,"%s",msg);
        return(HTTP_INTERNAL_SERVER_ERROR);
    }

    /* Set up the WebApp Library request structure client and server host
       data (from the connection */
    stmp=(char *)r->hostname;
    ctmp=(char *)ap_get_remote_host(con,r->per_dir_config, REMOTE_HOST, NULL);
    if (stmp==NULL) req->serv->host="";
    else req->serv->host=apr_pstrdup(req->pool,stmp);
    if (ctmp==NULL) req->clnt->host="";
    else req->clnt->host=apr_pstrdup(req->pool,ctmp);
    req->serv->addr=apr_pstrdup(req->pool,con->local_ip);
    req->clnt->addr=apr_pstrdup(req->pool,con->remote_ip);
    apr_sockaddr_port_get(&port, con->local_addr);
    req->serv->port=ntohs(port);
    apr_sockaddr_port_get(&port, con->remote_addr);
    req->clnt->port=ntohs(port);

    /* Set up all other members of the request structure */
    req->meth=apr_pstrdup(req->pool,(char *)r->method);
    req->ruri=apr_pstrdup(req->pool,r->uri);
    req->args=apr_pstrdup(req->pool,r->args);
    req->prot=apr_pstrdup(req->pool,r->protocol);
    req->schm=apr_pstrdup(req->pool,ap_http_method(r));
    req->user=apr_pstrdup(req->pool,r->user);
    req->auth=apr_pstrdup(req->pool,r->ap_auth_type);
    req->clen=0;
    req->ctyp="\0";
    req->rlen=0;

    /* Copy headers into webapp request structure */
    if (r->headers_in!=NULL) {
        apr_array_header_t *arr=apr_table_elts(r->headers_in);
        apr_table_entry_t *ele=(apr_table_entry_t *)arr->elts;
        int x=0;

        /* Copy headers one by one */
        for (x=0; x<arr->nelts;x++) {
            if (ele[x].key==NULL) continue;
            if (ele[x].val==NULL) continue;
            apr_table_add(req->hdrs,apr_pstrdup(req->pool,ele[x].key),
                                    apr_pstrdup(req->pool,ele[x].val));
            if (strcasecmp(ele[x].key,"Content-Length")==0)
                req->clen=atol(ele[x].val);
            if (strcasecmp(ele[x].key,"Content-Type")==0)
                req->ctyp=apr_pstrdup(req->pool,ele[x].val);
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


static void register_hooks(apr_pool_t *p)
{
    ap_hook_handler(wam_invoke, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_translate_name(wam_match, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_child_init(wam_startup, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Apache module declaration */
module AP_MODULE_DECLARE_DATA webapp_module = {
    STANDARD20_MODULE_STUFF,
    NULL,                               /* per-directory config creator */
    NULL,                               /* dir config merger */
    NULL,                               /* server config creator */
    NULL,                               /* server config merger */
    wam_directives,                     /* command table */
    register_hooks
};
