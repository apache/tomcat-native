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
 * @author      Mladen Turk <mturk@apache.org>
 * @version     $Revision$
 */


#include "jk_connect.h"
#include "jk_util.h"

#ifdef HAVE_APR
#include "apr_network_io.h"
#include "apr_errno.h"
#include "apr_general.h"
#include "apr_pools.h"
static apr_pool_t *jk_apr_pool = NULL;
#endif

#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
#define JK_IS_SOCKET_ERROR(x) ((x) == SOCKET_ERROR)
#define JK_GET_SOCKET_ERRNO() errno = WSAGetLastError() - WSABASEERR
#else
#define JK_IS_SOCKET_ERROR(x) ((x) == -1)
#define JK_GET_SOCKET_ERRNO() ((void)0)
#endif /* WIN32 */


/** resolve the host IP */

int jk_resolve(const char *host, int port, struct sockaddr_in *rc)
{
    int x;
    struct in_addr laddr;

    memset(rc, 0, sizeof(struct sockaddr_in));

    rc->sin_port = htons((short)port);
    rc->sin_family = AF_INET;

    /* Check if we only have digits in the string */
    for (x = 0; host[x] != '\0'; x++) {
        if (!isdigit(host[x]) && host[x] != '.') {
            break;
        }
    }

    /* If we found also characters we shoud make name to IP resolution */
    if (host[x] != '\0') {

#ifdef HAVE_APR
        apr_sockaddr_t *remote_sa, *temp_sa;
        char *remote_ipaddr;

        if (!jk_apr_pool) {
            if (apr_pool_create(&jk_apr_pool, NULL) != APR_SUCCESS)
                return JK_FALSE;
        }
        if (apr_sockaddr_info_get
            (&remote_sa, host, APR_UNSPEC, (apr_port_t) port, 0, jk_apr_pool)
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
        laddr.s_addr = inet_addr(remote_ipaddr);

#else /* HAVE_APR */

        /* XXX : WARNING : We should really use gethostbyname_r in multi-threaded env */
        /* Fortunatly when APR is available, ie under Apache 2.0, we use it */
        struct hostent *hoste = gethostbyname(host);
        if (!hoste) {
            return JK_FALSE;
        }

        laddr = *((struct in_addr *)hoste->h_addr_list[0]);

#endif /* HAVE_APR */
    }
    else {
        /* If we found only digits we use inet_addr() */
        laddr.s_addr = inet_addr(host);
    }
    memcpy(&(rc->sin_addr), &laddr, sizeof(laddr));

    return JK_TRUE;
}

/** connect to Tomcat */

int jk_open_socket(struct sockaddr_in *addr, int keepalive,
                   int timeout, int sock_buf, jk_logger_t *l)
{
    char buf[32];
    int sock;
    int set = 1;
    int ret;

    JK_TRACE_ENTER(l);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        JK_GET_SOCKET_ERRNO();
        jk_log(l, JK_LOG_ERROR,
               "socket() failed with errno=%d", errno);
        JK_TRACE_EXIT(l);
        return -1;
    }
    /* Disable Nagle algorithm */
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&set,
                   sizeof(set))) {
        jk_log(l, JK_LOG_ERROR,
                "failed setting TCP_NODELAY with errno=%d", errno);
        jk_close_socket(sock);
        JK_TRACE_EXIT(l);
        return -1;
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "socket TCP_NODELAY set to On");
    if (keepalive) {
        set = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char *)&set,
                       sizeof(set))) {
            jk_log(l, JK_LOG_ERROR,
                   "failed setting SO_KEEPALIVE with errno=%d", errno);
            jk_close_socket(sock);
            JK_TRACE_EXIT(l);
            return -1;
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "socket SO_KEEPALIVE set to On");
    }

    if (sock_buf > 0) {
        set = sock_buf;
        /* Set socket send buffer size */
        if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char *)&set,
                        sizeof(set))) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                    "failed setting SO_SNDBUF with errno=%d", errno);
            jk_close_socket(sock);
            JK_TRACE_EXIT(l);
            return -1;
        }
        set = sock_buf;
        /* Set socket receive buffer size */
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char *)&set,
                                sizeof(set))) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                    "failed setting SO_RCVBUF with errno=%d", errno);
            jk_close_socket(sock);
            JK_TRACE_EXIT(l);
            return -1;
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "socket SO_SNDBUF and  SO_RCVBUF set to d",
                   sock_buf);
    }

    if (timeout > 0) {
#ifdef WIN32
        timeout = timeout * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   (char *) &timeout, sizeof(int));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
                   (char *) &timeout, sizeof(int));
