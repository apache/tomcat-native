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
#include <wa_provider_warp.h>

/**
 * Create a new WARP packet with a given buffer size.
 *
 * @param size The size of the buffer of this packet.
 * @return A pointer to a wa_warp_packet structure.
 */
static wa_warp_packet *wa_warp_packet_create(int size) {
    wa_warp_packet *p=(wa_warp_packet *)malloc(sizeof(wa_warp_packet));
    p->typ=0;
    p->len=0;
    p->siz=size;
    if (size>0) p->buf=(char *)malloc(sizeof(char)*size);
    else p->buf=NULL;
    return(p);
}

/**
 * Release the memory allocated by a packet created by wa_warp_packet_create().
 *
 * @param p The WARP packet to release.
 */
static void wa_warp_packet_free(wa_warp_packet *p) {
    if (p==NULL) return;
    if ((p->buf)!=NULL) free(p->buf);
    free(p);
}

/**
 * Reset the content of a packet created by wa_warp_packet_create().
 *
 * @param p The WARP packet to reset.
 */
static void wa_warp_packet_reset(wa_warp_packet *p) {
    if (p==NULL) return;
    p->typ=0;
    p->len=0;
}

/**
 * Append a short integer (2 bytes) to a WARP packet.
 *
 * @param p The ponter to a packet structure.
 * @param k The value to append to the packet.
 * @return TRUE if we were able to store the short, FALSE otherwise.
 */
static boolean wa_warp_packet_set_short(wa_warp_packet *p, int k) {
    if (p==NULL) return(FALSE);
    // Check if we fit
    if ((p->len+2)>p->siz) return(FALSE);
    // Store the two bytes
    p->buf[p->len++]=(char)((k>>8)&0x0ff);
    p->buf[p->len++]=(char)(k&0x0ff);
    return(TRUE);
}

/**
 * Retrieve a short integer (2 bytes) from a WARP packet.
 *
 * @param p The ponter to a packet structure.
 */
static int wa_warp_packet_get_short(wa_warp_packet *p) {
    int k=0;
    int x=0;

    if (p==NULL) return(-1);
    // Store the two bytes
    x=p->len;
    k=((p->buf[p->len++]<<8)&0x0ff00);
    k=((k|(p->buf[p->len++]&0x0ff))&0x0ffff);
    return(k);
}

/**
 * Append a string to a WARP packet.
 *
 * @param p The ponter to a packet structure.
 * @param s The string to append to the packet buffer.
 * @return TRUE if we were able to store the string, FALSE otherwise.
 */
static boolean wa_warp_packet_set_string(wa_warp_packet *p, char *s) {
    int l=0;
    int x=0;

    if (p==NULL) return(FALSE);
    if (s==NULL) {
        return(wa_warp_packet_set_short(p,0));
    }

    // Evaluate string length
    l=strlen(s);
    // Store string length
    if (!wa_warp_packet_set_short(p,strlen(s))) return(FALSE);
    // Check if we have room in the buffer and copy the string
    if ((p->len+l)>p->siz) return(FALSE);
    for(x=0; x<l; x++) p->buf[p->len++]=s[x];
    return(TRUE);
}

/**
 * Retrieve a string from a WARP packet.
 *
 * @param p The ponter to a packet structure.
 * @param buf The buffer where data needs to be stored.
 * @param len The buffer length.
 * @return The number of bytes copied, or a number greater than len indicating
 *         the number of bytes required to read this string.
 */
static int wa_warp_packet_get_string(wa_warp_packet *p, char *buf, int len) {
    int k=wa_warp_packet_get_short(p);
    int x=0;

    if (k>len) return(k);
    for (x=0; x<k; x++) buf[x]=p->buf[p->len++];
    return(k);
}


/**
 * Send a short integer (2 bytes) over a warp connection.
 *
 * @param c The connection configuration structure.
 * @param k The short integer to send
 * @return TRUE if the data was successfully sent, FALSE otherwise
 */
