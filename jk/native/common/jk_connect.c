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

/*
 * Description: Socket/Naming manipulation functions
 * Based on:    Various Jserv files
 */
/**
 * @package jk_connect
 * @author      Gal Shachor <shachor@il.ibm.com>
 * @version     $Revision$
 */


#include "jk_connect.h"
#include "jk_util.h"

#ifdef HAVE_APR
#include "apr_network_io.h"
#include "apr_errno.h"
#include "apr_general.h"
#endif

#if defined(WIN32)
typedef u_long in_addr_t;
#endif


/** resolve the host IP */
 
int jk_resolve(char *host,
               int port,
               struct sockaddr_in *rc) 
{
    int x;

    /* TODO: Should be updated for IPV6 support. */
    /* for now use the correct type, in_addr_t */    
    /* except on NetWare since the MetroWerks compiler is so strict */
#if defined(NETWARE)
    u_long laddr;
#else
	in_addr_t laddr;
#endif

    memset(rc, 0, sizeof(struct sockaddr_in));

    rc->sin_port   = htons((short)port);
    rc->sin_family = AF_INET;

    /* Check if we only have digits in the string */
    for(x = 0 ; '\0' != host[x] ; x++) {
        if(!isdigit(host[x]) && host[x] != '.') {
            break;
        }
    }

    /* If we found also characters we shoud make name to IP resolution */
    if(host[x] != '\0') {

#ifdef HAVE_APR
        apr_pool_t *context;
        apr_sockaddr_t *remote_sa, *temp_sa;
        char *remote_ipaddr;

        /* May be we could avoid to recreate it each time ? */
        if (apr_pool_create(&context, NULL) != APR_SUCCESS)
            return JK_FALSE;

        if (apr_sockaddr_info_get(&remote_sa, host, APR_UNSPEC, (apr_port_t)port, 0, context)
            != APR_SUCCESS) 
            return JK_FALSE;

        /* Since we are only handling AF_INET (IPV4) address (in_addr_t) */
        /* make sure we find one of those.                               */
        temp_sa = remote_sa;
        while ((NULL != temp_sa) && (AF_INET != temp_sa->family))
            temp_sa = temp_sa->next;

        /* if temp_sa is set, we have a valid address otherwise, just return */
        if (NULL != temp_sa)
            remote_sa = temp_sa;
        else
            return JK_FALSE;

        apr_sockaddr_ip_get(&remote_ipaddr, remote_sa);
        laddr = inet_addr(remote_ipaddr);

        /* May be we could avoid to delete it each time ? */
        apr_pool_destroy(context);

#else /* HAVE_APR */

      /* XXX : WARNING : We should really use gethostbyname_r in multi-threaded env */
      /* Fortunatly when APR is available, ie under Apache 2.0, we use it */
        struct hostent *hoste = gethostbyname(host);
        if(!hoste) {
            return JK_FALSE;
        }

        laddr = ((struct in_addr *)hoste->h_addr_list[0])->s_addr;

#endif /* HAVE_APR */
    } else {
        /* If we found only digits we use inet_addr() */
        laddr = inet_addr(host);        
    }
    memcpy(&(rc->sin_addr), &laddr , sizeof(laddr));

    return JK_TRUE;
}

/** connect to Tomcat */

