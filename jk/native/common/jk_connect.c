/*
 * Copyright (c) 1997-1999 The Java Apache Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * 4. The names "Apache JServ", "Apache JServ Servlet Engine" and 
 *    "Java Apache Project" must not be used to endorse or promote products 
 *    derived from this software without prior written permission.
 *
 * 5. Products derived from this software may not be called "Apache JServ"
 *    nor may "Apache" nor "Apache JServ" appear in their names without 
 *    prior written permission of the Java Apache Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *    
 * THIS SOFTWARE IS PROVIDED BY THE JAVA APACHE PROJECT "AS IS" AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE JAVA APACHE PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Java Apache Group. For more information
 * on the Java Apache Project and the Apache JServ Servlet Engine project,
 * please see <http://java.apache.org/>.
 *
 */

/***************************************************************************
 * Description: Socket/Naming manipulation functions                       *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Based on:    Various Jserv files                                        *
 * Version:     $Revision$                                               *
 ***************************************************************************/


#include "jk_connect.h"
#include "jk_util.h"

int jk_resolve(char *host,
               short port,
               struct sockaddr_in *rc) 
{
    int x;
    u_long laddr;

    rc->sin_port   = htons((short)port);
    rc->sin_family = AF_INET;

    /* Check if we only have digits in the string */
    for(x = 0 ; '\0' != host[x] ; x++) {
        if(!isdigit(host[x]) && host[x] != '.') {
            break;
        }
    }

    if(host[x] != '\0') {
        /* If we found also characters we use gethostbyname()*/
        struct hostent *hoste = gethostbyname(host);
        if(!hoste) {
            return JK_FALSE;
        }

        laddr = ((struct in_addr *)hoste->h_addr_list[0])->s_addr;
    } else {
        /* If we found only digits we use inet_addr() */
        laddr = inet_addr(host);        
    }
    memcpy(&(rc->sin_addr), &laddr , sizeof(laddr));

    return JK_TRUE;
}


int jk_open_socket(struct sockaddr_in *addr, 
                   int ndelay,
                   jk_logger_t *l)
{
    int sock;

    jk_log(l, JK_LOG_DEBUG, "Into jk_open_socket\n");

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock > -1) {
        int ret;
        /* Tries to connect to JServ (continues trying while error is EINTR) */
        do {
            jk_log(l, JK_LOG_DEBUG, "jk_open_socket, try to connect socket = %d\n", sock);
            ret = connect(sock,
                          (struct sockaddr *)addr,
                          sizeof(struct sockaddr_in));
#ifdef WIN32
            if(SOCKET_ERROR == ret) { 
                errno = WSAGetLastError() - WSABASEERR;
            }
#endif /* WIN32 */
            jk_log(l, JK_LOG_DEBUG, "jk_open_socket, after connect ret = %d\n", ret);
        } while (-1 == ret && EINTR == errno);

        /* Check if we connected */
        if(0 == ret) {
            if(ndelay) {
                int set = 1;

                jk_log(l, JK_LOG_DEBUG, "jk_open_socket, set TCP_NODELAY to on\n");
                setsockopt(sock, 
                           IPPROTO_TCP, 
                           TCP_NODELAY, 
                           (char *)&set, 
                           sizeof(set));
            }   

            jk_log(l, JK_LOG_DEBUG, "jk_open_socket, return, sd = %d\n", sock);
            return sock;
        }   
        jk_log(l, JK_LOG_ERROR, "jk_open_socket, connect() failed errno = %d\n", errno);
        jk_close_socket(sock);
    } else {
#ifdef WIN32
        errno = WSAGetLastError() - WSABASEERR;
#endif /* WIN32 */
        jk_log(l, JK_LOG_ERROR, "jk_open_socket, socket() failed errno = %d\n", errno);
    }    

    return -1;
}

int jk_close_socket(int s)
{
#ifdef WIN32
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
#ifdef WIN32
            if(SOCKET_ERROR == this_time) { 
                errno = WSAGetLastError() - WSABASEERR;
            }
#endif /* WIN32 */

    	    if(EAGAIN == errno) {
                continue;
	        } 
		    return -1;
	    }
        if(0 == this_time) {
            return -1; 
        }
	    rdlen += this_time;
    }

    return rdlen;
}
