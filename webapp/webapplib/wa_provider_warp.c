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
    if ((rid=wa_warp_recv_short(c))<0) return(NULL);
    // Fix this for multithreaded environments (demultiplexing of packets)
    if (rid!=r) return(NULL);
    // Get the packet TYPE
    if ((typ=wa_warp_recv_short(c))<0) return(NULL);
    // Get the packet payload size
    if ((siz=wa_warp_recv_short(c))<0) return(NULL);
    // Create the packet
    p=wa_warp_packet_create(siz);
    p->typ=typ;
    p->siz=siz;
    if (siz==0) return(p);
    // Read from the socket and fill the packet buffer
    if (recv(c->sock,p->buf,siz,0)!=siz) {
        wa_warp_packet_free(p);
        return(NULL);
    }
    return(p);
}

/**
 * Connect to a warp server, and update the socket information in the
 * configuration member.
 *
 * @param conf The connection configuration member.
 * @return TRUE if we were able to connect to the warp server, FALSE otherwise.
 */
static boolean wa_warp_connect(wa_warp_conn_config *conf) {
    struct sockaddr_in a;
    int r;

    // Check if we have a valid host address
    a.sin_addr.s_addr = conf->addr;
    a.sin_port = htons(conf->port);
    a.sin_family = AF_INET;

    // Open the socket
    if ((conf->sock=socket(AF_INET, SOCK_STREAM, 0))<0) {
        conf->sock=SOCKET_NOT_CREATED;
        return(FALSE);
    }

    // Tries to connect to the WARP server (continues trying while errno=EINTR)
    do {
        r=connect(conf->sock,(struct sockaddr *)&a,sizeof(struct sockaddr_in));
#ifdef WIN32
        if (r==SOCKET_ERROR) errno=WSAGetLastError()-WSABASEERR;
#endif
    } while (r==-1 && errno==EINTR);

    // Check if we are finally connected
    if (r==-1) {
        conf->sock=SOCKET_NOT_CONNECTED;
        return(FALSE);
    }
    
    // Whohoo!
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
static const char *wa_warp_conn_configure(wa_connection *conn, char *param) {
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
 * @return The number of bytes written to the buffer.
 */
static int wa_warp_conninfo(wa_connection *conn, char *buf, int len) {
    wa_warp_conn_config *conf;
    char temp[1024];

    if ((buf==NULL)||(len==0)) return(0);

    // Check sanity
    if(conn==NULL) return(strlcpy(buf,"Null connection specified",len));
    if(conn->conf==NULL) return(strlcpy(buf,"Invalid configuration",len));
    conf=(wa_warp_conn_config *)conn->conf;

    // Prepare and return description
    sprintf(temp,"Host: %s (%d.%d.%d.%d) Port: %d",conf->name,
            (int)((conf->addr>>0)&0x0ff),  (int)((conf->addr>>8)&0x0ff),  
            (int)((conf->addr>>16)&0x0ff), (int)((conf->addr>>24)&0x0ff),
            conf->port);
    return(strlcpy(buf,temp,len));
}

/**
 * Describe the configuration member found in a web application.
 *
 * @param appl The application for wich a description must be produced.
 * @param buf The buffer where the description must be stored.
 * @param len The buffer length.
 * @return The number of bytes written to the buffer.
 */
static int wa_warp_applinfo(wa_application *conn, char *buf, int len) {
    wa_warp_appl_config *conf;
    char temp[1024];

    if ((buf==NULL)||(len==0)) return(0);

    // Check sanity
    if(conn==NULL) return(strlcpy(buf,"Null connection specified",len));
    if(conn->conf==NULL) return(strlcpy(buf,"Invalid configuration",len));
    conf=(wa_warp_appl_config *)conn->conf;

    // Prepare and return description
    sprintf(temp,"Host ID: %d Application ID: %d",conf->host,conf->appl);
    return(strlcpy(buf,temp,len));
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
    if(conn==NULL) return;
    if(conn->conf==NULL) return;
    conf=(wa_warp_conn_config *)conn->conf;

    // Try to open a connection with the server
    if (!wa_warp_connect(conf)) return;
    
    // Configure our list of hosts
    while(host!=NULL) {
        wa_application *appl=host->apps;
        int hid=0;

        // Setup the packet
        p->typ=TYP_HOST;
        wa_warp_packet_set_string(p,host->name);
        wa_warp_packet_set_short(p,host->port);

        // Send the packet describing the host
        if (!wa_warp_send(conf,RID_CONNECTION,p)) {
            wa_warp_packet_free(p);
            wa_warp_close(conf, "Cannot send host data");
            return;
        }
        wa_warp_packet_reset(p);
        
        // Retrieve the packet for the host ID
        in=wa_warp_recv(conf,RID_CONNECTION);
        if (in==NULL) {
            wa_warp_packet_free(p);
            wa_warp_close(conf, "Cannot retrieve host ID");
            return;
        }
        // Check if we got the right packet
        if (in->typ!=TYP_HOST_ID) {
            wa_warp_packet_free(p);
            wa_warp_packet_free(in);
            wa_warp_close(conf, "Cannot retrieve host ID (TYP)");
            return;
        }
        hid=wa_warp_packet_get_short(in);
        wa_warp_packet_free(in);

        // Iterate thru the list of configured applications
        while(appl!=NULL) {
            int aid=0;
            wa_warp_appl_config *cnf=NULL;

            p->typ=TYP_APPLICATION;
            wa_warp_packet_set_string(p,appl->name);
            wa_warp_packet_set_string(p,appl->path);

            // Send the packet
            if (!wa_warp_send(conf,RID_CONNECTION,p)) {
                wa_warp_packet_free(p);
                wa_warp_close(conf, "Cannot send application data");
                return;
            }
            wa_warp_packet_reset(p);

            // Retrieve the packet for the application ID
            in=wa_warp_recv(conf,RID_CONNECTION);
            if (in==NULL) {
                wa_warp_packet_free(p);
                wa_warp_close(conf, "Cannot retrieve application ID");
                return;
            }
            // Check if we got the right packet
            if (in->typ!=TYP_APPLICATION_ID) {
                wa_warp_packet_free(p);
                wa_warp_packet_free(in);
                wa_warp_close(conf, "Cannot retrieve application ID (TYP)");
                return;
            }
            aid=wa_warp_packet_get_short(in);
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
    wa_warp_packet_free(p);
}

/**
 * Destroys a connection.
 *
 * @param conn The connection to destroy.
 */
static void wa_warp_destroy(wa_connection *conn) {
    wa_warp_conn_config *conf;

    if(conn==NULL) return;
    if(conn->conf==NULL) return;
    conf=(wa_warp_conn_config *)conn->conf;

    wa_warp_close(conf,"Normal shutdown");
}

/**
 * Handle a connection from the web server.
 *
 * @param req The request data.
 * @param cb The web-server callback information.
 */
void wa_warp_handle(wa_request *req, wa_callbacks *cb) {    
    wa_warp_conn_config *conf=NULL;
    
    conf=(wa_warp_conn_config *)req->appl->conn->conf;
    if (conf->sock<0) {
        wa_callback_setstatus(cb,req,500);
        wa_callback_settype(cb,req,"text/html");
        wa_callback_commit(cb,req);
        wa_callback_printf(cb,req,"<html>\n");
        wa_callback_printf(cb,req," <head>\n");
        wa_callback_printf(cb,req,"  <title>mod_webapp(warp) error</title>\n");
        wa_callback_printf(cb,req," </head>\n");
        wa_callback_printf(cb,req," <body>\n");
        wa_callback_printf(cb,req,"  <h2>mod_webapp(warp) error</h2>\n");
        wa_callback_printf(cb,req,"  Unable to connect (%d) to\n", conf->sock);
        wa_callback_printf(cb,req,"  Host: %s (%d.%d.%d.%d) Port: %d\n",
                   conf->name,                   (int)((conf->addr>>0)&0x0ff),
                   (int)((conf->addr>>8)&0x0ff), (int)((conf->addr>>16)&0x0ff),
                   (int)((conf->addr>>24)&0x0ff), conf->port);
        wa_callback_printf(cb,req," </body>\n");
        wa_callback_printf(cb,req,"</html>\n");
        wa_callback_flush(cb,req);
        return;
    }
    wa_callback_setstatus(cb,req,200);
    wa_callback_settype(cb,req,"text/html");
    wa_callback_commit(cb,req);
    wa_callback_printf(cb,req,"<html>\n");
    wa_callback_printf(cb,req," <head>\n");
    wa_callback_printf(cb,req,"  <title>mod_webapp(warp) error</title>\n");
    wa_callback_printf(cb,req," </head>\n");
    wa_callback_printf(cb,req," <body>\n");
    wa_callback_printf(cb,req,"  <h2>mod_webapp(warp) error</h2>\n");
    wa_callback_printf(cb,req,"  Connected (socket=%d) to\n", conf->sock);
    wa_callback_printf(cb,req,"  Host: %s (%d.%d.%d.%d) Port: %d\n",
               conf->name,                   (int)((conf->addr>>0)&0x0ff),
               (int)((conf->addr>>8)&0x0ff), (int)((conf->addr>>16)&0x0ff),
               (int)((conf->addr>>24)&0x0ff), conf->port);
    wa_callback_printf(cb,req," </body>\n");
    wa_callback_printf(cb,req,"</html>\n");
    wa_callback_flush(cb,req);
}

/** WebAppLib plugin description. */
wa_provider wa_provider_warp = {
    "warp",
    wa_warp_conn_configure,
    wa_warp_init,
    wa_warp_destroy,
    wa_warp_handle,
    wa_warp_conninfo,
    wa_warp_applinfo,
};