static boolean wa_warp_send_short(wa_warp_conn_config *c, int k) {
    char buf[2];

    if (c->sock<0) return(FALSE);
    buf[0]=(k>>8)&0x0ff;
    buf[1]=k&0x0ff;
    if (send(c->sock,buf,2,0)!=2) return(FALSE);
    return(TRUE);
}

/**
 * Send a warp packet.
 *
 * @param c The connection configuration structure.
 * @param r The request ID (unique)
 * @param p The warp packet to send
 * @return TRUE if the packet was successfully sent, FALSE otherwise
 */
static boolean wa_warp_send(wa_warp_conn_config *c, int r, wa_warp_packet *p) {
    // Paranoia checks
    if (c->sock<0) return(FALSE);
    if (p==NULL) return(FALSE);
    // Send the connection ID
    if (!wa_warp_send_short(c,r)) return(FALSE);
    // Send the packet type
    if (!wa_warp_send_short(c,p->typ)) return(FALSE);
    // Send the packet length
    if (!wa_warp_send_short(c,p->len)) return(FALSE);

    // Check if we need to send the payload
    if (p->len<=0) return(TRUE);
    if (p->buf==NULL) return(FALSE);

    if (send(c->sock,p->buf,p->len,0)==p->len) return(TRUE);
    else return(FALSE);
}

/**
 * Receive a short integer (2 bytes) over a warp connection.
 */
static int wa_warp_recv_short(wa_warp_conn_config *c) {
    char buf[2];
    int k=0;

    if (c->sock<0) return(-1);
    if ((k=recv(c->sock,buf,2,0))!=2) return(-1);

    k=((((buf[0]<<8)&0x0ff00)|(buf[1]&0x0ff))&0x0ffff);
    return(k);
}

/**
 * Receive a warp packet.
 */
static wa_warp_packet *wa_warp_recv(wa_warp_conn_config *c, int r) {
    wa_warp_packet *p=NULL;
    int rid=-1;
    int typ=-1;
    int siz=-1;

    // Get the packet RID
    if ((rid=wa_warp_recv_short(c))<0) {
        fprintf(stderr,"NO RID (%d)\n",rid);
        return(NULL);
    }

    // Fix this for multithreaded environments (demultiplexing of packets)
    if (rid!=r) {
        fprintf(stderr,"INVALID RID got %d expected %d\n",rid,r);
        return(NULL);
    }

    // Get the packet TYPE
    if ((typ=wa_warp_recv_short(c))<0) {
        fprintf(stderr,"NO TYP rid=%d typ=%d\n",rid,typ);
        return(NULL);
    }

    // Get the packet payload size
    if ((siz=wa_warp_recv_short(c))<0) {
        fprintf(stderr,"NO SIZ rid=%d typ=%d siz=%d\n",rid,typ,siz);
        return(NULL);
    }

    // Create the packet
    p=wa_warp_packet_create(siz);
    p->typ=typ;
    p->siz=siz;
    p->len=0;
    if (siz==0) return(p);

    // Read from the socket and fill the packet buffer
    while(TRUE) {
        p->len+=recv(c->sock,p->buf+p->len,(siz-p->len),0);
        if (p->len<siz) {
            fprintf(stderr,"SHORT len=%d siz=%d\n",p->len,siz);
        } else if (p->len>siz) {
            fprintf(stderr,"INCONSIST len=%d siz=%d\n",p->len,siz);
        } else {
            p->len=0;
            return(p);
        }
    }
}

/**
 * Connect to a warp server, and update the socket information in the
 * configuration member.
 *
 * @param conf The connection configuration member.
 * @return TRUE if we were able to connect to the warp server, FALSE otherwise.
 */
