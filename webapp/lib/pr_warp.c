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

/* ************************************************************************* */
/* WARP CONSTANTS                                                         */
/* ************************************************************************* */

/* Look in WarpConstants.java for a description of these definitions. */
#define RID_CONNECTION  0x00000
#define RID_DISCONNECT  0x0ffff

#define TYP_CONINIT_HST 0x00000
#define TYP_CONINIT_HID 0x00001
#define TYP_CONINIT_APP 0x00002
#define TYP_CONINIT_AID 0x00003
#define TYP_CONINIT_REQ 0x00004
#define TYP_CONINIT_RID 0x00005
#define TYP_CONINIT_ERR 0x0000F

#define TYP_REQINIT_MET 0x00010
#define TYP_REQINIT_URI 0x00011
#define TYP_REQINIT_ARG 0x00012
#define TYP_REQINIT_PRO 0x00013
#define TYP_REQINIT_HDR 0x00014
#define TYP_REQINIT_VAR 0x00015
#define TYP_REQINIT_RUN 0x0001D
#define TYP_REQINIT_ERR 0x0001E
#define TYP_REQINIT_ACK 0x0001F

#define TYP_REQUEST_STA 0x00020
#define TYP_REQUEST_HDR 0x00021
#define TYP_REQUEST_CMT 0x00022
#define TYP_REQUEST_DAT 0x00023
#define TYP_REQUEST_ERR 0x0002E
#define TYP_REQUEST_ACK 0x0002F

#define MAXPAYLOAD 65536

/* ************************************************************************* */
/* STRUCTURES AND FUNCTIONS DECLARATION                                      */
/* ************************************************************************* */

/* The WARP connection configuration structure */
typedef struct warp_cconfig {
    apr_sockaddr_t *addr;
    apr_socket_t *sock;
    wa_boolean disc;
} warp_cconfig;

/* The WARP application configuration structure */
typedef struct warp_aconfig {
    int host;
    int appl;
    wa_boolean depl;
} warp_aconfig;

typedef struct warp_packet {
    apr_pool_t *pool; /* The APR memory pool where this packet is allocated */
    int type;   /* The packet type signature */
    int size;   /* The size of the buffer (used by read) */
    int bpos;   /* The current position in the buffer */
    char *buff; /* The payload buffer */
} warp_packet;

/* The list of all configured connections */
static wa_chain *warp_connections=NULL;
/* This provider */
wa_provider wa_provider_warp;

/* ************************************************************************* */
/* WARP PACKET FUNCTIONS                                                     */
/* ************************************************************************* */
static void p_reset(warp_packet *pack) {
    pack->type=0;
    pack->bpos=0;
    pack->size=0;
}

static warp_packet *p_create(apr_pool_t *pool) {
    warp_packet *pack=apr_palloc(pool,sizeof(warp_packet));
    pack->buff=apr_palloc(pool,MAXPAYLOAD*sizeof(char));
    pack->pool=pool;
    p_reset(pack);
    return(pack);
}

static wa_boolean p_wshort(warp_packet *p, int k) {
    if ((p->bpos+2)>MAXPAYLOAD) return(FALSE);
    p->buff[p->bpos++]=(char)((k>>8)&0x0ff);
    p->buff[p->bpos++]=(char)(k&0x0ff);
    return(TRUE);
}

static wa_boolean p_wstring(warp_packet *p, char *s) {
    int l=0;
    int x=0;

    if (s==NULL) return(p_wshort(p,0));

    // Evaluate string length
    l=strlen(s);
    // Store string length
    if (!p_wshort(p,strlen(s))) return(FALSE);
    // Check if we have room in the buffer and copy the string
    if ((p->bpos+l)>MAXPAYLOAD) return(FALSE);
    for(x=0; x<l; x++) p->buff[p->bpos++]=s[x];
    return(TRUE);
}

static int p_rshort(warp_packet *p) {
    int k=0;
    int x=0;

    // Store the two bytes
    x=p->bpos;
    k=((p->buff[p->bpos++]<<8)&0x0ff00);
    k=((k|(p->buff[p->bpos++]&0x0ff))&0x0ffff);
    return(k);
}

