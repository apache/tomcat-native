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

/**
 * Channel using APR sockets.
 *
 * @author:  Gal Shachor <shachor@il.ibm.com>                           
 * @author: Costin Manolache
 * @author: Jean-Frederic Clere <jfrederic.clere@fujitsu-siemens.com>
 */

#include "jk_global.h"
#include "jk_map.h"
#include "jk_env.h"
#include "jk_channel.h"
#include "jk_global.h"
#include "jk_registry.h"

/** Information specific for the socket channel
 */
struct jk_channel_apr_private {
    int ndelay;
    apr_sockaddr_t *addr;
    char *host;
    apr_port_t port;
    int keepalive;
    int timeout;
};

typedef struct jk_channel_apr_private jk_channel_apr_private_t;

/*
  We use the _privateInt field directly. Long term we can define our own
  jk_channel_apr_t structure and use the _private field, etc - but we 
  just need to store an int.

  XXX We could also use properties or 'notes'
*/

static int JK_METHOD jk2_channel_apr_resolve(jk_env_t *env, char *host,
                                                apr_port_t port,
                                                jk_channel_apr_private_t *rc);

static int JK_METHOD jk2_channel_apr_close(jk_env_t *env, jk_channel_t *_this,
                                              jk_endpoint_t *endpoint);


static char *jk2_channel_apr_socket_getAttributeInfo[]={"host", "port", "keepalive", "timeout", "nodelay", "graceful",
                                                        "debug", "disabled", NULL };
static char *jk2_channel_apr_socket_setAttributeInfo[]={"host", "port", "keepalive", "timeout", "nodelay", "graceful",
                                                        "debug", "disabled", NULL };

static int JK_METHOD jk2_channel_apr_setProperty(jk_env_t *env,
                                                    jk_bean_t *mbean, 
                                                    char *name, void *valueP)
{
    jk_channel_t *ch=(jk_channel_t *)mbean->object;
    char *value=valueP;
    jk_channel_apr_private_t *socketInfo=
        (jk_channel_apr_private_t *)(ch->_privatePtr);

    if( strcmp( "host", name ) == 0 ) {
        socketInfo->host=value;
    } else if( strcmp( "port", name ) == 0 ) {
        socketInfo->port=atoi( value );
    } else if( strcmp( "keepalive", name ) == 0 ) {
        socketInfo->keepalive=atoi( value );
    } else if( strcmp( "timeout", name ) == 0 ) {
        socketInfo->timeout=atoi( value );
    } else if( strcmp( "nodelay", name ) == 0 ) {
        socketInfo->timeout=atoi( value );
    } else {
        return jk2_channel_setAttribute( env, mbean, name, valueP );
    }
    return JK_OK;
}

static void * JK_METHOD jk2_channel_apr_socket_getAttribute(jk_env_t *env, jk_bean_t *bean,
                                                            char *name )
{
    jk_channel_t *ch=(jk_channel_t *)bean->object;
    jk_channel_apr_private_t *socketInfo=
        (jk_channel_apr_private_t *)(ch->_privatePtr);
    
    if( strcmp( name, "name" )==0 ) {
        return  bean->name;
    } else if( strcmp( "host", name ) == 0 ) {
        return socketInfo->host;
    } else if( strcmp( "port", name ) == 0 ) {
        return jk2_env_itoa( env, socketInfo->port );
    } else if( strcmp( "nodelay", name ) == 0 ) {
        return jk2_env_itoa( env, socketInfo->ndelay );
    } else if( strcmp( "keepalive", name ) == 0 ) {
        return jk2_env_itoa( env, socketInfo->keepalive );
    } else if( strcmp( "timeout", name ) == 0 ) {
        return jk2_env_itoa( env, socketInfo->timeout );
    } else if( strcmp( "graceful", name ) == 0 ) {
        return jk2_env_itoa( env, ch->worker->graceful );
    } else if( strcmp( "debug", name ) == 0 ) {
        return jk2_env_itoa( env, ch->mbean->debug );
    } else if( strcmp( "disabled", name ) == 0 ) {
        return jk2_env_itoa( env, ch->mbean->disabled );
    }
    return NULL;
}


/** resolve the host IP ( jk_resolve ) and initialize the channel.
 */
