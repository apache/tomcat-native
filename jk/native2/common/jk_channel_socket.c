/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2002 The Apache Software Foundation.          *
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
 * 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     *
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

/**
 * Channel using 'plain' TCP sockets. Based on jk_sockbuf. Will be replaced by
 * an APR-based mechanism.
 * 
 * Properties:
 *  - host
 *  - port
 *  - ndelay
 *
 * This channel should 'live' as much as the workerenv. It is stateless.
 * It allocates memory for endpoint private data ( using endpoint's pool ).
 *
 * @author:  Gal Shachor <shachor@il.ibm.com>                           
 * @author: Costin Manolache
 */

#include "jk_map.h"
#include "jk_env.h"
#include "jk_channel.h"
#include "jk_global.h"

#include <string.h>
#include "jk_registry.h"

#ifndef WIN32
    #define closesocket         close
#endif

#define DEFAULT_HOST "127.0.0.1"

/** Information specific for the socket channel
 */
struct jk_channel_socket_private {
    int ndelay;
    struct sockaddr_in addr;    
    char *host;
    short port;
    int keepalive;
    int timeout;
};

typedef struct jk_channel_socket_private jk_channel_socket_private_t;

/*
  We use the _privateInt field directly. Long term we can define our own
  jk_channel_socket_t structure and use the _private field, etc - but we 
  just need to store an int.

  XXX We could also use properties or 'notes'
*/

static int JK_METHOD jk2_channel_socket_resolve(jk_env_t *env, char *host,
                                               short port,
                                               struct sockaddr_in *rc);

static int JK_METHOD jk2_channel_socket_close(jk_env_t *env, jk_channel_t *ch,
                                             jk_endpoint_t *endpoint);

static char *jk2_channel_socket_multiValueInfo[]={"group", NULL };
static char *jk2_channel_socket_setAttributeInfo[]={"host", "port", "route", "lb_factor",
                                                    "level", NULL };

static int JK_METHOD jk2_channel_socket_setAttribute(jk_env_t *env,
                                           jk_bean_t *mbean,
                                           char *name, void *valueP)
{
    jk_channel_t *ch=(jk_channel_t *)mbean->object;
    char *value=(char *)valueP;
    jk_channel_socket_private_t *socketInfo=
    (jk_channel_socket_private_t *)(ch->_privatePtr);

    if( strcmp( "host", name ) == 0 ) {
    socketInfo->host=value;
    } else if( strcmp( "port", name ) == 0 ) {
        socketInfo->port=atoi( value );
    } else if( strcmp( "keepalive", name ) == 0 ) {
        socketInfo->keepalive=atoi( value );
    } else if( strcmp( "timeout", name ) == 0 ) {
        socketInfo->timeout=atoi( value );
    } else if( strcmp( "nodelay", name ) == 0 ) {
        socketInfo->ndelay=atoi( value );
    } else {
        return jk2_channel_setAttribute( env, mbean, name, valueP );
    }
    return JK_OK;
}

/** resolve the host IP ( jk_resolve ) and initialize the channel.
 */
static int JK_METHOD jk2_channel_socket_init(jk_env_t *env,
                                             jk_bean_t *chB )
{
    jk_channel_t *ch=chB->object;
    jk_channel_socket_private_t *socketInfo=
    (jk_channel_socket_private_t *)(ch->_privatePtr);
    int rc;
    char *host=socketInfo->host;

    /* Use information from name */
    if( socketInfo->port<=0 ) {
        char *localName=ch->mbean->localName;
        if( *localName=='\0' ) {
            /* Empty local part */
            socketInfo->port=8009;
            if( socketInfo->host==NULL) socketInfo->host=DEFAULT_HOST;
        } else {
            char *portIdx=strchr( localName, ':' );
            if( portIdx==NULL || portIdx[1]=='\0' ) {
                socketInfo->port=8009;
            } else {
                portIdx++;
                socketInfo->port=atoi( portIdx );
            }
            if( socketInfo->host==NULL ) {
                socketInfo->host=ch->mbean->pool->calloc( env, ch->mbean->pool, strlen( localName ) + 1 );
                if( portIdx==NULL ) {
                    strcpy( socketInfo->host, localName );
                } else {
                    strncpy( socketInfo->host, localName, portIdx-localName-1 );
                }
            }
        }
        
    }
    
    if( socketInfo->port<=0 )
        socketInfo->port=8009;

    if( socketInfo->host==NULL )
        socketInfo->host=DEFAULT_HOST;
    
    rc=jk2_channel_socket_resolve( env, socketInfo->host, socketInfo->port, &socketInfo->addr );
    if( rc!= JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "jk2_channel_socket_init: "
                      "can't resolve %s:%d errno=%d\n", socketInfo->host, socketInfo->port, errno );
    }

    if( ch->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "channel_socket.init(): %s:%d \n", socketInfo->host, socketInfo->port );
    
    return rc;
}

