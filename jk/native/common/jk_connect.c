/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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

#ifdef HAVE_SYS_FILIO_H
/* FIONREAD on Solaris et al. */
#include <sys/filio.h>
#endif

#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
#define JK_IS_SOCKET_ERROR(x) ((x) == SOCKET_ERROR)
#define JK_GET_SOCKET_ERRNO() errno = WSAGetLastError() - WSABASEERR
#else
#define JK_IS_SOCKET_ERROR(x) ((x) == -1)
#define JK_GET_SOCKET_ERRNO() ((void)0)
#endif /* WIN32 */

/* our compiler cant deal with char* <-> const char* ... */
#if defined(NETWARE) && !defined(__NOVELL_LIBC__)
typedef char* SET_TYPE;
#else
typedef const char* SET_TYPE;
#endif

/** Set socket to blocking
 * @param sd  socket to manipulate
 * @return    errno: fcntl returns -1 (!WIN32)
 *            pseudo errno: ioctlsocket returns SOCKET_ERROR (WIN32)
 *            0: success
 */
static int soblock(jk_sock_t sd)
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
    if (JK_IS_SOCKET_ERROR(ioctlsocket(sd, FIONBIO, &on))) {
        JK_GET_SOCKET_ERRNO();
        return errno;
    }
#endif /* WIN32 */
    return 0;
}

/** Set socket to non-blocking
 * @param sd  socket to manipulate
 * @return    errno: fcntl returns -1 (!WIN32)
 *            pseudo errno: ioctlsocket returns SOCKET_ERROR (WIN32)
 *            0: success
 */
static int sononblock(jk_sock_t sd)
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
    if (JK_IS_SOCKET_ERROR(ioctlsocket(sd, FIONBIO, &on))) {
        JK_GET_SOCKET_ERRNO();
        return errno;
    }
#endif /* WIN32 */
    return 0;
}

#if defined (WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
/* WIN32 implementation */
/** Non-blocking socket connect
 * @param sd       socket to connect
 * @param addr     address to connect to
 * @param timeout  connect timeout in seconds
 *                 (<=0: no timeout=blocking)
 * @param l        logger
 * @return         -1: some kind of error occured
 *                 SOCKET_ERROR: no timeout given and error
 *                               during blocking connect
 *                 0: success
 */
static int nb_connect(jk_sock_t sd, struct sockaddr *addr, int timeout, jk_logger_t *l)
{
    int rc;

    JK_TRACE_ENTER(l);

    if (timeout <= 0) {
        rc = connect(sd, addr, sizeof(struct sockaddr_in));
        JK_TRACE_EXIT(l);
        return rc;
    }

    if ((rc = sononblock(sd))) {
        JK_TRACE_EXIT(l);
        return -1;
    }
    if (JK_IS_SOCKET_ERROR(connect(sd, addr, sizeof(struct sockaddr_in)))) {
        struct timeval tv;
        fd_set wfdset, efdset;

        if ((rc = WSAGetLastError()) != WSAEWOULDBLOCK) {
            soblock(sd);
            WSASetLastError(rc);
            JK_TRACE_EXIT(l);
            return -1;
        }
        /* wait for the connect to complete or timeout */
        FD_ZERO(&wfdset);
        FD_SET(sd, &wfdset);
        FD_ZERO(&efdset);
        FD_SET(sd, &efdset);

        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;

        rc = select((int)sd + 1, NULL, &wfdset, &efdset, &tv);
        if (JK_IS_SOCKET_ERROR(rc) || rc == 0) {
            rc = WSAGetLastError();
            soblock(sd);
            WSASetLastError(rc);
            JK_TRACE_EXIT(l);
            return -1;
        }
        /* Evaluate the efdset */
        if (FD_ISSET(sd, &efdset)) {
            /* The connect failed. */
            int rclen = (int)sizeof(rc);
            if (getsockopt(sd, SOL_SOCKET, SO_ERROR, (char*) &rc, &rclen))
                rc = 0;
            soblock(sd);
            if (rc)
                WSASetLastError(rc);
            JK_TRACE_EXIT(l);
            return -1;
        }
    }
    soblock(sd);
    JK_TRACE_EXIT(l);
    return 0;
}