static char *p_rstring(warp_packet *p) {
    int k=p_rshort(p);
    char *ret=apr_pstrndup(p->pool,p->buff+p->bpos,k);
    p->bpos+=k;
    return(ret);
}


/* ************************************************************************* */
/* NETWORK FUNCTIONS                                                         */
/* ************************************************************************* */

/* Attempt to connect to the remote endpoint of the WARP connection (if not
   done already). */
static wa_boolean n_connect(wa_connection *conn) {
    warp_cconfig *conf=(warp_cconfig *)conn->conf;
    apr_status_t ret=APR_SUCCESS;

    /* Create the APR socket if that has not been yet created */
    if (conf->sock==NULL) {
        ret=apr_socket_create(&conf->sock,AF_INET,SOCK_STREAM,wa_pool);
        if (ret!=APR_SUCCESS) {
            conf->sock=NULL;
            wa_debug(WA_MARK,"Cannot create socket for \"%s\"",conn->name);
            return(FALSE);
        }
    }

    /* Attempt to connect to the remote endpoint */
    if (conf->disc) {
        ret=apr_connect(conf->sock, conf->addr);
        if (ret!=APR_SUCCESS) {
            conf->disc=TRUE;
            wa_debug(WA_MARK,"Connection \"%s\" cannot connect",conn->name);
            return(FALSE);
        } else {
            conf->disc=FALSE;
            return(TRUE);
        }
    }

    /* We were already connected */
    return(TRUE);
}

/* Attempt to disconnect a connection if connected. */
static void n_disconnect(wa_connection *conn) {
    warp_cconfig *conf=(warp_cconfig *)conn->conf;
    apr_status_t ret=APR_SUCCESS;

    wa_debug(WA_MARK,"Disconnecting \"%s\"",conn->name);

    /* Create the APR socket if that has not been yet created */
    if (conf->sock==NULL) return;
    if (conf->disc) return;

    /* Shutdown and close the socket (ignoring errors) */
    ret=apr_shutdown(conf->sock,APR_SHUTDOWN_READWRITE);
    if (ret!=APR_SUCCESS)
        wa_debug(WA_MARK,"Cannot shutdown \"%s\"",conn->name);
    ret=apr_socket_close(conf->sock);
    if (ret!=APR_SUCCESS)
        wa_debug(WA_MARK,"Cannot close \"%s\"",conn->name);

    /* Reset the state */
    conf->sock=NULL;
    conf->disc=TRUE;
}


/* Send a 16 bits value over a connection */
static wa_boolean n_sshort(wa_connection *conn, int k) {
    warp_cconfig *c=(warp_cconfig *)conn->conf;
	apr_size_t len=2;
    char buf[2];

    if (c->sock==NULL) {
        wa_log(WA_MARK,"Socket not initialized");
        n_disconnect(conn);
        return(FALSE);
    }

    if (c->disc) {
        wa_log(WA_MARK,"Socket not connected");
        n_disconnect(conn);
        return(FALSE);
    }

    buf[0]=(k>>8)&0x0ff;
    buf[1]=k&0x0ff;
    if (apr_send(c->sock,buf,&len)!=APR_SUCCESS) {
        wa_log(WA_MARK,"Cannot write to socket");
        n_disconnect(conn);
        return(FALSE);
    }
    if (len!=2) {
        wa_log(WA_MARK,"Cannot write full buffer to socket");
        n_disconnect(conn);
        return(FALSE);
    }

    return(TRUE);
}