/** private: resolve the address on init
 */
static int JK_METHOD jk2_channel_socket_resolve(jk_env_t *env, char *host, short port,
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
            return JK_ERR;
        }

        laddr = ((struct in_addr *)hoste->h_addr_list[0])->s_addr;
    } else {
        /* If we found only digits we use inet_addr() */
        laddr = inet_addr(host);        
    }
    memcpy(&(rc->sin_addr), &laddr , sizeof(laddr));

    return JK_OK;
}

static int jk2_close_socket(jk_env_t *env, int s)
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


/** connect to Tomcat (jk_open_socket)
 */
static int JK_METHOD jk2_channel_socket_open(jk_env_t *env,
                                            jk_channel_t *ch,
                                            jk_endpoint_t *endpoint)
{
/*    int err; */
    jk_channel_socket_private_t *socketInfo=
    (jk_channel_socket_private_t *)(ch->_privatePtr);

    struct sockaddr_in *addr=&socketInfo->addr;
    int ndelay=socketInfo->ndelay;
    int keepalive=socketInfo->keepalive;
    int ntimeout=socketInfo->timeout;

    int sock;
    int ret;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
#ifdef WIN32
        if(INVALID_SOCKET == sock) { 
            errno = WSAGetLastError() - WSABASEERR;
        }
#endif /* WIN32 */
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                 "channelSocket.open(): can't create socket %d %s\n",
                 errno, strerror( errno ) );
        return JK_ERR;
    }

    if (ntimeout >= 0) {
        /* convert from seconds to ms */
        int set = ntimeout * 1000;
        u_long zero = 0;
#ifdef WIN32
        if (ioctlsocket(sock, FIONBIO, &zero) == SOCKET_ERROR) {
            errno = WSAGetLastError() - WSABASEERR;
#else
        if (ioctl(sock, FIONBIO, &zero) != 0) {
#endif
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "channelSocket.open() ioctlcocket failed %s:%d %d %s \n",
                           socketInfo->host, socketInfo->port, errno, strerror( errno ) );
            return JK_ERR;
        }
        
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &set, sizeof(set));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &set, sizeof(set));
    }

    /* Tries to connect to JServ (continues trying while error is EINTR) */
    do {
        if( ch->mbean->debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "channelSocket.open() connect on %d\n",sock);
        
        ret = connect(sock,(struct sockaddr *)addr,
                      sizeof(struct sockaddr_in));
        
#ifdef WIN32
        if(SOCKET_ERROR == ret) { 
            errno = WSAGetLastError();
        }
    } while (ret == -1 && errno == WSAECONNREFUSED);
#else
    } while (-1 == ret && EINTR == errno);
#endif /* WIN32 */

    /* Check if we connected */
    if(ret != 0 ) {
        jk2_close_socket(env, sock);
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channelSocket.open() connect failed %s:%d %d %s \n",
                      socketInfo->host, socketInfo->port, errno, strerror( errno ) );
        return JK_ERR;
    }

    /* Enable the use of keep-alive packets on TCP connection */
    if(keepalive) {
        int set = 1;
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,(char *)&set,sizeof(set));
    }   

    /* Disable the Nagle algorithm if ndelay is set */
    if(ndelay) {
        int set = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,(char *)&set,sizeof(set));
    }   

    if( ch->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "channelSocket.connect(), sock = %d\n", sock);

    endpoint->sd=sock;

    return JK_OK;
}


