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

#ifdef HAVE_UNIXSOCKETS    

/** Information specific for the socket channel
 */
typedef struct jk_channel_un_private {
    int ndelay;
    struct sockaddr_un unix_addr;
    char *file;

    int l_onoff;                /* Nonzero to linger on close.  */
    int l_linger;               /* Time to linger.  */

    int backlog;

    int listenSocket;
} jk_channel_un_private_t;

static int JK_METHOD jk2_channel_un_close(jk_env_t *env, jk_channel_t *ch,
                                          jk_endpoint_t *endpoint);

static char *jk2_channel_un_multiValueInfo[]={"group",  NULL };
static char *jk2_channel_un_setAttributeInfo[]={"file", "route", "lb_factor",
                                                "level", NULL };

static int JK_METHOD jk2_channel_un_setAttribute(jk_env_t *env,
                                                jk_bean_t *mbean, 
                                                char *name, void *valueP)
{
    jk_channel_t *ch=(jk_channel_t *)mbean->object;
    char *value=valueP;
    jk_channel_un_private_t *socketInfo=
        (jk_channel_un_private_t *)(ch->_privatePtr);

    if( strcmp( "file", name ) == 0 ) {
        socketInfo->file=value;
    } else if( strcmp( "soLinger", name ) == 0 ) {
        socketInfo->l_linger=atoi( value );
    } else if( strcmp( "listen", name ) == 0 ) {
        socketInfo->backlog=atoi( value );
        ch->serverSide=JK_TRUE;
    } else {
        return jk2_channel_setAttribute( env, mbean, name, valueP );
    }
    return JK_OK;
}

/** resolve the host IP ( jk_resolve ) and initialize the channel.
 */
static int JK_METHOD jk2_channel_un_init(jk_env_t *env,
                                         jk_bean_t *chB)
{
    jk_channel_t *ch=chB->object;
    jk_channel_un_private_t *socketInfo=
        (jk_channel_un_private_t *)(ch->_privatePtr);
    int rc=JK_OK;
    int omask;

    if( socketInfo->file==NULL ) {
        char *localName=ch->mbean->localName;
        jk_config_t *cfg=ch->workerEnv->config;
        
        /* Set the 'name' property
         */
        localName = jk2_config_replaceProperties(env, cfg->map, cfg->map->pool, localName);

        /*   env->l->jkLog(env, env->l, JK_LOG_INFO, */
        /*                 "channelUn.init(): use name %s\n", localName ); */
        
        if (localName[0]=='/') {
            ch->mbean->setAttribute( env, ch->mbean, "file", localName );
        } 
    }
    
    if (socketInfo->file!=NULL && socketInfo->file[0]=='/') {
        memset(&socketInfo->unix_addr, 0, sizeof(struct sockaddr_un));
        socketInfo->unix_addr.sun_family = AF_UNIX;
        strcpy(socketInfo->unix_addr.sun_path,  socketInfo->file );

        if( ch->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "channelUn.init(): create AF_UNIX  %s\n", socketInfo->file );
    } else {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "channelUn.init(): "
                      "can't init %s errno=%d\n", socketInfo->file, errno );
    }

    if( ch->serverSide == JK_TRUE ) {
        
        socketInfo->listenSocket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (socketInfo->listenSocket < 0) {
            return JK_ERR;
        }
        
        omask = umask(0117); /* so that only Apache can use socket */
        
        rc=bind(socketInfo->listenSocket,
                (struct sockaddr *)& socketInfo->unix_addr,
                strlen( socketInfo->unix_addr.sun_path ) +
                sizeof( socketInfo->unix_addr.sun_family) );
        
        umask(omask); /* can't fail, so can't clobber errno */
        
        if (rc<0)
            return -errno;

        listen( socketInfo->listenSocket, socketInfo->backlog );
    
        if( ch->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "Unix socket listening on %d \n", socketInfo->listenSocket);
        /*
        {
        struct linger {
            int   l_onoff;    
            int   l_linger;   
        } lin;
        int rc;
        
        lin.l_onoff = l_onoff;
        lin.l_linger = l_linger;
        rc=setsockopt(sd, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin));
        if( rc < 0) {
            return -errno;
        }
        }
        */

    }

    return rc;
}

/** connect to Tomcat (jk_open_socket)
 */
static int JK_METHOD jk2_channel_un_open(jk_env_t *env,
                                            jk_channel_t *ch,
                                            jk_endpoint_t *endpoint)
{
    int err;
    jk_channel_un_private_t *socketInfo=
        (jk_channel_un_private_t *)(ch->_privatePtr);
    int unixsock;
    struct sockaddr_un client;
    int clientlen;

    if( ch->serverSide ) {
        while( 1 ) {
            clientlen=sizeof( client );
        
            unixsock=accept( socketInfo->listenSocket, (struct sockaddr *)&client, &clientlen );

            if( ch->mbean->debug > 0 )
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "channelUn.open(): accept  %d %d\n", unixsock, errno );

            /* XXX Should we return EINTR ? This would allow us to stop
             */
            if( unixsock < 0 ) {
                if( errno==EINTR ) {
                    if( ch->mbean->debug > 0 )
                        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                                      "channelUn.open(): accept EINTR  %d %d\n", unixsock, errno );
                    continue;
                } else {
                    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                                  "channelUn.open(): accept error  %d %d %s\n", unixsock, errno,
                                  strerror(errno));
                    return -errno;
                }
            }
            break;
        }
    } else {
        unixsock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (unixsock<0) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "channelUn.open(): can't create socket %d %s\n",
                          errno, strerror( errno ) );
            return JK_ERR;
        }
        
        if( ch->mbean->debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "channelUn.open(): create unix socket %s %d\n", socketInfo->file, unixsock );
        
        if (connect(unixsock,(struct sockaddr *)&(socketInfo->unix_addr),
                    sizeof(struct sockaddr_un))<0) {
            close(unixsock);
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "channelUn.connect() connect failed %d %s\n",
                          errno, strerror( errno ) );
            return JK_ERR;
        }
    }

    if( ch->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelUn.open(): connect unix socket %d %s\n", unixsock, socketInfo->file );
    /* store the channel information */
    endpoint->sd=unixsock;
    return JK_OK;
}