int jk_open_socket(struct sockaddr_in *addr, 
                   int ndelay,
                   int keepalive,
                   jk_logger_t *l)
{
    char buf[32];   
    int sock;

    jk_log(l, JK_LOG_DEBUG, "Into jk_open_socket\n");

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock > -1) {
        int ret;
        /* Tries to connect to Tomcat (continues trying while error is EINTR) */
        do {
            jk_log(l, JK_LOG_DEBUG, "jk_open_socket, try to connect socket = %d to %s\n", 
                   sock, jk_dump_hinfo(addr, buf));
                   
            ret = connect(sock,
                          (struct sockaddr *)addr,
                          sizeof(struct sockaddr_in));
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
            if(SOCKET_ERROR == ret) { 
                errno = WSAGetLastError() - WSABASEERR;
            }
#endif /* WIN32 */
            jk_log(l, JK_LOG_DEBUG, "jk_open_socket, after connect ret = %d\n", ret);
        } while (-1 == ret && EINTR == errno);

        /* Check if we connected */
        if(0 == ret) {
                                                int keep = 1;
            if(ndelay) {
                int set = 1;

                jk_log(l, JK_LOG_DEBUG, "jk_open_socket, set TCP_NODELAY to on\n");
                setsockopt(sock, 
                           IPPROTO_TCP, 
                           TCP_NODELAY, 
                           (char *)&set, 
                           sizeof(set));
            }   

            if (keepalive) {
                jk_log(l, JK_LOG_DEBUG, "jk_open_socket, set SO_KEEPALIVE to on\n");
                setsockopt(sock,
                           SOL_SOCKET,
                           SO_KEEPALIVE,
                           (char *)&keep,
                           sizeof(keep));
            }

            jk_log(l, JK_LOG_DEBUG, "jk_open_socket, return, sd = %d\n", sock);
            return sock;
        }   
        jk_log(l, JK_LOG_INFO, "jk_open_socket, connect() failed errno = %d\n", errno);
        jk_close_socket(sock);
    } else {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
        errno = WSAGetLastError() - WSABASEERR;
#endif /* WIN32 */
        jk_log(l, JK_LOG_ERROR, "jk_open_socket, socket() failed errno = %d\n", errno);
    }    

    return -1;
}

/** close the socket */

int jk_close_socket(int s)
{
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
    if(INVALID_SOCKET  != s) {
        return closesocket(s) ? -1 : 0; 
    }
#else 
    if(-1 != s) {
        return close(s); 
    }
#endif

    return -1;
}

/** send a long message
 * @param sd  opened socket.
 * @param b   buffer containing the data.
 * @param len length to send.
 * @return    -2: send returned 0 ? what this that ?
 *            -3: send failed.
 *            >0: total size send.
 * @bug       this fails on Unixes if len is too big for the underlying
 *             protocol.
 */
int jk_tcp_socket_sendfull(int sd, 
                           const unsigned char *b,
                           int len)
{
    int sent = 0;

    while(sent < len) {
        int this_time = send(sd, 
                             (char *)b + sent , 
                             len - sent, 
                             0);
        
        if(0 == this_time) {
            return -2;
        }
        if(this_time < 0) {
            return -3;
        }
        sent += this_time;
    }

    return sent;
}

/** receive len bytes. Used in ajp_common.
 * @param sd  opened socket.
 * @param b   buffer to store the data.
 * @param len length to receive.
 * @return    <0: receive failed or connection closed.
 *            >0: length of the received data.
 */
int jk_tcp_socket_recvfull(int sd, 
                           unsigned char *b, 
                           int len) 
{
    int rdlen = 0;

    while(rdlen < len) {
        int this_time = recv(sd, 
                             (char *)b + rdlen, 
                             len - rdlen, 
                             0);    
        if(-1 == this_time) {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
            /* I assume SOCKET_ERROR == -1 */
            if(SOCKET_ERROR == this_time) { 
                errno = WSAGetLastError() - WSABASEERR;
            }
#endif /* WIN32 */

            if(EAGAIN == errno) {
                continue;
            }
            /** Pass the errno to the caller */
            return (errno>0) ? -errno : errno;
        }
        if(0 == this_time) {
            return -1; 
        }
        rdlen += this_time;
    }

    return rdlen;
}

/**
 * dump a sockaddr_in in A.B.C.D:P in ASCII buffer
 *
 */
char * jk_dump_hinfo(struct sockaddr_in *saddr, char * buf)
{
	unsigned long laddr = (unsigned long)htonl(saddr->sin_addr.s_addr);
	unsigned short lport = (unsigned short)htons(saddr->sin_port);

	sprintf(buf, "%d.%d.%d.%d:%d", 
	        (int)(laddr >> 24), (int)((laddr >> 16) & 0xff), (int)((laddr >> 8) & 0xff), (int)(laddr & 0xff), (int)lport);
	        
	return buf;
}