/** close the socket  ( was: jk2_close_socket )
*/
static int JK_METHOD jk2_channel_socket_close(jk_env_t *env,jk_channel_t *ch,
                                             jk_endpoint_t *endpoint)
{
    int sd;

    sd=endpoint->sd;
    endpoint->sd=-1;
    /* nothing else to clean, the socket_data was allocated ouf of
     *  endpoint's pool
     */
    return jk2_close_socket(env, sd);
}

/** send a long message
 * @param sd  opened socket.
 * @param b   buffer containing the data.
 * @param len length to send.
 * @return    -2: send returned 0 ? what this that ?
 *            -3: send failed.
 *            JK_OK: success
 * @bug       this fails on Unixes if len is too big for the underlying
 *             protocol.
 * @was: jk_tcp_socket_sendfull
 */
static int JK_METHOD jk2_channel_socket_send(jk_env_t *env, jk_channel_t *ch,
                                            jk_endpoint_t *endpoint,
                                            jk_msg_t *msg) 
{
    unsigned char *b;
    int len;
    int sd;
    int  sent=0;

    sd=endpoint->sd;
    if( sd<0 )
        return JK_ERR;
    
    msg->end( env, msg );
    len=msg->len;
    b=msg->buf;


    while(sent < len) {
#ifdef WIN32
        int this_time = send(sd, (char *)b + sent , len - sent, 0);
#else
        int this_time = write(sd, (char *)b + sent , len - sent);
#endif
    if(0 == this_time) {
        return -2;
    }
    if(this_time < 0) {
        return this_time;
    }
    sent += this_time;
    }
    /*     return sent; */
    return JK_OK; /* 0 */
}


/** receive len bytes.
 * @param sd  opened socket.
 * @param b   buffer to store the data.
 * @param len length to receive.
 * @return    -1: receive failed or connection closed.
 *            >0: length of the received data.
 * Was: tcp_socket_recvfull
 */