/* Send a packet over a connection */
static wa_boolean n_send(wa_connection *conn, int r, warp_packet *p) {
    warp_cconfig *c=(warp_cconfig *)conn->conf;
    apr_size_t len=2;

    /* Send the connection ID */
    if (!n_sshort(conn,r)) {
        p_reset(p);
        return(FALSE);
    }
    /* Send the packet type */
    if (!n_sshort(conn,p->type)) {
        p_reset(p);
        return(FALSE);
    }
    /* Send the packet length */
    if (!n_sshort(conn,p->bpos)) {
        p_reset(p);
        return(FALSE);
    }

    /* Check if we need to send the payload */
    if (p->bpos<=0) {
        p_reset(p);
        return(TRUE);
    }

    /* Send the payload */
    len=p->bpos;
    if (apr_send(c->sock,p->buff,&len)!=APR_SUCCESS) {
        wa_log(WA_MARK,"Cannot write to socket");
        n_disconnect(conn);
        p_reset(p);
        return(FALSE);
    }
    if (len!=p->bpos) {
        wa_log(WA_MARK,"Cannot write full buffer to socket");
        n_disconnect(conn);
        p_reset(p);
        return(FALSE);
    }

    wa_debug(WA_MARK,"Sent packet CID=%d TYP=%d LEN=%d",r,p->type,p->bpos);
    p_reset(p);
    return(TRUE);
}

/* Receive a short integer (2 bytes) over a warp connection. */
static int n_rshort(wa_connection *conn) {
    warp_cconfig *c=(warp_cconfig *)conn->conf;
    char buf[2];
    apr_size_t k=2;

    if (c->sock==NULL) {
        wa_log(WA_MARK,"Socket not initialized");
        n_disconnect(conn);
        return(FALSE);
    }

    if (c->disc) {
        wa_log(WA_MARK,"Socket not connected");
        n_disconnect(conn);
        return(FALSE);
    }

    if (apr_recv(c->sock,buf,&k)!=APR_SUCCESS) {
        wa_log(WA_MARK,"Cannot read from socket");
        n_disconnect(conn);
        return(-1);
    }
    if (k!=2) {
        wa_log(WA_MARK,"Cannot read full buffer from socket");
        n_disconnect(conn);
        return(-1);
    }

    return((((buf[0]<<8)&0x0ff00)|(buf[1]&0x0ff))&0x0ffff);
}

/* Receive a warp packet. */
static wa_boolean n_recv(wa_connection *conn, int r, warp_packet *p) {
    warp_cconfig *c=(warp_cconfig *)conn->conf;
    apr_size_t num=0;
    int rid=-1;
    int typ=-1;
    int siz=-1;

    p_reset(p);

    /* Get the packet RID */
    if ((rid=n_rshort(conn))<0) {
        wa_log(WA_MARK,"NO RID (%d)\n",rid);
        return(FALSE);
    }

    /* Fix this for multithreaded environments (demultiplexing of packets) */
    if (rid!=r) {
        wa_log(WA_MARK,"INVALID RID got %d expected %d\n",rid,r);
        n_disconnect(conn);
        return(FALSE);
    }

    /* Get the packet TYPE */
    if ((typ=n_rshort(conn))<0) {
        wa_log(WA_MARK,"NO TYP rid=%d typ=%d\n",rid,typ);
        return(FALSE);
    }

    /* Get the packet payload size */
    if ((siz=n_rshort(conn))<0) {
        wa_log(WA_MARK,"NO SIZ rid=%d typ=%d siz=%d\n",rid,typ,siz);
        return(FALSE);
    }

    /* Update the packet */
    p->type=typ;
    p->size=siz;
    if (siz==0) return(TRUE);

    /* Read from the socket and fill the packet buffer */
    while(TRUE) {
        num=siz-p->bpos;
        if (apr_recv(c->sock,p->buff+p->bpos,&num)!=APR_SUCCESS) {
            wa_log(WA_MARK,"Cannot read from socket");
            n_disconnect(conn);
            return(FALSE);
        }
        p->bpos+=num;

        if (p->bpos<siz) {
            wa_debug(WA_MARK,"SHORT len=%d siz=%d\n",p->bpos,siz);
        } else if (p->bpos>siz) {
            wa_log(WA_MARK,"INCONSIST len=%d siz=%d\n",p->bpos,siz);
            n_disconnect(conn);
            return(FALSE);
        } else {
            p->bpos=0;
            return(TRUE);
        }
    }
}

/* ************************************************************************* */
/* LOCAL FUNCTIONS FOR CONFIGURATION                                         */
/* ************************************************************************* */

