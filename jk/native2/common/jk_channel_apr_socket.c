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
 * Channel using 'plain' TCP sockets or UNIX sockets.
 * Based on jk_sockbuf. It uses a an APR-based mechanism.
 * The UNIX sockets are not yet in APR (the code has to been written).
 * 
 * Properties:
 *  - host/filename
 *  - port
 *  - ndelay (Where the hell we set it?)
 *
 * This channel should 'live' as much as the workerenv. It is stateless.
 * It allocates memory for endpoint private data ( using endpoint's pool ).
 *
 * @author:  Gal Shachor <shachor@il.ibm.com>                           
 * @author: Costin Manolache
 * @author: Jean-Frederic Clere <jfrederic.clere@fujitsu-siemens.com>
 */

#include "jk_map.h"
#include "jk_env.h"
#include "jk_channel.h"
#include "jk_global.h"

#include <string.h>
#include "jk_registry.h"

#include "apr_network_io.h"
#include "apr_errno.h"
#include "apr_general.h"


#define DEFAULT_HOST "127.0.0.1"
#define TYPE_UNIX 1 /* to be move in APR. */
#define TYPE_NET  2 /* to be move in APR. */

/** Information specific for the socket channel
 */
struct jk_channel_apr_private {
    int ndelay;
    apr_sockaddr_t *addr;
    struct sockaddr_un unix_addr;
    int type; /* AF_INET or AF_UNIX */
    char *host;
    short port;
};

/** Informations for each connection
 */
typedef struct jk_channel_apr_data {
    int type; /* AF_INET or AF_UNIX */
    apr_socket_t *sock;
    int unixsock;
} jk_channel_apr_data_t;

typedef struct jk_channel_apr_private jk_channel_apr_private_t;

/*
  We use the _privateInt field directly. Long term we can define our own
  jk_channel_apr_t structure and use the _private field, etc - but we 
  just need to store an int.

  XXX We could also use properties or 'notes'
*/

static int JK_METHOD jk2_channel_apr_resolve(jk_env_t *env, char *host,
                                                short port,
                                                jk_channel_apr_private_t *rc);

static int JK_METHOD jk2_channel_apr_close(jk_env_t *env, jk_channel_t *_this,
                                              jk_endpoint_t *endpoint);


static int JK_METHOD jk2_channel_apr_setProperty(jk_env_t *env,
                                                    jk_bean_t *mbean, 
                                                    char *name, void *valueP)
{
    jk_channel_t *_this=(jk_channel_t *)mbean->object;
    char *value=valueP;
    jk_channel_apr_private_t *socketInfo=
        (jk_channel_apr_private_t *)(_this->_privatePtr);

    if( strcmp( "host", name ) == 0 ) {
        socketInfo->host=value;
    } else if( strcmp( "port", name ) == 0 ) {
        socketInfo->port=atoi( value );
    } else if( strcmp( "file", name ) == 0 ) {
        socketInfo->host=value;
        socketInfo->type=AF_UNIX;
    } else {
        return JK_FALSE;
    }
    return JK_TRUE;
}

/** resolve the host IP ( jk_resolve ) and initialize the channel.
 */
static int JK_METHOD jk2_channel_apr_init(jk_env_t *env,
                                             jk_channel_t *_this)
{
    jk_channel_apr_private_t *socketInfo=
        (jk_channel_apr_private_t *)(_this->_privatePtr);
    int rc;
    short port=socketInfo->port;

    if( socketInfo->host==NULL ) {
        char *localName=_this->mbean->localName;
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelApr.init(): use name %s\n", localName );
        
        if (localName[0]=='/') {
            _this->mbean->setAttribute( env, _this->mbean, "file", localName );
        } else {
            _this->mbean->setAttribute( env, _this->mbean, "host", localName );
        }
    }
    
    if( socketInfo->port<=0 )
        socketInfo->port=8009;

    if( socketInfo->host==NULL )
        socketInfo->host=DEFAULT_HOST;

    rc=jk2_channel_apr_resolve( env, socketInfo->host, socketInfo->port, socketInfo );
    if( rc!= JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "jk2_channel_apr_init: "
                      "can't resolve %s:%d errno=%d\n", socketInfo->host, socketInfo->port, errno );
    }
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channel_apr.init(): %s:%d for %s\n", socketInfo->host,
                  socketInfo->port, _this->worker->mbean->name );

    return rc;
}

/** private: resolve the address on init
 */
static int JK_METHOD jk2_channel_apr_resolve(jk_env_t *env,
                                               char *host, short port,
                                               jk_channel_apr_private_t *rc)
{
    /*
     * If the hostname is an absolut path, we want a UNIX socket.
     * otherwise it is a TCP/IP socket.
     */ 
    env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "jk2_channel_apr_resolve: %s %d\n",
                          host, port);
    if (host[0]=='/') {
        rc->type = TYPE_UNIX;
        memset(&rc->unix_addr, 0, sizeof(struct sockaddr_un));
        rc->unix_addr.sun_family = AF_UNIX;
        strcpy(rc->unix_addr.sun_path, host);
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelApr.resolve(): create unix_addr  %s\n", host );
    } else {
        rc->type = TYPE_NET;
        if ( apr_sockaddr_info_get(&rc->addr, host, APR_UNSPEC, port, 0,
            (apr_pool_t *)env->globalPool->_private)!=APR_SUCCESS) {
            return JK_FALSE;
        }
    }
    return JK_TRUE;
}