static int JK_METHOD jk2_channel_socket_readN( jk_env_t *env,
                                               jk_channel_t *ch,
                                               jk_endpoint_t *endpoint,
                                               unsigned char *b, int len )
{
    int sd;
    int rdlen;

    sd=endpoint->sd;
    rdlen = 0;

    if( sd<0 ) return JK_ERR;
    
    while(rdlen < len) {
#ifdef WIN32
    /* WIN32 read cannot operate on sockets */
    int this_time = recv(sd, 
                 (char *)b + rdlen, 
                 len - rdlen, 0);   
#else
    int this_time = read(sd, 
                 (char *)b + rdlen, 
                 len - rdlen);  
#endif
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

static int JK_METHOD jk2_channel_socket_readN2( jk_env_t *env,
                                                jk_channel_t *ch,
                                                jk_endpoint_t *endpoint,
                                                unsigned char *b, int minLen, int maxLen )
{
    int sd;
    int rdlen;

    sd=endpoint->sd;
    rdlen = 0;

    if( sd<0 ) return JK_ERR;
    
    while(rdlen < minLen ) {
#ifdef WIN32
    /* WIN32 read cannot operate on sockets */
    int this_time = recv(sd, 
                 (char *)b + rdlen, 
                 maxLen - rdlen, 0);    
#else
    int this_time = read(sd, 
                 (char *)b + rdlen, 
                 maxLen - rdlen);   
#endif
/*         fprintf(stderr, "XXX received %d\n", this_time ); */
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


/** receive len bytes.
 * @param sd  opened socket.
 * @param b   buffer to store the data.
 * @param len length to receive.
 * @return    -1: receive failed or connection closed.
 *            >0: length of the received data.
 * Was: tcp_socket_recvfull
 */
static int JK_METHOD jk2_channel_socket_recv( jk_env_t *env, jk_channel_t *ch,
                                              jk_endpoint_t *endpoint,
                                              jk_msg_t *msg )
{
    int hlen=msg->headerLength;
    int blen;
    int rc;

    jk2_channel_socket_readN( env, ch, endpoint, msg->buf, hlen );

    blen=msg->checkHeader( env, msg, endpoint );
    if( blen < 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channelSocket.receive(): Bad header\n" );
        return JK_ERR;
    }
    
    rc= jk2_channel_socket_readN( env, ch, endpoint, msg->buf + hlen, blen);

    if(rc < 0) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
               "channelSocket.receive(): Error receiving message body %d %d\n",
                      rc, errno);
        return JK_ERR;
    }

    if( ch->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "channelSocket.receive(): Received len=%d type=%d\n",
                      blen, (int)msg->buf[hlen]);
    return JK_OK;
}

static int JK_METHOD jk2_channel_socket_recvNew( jk_env_t *env, jk_channel_t *ch,
                                             jk_endpoint_t *endpoint,
                                             jk_msg_t *msg )
{
    int hlen=msg->headerLength;
    int blen;
    int inBuf=0;

    if( endpoint->bufPos > 0 ) {
        memcpy( msg->buf, endpoint->readBuf, endpoint->bufPos );
        inBuf=endpoint->bufPos;
        endpoint->bufPos=0;
    }

    /* Read at least hlen, at most maxlen ( we try to minimize the number
     of read() operations )
    */
    if( inBuf < hlen ) {
        /* Need more data to get the header
         */
        int newData=jk2_channel_socket_readN2( env, ch, endpoint, msg->buf + inBuf, hlen - inBuf, msg->maxlen - inBuf );
        if(newData < 0) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "channelSocket.receive(): Error receiving message head %d %d\n",
                          inBuf, errno);
            return JK_ERR;
        }
        inBuf+=newData;
    }

    blen=msg->checkHeader( env, msg, endpoint );
    if( blen < 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channelSocket.receive(): Bad header\n" );
        return JK_ERR;
    }

    if( inBuf < hlen + blen ) {
        /* We need more data */
        int newData=jk2_channel_socket_readN2( env, ch, endpoint, msg->buf + inBuf,
                                               blen + hlen - inBuf, msg->maxlen - inBuf );
        inBuf+=newData;
        if(newData < 0) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "channelSocket.receive(): Error receiving message body %d %d\n",
                          newData, errno);
            return JK_ERR;
        }
    }

    /* Now we have enough data - possibly more */
    endpoint->bufPos = inBuf - hlen - blen;
    if( endpoint->bufPos > 0 ) {
        memcpy( endpoint->readBuf, msg->buf + hlen + blen, endpoint->bufPos );
    }
    
    if( ch->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "channelSocket.receive(): Received len=%d type=%d total=%d\n",
                      blen, (int)msg->buf[hlen], inBuf );
    return JK_OK;
}



int JK_METHOD jk2_channel_socket_factory(jk_env_t *env,
                                         jk_pool_t *pool, 
                                         jk_bean_t *result,
                                         const char *type, const char *name)
{
    jk_channel_t *ch;
    
    ch=(jk_channel_t *)pool->calloc(env, pool, sizeof( jk_channel_t));
    
    ch->_privatePtr= (jk_channel_socket_private_t *)
    pool->calloc( env, pool, sizeof( jk_channel_socket_private_t));

    ch->recv= jk2_channel_socket_recv; 
    ch->send= jk2_channel_socket_send; 
    ch->open= jk2_channel_socket_open; 
    ch->close= jk2_channel_socket_close; 

    ch->is_stream=JK_TRUE;

    result->setAttribute= jk2_channel_socket_setAttribute; 
    result->init= jk2_channel_socket_init; 

    /*result->getAttributeInfo=jk2_channel_socket_getAttributeInfo;*/
    result->multiValueInfo=jk2_channel_socket_multiValueInfo;
    result->setAttributeInfo=jk2_channel_socket_setAttributeInfo;
    
    result->object= ch;
    ch->mbean=result;

    ch->workerEnv=env->getByName( env, "workerEnv" );
    ch->workerEnv->addChannel( env, ch->workerEnv, ch );

    return JK_OK;
}
