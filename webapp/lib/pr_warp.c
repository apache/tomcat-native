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
#include "pr_warp.h"

/* Initialize this provider. */
static const char *warp_init(void) {
    wa_debug(WA_MARK,"WARP provider initialized");
    return(NULL);
}

/* Notify this provider of its imminent startup. */
static void warp_startup(void) {
    wa_chain *elem=warp_connections;

    /* Open all connections having deployed applications */
    while (elem!=NULL) {
        wa_connection *curr=(wa_connection *)elem->curr;
        wa_debug(WA_MARK,"Opening connection \"%s\"",curr->name);
        if (n_connect(curr)==wa_true) {
            wa_debug(WA_MARK,"Connection \"%s\" opened",curr->name);
            if (c_configure(curr)==wa_true) {
                wa_debug(WA_MARK,"Connection \"%s\" configured",curr->name);
            } else {
                wa_log(WA_MARK,"Cannot configure connection \"%s\"",curr->name);
            }
        } else wa_log(WA_MARK,"Cannot open connection \"%s\"",curr->name);
        elem=elem->next;
    }

    wa_debug(WA_MARK,"WARP provider started");
}

/* Cleans up all resources allocated by this provider. */
static void warp_shutdown(void) {
    wa_debug(WA_MARK,"WARP provider shut down");
}

/* Configure a connection with the parameter from the web server
   configuration file. */
static const char *warp_connect(wa_connection *conn, const char *param) {
    apr_status_t r=APR_SUCCESS;
    warp_config *conf=NULL;
    apr_port_t port=0;
    char *addr=NULL;
    char *scop=NULL;

    /* Allocation and checking */
    conf=(warp_config *)apr_palloc(wa_pool,sizeof(warp_config));
    if (conf==NULL) return("Cannot allocate connection configuration");

    /* Check and parse parameter */
    if (param==NULL) return("Parameter for connection not specified");
    if (apr_parse_addr_port(&addr,&scop,&port,param,wa_pool)!=APR_SUCCESS)
        return("Invalid format for parameter");
    if (addr==NULL) return("Host name not specified in parameter");
    if (scop!=NULL) return("Invalid format for parameter (scope)");
    if (port==0) return("Port number not specified in parameter");

    /* Create and APR sockaddr structure */
    r=apr_sockaddr_info_get(&(conf->addr),addr,APR_INET,port,0,wa_pool);
    if (r!=APR_SUCCESS) return("Cannot get socket address information");

    /* Done */
    conf->sock=NULL;
    conf->serv=0;
    conn->conf=conf;
    return(NULL);
}

/* Receive notification of the deployment of an application. */
static const char *warp_deploy(wa_application *appl) {
    wa_chain *elem=warp_connections;
    wa_connection *conn=appl->conn;

    /* Integer configuration -1 equals application not deployed */
    appl->conf=(void *)-1;

    /* Check if the connection specified in this application has already
       been stored in our local array of connections */
    while (elem!=NULL) {
        if (conn==elem->curr) break;
        elem=elem->next;
    }
    if (elem==NULL) {
        elem=(wa_chain *)apr_palloc(wa_pool,sizeof(wa_chain));
        elem->curr=conn;
        elem->next=warp_connections;
        warp_connections=elem;
    }

    /* Check if this application has already been stored in our local array of
       applications */
    elem=warp_applications;
    while (elem!=NULL) {
        if (appl==elem->curr) break;
        elem=elem->next;
    }
    if (elem==NULL) {
        elem=(wa_chain *)apr_palloc(wa_pool,sizeof(wa_chain));
        elem->curr=appl;
        elem->next=warp_applications;
        warp_applications=elem;
    }

    return(NULL);
}

/* Describe the configuration member found in a connection. */
static char *warp_conninfo(wa_connection *conn, apr_pool_t *pool) {
    warp_config *conf=(warp_config *)conn->conf;
    apr_port_t port=0;
    char *addr=NULL;
    char *name=NULL;
    char *mesg=NULL;
    char *buff=NULL;

    if (conf==NULL) return("Invalid configuration member");

    apr_sockaddr_port_get(&port,conf->addr);
    apr_sockaddr_ip_get(&addr,conf->addr);
    apr_getnameinfo(&name,conf->addr,0);

    if (conf->sock==NULL) mesg="Not Connected";
    else mesg="Connected";

    buff=apr_psprintf(pool,"Host: %s Port:%d Address:%s (%s) Server ID: %d",
                      name,port,addr,mesg,conf->serv);
    return(buff);
}

/* Describe the configuration member found in a web application. */
static char *warp_applinfo(wa_application *appl, apr_pool_t *pool) {
    return(apr_psprintf(pool,"Application ID: %d",(int)(appl->conf)));
}

