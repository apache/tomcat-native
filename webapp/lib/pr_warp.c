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
2 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
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
/* STRUCTURES AND FUNCTIONS DECLARATION                                      */
/* ************************************************************************* */

/* The WARP connection configuration structure */
typedef struct warp_config {
    apr_sockaddr_t *addr;
    apr_socket_t *sock;
    int serv;
} warp_config;

/* The WARP packet structure */
typedef struct warp_packet {
    apr_pool_t *pool;
    int type;
    int size;
    int curr;
    char buff[65536];
} warp_packet;

/* WARP definitions */
#define VERS_MAJOR 0
#define VERS_MINOR 9

#define TYPE_INVALID -1
#define TYPE_ERROR   0x00
#define TYPE_FATAL   0xff

#define TYPE_CONF_WELCOME 0x01
#define TYPE_CONF_DEPLOY  0x02
#define TYPE_CONF_APPLIC  0x03
#define TYPE_CONF_DONE    0x04


/* The list of all configured connections */
static wa_chain *warp_connections=NULL;
/* The list of all deployed connections */
static wa_chain *warp_applications=NULL;
/* This provider */
wa_provider wa_provider_warp;

/* ************************************************************************* */
/* PACKET FUNCTIONS                                                          */
/* ************************************************************************* */
static void p_reset(warp_packet *pack) {
    pack->type=TYPE_INVALID;
    pack->type=TYPE_INVALID;
    pack->size=0;
    pack->curr=0;
    pack->buff[0]='\0';
}

static warp_packet *p_create(apr_pool_t *pool) {
    warp_packet *pack=NULL;

    if (pool==NULL) return(NULL);
    pack=(warp_packet *)apr_palloc(pool,sizeof(warp_packet));
    pack->pool=pool;
    p_reset(pack);
    return(pack);
}

static wa_boolean p_read_ushort(warp_packet *pack, int *x) {
    int k=0;

    if ((pack->curr+2)>=pack->size) return(wa_false);
    k=(pack->buff[pack->curr++]&0x0ff)<<8;
    k=k|(pack->buff[pack->curr++]&0x0ff);
    *x=k;
    return(wa_true);
}

static wa_boolean p_read_int(warp_packet *pack, int *x) {
    int k=0;

    if ((pack->curr+2)>=pack->size) return(wa_false);
    k=(pack->buff[pack->curr++]&0x0ff)<<24;
    k=k|((pack->buff[pack->curr++]&0x0ff)<<16);
    k=k|((pack->buff[pack->curr++]&0x0ff)<<8);
    k=k|(pack->buff[pack->curr++]&0x0ff);
    *x=k;
    return(wa_true);
}

static wa_boolean p_read_string(warp_packet *pack, char **x) {
    int len=0;

    if (p_read_ushort(pack,&len)==wa_false) {
        *x=NULL;
        return(wa_false);
    }
    if ((pack->curr+len)>=pack->size) {
        *x=NULL;
        return(wa_false);
    }

    *x=(char *)apr_palloc(pack->pool,(len+1)*sizeof(char));
    if (*x==NULL) return(wa_false);

    apr_cpystrn(*x,&pack->buff[pack->curr],len);
    pack->curr+=len;
    return(wa_true);
}

static wa_boolean p_write_ushort(warp_packet *pack, int x) {
    if (pack->size>65533) return(wa_false);
    pack->buff[pack->size++]=(x>>8)&0x0ff;
    pack->buff[pack->size++]=x&0x0ff;
    return(wa_true);
}

static wa_boolean p_write_int(warp_packet *pack, int x) {
    if (pack->size>65531) return(wa_false);
    pack->buff[pack->size++]=(x>>24)&0x0ff;
    pack->buff[pack->size++]=(x>>16)&0x0ff;
    pack->buff[pack->size++]=(x>>8)&0x0ff;
    pack->buff[pack->size++]=x&0x0ff;
    return(wa_true);
}