static int JK_METHOD jk2_channel_apr_init(jk_env_t *env,
                                          jk_bean_t *chB)
{
    jk_channel_t *ch=chB->object;
    jk_channel_apr_private_t *socketInfo=
        (jk_channel_apr_private_t *)(ch->_privatePtr);
    int rc;

    if( socketInfo->host==NULL ) {
        char *localName=ch->mbean->localName;
        
        char *portIdx=strchr( localName, ':' );

        if( portIdx==NULL || portIdx[1]=='\0' ) {
            socketInfo->port=AJP13_DEF_PORT;
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
    
    if( socketInfo->port<=0 )
        socketInfo->port=AJP13_DEF_PORT;

    if( socketInfo->host==NULL )
        socketInfo->host=AJP13_DEF_HOST;

    rc=jk2_channel_apr_resolve( env, socketInfo->host, socketInfo->port, socketInfo );
    if( rc!= JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "jk2_channel_apr_init: "
                      "can't resolve %s:%d errno=%d\n", socketInfo->host, socketInfo->port, errno );
    }

    return rc;
}

/*
 * Wait input event on socket for timeout ms
 */
static int JK_METHOD jk2_channel_apr_hasinput(jk_env_t *env,
                                              jk_channel_t *ch,
                                              jk_endpoint_t *endpoint,
                                              int timeout)

{
    /*
     * Should implements the APR select/poll for socket here
     */
    return (JK_TRUE) ;
}


/** private: resolve the address on init
 */
static int JK_METHOD jk2_channel_apr_resolve(jk_env_t *env,
                                             char *host, apr_port_t port,
                                             jk_channel_apr_private_t *rc)
{
    int err;
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channelApr.resolve(): create AF_NET  %s %d\n", host, port );
    err=apr_sockaddr_info_get(&rc->addr, host, APR_UNSPEC, port, 0,
                              (apr_pool_t *)env->globalPool->_private);
    if ( err != APR_SUCCESS) {
        return err;
    }
    return JK_OK;
}


/** connect to Tomcat (jk_open_socket)
 */
static int JK_METHOD jk2_channel_apr_open(jk_env_t *env,
                                            jk_channel_t *ch,
                                            jk_endpoint_t *endpoint)
{
    jk_channel_apr_private_t *socketInfo=
        (jk_channel_apr_private_t *)(ch->_privatePtr);

    apr_sockaddr_t *remote_sa=socketInfo->addr;
    int ndelay=socketInfo->ndelay;
    int keepalive=socketInfo->keepalive;

    apr_socket_t *sock;
    apr_status_t ret;
    apr_int32_t timeout = (apr_int32_t)(socketInfo->timeout * APR_USEC_PER_SEC);
    char msg[128];
    int connected = 0;

    while (remote_sa && !connected) {
        if ((ret = apr_socket_create(&sock, remote_sa->family, SOCK_STREAM, 
#if (APR_MAJOR_VERSION > 0) 
                                     APR_PROTO_TCP,
#endif
                                     (apr_pool_t *)env->globalPool->_private))
                                    != APR_SUCCESS) {
            if (remote_sa->next) {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "channelApr.open(): error %d creating socket to %s\n",
                              ret, socketInfo->host);
            }
            else {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "channelApr.open(): error %d creating socket to %s\n",
                              ret, socketInfo->host);
            }
            remote_sa = remote_sa->next;
            continue;
        }
        /* store the channel information */
        endpoint->channelData=sock;

        env->l->jkLog(env, env->l, JK_LOG_INFO,
            "channelApr.open(): create tcp socket %d\n", sock );
        
        /* the default timeout (0) will set the socket to blocking with
           infinite timeouts.
        */

        if (timeout <= 0)
            apr_socket_timeout_set(sock, -1);
        else
            apr_socket_timeout_set(sock, timeout);

        /* make the connection out of the socket */
        do { 
            ret = apr_socket_connect(sock, remote_sa);
        } while (APR_STATUS_IS_EINTR(ret));
        
        /* if an error occurred, loop round and try again */
        if (ret != APR_SUCCESS) {
            apr_socket_close(sock);
            if (remote_sa->next) {
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "channelApr.open() attempt to connect to %pI (%s) failed %d\n",
                              remote_sa,
                              socketInfo->host,
                              ret);
            }
            else {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "channelApr.open() attempt to connect to %pI (%s) failed %d\n",
                              remote_sa,
                              socketInfo->host,
                              ret);
            }
            remote_sa = remote_sa->next;
            continue;
        }
        connected = 1;
    }

    if (!connected) {
        apr_socket_close(sock);   
        return JK_ERR;
    }
    /* enable the use of keep-alive packets on TCP connection */
    if(keepalive) {
        int set = 1;
        if((ret = apr_socket_opt_set(sock, APR_SO_KEEPALIVE, set)) != APR_SUCCESS ) {
            apr_socket_close(sock);
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "channelApr.open() keepalive failed %d %s\n",
                          ret, apr_strerror( ret, msg, sizeof(msg) ) );
            return JK_ERR;                        
        }
    }

    /* Disable the Nagle algorithm if ndelay is set */
    if(ndelay) {
        int set = 1;
        if((ret = apr_socket_opt_set(sock, APR_TCP_NODELAY, set)) != APR_SUCCESS ) {
            apr_socket_close(sock);
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "channelApr.open() nodelay failed %d %s\n",
                          ret, apr_strerror( ret, msg, sizeof(msg) ) );
            return JK_ERR;                        
        }
    }
        
    if( ch->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "channelApr.open(), sock = %d\n", sock);

    return JK_OK;
}