/* Transmit headers */
static int headers(void *d, const char *n, const char *v) {
    warp_header *data=(warp_header *)d;
    wa_connection *conn=data->conn;
    warp_config *conf=(warp_config *)conn->conf;
    warp_packet *pack=data->pack;

    pack->type=TYPE_REQ_HEADER;
    p_write_string(pack,(char *)n);
    p_write_string(pack,(char *)v);
    if (n_send(conf->sock,pack)!=wa_true) {
        data->fail=wa_true;
        return(FALSE);
    }
    wa_debug(WA_MARK,"Req. header %s: %s",n,v);
    return(TRUE);
}

/* Handle a connection from the web server. */
static int warp_handle(wa_request *r, wa_application *appl) {
    warp_header *h=(warp_header *)apr_palloc(r->pool,sizeof(warp_header));
    wa_connection *conn=appl->conn;
    warp_config *conf=(warp_config *)conn->conf;
    warp_packet *pack=p_create(r->pool);
    int status=0;

    // Check packet
    if (pack==NULL)
        return(wa_rerror(WA_MARK,r,500,"Cannot create WARP packet"));

    // Check application
    if (((int)(appl->conf))==-1)
        return(wa_rerror(WA_MARK,r,404,"Application not deployed"));

    // Attempt to reconnect if disconnected
    if (conf->sock==NULL) {
        if (n_connect(conn)==wa_true) {
            wa_debug(WA_MARK,"Connection \"%s\" opened",conn->name);
            if (c_configure(conn)==wa_true) {
                wa_debug(WA_MARK,"Connection \"%s\" configured",conn->name);
            } else {
                wa_log(WA_MARK,"Cannot configure connection %s",conn->name);
                return(wa_rerror(WA_MARK,r,500,
                                 "Cannot configure connection \"%s\"",
                                 conn->name));
            }
        } else {
            wa_log(WA_MARK,"Cannot open connection %s",conn->name);
            return(wa_rerror(WA_MARK,r,500,"Cannot open connection %s",
                             conn->name));
        }
    }

    // Let's do it
    pack->type=TYPE_REQ_INIT;
    p_write_int(pack,(int)(appl->conf));
    p_write_string(pack,r->meth);
    p_write_string(pack,r->ruri);
    p_write_string(pack,r->args);
    p_write_string(pack,r->prot);
    if (n_send(conf->sock,pack)!=wa_true) {
        n_disconnect(conn);
        if (n_connect(conn)==wa_true) {
            wa_debug(WA_MARK,"Connection \"%s\" reopened",conn->name);
            if (c_configure(conn)==wa_true) {
                wa_debug(WA_MARK,"Connection \"%s\" reconfigured",conn->name);
            } else {
                wa_log(WA_MARK,"Cannot reconfigure connection %s",conn->name);
                return(wa_rerror(WA_MARK,r,500,
                                 "Cannot reconfigure connection \"%s\"",
                                 conn->name));
            }
            if (n_send(conf->sock,pack)!=wa_true) {
              return(wa_rerror(WA_MARK,r,500,
                     "Communitcation broken while reconnecting"));
            } else {
                wa_debug(WA_MARK,"Re-Req. %s %s %s",r->meth,r->ruri,r->prot);
            }
        } else {
            wa_log(WA_MARK,"Cannot open connection %s",conn->name);
            return(wa_rerror(WA_MARK,r,500,"Cannot open connection %s",
                             conn->name));
        }
    } else {
        wa_debug(WA_MARK,"Req. %s %s %s",r->meth,r->ruri,r->prot);
    }
    
    p_reset(pack);
    pack->type=TYPE_REQ_CONTENT;
    p_write_string(pack,r->ctyp);
    p_write_int(pack,r->clen);
    if (n_send(conf->sock,pack)!=wa_true) {
        n_disconnect(conn);
        return(wa_rerror(WA_MARK,r,500,"Communitcation interrupted"));
    } else {
        wa_debug(WA_MARK,"Req. content typ=%s len=%d",r->ctyp,r->clen);
    }

    if (r->schm!=NULL) {
        p_reset(pack);
        pack->type=TYPE_REQ_SCHEME;
        p_write_string(pack,r->schm);
        if (n_send(conf->sock,pack)!=wa_true) {
            n_disconnect(conn);
            return(wa_rerror(WA_MARK,r,500,"Communitcation interrupted"));
        } else {
            wa_debug(WA_MARK,"Req. scheme %s",r->schm);
        }
    }

    if ((r->user!=NULL)||(r->auth!=NULL)) {
        p_reset(pack);
        pack->type=TYPE_REQ_AUTH;
        if (r->user==NULL) r->user="\0";
        if (r->auth==NULL) r->auth="\0";
        p_write_string(pack,r->user); 
        p_write_string(pack,r->auth); 
        if (n_send(conf->sock,pack)!=wa_true) {
            n_disconnect(conn);
            return(wa_rerror(WA_MARK,r,500,"Communitcation interrupted"));
        } else {
            wa_debug(WA_MARK,"Req. user %s auth %s",r->user,r->auth);
        }
    }

    /* The request headers */
    h->conn=conn;
    h->pack=pack;
    h->fail=wa_false;
    apr_table_do(headers,h,r->hdrs,NULL);
    if (h->fail==wa_true) {
        n_disconnect(conn);
        return(wa_rerror(WA_MARK,r,500,"Communitcation interrupted"));
    }

    /* The request client data */
    if (r->clnt!=NULL) {
        p_reset(pack);
        pack->type=TYPE_REQ_CLIENT;
        p_write_string(pack,r->clnt->host);
        p_write_string(pack,r->clnt->addr);
        p_write_ushort(pack,r->clnt->port);
        if (n_send(conf->sock,pack)!=wa_true) {
            n_disconnect(conn);
            return(wa_rerror(WA_MARK,r,500,"Communitcation interrupted"));
        } else {
            wa_debug(WA_MARK,"Req. server %s:%d (%s)",r->clnt->host,
                     r->clnt->port,r->clnt->addr);
        }
    }

    /* The request server data */
    if (r->serv!=NULL) {
        p_reset(pack);
        pack->type=TYPE_REQ_SERVER;
        p_write_string(pack,r->serv->host);
        p_write_string(pack,r->serv->addr);
        p_write_ushort(pack,r->serv->port);
        if (n_send(conf->sock,pack)!=wa_true) {
            n_disconnect(conn);
            return(wa_rerror(WA_MARK,r,500,"Communitcation interrupted"));
        } else {
            wa_debug(WA_MARK,"Req. client %s:%d (%s)",r->serv->host,
                     r->serv->port,r->serv->addr);
        }
    }

    p_reset(pack);
    pack->type=TYPE_REQ_PROCEED;
    if (n_send(conf->sock,pack)!=wa_true) {
        n_disconnect(conn);
        return(wa_rerror(WA_MARK,r,500,"Communitcation interrupted"));
    }

    
    while (1) {
        if (n_recv(conf->sock,pack)!=wa_true) {
            n_disconnect(conn);
            return(wa_rerror(WA_MARK,r,500,"Communitcation interrupted"));
        }
        switch (pack->type) {
            case TYPE_RES_STATUS: {
                char *mesg=NULL;
                p_read_ushort(pack,&status);
                p_read_string(pack,&mesg);
                wa_debug(WA_MARK,"=== %d %s",status,mesg);
                wa_rsetstatus(r,status);
                break;
            }
            case TYPE_RES_HEADER: {
                char *name=NULL;
                char *valu=NULL;
                p_read_string(pack,&name);
                p_read_string(pack,&valu);
                if (strcasecmp("content-type",name)==0)
                    wa_rsetctype(r,valu);
                else wa_rsetheader(r,name,valu);
                wa_debug(WA_MARK,"=== %s: %s",name,valu);
                break;
            }
            case TYPE_RES_COMMIT: {
                wa_rcommit(r);
                wa_debug(WA_MARK,"=== ");
                break;
            }
            case TYPE_RES_BODY: {
                wa_rwrite(r,pack->buff,pack->size);
                wa_rflush(r);
                pack->buff[pack->size]='\0';
                wa_debug(WA_MARK,"=== %s",pack->buff);
                break;
            }
            case TYPE_RES_DONE: {
                wa_debug(WA_MARK,"=== DONE ===");
                return(status);
                break;
            }
            case TYPE_CBK_READ: {
                int size=-1;
                p_read_ushort(pack,&size);
                p_reset(pack);
                size=wa_rread(r,pack->buff,size);
                if (size==0) {
                    pack->type=TYPE_CBK_DONE;
                } else if (size>0) {
                    pack->type=TYPE_CBK_DATA;
                    pack->size=size;
                } else {
                    pack->type=TYPE_ERROR;
                    p_write_string(pack,"Transfer interrupted");
                }
                if (n_send(conf->sock,pack)!=wa_true) {
                    n_disconnect(conn);
                    return(wa_rerror(WA_MARK,r,500,"Communitcation interrupted"));
                }
                break;
            }
            case TYPE_ERROR: {
                char *mesg=NULL;
                p_read_string(pack,&mesg);
                return(wa_rerror(WA_MARK,r,500,"%s",mesg));
            }
            default: {
                n_disconnect(conn);
                return(wa_rerror(WA_MARK,r,500,"Invalid packet %d",pack->type));
            }
        }           
    }
}

/* The list of all configured connections */
wa_chain *warp_connections=NULL;
/* The list of all deployed connections */
wa_chain *warp_applications=NULL;
/* The warp provider structure */
wa_provider wa_provider_warp = {
    "warp",
    warp_init,
    warp_startup,
    warp_shutdown,
    warp_connect,
    warp_deploy,
    warp_conninfo,
    warp_applinfo,
    warp_handle,
};