#else
        /* TODO: How to set the timeout for other platforms? */
#endif
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "timeout %d set for socket=%d",
                   timeout, sock);
    }
#ifdef SO_NOSIGPIPE
    /* The preferred method on Mac OS X (10.2 and later) to prevent SIGPIPEs when
     * sending data to a dead peer. Possibly also existing and in use on other BSD
     * systems?
    */
    set = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&set,
                   sizeof(int))) {
        JK_GET_SOCKET_ERRNO();
        jk_log(l, JK_LOG_ERROR,
                "failed setting SO_NOSIGPIPE with errno=%d", errno);
        jk_close_socket(sock);
        JK_TRACE_EXIT(l);
        return -1;
    }
#endif
    /* Tries to connect to Tomcat (continues trying while error is EINTR) */
    do {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "trying to connect socket %d to %s", sock,
                   jk_dump_hinfo(addr, buf));

/* Need more infos for BSD 4.4 and Unix 98 defines, for now only
iSeries when Unix98 is required at compil time */
#if (_XOPEN_SOURCE >= 520) && defined(AS400)
        ((struct sockaddr *)addr)->sa_len = sizeof(struct sockaddr_in);
#endif
        ret = connect(sock, (struct sockaddr *)addr,
                      sizeof(struct sockaddr_in));
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
        if (ret == SOCKET_ERROR) {
            errno = WSAGetLastError() - WSABASEERR;
        }
#endif /* WIN32 */
    } while (ret == -1 && errno == EINTR);

    /* Check if we are connected */
    if (ret == -1) {
        jk_log(l, JK_LOG_INFO,
                "connect to %s failed with errno=%d",
                jk_dump_hinfo(addr, buf), errno);
        jk_close_socket(sock);
        sock = -1;
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG, "socket %d connected to %s",
                   sock, jk_dump_hinfo(addr, buf));
    }
    JK_TRACE_EXIT(l);
    return sock;
}

/** close the socket */