#elif !defined(NETWARE)
/* POSIX implementation */
/** Non-blocking socket connect
 * @param sd       socket to connect
 * @param addr     address to connect to
 * @param timeout  connect timeout in seconds
 *                 (<=0: no timeout=blocking)
 * @param l        logger
 * @return         -1: some kind of error occured
 *                 0: success
 */
static int nb_connect(jk_sock_t sd, struct sockaddr *addr, int timeout, jk_logger_t *l)
{
    int rc = 0;

    JK_TRACE_ENTER(l);

    if (timeout > 0) {
        if (sononblock(sd)) {
            JK_TRACE_EXIT(l);
            return -1;
        }
    }
    do {
        rc = connect(sd, addr, sizeof(struct sockaddr_in));
    } while (rc == -1 && errno == EINTR);

    if ((rc == -1) && (errno == EINPROGRESS || errno == EALREADY)
                   && (timeout > 0)) {
        fd_set wfdset;
        struct timeval tv;
        socklen_t rclen = (socklen_t)sizeof(rc);

        FD_ZERO(&wfdset);
        FD_SET(sd, &wfdset);
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        rc = select(sd + 1, NULL, &wfdset, NULL, &tv);
        if (rc <= 0) {
            /* Save errno */
            int err = errno;
            soblock(sd);
            errno = err;
            JK_TRACE_EXIT(l);
            return -1;
        }
        rc = 0;
#ifdef SO_ERROR
        if (!FD_ISSET(sd, &wfdset) ||
            (getsockopt(sd, SOL_SOCKET, SO_ERROR,
                        (char *)&rc, &rclen) < 0) || rc) {
            if (rc)
                errno = rc;
            rc = -1;
        }
#endif /* SO_ERROR */
    }
    /* Not sure we can be already connected */
    if (rc == -1 && errno == EISCONN)
        rc = 0;
    soblock(sd);
    JK_TRACE_EXIT(l);
    return rc;
}
#else
/* NETWARE implementation - blocking for now */
/** Non-blocking socket connect
 * @param sd       socket to connect
 * @param addr     address to connect to
 * @param timeout  connect timeout in seconds (ignored!)
 * @param l        logger
 * @return         -1: some kind of error occured
 *                 0: success
 */
static int nb_connect(jk_sock_t sd, struct sockaddr *addr, int timeout, jk_logger_t *l)
{
    int rc;

    JK_TRACE_ENTER(l);

    rc = connect(sd, addr, sizeof(struct sockaddr_in));
    JK_TRACE_EXIT(l);
    return rc;
}
#endif


#ifdef AS400_UTF8

/*
 *  i5/OS V5R4 need EBCDIC for its runtime calls but APR/APACHE works in UTF
 */
in_addr_t jk_inet_addr(const char * addrstr)
{
    in_addr_t addr;
    char *ptr;

    ptr = (char *)malloc(strlen(addrstr) + 1);
    jk_ascii2ebcdic((char *)addrstr, ptr);
    addr = inet_addr(ptr);
    free(ptr);

    return(addr);
}

#endif

/** Resolve the host IP
 * @param host     host or ip address
 * @param port     port
 * @param rc       return value pointer
 * @param l        logger
 * @return         JK_FALSE: some kind of error occured
 *                 JK_TRUE: success
 */