/** close the socket  ( was: jk2_close_socket )
*/
static int JK_METHOD jk2_channel_un_close(jk_env_t *env,jk_channel_t *ch,
                                          jk_endpoint_t *endpoint)
{
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channelUn.close(): close unix socket %d \n",  endpoint->sd );
    close( endpoint->sd );
    endpoint->sd=-1;
    return JK_OK;
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
static int JK_METHOD jk2_channel_un_send(jk_env_t *env, jk_channel_t *ch,
                                         jk_endpoint_t *endpoint,
                                         jk_msg_t *msg) 
{
    unsigned char *b;
    int len;
    int  sent=0;
    int this_time;
    int unixsock;

    msg->end( env, msg );
    len=msg->len;
    b=msg->buf;

    unixsock=endpoint->sd;
    if( unixsock < 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel.apr:send() not connected %d\n", unixsock );
        return JK_ERR;
    }

    while(sent < len) {
        errno=0;
        this_time = write(unixsock, (char *)b + sent , len - sent);

        if( ch->mbean->debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "channel.apr:send() write() %d %d %s\n", this_time, errno,
                          strerror( errno));
        if(0 == this_time) {
            return -2;
        }
        if(this_time < 0) {
            return -3;
        }
        sent += this_time;
    }
    /*     return sent; */
    return JK_OK;
}


/** receive len bytes.
 * @param sd  opened socket.
 * @param b   buffer to store the data.
 * @param len length to receive
 * @return    -1: receive failed or connection closed.
 *            >0: length of the received data.
 * Was: tcp_socket_recvfull
 */
static int JK_METHOD jk2_channel_un_readN( jk_env_t *env,
                                           jk_channel_t *ch,
                                           jk_endpoint_t *endpoint,
                                           unsigned char *b, int len ) 
{
    int sd;
    int rdlen;

    sd=endpoint->sd;
    
    if( sd < 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel.apr:readN() not connected %d\n", sd );
        return -3;
    }

    rdlen = 0;

    while(rdlen < len) {
        int this_time = recv(sd, (char *)b + rdlen, 
                             len - rdlen, 0);        
        if( this_time < 0 ) {
            if(EAGAIN == errno) {
                continue;
            } 
            return -2;
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
static int JK_METHOD jk2_channel_un_recv( jk_env_t *env, jk_channel_t *ch,
                                             jk_endpoint_t *endpoint,
                                             jk_msg_t *msg )
{
    int hlen=msg->headerLength;
    int blen;
    int rc=JK_OK;
    

    blen=jk2_channel_un_readN( env, ch, endpoint, msg->buf, hlen );
    if( blen <= 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channelUn.receive(): error receiving %d %d %s %#lx %d\n",
                      blen, errno, strerror( errno ), endpoint, endpoint->sd);
        return JK_ERR;
    }

    blen=msg->checkHeader( env, msg, endpoint );
    if( blen < 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channelUn.receive(): Bad header\n" );
        return JK_ERR;
    }
    
    rc= jk2_channel_un_readN( env, ch, endpoint, msg->buf + hlen, blen);

    if(rc < 0) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
               "channelUn.receive(): Error receiving message body %d %d\n",
                      rc, errno);
        return JK_ERR;
    }

    if( ch->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelUn.receive(): Received len=%d type=%d\n",
                      blen, (int)msg->buf[hlen]);
    return JK_OK;

}


int JK_METHOD jk2_channel_un_factory(jk_env_t *env,
                                     jk_pool_t *pool, 
                                     jk_bean_t *result,
                                     const char *type, const char *name)
{
    jk_channel_t *ch;
    
    ch=(jk_channel_t *)pool->calloc(env, pool, sizeof( jk_channel_t));
    
    ch->_privatePtr= (jk_channel_un_private_t *)
        pool->calloc( env, pool, sizeof( jk_channel_un_private_t));

    ch->recv= jk2_channel_un_recv; 
    ch->send= jk2_channel_un_send; 
    ch->open= jk2_channel_un_open; 
    ch->close= jk2_channel_un_close; 
    ch->is_stream=JK_TRUE;
    ch->serverSide=JK_FALSE;
    
    result->setAttribute= jk2_channel_un_setAttribute; 
    result->multiValueInfo=jk2_channel_un_multiValueInfo;
    result->setAttributeInfo=jk2_channel_un_setAttributeInfo;
    result->invoke=jk2_channel_invoke;

    ch->mbean=result;
    result->object= ch;
    result->init= jk2_channel_un_init; 

    ch->workerEnv=env->getByName( env, "workerEnv" );
    ch->workerEnv->addChannel( env, ch->workerEnv, ch );

    return JK_OK;
}

#else

int JK_METHOD jk2_channel_un_factory(jk_env_t *env,
                                     jk_pool_t *pool, 
                                     jk_bean_t *result,
                                     const char *type, const char *name)
{
    env->l->jkLog( env, env->l, JK_LOG_ERROR,
                   "channelUn.factory(): Support for unix sockets is disabled, "
                   "you need to set HAVE_UNIXSOCKETS at compile time\n");
    result->disabled=1;
    return JK_FALSE;
}
#endif