int jk_close_socket(int s)
{
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
    if (s != INVALID_SOCKET)
        return closesocket(s) ? -1 : 0;
#else
    if (s != -1)
        return close(s);
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
int jk_tcp_socket_sendfull(int sd, const unsigned char *b, int len)
{
    int sent = 0;
    int wr;

    while (sent < len) {
        do {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
            wr = send(sd, (const char*)(b + sent),
                      len - sent, 0);
            if (wr == SOCKET_ERROR)
                errno = WSAGetLastError() - WSABASEERR;
#else
            wr = write(sd, b + sent, len - sent);
#endif
        } while (wr == -1 && errno == EINTR);

        if (wr == -1)
            return (errno > 0) ? -errno : errno;
        else if (wr == 0)
            return JK_SOCKET_EOF;
        sent += wr;
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
int jk_tcp_socket_recvfull(int sd, unsigned char *b, int len)
{
    int rdlen = 0;
    int rd;

    while (rdlen < len) {
        do {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
            rd = recv(sd, (char *)b + rdlen,
                      len - rdlen, 0);
            /* Assuming SOCKET_ERROR is -1 on NETWARE too */
            if (rd == SOCKET_ERROR)
                errno = WSAGetLastError() - WSABASEERR;
#else
            rd = read(sd, (char *)b + rdlen, len - rdlen);
#endif
        } while (rd == -1 && errno == EINTR);

        if (rd == -1)
            return (errno > 0) ? -errno : errno;
        else if (rd == 0)
            return JK_SOCKET_EOF;
        rdlen += rd;
    }

    return rdlen;
}

/**
 * dump a sockaddr_in in A.B.C.D:P in ASCII buffer
 *
 */
char *jk_dump_hinfo(struct sockaddr_in *saddr, char *buf)
{
    unsigned long laddr = (unsigned long)htonl(saddr->sin_addr.s_addr);
    unsigned short lport = (unsigned short)htons(saddr->sin_port);

    sprintf(buf, "%d.%d.%d.%d:%d",
            (int)(laddr >> 24), (int)((laddr >> 16) & 0xff),
            (int)((laddr >> 8) & 0xff), (int)(laddr & 0xff), (int)lport);

    return buf;
}

static int soblock(int sd)
{
/* BeOS uses setsockopt at present for non blocking... */
#ifndef WIN32
    int fd_flags;

    fd_flags = fcntl(sd, F_GETFL, 0);
#if defined(O_NONBLOCK)
    fd_flags &= ~O_NONBLOCK;
#elif defined(O_NDELAY)
    fd_flags &= ~O_NDELAY;
#elif defined(FNDELAY)
    fd_flags &= ~FNDELAY;
#else
#error Please teach JK how to make sockets blocking on your platform.
#endif
    if (fcntl(sd, F_SETFL, fd_flags) == -1) {
        return errno;
    }
#else
    u_long on = 0;
    if (ioctlsocket(sd, FIONBIO, &on) == SOCKET_ERROR) {
        errno = WSAGetLastError() - WSABASEERR;
        return errno;
    }
#endif /* WIN32 */
    return 0;
}

static int sononblock(int sd)
{
#ifndef WIN32
    int fd_flags;

    fd_flags = fcntl(sd, F_GETFL, 0);
#if defined(O_NONBLOCK)
    fd_flags |= O_NONBLOCK;
#elif defined(O_NDELAY)
    fd_flags |= O_NDELAY;
#elif defined(FNDELAY)
    fd_flags |= FNDELAY;
#else
#error Please teach JK how to make sockets non-blocking on your platform.
#endif
    if (fcntl(sd, F_SETFL, fd_flags) == -1) {
        return errno;
    }
#else
    u_long on = 1;
    if (ioctlsocket(sd, FIONBIO, &on) == SOCKET_ERROR) {
        errno = WSAGetLastError() - WSABASEERR;
        return errno;
    }
#endif /* WIN32 */
    return 0;
}

#if 1
int jk_is_socket_connected(int sd, int timeout)
{
    fd_set fd;
    struct timeval tv;
 
    FD_ZERO(&fd);
    FD_SET(sd, &fd);

    /* Wait one microsecond */
    tv.tv_sec  = 0;
    tv.tv_usec = 1;

    /* If we get a timeout, then we are still connected */
    if (select(1, &fd, NULL, NULL, &tv) == 0)
        return 1;
    else {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
        errno = WSAGetLastError() - WSABASEERR;
#endif
        return 0;
    }
}

#else

#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
#define EWOULDBLOCK (WSAEWOULDBLOCK - WSABASEERR)
#endif

int jk_is_socket_connected(int sd, int timeout)
{
    unsigned char test_buffer[1];
    int  rc;
    /* Set socket to nonblocking */
    if ((rc = sononblock(sd)) != 0)
        return (errno > 0) ? -errno : errno;

    rc = jk_tcp_socket_recvfull(sd, test_buffer, 1) * (-1);
    soblock(sd);
#ifdef WIN32
    /* Reset socket timeouts if the new timeout differs from the old timeout */
    if (timeout > 0) {
        /* Timeouts are in msec, represented as int */
        setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO,
                    (char *) &timeout, sizeof(int));
        setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO,
                    (char *) &timeout, sizeof(int));
    }
#endif
    if (rc == EWOULDBLOCK || rc == -1)
        return 1;
    else
        return rc;
}

#endif