int jk_resolve(const char *host, int port, struct sockaddr_in *rc, jk_logger_t *l)
{
    int x;
    struct in_addr laddr;

    JK_TRACE_ENTER(l);

    memset(rc, 0, sizeof(struct sockaddr_in));

    rc->sin_port = htons((short)port);
    rc->sin_family = AF_INET;

    /* Check if we only have digits in the string */
    for (x = 0; host[x] != '\0'; x++) {
        if (!isdigit((int)(host[x])) && host[x] != '.') {
            break;
        }
    }

    /* If we found also characters we should make name to IP resolution */
    if (host[x] != '\0') {

#ifdef HAVE_APR
        apr_sockaddr_t *remote_sa, *temp_sa;
        char *remote_ipaddr;

        if (!jk_apr_pool) {
            if (apr_pool_create(&jk_apr_pool, NULL) != APR_SUCCESS) {
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }
        }
        if (apr_sockaddr_info_get
            (&remote_sa, host, APR_UNSPEC, (apr_port_t) port, 0, jk_apr_pool)
            != APR_SUCCESS) {
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        /* Since we are only handling AF_INET (IPV4) address (in_addr_t) */
        /* make sure we find one of those.                               */
        temp_sa = remote_sa;
        while ((NULL != temp_sa) && (AF_INET != temp_sa->family))
            temp_sa = temp_sa->next;

        /* if temp_sa is set, we have a valid address otherwise, just return */
        if (NULL != temp_sa)
            remote_sa = temp_sa;
        else {
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        apr_sockaddr_ip_get(&remote_ipaddr, remote_sa);

        laddr.s_addr = jk_inet_addr(remote_ipaddr);

#else /* HAVE_APR */

        /* XXX : WARNING : We should really use gethostbyname_r in multi-threaded env */
        /* Fortunatly when APR is available, ie under Apache 2.0, we use it */
#if defined(NETWARE) && !defined(__NOVELL_LIBC__)
        struct hostent *hoste = gethostbyname((char*)host);
#else
        struct hostent *hoste = gethostbyname(host);
#endif
        if (!hoste) {
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        laddr = *((struct in_addr *)hoste->h_addr_list[0]);

#endif /* HAVE_APR */
    }
    else {
        /* If we found only digits we use inet_addr() */
        laddr.s_addr = jk_inet_addr(host);
    }
    memcpy(&(rc->sin_addr), &laddr, sizeof(laddr));

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

/** Connect to Tomcat
 * @param addr      address to connect to
 * @param keepalive should we set SO_KEEPALIVE (if !=0)
 * @param timeout   connect timeout in seconds
 *                  (<=0: no timeout=blocking)
 * @param sock_buf  size of send and recv buffer
 *                  (<=0: use default)
 * @param l         logger
 * @return          JK_INVALID_SOCKET: some kind of error occured
 *                  created socket: success
 * @remark          Cares about errno
 */
jk_sock_t jk_open_socket(struct sockaddr_in *addr, int keepalive,
                         int timeout, int connect_timeout,
                         int sock_buf, jk_logger_t *l)
{
    char buf[32];
    jk_sock_t sd;
    int set = 1;
    int ret = 0;
#ifdef SO_LINGER
    struct linger li;
#endif

    JK_TRACE_ENTER(l);

    errno = 0;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (!IS_VALID_SOCKET(sd)) {
        JK_GET_SOCKET_ERRNO();
        jk_log(l, JK_LOG_ERROR,
               "socket() failed (errno=%d)", errno);
        JK_TRACE_EXIT(l);
        return JK_INVALID_SOCKET;
    }
    /* Disable Nagle algorithm */
    if (setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (SET_TYPE)&set,
                   sizeof(set))) {
        JK_GET_SOCKET_ERRNO();
        jk_log(l, JK_LOG_ERROR,
                "failed setting TCP_NODELAY (errno=%d)", errno);
        jk_close_socket(sd, l);
        JK_TRACE_EXIT(l);
        return JK_INVALID_SOCKET;
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "socket TCP_NODELAY set to On");
    if (keepalive) {
#if defined(WIN32) && !defined(NETWARE)
        DWORD dw;
        struct tcp_keepalive ka = { 0 }, ks = { 0 };
        if (timeout)
            ka.keepalivetime = timeout * 10000;
        else
            ka.keepalivetime = 60 * 10000; /* 10 minutes */
        ka.keepaliveinterval = 1000;
        ka.onoff = 1;
        if (WSAIoctl(sd, SIO_KEEPALIVE_VALS, &ka, sizeof(ka),
                     &ks, sizeof(ks), &dw, NULL, NULL)) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                   "failed setting SIO_KEEPALIVE_VALS (errno=%d)", errno);
            jk_close_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_INVALID_SOCKET;                                                
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "socket SO_KEEPALIVE set to %d seconds",
                   ka.keepalivetime / 1000);
#else
        set = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, (SET_TYPE)&set,
                       sizeof(set))) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                   "failed setting SO_KEEPALIVE (errno=%d)", errno);
            jk_close_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_INVALID_SOCKET;
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "socket SO_KEEPALIVE set to On");
#endif
    }

    if (sock_buf > 0) {
        set = sock_buf;
        /* Set socket send buffer size */
        if (setsockopt(sd, SOL_SOCKET, SO_SNDBUF, (SET_TYPE)&set,
                        sizeof(set))) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                    "failed setting SO_SNDBUF (errno=%d)", errno);
            jk_close_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_INVALID_SOCKET;
        }
        set = sock_buf;
        /* Set socket receive buffer size */
        if (setsockopt(sd, SOL_SOCKET, SO_RCVBUF, (SET_TYPE)&set,
                                sizeof(set))) {
            JK_GET_SOCKET_ERRNO();
            jk_log(l, JK_LOG_ERROR,
                    "failed setting SO_RCVBUF (errno=%d)", errno);
            jk_close_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_INVALID_SOCKET;
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "socket SO_SNDBUF and SO_RCVBUF set to %d",
                   sock_buf);
    }

    if (timeout > 0) {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
        int tmout = timeout * 1000;
        setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *) &tmout, sizeof(int));
        setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO,
                   (const char *) &tmout, sizeof(int));
        JK_GET_SOCKET_ERRNO();