/* Configure a web application over its connection */
static void x_configure(wa_application *appl, apr_pool_t *pool) {
    wa_connection *conn=(wa_connection *)appl->conn;
    warp_aconfig *conf=(warp_aconfig *)appl->conf;
    warp_packet *i=NULL;
    warp_packet *o=NULL;
    int hostid=-1;
    int applid=-1;

    if (!n_connect(conn)) return;

    wa_debug(WA_MARK,"Configuring application \"%s\" for http://%s:%d%s",
             appl->name, appl->host->name, appl->host->port, appl->rpth);

    /* Allocate a couple of packets */
    i=p_create(pool);
    o=p_create(pool);

    /* Retrieve the WARP HOSTID for the host */
    o->type=TYP_CONINIT_HST;
    p_wstring(o,appl->host->name);
    p_wshort(o,appl->host->port);
    if (!n_send(conn,RID_CONNECTION,o)) return;
    if (!n_recv(conn,RID_CONNECTION,i)) return;
    if (i->type!=TYP_CONINIT_HID) {
        wa_debug(WA_MARK,"Invalid response received (%04X)",i->type);
        wa_log(WA_MARK,"Invalid virtual host%s:%d", appl->host->name,
               appl->host->port);
        return;
    }
    hostid=p_rshort(i);
    wa_debug(WA_MARK,"Host %s:%d has ID %d",appl->host->name,
             appl->host->port, hostid);
    p_reset(i);
    p_reset(o);

    /* Retrieve the WARP APPLID for the application */
    o->type=TYP_CONINIT_APP;
    p_wshort(o,hostid);
    p_wstring(o,appl->name);
    p_wstring(o,appl->rpth);
    if (!n_send(conn,RID_CONNECTION,o)) return;
    if (!n_recv(conn,RID_CONNECTION,i)) return;
    if (i->type!=TYP_CONINIT_AID) {
        wa_debug(WA_MARK,"Invalid response received (%04X)",i->type);
        wa_log(WA_MARK,"Invalid application %s for http://%s:%d%s",appl->name,
               appl->host->name, appl->host->port, appl->rpth);
        return;
    }
    applid=p_rshort(i);
    wa_debug(WA_MARK,"Application %s under %s has ID %d",appl->name,
             appl->rpth, applid);

    conf->host=hostid;
    conf->appl=applid;
    conf->depl=TRUE;
}

/* ************************************************************************* */
/* WEBAPP LIBRARY PROVIDER FUNCTIONS                                         */
/* ************************************************************************* */

/* Initialize this provider. */
static const char *warp_init(void) {
    wa_debug(WA_MARK,"WARP provider initialized");

    return(NULL);
}

/* Notify this provider of its imminent startup. */
static void warp_startup(void) {
    wa_chain *elem=wa_configuration;
    apr_pool_t *pool=NULL;

    /* Configre all deployed applications */
    while(elem!=NULL) {
        wa_virtualhost *host=(wa_virtualhost *)elem->curr;
        wa_chain *apps=host->apps;

        while(apps!=NULL) {
            wa_application *appl=(wa_application *)apps->curr;

            /* Configure the application with a temporary pool */
            if (strcasecmp(appl->conn->prov->name,"warp")==0) {
                apr_pool_create(&pool,wa_pool);
                x_configure(appl,pool);
                apr_pool_destroy(pool);
            }

            apps=apps->next;
        }
        elem=elem->next;
    }

    wa_debug(WA_MARK,"WARP provider started");
}

/* Cleans up all resources allocated by this provider. */
static void warp_destroy(void) {
    wa_chain *elem=warp_connections;

    while(elem!=NULL) {
        wa_connection *curr=(wa_connection *)elem->curr;
        wa_debug(WA_MARK,"Destroying connection \"%s\"",curr->name);
        n_disconnect(curr);
        elem=elem->next;
    }

    wa_debug(WA_MARK,"WARP provider destroyed");
}

/* Configure a connection with the parameter from the web server
   configuration file. */
static const char *warp_connect(wa_connection *conn, const char *param) {
    apr_status_t r=APR_SUCCESS;
    warp_cconfig *conf=NULL;
    apr_port_t port=0;
    char *addr=NULL;
    char *scop=NULL;

    /* Allocation and checking */
    conf=(warp_cconfig *)apr_palloc(wa_pool,sizeof(warp_cconfig));
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
    conf->disc=TRUE;
    conn->conf=conf;
    return(NULL);
}