/** close the socket  ( was: jk2_close_socket )
*/
static int JK_METHOD jk2_channel_apr_close(jk_env_t *env,jk_channel_t *ch,
                                             jk_endpoint_t *endpoint)
{
    apr_socket_t *sd;
    apr_status_t rc;
    
    sd=endpoint->channelData;
    if( sd==NULL ) 
        return JK_ERR;

    endpoint->channelData=NULL; /* XXX check it. */
    endpoint->sd=-1;
    
    /* nothing else to clean, the socket_data was allocated ouf of
     *  endpoint's pool
     */
    rc=apr_socket_close(sd);
    return rc;
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
static int JK_METHOD jk2_channel_apr_send(jk_env_t *env, jk_channel_t *ch,
                                            jk_endpoint_t *endpoint,
                                            jk_msg_t *msg) 
{
    char *b;
    int len;
    apr_socket_t *sock;
    apr_status_t stat;
    apr_size_t length;
    char data[128];

    sock=endpoint->channelData;

    if( sock==NULL ) 
        return JK_ERR;

    msg->end( env, msg );
    len=msg->len;
    b=msg->buf;

    length = (apr_size_t) len;
    do {
        apr_size_t written = length;

        stat = apr_socket_send(sock, b, &written);
        if (stat!= APR_SUCCESS) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                "jk2_channel_apr_send send failed %d %s\n",
                stat, apr_strerror( stat, data, sizeof(data) ) );
            return -3; /* -2 is not possible... */
        }
        length -= written;
        b += written;
    } while (length); 

    return JK_OK;
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
                                            jk_channel_t *ch,
                                            jk_endpoint_t *endpoint,
                                            char *b, int len ) 
{
    apr_socket_t *sock;
    apr_size_t length;
    apr_status_t stat;
    int rdlen;

    sock=endpoint->channelData;

    if( sock==NULL ) 
        return JK_ERR;

    rdlen = 0;
    length = (apr_size_t)len;
    while (rdlen < len) {

        stat =  apr_socket_recv(sock, b + rdlen, &length);

        if (stat == APR_EOF)
            return -1; /* socket closed. */
        else if (APR_STATUS_IS_EAGAIN(stat))
            continue;
        else if (stat != APR_SUCCESS)
            return -1; /* any error. */
        rdlen += length;
        length = (apr_size_t)(len - rdlen);
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
static int JK_METHOD jk2_channel_apr_recv( jk_env_t *env, jk_channel_t *ch,
                                             jk_endpoint_t *endpoint,
                                             jk_msg_t *msg )
{
    int hlen=msg->headerLength;
    int blen;
    int rc;
    

    jk2_channel_apr_readN( env, ch, endpoint, msg->buf, hlen );

    blen=msg->checkHeader( env, msg, endpoint );
    if( blen < 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channelApr.receive(): Bad header\n" );
        return JK_ERR;
    }
    
    rc= jk2_channel_apr_readN( env, ch, endpoint, msg->buf + hlen, blen);

    if(rc < 0) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
               "channelApr.receive(): Error receiving message body %d %d\n",
                      rc, errno);
        return JK_ERR;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channelApr.receive(): Received len=%d type=%d\n",
                  blen, (int)msg->buf[hlen]);
    return JK_OK;

}


int JK_METHOD jk2_channel_apr_socket_factory(jk_env_t *env,
                                             jk_pool_t *pool, 
                                             jk_bean_t *result,
                                             const char *type, const char *name)
{
    jk_channel_t *ch;

    ch=(jk_channel_t *)pool->calloc(env, pool, sizeof( jk_channel_t));
    
    ch->_privatePtr= (jk_channel_apr_private_t *)
                     pool->calloc( env, pool, sizeof( jk_channel_apr_private_t));

    ch->recv= jk2_channel_apr_recv; 
    ch->send= jk2_channel_apr_send; 
    ch->open= jk2_channel_apr_open; 
    ch->close= jk2_channel_apr_close; 
    ch->hasinput= jk2_channel_apr_hasinput; 

    ch->is_stream=JK_TRUE;

    result->setAttribute= jk2_channel_apr_setProperty; 
    result->getAttribute= jk2_channel_apr_socket_getAttribute; 
    result->init= jk2_channel_apr_init; 
    result->getAttributeInfo=jk2_channel_apr_socket_getAttributeInfo;
    result->multiValueInfo=NULL;
    result->setAttributeInfo=jk2_channel_apr_socket_setAttributeInfo;

    ch->mbean=result;
    result->object= ch;


    ch->workerEnv=env->getByName( env, "workerEnv" );
    ch->workerEnv->addChannel( env, ch->workerEnv, ch );

    return JK_OK;
}