static wa_boolean p_write_string(warp_packet *pack, char *x) {
    int len=0;
    char *k=NULL;
    int q=0;

    if (x==NULL) return(p_write_ushort(pack,0));
    for (k=x; k[0]!='\0'; k++);
    len=k-x;
    if (p_write_ushort(pack,len)==wa_false) {
        pack->size-=2;
        return(wa_false);
    }
    if ((pack->size+len)>65535) {
        pack->size-=2;
        return(wa_false);
    }
    for (q=0;q<len;q++) pack->buff[pack->size++]=x[q];
    return(wa_true);
}

/* ************************************************************************* */
/* NETWORK FUNCTIONS                                                         */
/* ************************************************************************* */

static wa_boolean n_recv(apr_socket_t *sock, warp_packet *pack) {
    apr_size_t len=0;
    char hdr[3];
    int ptr=0;

    if (sock==NULL) return(wa_false);
    if (pack==NULL) return(wa_false);

    p_reset(pack);
    len=3;
    while(1) {
        if (apr_recv(sock,&hdr[ptr],&len)!=APR_SUCCESS) return(wa_false);
        ptr+=len;
        len=3-ptr;
        if (len==0) break;
    }
    pack->type=((int)hdr[0])&0x0ff;
    pack->size=(hdr[1]&0x0ff)<<8;
    pack->size=pack->size|(hdr[2]&0x0ff);

    len=pack->size;
    ptr=0;
    while(1) {
        if (apr_recv(sock,&pack->buff[ptr],&len)!=APR_SUCCESS)
            return(wa_false);
        ptr+=len;
        len=pack->size-ptr;
        if (len==0) break;
    }

    wa_debug(WA_MARK,"WARP <<< TYP=%d LEN=%d",pack->type,pack->size);

    return(wa_true);
}

static wa_boolean n_send(apr_socket_t *sock, warp_packet *pack) {
    apr_size_t len=0;
    char hdr[3];
    int ptr=0;

    if (sock==NULL) return(wa_false);
    if (pack==NULL) return(wa_false);

    hdr[0]=(char)(pack->type&0x0ff);
    hdr[1]=(char)((pack->size>>8)&0x0ff);
    hdr[2]=(char)(pack->size&0x0ff);

    len=3;
    while(1) {
        if (apr_send(sock,&hdr[ptr],&len)!=APR_SUCCESS) return(wa_false);
        ptr+=len;
        len=3-ptr;
        if (len==0) break;
    }

    len=pack->size;
    ptr=0;
    while(1) {
        if (apr_send(sock,&pack->buff[ptr],&len)!=APR_SUCCESS)
            return(wa_false);
        ptr+=len;
        len=pack->size-ptr;
        if (len==0) break;
    }

    wa_debug(WA_MARK,"WARP >>> TYP=%d LEN=%d",pack->type,pack->size);

    p_reset(pack);
    return(wa_true);
}

/* Attempt to connect to the remote endpoint of the WARP connection (if not
   done already). */
static wa_boolean n_connect(wa_connection *conn) {
    warp_config *conf=(warp_config *)conn->conf;
    apr_status_t ret=APR_SUCCESS;

    /* Create the APR socket if that has not been yet created */
    if (conf->sock!=NULL) {
        wa_debug(WA_MARK,"Connection \"%s\" already opened",conn->conf);
        return(wa_true);
    }

    ret=apr_socket_create(&conf->sock,AF_INET,SOCK_STREAM,wa_pool);
    if (ret!=APR_SUCCESS) {
        conf->sock=NULL;
        wa_log(WA_MARK,"Cannot create socket for conn. \"%s\"",conn->name);
        return(wa_false);
    }

    /* Attempt to connect to the remote endpoint */
    ret=apr_connect(conf->sock, conf->addr);
    if (ret!=APR_SUCCESS) {
        apr_shutdown(conf->sock,APR_SHUTDOWN_READWRITE);
        conf->sock=NULL;
        wa_log(WA_MARK,"Connection \"%s\" cannot connect",conn->name);
        return(wa_false);
    }

    return(wa_true);
}