#elif defined(SO_RCVTIMEO) && defined(USE_SO_RCVTIMEO) && defined(SO_SNDTIMEO) && defined(USE_SO_SNDTIMEO)
        struct timeval tv;
        tv.tv_sec  = timeout;
        tv.tv_usec = 0;
        setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO,
                   (const void *) &tv, sizeof(tv));
        setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO,
                   (const void *) &tv, sizeof(tv));
#endif
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "timeout %d set for socket=%d",
                   timeout, sd);
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
                "failed setting SO_NOSIGPIPE (errno=%d)", errno);
        jk_close_socket(sd, l);
        JK_TRACE_EXIT(l);
        return JK_INVALID_SOCKET;
    }
#endif
#ifdef SO_LINGER
    /* Make hard closesocket by disabling lingering */
    li.l_linger = li.l_onoff = 0;
    if (setsockopt(sd, SOL_SOCKET, SO_LINGER, (SET_TYPE)&li,
                   sizeof(li))) {
        JK_GET_SOCKET_ERRNO();
        jk_log(l, JK_LOG_ERROR,
                "failed setting SO_LINGER (errno=%d)", errno);
        jk_close_socket(sd, l);
        JK_TRACE_EXIT(l);
        return JK_INVALID_SOCKET;
    }
#endif
    /* Tries to connect to Tomcat (continues trying while error is EINTR) */
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
                "trying to connect socket %d to %s", sd,
                jk_dump_hinfo(addr, buf));

/* Need more infos for BSD 4.4 and Unix 98 defines, for now only
iSeries when Unix98 is required at compil time */
#if (_XOPEN_SOURCE >= 520) && defined(AS400)
    ((struct sockaddr *)addr)->sa_len = sizeof(struct sockaddr_in);
#endif
    ret = nb_connect(sd, (struct sockaddr *)addr, connect_timeout, l);
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
    if (JK_IS_SOCKET_ERROR(ret)) {
        JK_GET_SOCKET_ERRNO();
    }
#endif /* WIN32 */

    /* Check if we are connected */
    if (ret) {
        jk_log(l, JK_LOG_INFO,
               "connect to %s failed (errno=%d)",
               jk_dump_hinfo(addr, buf), errno);
        jk_close_socket(sd, l);
        sd = JK_INVALID_SOCKET;
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG, "socket %d connected to %s",
                   sd, jk_dump_hinfo(addr, buf));
    }
    JK_TRACE_EXIT(l);
    return sd;
}

/** Close the socket
 * @param sd        socket to close
 * @param l         logger
 * @return          -1: some kind of error occured (!WIN32)
 *                  SOCKET_ERROR: some kind of error occured  (WIN32)
 *                  0: success
 * @remark          Does not change errno
 */
int jk_close_socket(jk_sock_t sd, jk_logger_t *l)
{
    int rc;
    int save_errno;

    JK_TRACE_ENTER(l);

    if (!IS_VALID_SOCKET(sd)) {
        JK_TRACE_EXIT(l);
        return -1;
    }

    save_errno = errno;
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
    rc = closesocket(sd) ? -1 : 0;
#else
    rc = close(sd);
#endif
    errno = save_errno;
    JK_TRACE_EXIT(l);
    return rc;
}

#ifndef MAX_SECS_TO_LINGER
#define MAX_SECS_TO_LINGER 30
#endif
#define SECONDS_TO_LINGER  2

#ifndef SHUT_WR
#ifdef SD_SEND
#define SHUT_WR SD_SEND
#else
#define SHUT_WR 0x01
#endif
#endif

