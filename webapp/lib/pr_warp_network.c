/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/* @version $Id$ */
#include "pr_warp.h"

wa_boolean n_recv(apr_socket_t *sock, warp_packet *pack) {
    apr_size_t len=0;
    char hdr[3];
    int ptr=0;

    if (sock==NULL) return(wa_false);
    if (pack==NULL) return(wa_false);

    p_reset(pack);
    len=3;
    while(1) {
        if (apr_recv(sock,&hdr[ptr],&len)!=APR_SUCCESS) {
            wa_debug(WA_MARK,"Cannot receive header");
            return(wa_false);
        }
        ptr+=len;
        len=3-ptr;
        if (len==0) break;
    }
    pack->type=((int)hdr[0])&0x0ff;
    pack->size=(hdr[1]&0x0ff)<<8;
    pack->size=pack->size|(hdr[2]&0x0ff);

    if (pack->size>0) {
        len=pack->size;
        ptr=0;
        while(1) {
            if (apr_recv(sock,&pack->buff[ptr],&len)!=APR_SUCCESS) {
                wa_debug(WA_MARK,"Cannot receive payload");
                return(wa_false);
            }
            ptr+=len;
            len=pack->size-ptr;
            if (len==0) break;
        }
    }
    
    wa_debug(WA_MARK,"WARP <<< TYP=%02X LEN=%d",pack->type,pack->size);

    return(wa_true);
}

wa_boolean n_send(apr_socket_t *sock, warp_packet *pack) {
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

    wa_debug(WA_MARK,"WARP >>> TYP=%2X LEN=%d",pack->type,pack->size);

    p_reset(pack);
    return(wa_true);
}

/* Attempt to connect to the remote endpoint of the WARP connection (if not
   done already). */
apr_socket_t *n_connect(wa_connection *conn) {
    warp_config *conf=(warp_config *)conn->conf;
    apr_status_t ret=APR_SUCCESS;
    apr_socket_t *sock=NULL;

    ret=apr_socket_create(&sock,AF_INET,SOCK_STREAM,wa_pool);
    if (ret!=APR_SUCCESS) {
        sock=NULL;
        wa_log(WA_MARK,"Cannot create socket for conn. \"%s\"",conn->name);
        return(sock);
    }

    /* Attempt to connect to the remote endpoint */
    ret=apr_connect(sock, conf->addr);
    if (ret!=APR_SUCCESS) {
        apr_shutdown(sock,APR_SHUTDOWN_READWRITE);
        sock=NULL;
        wa_log(WA_MARK,"Connection \"%s\" cannot connect",conn->name);
        return(sock);
    }

#if APR_HAS_THREADS
    apr_atomic_inc(&conf->open_socket_count);
#else
    conf->open_socket_count++;
#endif

    return(sock);
}

/* Attempt to disconnect a connection if connected. */
void n_disconnect(wa_connection *conn, apr_socket_t * sock) {
    warp_config *conf=(warp_config *)conn->conf;
    apr_status_t ret=APR_SUCCESS;

    wa_debug(WA_MARK,"Disconnecting \"%s\"",conn->name);

    /* Create the APR socket if that has not been yet created */
    if (sock==NULL) return;

    /* Shutdown and close the socket (ignoring errors) */
    ret=apr_shutdown(sock,APR_SHUTDOWN_READWRITE);
    if (ret!=APR_SUCCESS)
        wa_log(WA_MARK,"Cannot shutdown \"%s\"",conn->name);
    ret=apr_socket_close(sock);
    if (ret!=APR_SUCCESS)
        wa_log(WA_MARK,"Cannot close \"%s\"",conn->name);

#if APR_HAS_THREADS
    apr_atomic_dec(&conf->open_socket_count);
#else
    conf->open_socket_count--;
#endif
}