/* Receive notification of the deployment of an application. */
static const char *warp_deploy(wa_application *appl) {
    wa_chain *elem=warp_connections;
    wa_connection *conn=appl->conn;
    warp_aconfig *conf=NULL;

    conf=(warp_aconfig *)apr_palloc(wa_pool,sizeof(warp_aconfig));
    if (conf==NULL) return("Cannot allocate application configuration");
    conf->host=-1;
    conf->appl=-1;
    conf->depl=FALSE;

    appl->conf=conf;

    /* Check if the connection specified in this application has already
       been stored in our local array of connections */
    while (elem!=NULL) {
        wa_connection *curr=(wa_connection *)elem->curr;
        if (strcasecmp(conn->name,curr->name)==0) break;
        elem=elem->next;
    }
    if (elem==NULL) {
        elem=(wa_chain *)apr_palloc(wa_pool,sizeof(wa_chain));
        elem->curr=conn;
        elem->next=warp_connections;
        warp_connections=elem;
    }

    return(NULL);
}

/* Describe the configuration member found in a connection. */
static char *warp_conninfo(wa_connection *conn, apr_pool_t *pool) {
    warp_cconfig *conf=(warp_cconfig *)conn->conf;
    apr_port_t port=0;
    char *addr=NULL;
    char *name=NULL;
    char *mesg=NULL;
    char *buff=NULL;

    if (conf==NULL) return("Invalid configuration member");

    apr_sockaddr_port_get(&port,conf->addr);
    apr_sockaddr_ip_get(&addr,conf->addr);
    apr_getnameinfo(&name,conf->addr,0);

    if (conf->sock==NULL) mesg="Socket not initialized";
    else {
        if (conf->disc) mesg="Socket disconnected";
        else mesg="Socket connected";
    }

    buff=apr_psprintf(pool,"Host: %s Port:%d Address:%s (%s)",
                      name,port,addr,mesg);
    return(buff);
}

/* Describe the configuration member found in a web application. */
static char *warp_applinfo(wa_application *appl, apr_pool_t *pool) {
    warp_aconfig *conf=(warp_aconfig *)appl->conf;
    char *h=NULL;
    char *a=NULL;

    if (conf==NULL) return("Invalid configuration member");

    if (conf->host<0) h="(Not initialized)";
    else h=apr_psprintf(pool,"%d",conf->host);

    if (conf->appl<0) a="(Not initialized)";
    else a=apr_psprintf(pool,"%d",conf->appl);

    return(apr_psprintf(pool,"Application ID: %s Host ID: %s",a,h));
}

typedef struct warp_rheader {
    wa_connection *conn;
    warp_packet *pack;
    wa_boolean fail;
    int wrid;
} warp_rheader;

/* Transmit headers */
static int r_headers(void *d, const char *n, const char *v) {
    warp_rheader *data=(warp_rheader *)d;
    wa_connection *conn=data->conn;
    warp_packet *p=data->pack;
    int rid=data->wrid;

    p->type=TYP_REQINIT_HDR;
    p_wstring(p,(char *)n);
    p_wstring(p,(char *)v);
    n_send(conn,rid,p);
    return(TRUE);
}