/** Drain and close the socket
 * @param sd        socket to close
 * @param l         logger
 * @return          -1: socket to close is invalid
 *                  -1: some kind of error occured (!WIN32)
 *                  SOCKET_ERROR: some kind of error occured  (WIN32)
 *                  0: success
 * @remark          Does not change errno
 */
int jk_shutdown_socket(jk_sock_t sd, jk_logger_t *l)
{
    char dummy[512];
    int rc = 0;
    int save_errno;
    fd_set rs;
    struct timeval tv;
    time_t start = time(NULL);

    JK_TRACE_ENTER(l);

    if (!IS_VALID_SOCKET(sd)) {
        JK_TRACE_EXIT(l);
        return -1;
    }

    save_errno = errno;
    /* Shut down the socket for write, which will send a FIN
     * to the peer.
     */
    if (shutdown(sd, SHUT_WR)) {
        rc = jk_close_socket(sd, l);
        errno = save_errno;
        JK_TRACE_EXIT(l);
        return rc;
    }

    /* Set up to wait for readable data on socket... */
    FD_ZERO(&rs);

    do {
        /* Read all data from the peer until we reach "end-of-file"
         * (FIN from peer) or we've exceeded our overall timeout. If the
         * backend does not send us bytes within 2 seconds
         * (a value pulled from Apache 1.3 which seems to work well),
         * close the connection.
         */
        FD_SET(sd, &rs);
        tv.tv_sec  = SECONDS_TO_LINGER;
        tv.tv_usec = 0;

        if (select((int)sd + 1, &rs, NULL, NULL, &tv) > 0) {
            do {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
                rc = recv(sd, &dummy[0], sizeof(dummy), 0);
                if (JK_IS_SOCKET_ERROR(rc))
                    JK_GET_SOCKET_ERRNO();
#else
                rc = read(sd, &dummy[0], sizeof(dummy));
#endif
            } while (JK_IS_SOCKET_ERROR(rc) && (errno == EINTR || errno == EAGAIN));

            if (rc <= 0)
                break;
        }
        else
            break;

    } while (difftime(time(NULL), start) < MAX_SECS_TO_LINGER);

    rc = jk_close_socket(sd, l);
    errno = save_errno;
    JK_TRACE_EXIT(l);
    return rc;
}

/** send a message
 * @param sd  socket to use
 * @param b   buffer containing the data
 * @param len length to send
 * @param l   logger
 * @return    negative errno: write returns a fatal -1 (!WIN32)
 *            negative pseudo errno: send returns SOCKET_ERROR (WIN32)
 *            JK_SOCKET_EOF: no bytes could be sent
 *            >0: success, provided number of bytes send
 * @remark    Always closes socket in case of error
 * @remark    Cares about errno
 * @bug       this fails on Unixes if len is too big for the underlying
 *            protocol
 */
int jk_tcp_socket_sendfull(jk_sock_t sd, const unsigned char *b, int len, jk_logger_t *l)
{
    int sent = 0;
    int wr;

    JK_TRACE_ENTER(l);

    errno = 0;
    while (sent < len) {
        do {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
            wr = send(sd, (const char*)(b + sent),
                      len - sent, 0);
            if (JK_IS_SOCKET_ERROR(wr))
                JK_GET_SOCKET_ERRNO();
#else
            wr = write(sd, b + sent, len - sent);
#endif
        } while (JK_IS_SOCKET_ERROR(wr) && (errno == EINTR || errno == EAGAIN));

        if (JK_IS_SOCKET_ERROR(wr)) {
            jk_shutdown_socket(sd, l);
            JK_TRACE_EXIT(l);
            return (errno > 0) ? -errno : errno;
        }
        else if (wr == 0) {
            jk_shutdown_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_SOCKET_EOF;
        }
        sent += wr;
    }

    JK_TRACE_EXIT(l);
    return sent;
}

/** receive a message
 * @param sd  socket to use
 * @param b   buffer to store the data
 * @param len length to receive
 * @param l   logger
 * @return    negative errno: read returns a fatal -1 (!WIN32)
 *            negative pseudo errno: recv returns SOCKET_ERROR (WIN32)
 *            JK_SOCKET_EOF: no bytes could be read
 *            >0: success, requested number of bytes received
 * @remark    Always closes socket in case of error
 * @remark    Cares about errno
 */