/** connect to Tomcat (jk_open_socket)
 */
static int JK_METHOD jk2_channel_apr_open(jk_env_t *env,
                                            jk_channel_t *_this,
                                            jk_endpoint_t *endpoint)
{
    int err;
    jk_channel_apr_private_t *socketInfo=
        (jk_channel_apr_private_t *)(_this->_privatePtr);

    apr_sockaddr_t *remote_sa=socketInfo->addr;
    int ndelay=socketInfo->ndelay;
    jk_channel_apr_data_t *sd=endpoint->channelData;

    apr_socket_t *sock;
    apr_status_t ret;
    apr_interval_time_t timeout = 2 * APR_USEC_PER_SEC;
    char msg[128];

    int unixsock;

    /* UNIX socket (to be moved in APR) */
    if (socketInfo->type==TYPE_UNIX) {
        unixsock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (unixsock<0) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "channelApr.open(): can't create socket %d %s\n",
                          errno, strerror( errno ) );
            return JK_FALSE;
        }
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelApr.open(): create unix socket %d\n", unixsock );
        if (connect(unixsock,(struct sockaddr *)&(socketInfo->unix_addr),
                    sizeof(struct sockaddr_un))<0) {
            close(unixsock);
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "channelApr.connect() connect failed %d %s\n",
                          errno, strerror( errno ) );
            return JK_FALSE;
        }
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelApr.open(): connect unix socket %d %s\n", unixsock, socketInfo->host );
        /* store the channel information */
        if( sd==NULL ) {
            sd=(jk_channel_apr_data_t *)
                endpoint->pool->calloc( env, endpoint->pool,
                                        sizeof( jk_channel_apr_data_t ));
            endpoint->channelData=sd;
        }

        sd->unixsock = unixsock;
        sd->type = socketInfo->type;
        return JK_TRUE;
    }


    if (apr_socket_create(&sock, remote_sa->family, SOCK_STREAM,
                          (apr_pool_t *)env->globalPool->_private)
                         != APR_SUCCESS) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                 "channelApr.open(): can't create socket %d %s\n",
                 errno, strerror( errno ) );
        return JK_FALSE;
    } 
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channelApr.open(): create tcp socket %d\n", sock );

    if (apr_setsocketopt(sock, APR_SO_TIMEOUT, timeout)!= APR_SUCCESS) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                 "channelApr.open(): can't set timeout %d %s\n",
                 errno, strerror( errno ) );
        return JK_FALSE;
    }

    /* Tries to connect to JServ (continues trying while error is EINTR) */
    do {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelApr.open() connect on %d\n",sock);
        ret = apr_connect(sock, remote_sa);
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "jk2_channel_apr_open: %d %s %s\n",ret, strerror( errno ),
                      socketInfo->host);

    } while (ret == APR_EINTR);

    /* Check if we connected */
    if(ret != APR_SUCCESS ) {
        apr_socket_close(sock);
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channelApr.connect() connect failed %d %s\n",
                      ret, apr_strerror( ret, msg, sizeof(msg) ) );
        return JK_FALSE;
    }

    /* XXX needed?
    if(ndelay) {
        int set = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,(char *)&set,sizeof(set));
    }
        
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channelApr.connect(), sock = %d\n", sock);
    */

    /* store the channel information */
    if( sd==NULL ) {
        sd=(jk_channel_apr_data_t *)
            endpoint->pool->calloc( env, endpoint->pool,
                                    sizeof( jk_channel_apr_data_t ));
        endpoint->channelData=sd;
    }
    sd->sock = sock;
    sd->type = socketInfo->type; /* APR should handle it. */

    return JK_TRUE;
}


