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

#include <string.h>
#include "jk_registry.h"


#ifdef HAS_APR
#include "apr_network_io.h"
#include "apr_errno.h"
#include "apr_general.h"


#define DEFAULT_HOST "127.0.0.1"

/** Information specific for the socket channel
 */
struct jk_channel_apr_private {
    int ndelay;
    apr_sockaddr_t *addr;
    char *host;
    short port;
};

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
    jk_channel_t *ch=(jk_channel_t *)mbean->object;
    char *value=valueP;
    jk_channel_apr_private_t *socketInfo=
        (jk_channel_apr_private_t *)(ch->_privatePtr);

    if( strcmp( "host", name ) == 0 ) {
        socketInfo->host=value;
    } else if( strcmp( "port", name ) == 0 ) {
        socketInfo->port=atoi( value );
    } else {
	if( ch->worker!=NULL ) {
            return ch->worker->mbean->setAttribute( env, ch->worker->mbean, name, valueP );
        }
        return JK_ERR;
    }
    return JK_OK;
}

/** resolve the host IP ( jk_resolve ) and initialize the channel.
 */
static int JK_METHOD jk2_channel_apr_init(jk_env_t *env,
                                          jk_channel_t *ch)
{
    jk_channel_apr_private_t *socketInfo=
        (jk_channel_apr_private_t *)(ch->_privatePtr);
    int rc;
    short port=socketInfo->port;

    if( socketInfo->host==NULL ) {
        char *localName=ch->mbean->localName;
        jk_config_t *cfg=ch->workerEnv->config;
        
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
    
    if( socketInfo->port<=0 )
        socketInfo->port=8009;

    if( socketInfo->host==NULL )
        socketInfo->host=DEFAULT_HOST;

    rc=jk2_channel_apr_resolve( env, socketInfo->host, socketInfo->port, socketInfo );
    if( rc!= JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "jk2_channel_apr_init: "
                      "can't resolve %s:%d errno=%d\n", socketInfo->host, socketInfo->port, errno );
    }

    return rc;
}

/** private: resolve the address on init
 */
static int JK_METHOD jk2_channel_apr_resolve(jk_env_t *env,
                                             char *host, short port,
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

    apr_socket_t *sock=endpoint->channelData;
    apr_status_t ret;
    apr_interval_time_t timeout = 2 * APR_USEC_PER_SEC;
    char msg[128];

    if (apr_socket_create(&sock, remote_sa->family, SOCK_STREAM,
                          (apr_pool_t *)env->globalPool->_private)
                         != APR_SUCCESS) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                 "channelApr.open(): can't create socket %d %s\n",
                 errno, strerror( errno ) );
        return JK_ERR;
    } 
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channelApr.open(): create tcp socket %d\n", sock );

    if (apr_setsocketopt(sock, APR_SO_TIMEOUT, timeout)!= APR_SUCCESS) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                 "channelApr.open(): can't set timeout %d %s\n",
                 errno, strerror( errno ) );
        return JK_ERR;
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
        return JK_ERR;
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
    endpoint->channelData=sock;

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

    int  sent=0;

    sock=endpoint->channelData;

    if( sock==NULL ) 
        return JK_ERR;

    msg->end( env, msg );
    len=msg->len;
    b=msg->buf;

    length = (apr_size_t) len;
    stat = apr_send(sock, b, &length);
    if (stat!= APR_SUCCESS) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "jk2_channel_apr_send send failed %d %s\n",
                      stat, apr_strerror( stat, data, sizeof(data) ) );
        return -3; /* -2 is not possible... */
    }
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
    ch->init= jk2_channel_apr_init; 
    ch->open= jk2_channel_apr_open; 
    ch->close= jk2_channel_apr_close; 
    ch->is_stream=JK_TRUE;

    result->setAttribute= jk2_channel_apr_setProperty; 
    ch->mbean=result;
    result->object= ch;

    ch->workerEnv=env->getByName( env, "workerEnv" );
    ch->workerEnv->addChannel( env, ch->workerEnv, ch );

    return JK_OK;
}
#else /* HAS_APR */

int JK_METHOD jk2_channel_apr_socket_factory(jk_env_t *env,
                                             jk_pool_t *pool, 
                                             jk_bean_t *result,
                                             const char *type, const char *name)
{

    result->disabled=1;
    result->object= NULL;

    return JK_OK;
}


#endif