int jk_tcp_socket_recvfull(jk_sock_t sd, unsigned char *b, int len, jk_logger_t *l)
{
    int rdlen = 0;
    int rd;

    JK_TRACE_ENTER(l);

    errno = 0;
    while (rdlen < len) {
        do {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
            rd = recv(sd, (char *)b + rdlen,
                      len - rdlen, 0);
            if (JK_IS_SOCKET_ERROR(rd))
                JK_GET_SOCKET_ERRNO();
#else
            rd = read(sd, (char *)b + rdlen, len - rdlen);
#endif
        } while (JK_IS_SOCKET_ERROR(rd) && (errno == EINTR || errno == EAGAIN));

        if (JK_IS_SOCKET_ERROR(rd)) {
            jk_shutdown_socket(sd, l);
            JK_TRACE_EXIT(l);
            return (errno > 0) ? -errno : errno;
        }
        else if (rd == 0) {
            jk_shutdown_socket(sd, l);
            JK_TRACE_EXIT(l);
            return JK_SOCKET_EOF;
        }
        rdlen += rd;
    }

    JK_TRACE_EXIT(l);
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

/** Wait for input event on socket until timeout
 * @param sd      socket to use
 * @param timeout wait timeout in milliseconds
 * @param l       logger
 * @return        JK_FALSE: Timeout expired without something to read
 *                JK_FALSE: Error during waiting
 *                JK_TRUE: success
 * @remark        Does not close socket in case of error
 *                to allow for iterative waiting
 * @remark        Cares about errno
 */
int jk_is_input_event(jk_sock_t sd, int timeout, jk_logger_t *l)
{
    fd_set rset;
    struct timeval tv;
    int rc;
    int save_errno;

    JK_TRACE_ENTER(l);

    errno = 0;
    FD_ZERO(&rset);
    FD_SET(sd, &rset);
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    do {
        rc = select((int)sd + 1, &rset, NULL, NULL, &tv);
    } while (rc < 0 && errno == EINTR);

    if (rc == 0) {
        /* Timeout. Set the errno to timeout */
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
        errno = WSAETIMEDOUT - WSABASEERR;
#else
        errno = ETIMEDOUT;
#endif
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    else if (rc < 0) {
        save_errno = errno;
        jk_log(l, JK_LOG_WARNING,
               "error during select on socket sd = %d (errno=%d)", sd, errno);
        errno = save_errno;
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    errno = 0;
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

/** Test if a socket is still connected
 * @param sd   socket to use
 * @param l    logger
 * @return     JK_FALSE: failure
 *             JK_TRUE: success
 * @remark     Always closes socket in case of error
 * @remark     Cares about errno
 */
int jk_is_socket_connected(jk_sock_t sd, jk_logger_t *l)
{
    fd_set fd;
    struct timeval tv;
    int rc;

    JK_TRACE_ENTER(l);

    errno = 0;
    FD_ZERO(&fd);
    FD_SET(sd, &fd);

    /* Initially test the socket without any blocking.
     */
    tv.tv_sec  = 0;
    tv.tv_usec = 0;

    do {
        rc = select((int)sd + 1, &fd, NULL, NULL, &tv);
        JK_GET_SOCKET_ERRNO();
        /* Wait one microsecond on next select, if EINTR */
        tv.tv_sec  = 0;
        tv.tv_usec = 1;
    } while (JK_IS_SOCKET_ERROR(rc) && errno == EINTR);

    errno = 0;
    if (rc == 0) {
        /* If we get a timeout, then we are still connected */
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }
    else if (rc == 1) {
#if defined(WIN32) || (defined(NETWARE) && defined(__NOVELL_LIBC__))
        u_long nr;
        rc = ioctlsocket(sd, FIONREAD, &nr);
        if (rc == 0) {
            if (WSAGetLastError() == 0)
                errno = 0;
            else
                JK_GET_SOCKET_ERRNO();
        }
#else
        int nr;
        rc = ioctl(sd, FIONREAD, (void*)&nr);
#endif
        if (rc == 0 && nr != 0) {
            JK_TRACE_EXIT(l);
            return JK_TRUE;
        }
        JK_GET_SOCKET_ERRNO();
    }
    jk_shutdown_socket(sd, l);

    JK_TRACE_EXIT(l);
    return JK_FALSE;
}