/* Handle a connection from the web server. */
static int warp_handle(wa_request *r, wa_application *a) {
    warp_rheader *h=(warp_rheader *)apr_palloc(r->pool,sizeof(warp_rheader));
    warp_cconfig *cc=(warp_cconfig *)a->conn->conf;
    warp_aconfig *ac=(warp_aconfig *)a->conf;
    warp_packet *i=p_create(r->pool);
    warp_packet *o=p_create(r->pool);
    wa_connection *conn=a->conn;
    wa_boolean committed=FALSE;
    int rid=0;
    int sta=500;

    /* Check wether we are connected and configured right */
    if (cc->disc) wa_rerror(WA_MARK,r,500,"We are disconnected");
    if (!ac->depl) wa_rerror(WA_MARK,r,500,"Application not deployed correctly");

    /* Request connection ID */
    o->type=TYP_CONINIT_REQ;
    p_wshort(o,ac->host);
    p_wshort(o,ac->appl);
    if (!n_send(conn,RID_CONNECTION,o)) return(wa_rerror(WA_MARK,r,500,"I/O Error"));
    if (!n_recv(conn,RID_CONNECTION,i)) return(wa_rerror(WA_MARK,r,500,"I/O Error"));
    if (i->type!=TYP_CONINIT_RID) {
        wa_debug(WA_MARK,"Invalid response received (%04X)",i->type);
        return(wa_rerror(WA_MARK,r,500,"Cannot retrieve request ID"));
    }
    rid=p_rshort(i);

    /* The request method */
    o->type=TYP_REQINIT_MET;
    p_wstring(o,r->meth);
    if (!n_send(conn,rid,o)) return(wa_rerror(WA_MARK,r,500,"I/O Error"));

    /* The request URI */
    o->type=TYP_REQINIT_URI;
    p_wstring(o,r->ruri);
    if (!n_send(conn,rid,o)) return(wa_rerror(WA_MARK,r,500,"I/O Error"));

    /* The request arguments */
    o->type=TYP_REQINIT_ARG;
    p_wstring(o,r->args);
    if (!n_send(conn,rid,o)) return(wa_rerror(WA_MARK,r,500,"I/O Error"));

    /* The request protocol */
    o->type=TYP_REQINIT_PRO;
    p_wstring(o,r->prot);
    if (!n_send(conn,rid,o)) return(wa_rerror(WA_MARK,r,500,"I/O Error"));

    /* The request headers */
    h->conn=conn;
    h->pack=o;
    h->wrid=rid;
    apr_table_do(r_headers,h,r->hdrs,NULL);

    /* And we're ready to go */
    o->type=TYP_REQINIT_RUN;
    if (!n_send(conn,rid,o)) return(wa_rerror(WA_MARK,r,500,"I/O Error"));

    /* Now we need to wait for what the servlet container issues us */
    while (TRUE) {
        if(!n_recv(conn,rid,i)) {
            n_disconnect(conn);
            return(wa_rerror(WA_MARK,r,500,"No packets received waiting response"));
        }

        /* Check if we got an ERR packet */
        if (i->type==TYP_REQUEST_ERR) {
            return(wa_rerror(WA_MARK,r,500,"Error in response"));
        }

        /* Check if we got an ACK packet (close) */
        if (i->type==TYP_REQUEST_ACK) {
            wa_rflush(r);
            return(sta);
        }

        /* Check if we got an STA packet */
        if (i->type==TYP_REQUEST_STA) {
            wa_debug(WA_MARK,"Status1 \"%s\"",p_rstring(i));
            wa_rsetstatus(r,p_rshort(i));
            wa_debug(WA_MARK,"Status2 \"%s\"",p_rstring(i));
        }

        /* Check if we got an HDR packet (header) */
        if (i->type==TYP_REQUEST_HDR) {
            char *nam=p_rstring(i);
            char *val=p_rstring(i);
            wa_debug(WA_MARK,"Header \"%s\": \"%s\"",nam,val);
            if (strcasecmp("Connection",nam)!=0) {
                wa_rsetheader(r,nam,val);
            }
            if (strcasecmp("Content-Type",nam)==0) {
                wa_rsetctype(r,val);
            }
        }

        /* Check if we got an CMT packet (commit) */
        if (i->type==TYP_REQUEST_CMT) {
            if (!committed) {
                wa_rcommit(r);
                committed=TRUE;
            }
        }

        /* Check if we got an DAT packet (data) */
        if (i->type==TYP_REQUEST_DAT) {
            if (!committed) {
                wa_rcommit(r);
                committed=TRUE;
            }
            wa_rwrite(r,i->buff,i->size);
            wa_rflush(r);
        }
    }
}

/* The warp provider structure */
wa_provider wa_provider_warp = {
    "warp",
    warp_init,
    warp_startup,
    warp_destroy,
    warp_connect,
    warp_deploy,
    warp_conninfo,
    warp_applinfo,
    warp_handle,
};