static boolean wa_warp_open(wa_warp_conn_config *conf) {
    struct sockaddr_in a;
    int r;

    wa_callback_debug(WA_LOG,NULL,"Connecting to %s:%d (%d.%d.%d.%d)",
                conf->name,conf->port,
                (int)((conf->addr>>0)&0x0ff),  (int)((conf->addr>>8)&0x0ff),
                (int)((conf->addr>>16)&0x0ff), (int)((conf->addr>>24)&0x0ff));

    // Check if we have a valid host address
    a.sin_addr.s_addr = conf->addr;
    a.sin_port = htons(conf->port);
    a.sin_family = AF_INET;

    // Open the socket
    if ((conf->sock=socket(AF_INET, SOCK_STREAM, 0))<0) {
        conf->sock=SOCKET_NOT_CREATED;
        wa_callback_debug(WA_LOG,NULL,"Cannot create socket");
        return(FALSE);
    }

    // Tries to connect to the WARP server (continues trying while errno=EINTR)
    do {
        wa_callback_debug(WA_LOG,NULL,"Attempting to connect");
        r=connect(conf->sock,(struct sockaddr *)&a,sizeof(struct sockaddr_in));
#ifdef WIN32
        if (r==SOCKET_ERROR) errno=WSAGetLastError()-WSABASEERR;
#endif
        wa_callback_debug(WA_LOG,NULL,"Connect returned %d (err=%d eintr=%d)",
                          r,errno,EINTR);
    } while (r==-1 && errno==EINTR);

    // Check if we are finally connected
    if (r==-1) {
        wa_callback_debug(WA_LOG,NULL,"Unable to connect (err=%d)",errno);
        conf->sock=SOCKET_NOT_CONNECTED;
        return(FALSE);
    }

    // Whohoo!
    wa_callback_debug(WA_LOG,NULL,"Connected to %s:%d (%d.%d.%d.%d)",
                conf->name,conf->port,
                (int)((conf->addr>>0)&0x0ff),  (int)((conf->addr>>8)&0x0ff),
                (int)((conf->addr>>16)&0x0ff), (int)((conf->addr>>24)&0x0ff));

    return(TRUE);
}

/**
 * Close connection to the warp server.
 *
 * @param conf The connection configuration member.
 * @return TRUE if we were able to close the connection, FALSE otherwise.
 */
static boolean wa_warp_close(wa_warp_conn_config *conf, char *description) {
    wa_warp_packet p;

    // Setup the packet to send
    p.typ=0x0ffff;
    if (description==NULL) {
        p.len=10;
        p.buf="No detalis";
    } else {
        p.len=strlen(description);
        p.buf=description;
    }

    // Send the packet
    wa_warp_send(conf,RID_DISCONNECT,&p);

    // Close the socket
    shutdown(conf->sock, 2);
    close(conf->sock);
    conf->sock=SOCKET_NOT_CONNECTED;

    return(TRUE);
}

/**
 * Configure a connection with the parameter from the web server configuration
 * file.
 *
 * @param conn The connection to configure.
 * @param param The extra parameter from web server configuration.
 * @return An error message or NULL.
 */
static const char *wa_warp_configure(wa_connection *conn, char *param) {
    wa_warp_conn_config *conf=NULL;
    struct hostent *host=NULL;
    char *temp=NULL;
    int port;

    // Primary checks
    if (conn==NULL) return("Connection not specified");
    if (param==NULL) return("Invalid host:port for warp connection");
    conf=(wa_warp_conn_config *)malloc(sizeof(wa_warp_conn_config));

    // Find the host name in the parameter (separated by ':')
    conf->sock=SOCKET_NOT_INITIALIZED;
    conf->name=strdup(param);
    temp=strchr(conf->name,':');
    if (temp==NULL) return("Parameter not in host:port format");
    if (temp!=strrchr(conf->name,':'))
        return("Parameter not in host:port format");

    // Separate the host name from the port and translate port number
    temp[0]='\0';
    temp++;
    port=atoi(temp);
    if ((port<1)||(port>65534)) return("Invalid port number specified");
    else conf->port=(unsigned short)port;

    // Resolve the server host address (lazy slow gethostbyname method)
    host=gethostbyname(conf->name);
    if (host==NULL) return("Cannot resolve host name");
    conf->addr=((struct in_addr *)host->h_addr_list[0])->s_addr;

    // Update the configuration member
    conn->conf=(void *)conf;

    // Whohooo! :)
    return(NULL);
}