/** close the socket  ( was: jk2_close_socket )
*/
static int JK_METHOD jk2_channel_apr_close(jk_env_t *env,jk_channel_t *_this,
                                             jk_endpoint_t *endpoint)
{
    apr_socket_t *sd;
    jk_channel_apr_data_t *chD=endpoint->channelData;
    if( chD==NULL ) 
        return JK_FALSE;

    if (chD->type==TYPE_UNIX) { 
        close( chD->unixsock );
        return 0;
    }

    sd=chD->sock;
    chD->sock=NULL; /* XXX check it. */
    /* nothing else to clean, the socket_data was allocated ouf of
     *  endpoint's pool
     */
    if (apr_socket_close(sd)==APR_SUCCESS)
        return(0);
    else
        return(-1);
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
 * @was: jk_tcp_socket_sendfull
 */
static int JK_METHOD jk2_channel_apr_send(jk_env_t *env, jk_channel_t *_this,
                                            jk_endpoint_t *endpoint,
                                            jk_msg_t *msg) 
{
    char *b;
    int len;
    apr_socket_t *sock;
    apr_status_t stat;
    apr_size_t length;
    char data[128];

    int  sent=0;
    int this_time;
    int unixsock;

    jk_channel_apr_data_t *chD=endpoint->channelData;

    if( chD==NULL ) 
        return JK_FALSE;

    msg->end( env, msg );
    len=msg->len;
    b=msg->buf;

    sock=chD->sock;
    unixsock=chD->unixsock;

    env->l->jkLog(env, env->l, JK_LOG_ERROR,
                  "jk2_channel_apr_send %d\n",chD->type);

    if (chD->type==TYPE_NET) {
        length = (apr_size_t) len;
        stat = apr_send(sock, b, &length);
        if (stat!= APR_SUCCESS) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "jk2_channel_apr_send send failed %d %s\n",
                          stat, apr_strerror( stat, data, sizeof(data) ) );
            return -3; /* -2 is not possible... */
        }
        return JK_TRUE;
    }

    while(sent < len) {
        this_time = send(unixsock, (char *)b + sent , len - sent,  0);
            
        if(0 == this_time) {
            return -2;
        }
        if(this_time < 0) {
            return -3;
        }
        sent += this_time;
    }
    /*     return sent; */
    return JK_TRUE;
}


/** receive len bytes.
 * @param sd  opened socket.
 * @param b   buffer to store the data.
 * @param len length to receive.
 * @return    -1: receive failed or connection closed.
 *            >0: length of the received data.
 * Was: tcp_socket_recvfull
 */
static int JK_METHOD jk2_channel_apr_readN( jk_env_t *env,
                                            jk_channel_t *_this,
                                            jk_endpoint_t *endpoint,
                                            char *b, int len ) 
{
    jk_channel_apr_data_t *chD=endpoint->channelData;

    apr_socket_t *sock;
    apr_size_t length;
    apr_status_t stat;

    int sd;
    int rdlen;

    if( chD==NULL ) 
        return JK_FALSE;
    sd=chD->unixsock;
    sock=chD->sock;
    rdlen = 0;
  
    /* this should be moved in APR */ 
    if (chD->type==TYPE_UNIX) { 
        while(rdlen < len) {
            int this_time = recv(sd, 
                                 (char *)b + rdlen, 
                                 len - rdlen, 
                                 0);        
            if(-1 == this_time) {
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

    length = (apr_size_t) len;
    stat =  apr_recv(sock, b, &length);

    if ( stat == APR_EOF)
        return -1; /* socket closed. */
    else if ( stat == APR_SUCCESS) {
        rdlen = (int) length;
        return rdlen; 
    } else
        return -1; /* any error. */
}

/** receive len bytes.
 * @param sd  opened socket.
 * @param b   buffer to store the data.
 * @param len length to receive.
 * @return    -1: receive failed or connection closed.
 *            >0: length of the received data.
 * Was: tcp_socket_recvfull
 */
static int JK_METHOD jk2_channel_apr_recv( jk_env_t *env, jk_channel_t *_this,
                                             jk_endpoint_t *endpoint,
                                             jk_msg_t *msg )
{
    int hlen=msg->headerLength;
    int blen;
    int rc;
    

    jk2_channel_apr_readN( env, _this, endpoint, msg->buf, hlen );

    blen=msg->checkHeader( env, msg, endpoint );
    if( blen < 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channelAprArp.receive(): Bad header\n" );
        return JK_FALSE;
    }
    
    rc= jk2_channel_apr_readN( env, _this, endpoint, msg->buf + hlen, blen);

    if(rc < 0) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
               "channelAprApr.receive(): Error receiving message body %d %d\n",
                      rc, errno);
        return JK_FALSE;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channelApr.receive(): Received len=%d type=%d\n",
                  blen, (int)msg->buf[hlen]);
    return JK_TRUE;

}


int JK_METHOD jk2_channel_apr_socket_factory(jk_env_t *env,
                                             jk_pool_t *pool, 
                                             jk_bean_t *result,
                                             const char *type, const char *name)
{
    jk_channel_t *_this;
    
    _this=(jk_channel_t *)pool->calloc(env, pool, sizeof( jk_channel_t));
    
    _this->_privatePtr= (jk_channel_apr_private_t *)
        pool->calloc( env, pool, sizeof( jk_channel_apr_private_t));

    _this->recv= jk2_channel_apr_recv; 
    _this->send= jk2_channel_apr_send; 
    _this->init= jk2_channel_apr_init; 
    _this->open= jk2_channel_apr_open; 
    _this->close= jk2_channel_apr_close; 
    _this->is_stream=JK_TRUE;

    result->setAttribute= jk2_channel_apr_setProperty; 
    _this->mbean=result;
    result->object= _this;
    
    return JK_TRUE;
}