/* Attempt to disconnect a connection if connected. */
static void n_disconnect(wa_connection *conn) {
    warp_config *conf=(warp_config *)conn->conf;
    apr_status_t ret=APR_SUCCESS;

    wa_debug(WA_MARK,"Disconnecting \"%s\"",conn->name);

    /* Create the APR socket if that has not been yet created */
    if (conf->sock==NULL) return;

    /* Shutdown and close the socket (ignoring errors) */
    ret=apr_shutdown(conf->sock,APR_SHUTDOWN_READWRITE);
    if (ret!=APR_SUCCESS)
        wa_log(WA_MARK,"Cannot shutdown \"%s\"",conn->name);
    ret=apr_socket_close(conf->sock);
    if (ret!=APR_SUCCESS)
        wa_log(WA_MARK,"Cannot close \"%s\"",conn->name);

    /* Reset the state */
    conf->sock=NULL;
}

static wa_boolean n_check(wa_connection *conn, warp_packet *pack) {
    warp_config *conf=(warp_config *)conn->conf;
    int maj=-1;
    int min=-1;
    int sid=-1;

    if (n_recv(conf->sock,pack)!=wa_true) {
        wa_log(WA_MARK,"Cannot receive handshake WARP packet");
        return(wa_false);
    }

    if (pack->type!=TYPE_CONF_WELCOME) {
        wa_log(WA_MARK,"Invalid WARP packet %d (WELCOME)",pack->type);
        return(wa_false);
    }
        
    if (p_read_ushort(pack,&maj)!=wa_true) {
        wa_log(WA_MARK,"Cannot read major version");
        return(wa_false);
    }

    if (p_read_ushort(pack,&min)!=wa_true) {
        wa_log(WA_MARK,"Cannot read minor version");
        return(wa_false);
    }

    if ((maj!=VERS_MAJOR)||(min!=VERS_MINOR)) {
        wa_log(WA_MARK,"Invalid WARP protocol version %d.%d",maj,min);
        return(wa_false);
    }

    if (p_read_int(pack,&sid)!=wa_true) {
        wa_log(WA_MARK,"Cannot read server id");
        return(wa_false);
    }

    conf->serv=sid;
    wa_debug(WA_MARK,"Connection \"%s\" checked WARP/%d.%d (SERVER ID=%d)",
        conn->name,maj,min,conf->serv);
    return(wa_true);
}

static wa_boolean n_configure(wa_connection *conn) {
    warp_config *conf=(warp_config *)conn->conf;
    wa_chain *elem=warp_applications;
    apr_pool_t *pool=NULL;
    wa_boolean ret=wa_false;
    warp_packet *pack=NULL;

    if (apr_pool_create(&pool,wa_pool)!=APR_SUCCESS) {
        wa_log(WA_MARK,"Cannot create WARP temporary configuration pool");
        n_disconnect(conn);
        return(wa_false);
    }

    if ((pack=p_create(wa_pool))==NULL) {
        wa_log(WA_MARK,"Cannot create WARP configuration packet");
        apr_pool_destroy(pool);
        return(wa_false);
    }

    if ((ret=n_check(conn,pack))==wa_false) n_disconnect(conn);

    while (elem!=NULL) {
        wa_application *appl=(wa_application *)elem->curr;
        wa_debug(WA_MARK,"Deploying \"%s\" via \"%s\" in \"http://%s:%d%s\"",
                appl->name,conn->name,appl->host->name,appl->host->port,
                appl->rpth);
        p_reset(pack);
        pack->type=TYPE_CONF_DEPLOY;
        p_write_string(pack,appl->name);
        p_write_string(pack,appl->host->name);
        p_write_ushort(pack,appl->host->port);
        p_write_string(pack,appl->rpth);
        n_send(conf->sock,pack);

        elem=elem->next;
    }

    apr_pool_destroy(pool);
    return(ret);
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
    wa_chain *elem=warp_connections;

    /* Open all connections having deployed applications */
    while (elem!=NULL) {
        wa_connection *curr=(wa_connection *)elem->curr;
        wa_debug(WA_MARK,"Opening connection \"%s\"",curr->name);
        if (n_connect(curr)==wa_true) {
            wa_debug(WA_MARK,"Connection \"%s\" opened",curr->name);
            if (n_configure(curr)==wa_true)
                wa_debug(WA_MARK,"Connection \"%s\" configured",curr->name);
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

/* Handle a connection from the web server. */
static int warp_handle(wa_request *r, wa_application *a) {
    return(wa_rerror(WA_MARK,r,500,"Not yet implemented"));
}

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