/**
 * Describe the configuration member found in a connection.
 *
 * @param conn The connection for wich a description must be produced.
 * @param buf The buffer where the description must be stored.
 * @param len The buffer length.
 * @return The number of bytes written to the buffer (terminator included).
 */
static int wa_warp_conninfo(wa_connection *conn, char *buf, int len) {
    wa_warp_conn_config *conf;
    char temp[1024];
    char *msg=NULL;
    int x=0;

    if ((buf==NULL)||(len==0)) return(0);

    // Check sanity
    if(conn==NULL) msg="Null connection specified\0";
    else if(conn->conf==NULL) msg="Invalid configuration\0";
    else {
        conf=(wa_warp_conn_config *)conn->conf;

        // Prepare and return description
        snprintf(temp,1024,"Host: %s (%d.%d.%d.%d) Port: %d",conf->name,
                (int)((conf->addr>>0)&0x0ff),  (int)((conf->addr>>8)&0x0ff),
                (int)((conf->addr>>16)&0x0ff), (int)((conf->addr>>24)&0x0ff),
                conf->port);
        msg=temp;
    }

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
static int wa_warp_applinfo(wa_application *conn, char *buf, int len) {
    wa_warp_appl_config *conf;
    char temp[1024];
    char *msg=NULL;
    int x=0;

    if ((buf==NULL)||(len==0)) return(0);

    // Check sanity
    if(conn==NULL) msg="Null connection specified\0";
    else if(conn->conf==NULL) msg="Invalid configuration\0";
    else {
        conf=(wa_warp_appl_config *)conn->conf;

        // Prepare and return description
        sprintf(temp,"Host ID: %d Application ID: %d",conf->host,conf->appl);
        msg=temp;
    }

    // Copy the message string in the buffer and return
    for (x=0; x<len; x++) {
        buf[x]=msg[x];
        if (msg[x]=='\0') return(x+1);
    }
    buf[x-1]='\0';
    return(x);
}

/**
 * Initialize a connection.
 *
 * @param conn The connection to initialize.
 */
static void wa_warp_init(wa_connection *conn) {
    wa_warp_conn_config *conf;
    wa_host *host=wa_hosts;
    wa_warp_packet *p=wa_warp_packet_create(1024);
    wa_warp_packet *in=NULL;

    // Sanity checks
    if(conn==NULL) {
        wa_callback_log(WA_LOG,NULL,"Can't initialize NULL connection");
        return;
    }
    if(conn->conf==NULL) {
        wa_callback_log(WA_LOG,NULL,"Can't initialize unconfigured connection");
        return;
    }
    conf=(wa_warp_conn_config *)conn->conf;

    wa_callback_debug(WA_LOG,NULL,"Configuring connection %s",conn->name);
    // Try to open a connection with the server
    wa_callback_debug(WA_LOG,NULL,"Attempting to connect");
    if (!wa_warp_open(conf)) {
        wa_callback_log(WA_LOG,NULL,"Can't connect [%s] to %s:%d (%d.%d.%d.%d)",
                conn->name,conf->name,conf->port,
                (int)((conf->addr>>0)&0x0ff),  (int)((conf->addr>>8)&0x0ff),
                (int)((conf->addr>>16)&0x0ff), (int)((conf->addr>>24)&0x0ff));
        return;
    }
    wa_callback_debug(WA_LOG,NULL,"Connected");

    // Configure our list of hosts
    while(host!=NULL) {
        wa_application *appl=host->apps;
        boolean found=FALSE;
        int hid=0;
        

        // Check if this host has applications configured with this provider
        while(appl!=NULL) {
            if (appl->conn->prov==&wa_provider_warp) {
                found=TRUE;
                break;
            } else appl=appl->next;
        }
        if (found==FALSE) {
            host=host->next;
            continue;
        } else appl=host->apps;

        wa_callback_debug(WA_LOG,NULL,"Attempting to configure host %s:%d",
                         host->name,host->port);
        // Setup the packet
        p->typ=TYP_CONINIT_HST;
        wa_warp_packet_set_string(p,host->name);
        wa_warp_packet_set_short(p,host->port);

        // Send the packet describing the host
        if (!wa_warp_send(conf,RID_CONNECTION,p)) {
            wa_warp_packet_free(p);
            wa_warp_close(conf, "Cannot send host data");
            wa_callback_log(WA_LOG,NULL,"Cannot send host %s:%d data",
                            host->name,host->port);
            return;
        }
        wa_warp_packet_reset(p);

        // Retrieve the packet for the host ID
        in=wa_warp_recv(conf,RID_CONNECTION);
        if (in==NULL) {
            wa_warp_packet_free(p);
            wa_warp_close(conf, "Cannot retrieve host ID");
            wa_callback_log(WA_LOG,NULL,"Cannot retrieve host ID for %s:%d",
                            host->name,host->port);
            return;
        }
        // Check if we got the right packet
        if (in->typ!=TYP_CONINIT_HID) {
            wa_warp_packet_free(p);
            wa_warp_packet_free(in);
            wa_warp_close(conf, "Cannot retrieve host ID");
            wa_callback_log(WA_LOG,NULL,"Cannot retrieve host ID for %s:%d",
                            host->name,host->port);
            return;
        }
        hid=wa_warp_packet_get_short(in);
        wa_warp_packet_free(in);
        wa_callback_debug(WA_LOG,NULL,"Host %s:%d configured with ID %d",
                        host->name,host->port,hid);

        // Iterate thru the list of configured applications
        while(appl!=NULL) {
            wa_warp_appl_config *cnf=NULL;
            int aid=0;
            
            // Check if the current application is a warp application
            if (appl->conn->prov!=&wa_provider_warp) {
                appl=appl->next;
                continue;
            }
            
            wa_callback_debug(WA_LOG,NULL,"Attempting to configure app %s %s",
                             appl->name,appl->path);
            p->typ=TYP_CONINIT_APP;
            wa_warp_packet_set_short(p,hid);
            wa_warp_packet_set_string(p,appl->name);
            wa_warp_packet_set_string(p,appl->path);

            // Send the packet
            if (!wa_warp_send(conf,RID_CONNECTION,p)) {
                wa_warp_packet_free(p);
                wa_warp_close(conf, "Cannot send application data");
                wa_callback_log(WA_LOG,NULL,"Can't configure app %s %s",
                                 appl->name,appl->path);
                return;
            }
            wa_warp_packet_reset(p);

            // Retrieve the packet for the application ID
            in=wa_warp_recv(conf,RID_CONNECTION);
            if (in==NULL) {
                wa_warp_packet_free(p);
                wa_warp_close(conf, "Cannot retrieve application ID");
                wa_callback_log(WA_LOG,NULL,"Can't get app %s %s ID",
                                 appl->name,appl->path);
                return;
            }
            // Check if we got the right packet
            if (in->typ!=TYP_CONINIT_AID) {
                wa_warp_packet_free(p);
                wa_warp_packet_free(in);
                wa_warp_close(conf, "Cannot retrieve application ID (TYP)");
                wa_callback_log(WA_LOG,NULL,"Can't get app %s %s ID",
                                 appl->name,appl->path);
                return;
            }
            aid=wa_warp_packet_get_short(in);
            wa_callback_debug(WA_LOG,NULL,"Applic. %s %s configured with ID %d",
                             appl->name,appl->path,aid);
            wa_warp_packet_free(in);

            // Setup this application configuration
            cnf=(wa_warp_appl_config *)malloc(sizeof(wa_warp_appl_config));
            cnf->host=hid;
            cnf->appl=aid;
            appl->conf=cnf;

            // Process the next configured application
            appl=appl->next;
        }

        // Check the next configured host.
        host=host->next;
    }

    // All done (free packet)
    wa_callback_debug(WA_LOG,NULL,"Connection %s configured",conn->name);
    wa_warp_packet_free(p);
}

/**
 * Destroys a connection.
 *
 * @param conn The connection to destroy.
 */
static void wa_warp_destroy(wa_connection *conn) {
    wa_warp_conn_config *conf;

    // Sanity checks
    if(conn==NULL) {
        wa_callback_log(WA_LOG,NULL,"Can't initialize NULL connection");
        return;
    }
    if(conn->conf==NULL) {
        wa_callback_log(WA_LOG,NULL,"Can't initialize unconfigured connection");
        return;
    }
    conf=(wa_warp_conn_config *)conn->conf;

    wa_warp_close(conf,"Normal shutdown");
    wa_callback_debug(WA_LOG,NULL,"Connection %s destroyed",conn->name);
}

static void wa_warp_handle_error(wa_request *req, const char *fmt, ...) {
    char buf[1024];
    va_list ap;

    va_start(ap,fmt);
    vsprintf(buf,fmt,ap);
    va_end(ap);

    // Dump this error to the web server log file
    wa_callback_log(WA_LOG,req,"Error handling \"%s://%s:%d%s%s%s\": %s",
                    req->schm, req->name, req->port, req->ruri, 
                    req->args==NULL?"":"?",req->args==NULL?"":req->args, buf);

    // Dump this error back to the client
    wa_callback_setstatus(req,500);
    wa_callback_settype(req,"text/html");
    wa_callback_commit(req);
    wa_callback_printf(req,"<html>\n");
    wa_callback_printf(req," <head>\n");
    wa_callback_printf(req,"  <title>mod_webapp(warp) error</title>\n");
    wa_callback_printf(req," </head>\n");
    wa_callback_printf(req," <body>\n");
    wa_callback_printf(req,"  <h2>mod_webapp(warp) error</h2>\n");
    wa_callback_printf(req,"  %s\n",buf);
    wa_callback_printf(req," </body>\n");
    wa_callback_printf(req,"</html>\n");
    wa_callback_flush(req);
    return;
}

/**
 * Handle a connection from the web server.
 *
 * @param req The request data.
 * @param cb The web-server callback information.
 */
static void wa_warp_handle(wa_request *req) {
    wa_warp_conn_config *cc=NULL;
    wa_warp_appl_config *ac=NULL;
    wa_warp_packet *in=NULL;
    wa_warp_packet *out=NULL;
    int rid=0;
    int x=0;
    char buf1[8192];
    char buf2[8192];
    boolean committed=FALSE;

    cc=(wa_warp_conn_config *)req->appl->conn->conf;
    ac=(wa_warp_appl_config *)req->appl->conf;
    // If we're not connected, let's try to connect
    if (cc->sock<0) wa_warp_init(req->appl->conn);
    if (cc->sock<0) {
        wa_warp_handle_error(req,
                 "Unable to connect (%d) to Host: %s (%d.%d.%d.%d) Port: %d\n",
                   cc->sock, cc->name,          (int)((cc->addr>>0)&0x0ff),
                   (int)((cc->addr>>8)&0x0ff),  (int)((cc->addr>>16)&0x0ff),
                   (int)((cc->addr>>24)&0x0ff), cc->port);
        return;
    }

    // We are connected... Let's give it a try.
    out=wa_warp_packet_create(4096);

    // Let's start requesting the RID for this request.
    out->typ=TYP_CONINIT_REQ;
    wa_warp_packet_set_short(out,ac->host);
    wa_warp_packet_set_short(out,ac->appl);
    if (!wa_warp_send(cc,RID_CONNECTION,out)) {
        wa_warp_packet_free(out);
        wa_warp_close(cc, "Cannot send RID request");
        wa_warp_handle_error(req,"Cannot send RID request");
        return;
    }
    wa_warp_packet_reset(out);

    // Retrieve the packet containing our RID
    in=wa_warp_recv(cc,RID_CONNECTION);
    if (in==NULL) {
        wa_warp_packet_free(out);
        wa_warp_close(cc, "Cannot retrieve request RID");
        wa_warp_handle_error(req,"Cannot retrieve request RID");
        return;
    }
    // Check if we got the right packet
    if (in->typ!=TYP_CONINIT_RID) {
        wa_warp_packet_free(out);
        wa_warp_packet_free(in);
        wa_warp_close(cc, "Cannot retrieve request RID (TYP)");
        wa_warp_handle_error(req,"Cannot retrieve request RID (TYP)");
        return;
    }
    rid=wa_warp_packet_get_short(in);
    wa_warp_packet_free(in);

    // Send the request method
    wa_warp_packet_reset(out);
    if (req->meth!=NULL) {
        wa_callback_debug(WA_LOG,req,"Sending method \"%s\"",req->meth);
        out->typ=TYP_REQINIT_MET;
        wa_warp_packet_set_string(out,req->meth);
        wa_warp_send(cc,rid,out);
    } else {
        out->typ=TYP_REQINIT_ERR;
        wa_warp_send(cc,rid,out);
        wa_warp_handle_error(req,"Invalid request method (req->meth=NULL)");
        wa_warp_packet_free(out);
        return;
    }

    // Send the request URI
    wa_warp_packet_reset(out);
    if (req->ruri!=NULL) {
        wa_callback_debug(WA_LOG,req,"Sending URI \"%s\"",req->ruri);
        out->typ=TYP_REQINIT_URI;
        wa_warp_packet_set_string(out,req->ruri);
        wa_warp_send(cc,rid,out);
    } else {
        out->typ=TYP_REQINIT_ERR;
        wa_warp_send(cc,rid,out);
        wa_warp_handle_error(req,"Invalid request URI (req->ruri=NULL)");
        wa_warp_packet_free(out);
        return;
    }

    // Send the request arguments
    if (req->args!=NULL) {
        wa_warp_packet_reset(out);
        wa_callback_debug(WA_LOG,req,"Sending query args \"%s\"",req->args);
        out->typ=TYP_REQINIT_ARG;
        wa_warp_packet_set_string(out,req->args);
        wa_warp_send(cc,rid,out);
    }
    
    // Send the request protocol
    wa_warp_packet_reset(out);
    if (req->prot!=NULL) {
        wa_callback_debug(WA_LOG,req,"Sending protocol \"%s\"",req->prot);
        out->typ=TYP_REQINIT_PRO;
        wa_warp_packet_set_string(out,req->prot);
        wa_warp_send(cc,rid,out);
    } else {
        out->typ=TYP_REQINIT_ERR;
        wa_warp_send(cc,rid,out);
        wa_warp_handle_error(req,"Invalid protocol (req->prot=NULL)");
        wa_warp_packet_free(out);
        return;
    }
    

    // Send the request headers
    for (x=0; x<req->hnum; x++) {
        wa_warp_packet_reset(out);
        out->typ=TYP_REQINIT_HDR;
        wa_warp_packet_set_string(out,req->hnam[x]);
        wa_warp_packet_set_string(out,req->hval[x]);
        wa_warp_send(cc,rid,out);
    }

    // Send the request variables (TODO)

    // Try to issue a GO
    wa_warp_packet_reset(out);
    out->typ=TYP_REQINIT_RUN;
    wa_warp_send(cc,rid,out);

    // Retrieve the packet containing the ACK/ERR for this request
    in=wa_warp_recv(cc,rid);
    if (in==NULL) {
        wa_warp_packet_free(out);
        wa_warp_close(cc, "Cannot switch to passive mode");
        wa_warp_handle_error(req,"Cannot switch to passive mode");
        return;
    }
    // Check if we got an ERR packet
    if (in->typ==TYP_REQINIT_ERR) {
        wa_warp_packet_free(out);
        wa_warp_packet_free(in);
        wa_warp_handle_error(req,"Request not accepted by the server");
        return;
    }
    // Check if we got the right (ACK) packet
    if (in->typ!=TYP_REQINIT_ACK) {
        wa_warp_packet_free(out);
        wa_warp_packet_free(in);
        wa_warp_handle_error(req,"Unknown packet received (%d)",in->typ);
        return;
    }

    wa_warp_packet_free(out);
    while (TRUE) {
        in=wa_warp_recv(cc,rid);
        if (in==NULL) {
            wa_warp_close(cc, "No packets received waiting response");
            wa_warp_handle_error(req,"No packets received waiting response");
            return;
        }
        // Check if we got an ERR packet
        if (in->typ==TYP_REQUEST_ERR) {
            wa_warp_packet_free(in);
            wa_warp_handle_error(req,"Error in response");
            return;
        }
        // Check if we got an ACK packet (close)
        if (in->typ==TYP_REQUEST_ACK) {
            wa_warp_packet_free(in);
            wa_callback_flush(req);
            return;
        }
        // Check if we got an STA packet (close)
        if (in->typ==TYP_REQUEST_STA) {
            wa_warp_packet_get_string(in,buf1,8192);
            x=wa_warp_packet_get_short(in);
            wa_warp_packet_get_string(in,buf1,8192);
            wa_callback_setstatus(req,x);
        }
        // Check if we got an HDR packet (header)
        if (in->typ==TYP_REQUEST_HDR) {
            x=wa_warp_packet_get_string(in,buf1,8192);
            buf1[x]='\0';
            x=wa_warp_packet_get_string(in,buf2,8192);
            buf2[x]='\0';
            fprintf(stderr,"HEADER \"%s: %s\"\n",buf1,buf2);
            if (strcasecmp("Connection",buf1)!=0) {
                wa_callback_setheader(req,buf1,buf2);
            }
            if (strcasecmp("Content-Type",buf1)==0) {
                wa_callback_settype(req,buf2);
            }
        }
        // Check if we got an CMT packet (commit)
        if (in->typ==TYP_REQUEST_CMT) {
            if (committed==FALSE) {
                wa_callback_commit(req);
                committed=TRUE;
            }
        }
        // Check if we got an DAT packet (data)
        if (in->typ==TYP_REQUEST_DAT) {
            if (committed==FALSE) {
                wa_callback_commit(req);
                committed=TRUE;
            }
            wa_callback_write(req,in->buf,in->siz);
            wa_callback_flush(req);            
        }
        wa_warp_packet_free(in);
    }
/*
    wa_callback_setstatus(req,200);
    wa_callback_settype(req,"text/html");
    wa_callback_commit(req);
    wa_callback_printf(req,"<html>\n");
    wa_callback_printf(req," <head>\n");
    wa_callback_printf(req,"  <title>mod_webapp(warp) error</title>\n");
    wa_callback_printf(req," </head>\n");
    wa_callback_printf(req," <body>\n");
    wa_callback_printf(req,"  <h2>mod_webapp(warp) request</h2>\n");
    wa_callback_printf(req,"  Connected (socket=%d) to\n", cc->sock);
    wa_callback_printf(req,"  Host: %s (%d.%d.%d.%d) Port: %d<br>\n",
               cc->name,                   (int)((cc->addr>>0)&0x0ff),
               (int)((cc->addr>>8)&0x0ff), (int)((cc->addr>>16)&0x0ff),
               (int)((cc->addr>>24)&0x0ff), cc->port);
    wa_callback_printf(req,"  Received Request ID: %d<br>\n",rid);
    wa_callback_printf(req," </body>\n");
    wa_callback_printf(req,"</html>\n");
    wa_callback_flush(req);
*/
}

/** WebAppLib plugin description. */
wa_provider wa_provider_warp = {
    "warp",
    wa_warp_configure,
    wa_warp_init,
    wa_warp_destroy,
    wa_warp_conninfo,
    wa_warp_applinfo,
    wa_warp_handle,
};
