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
	#define closesocket			close
#endif

#define DEFAULT_HOST "127.0.0.1"

/** Information specific for the socket channel
 */
struct jk_channel_socket_private {
    int ndelay;
    struct sockaddr_in addr;    
    char *host;
    short port;
};

/** Informations for each connection
 */
typedef struct jk_channel_socket_data {
    int sock;
} jk_channel_socket_data_t;

typedef struct jk_channel_socket_private jk_channel_socket_private_t;

/*
  We use the _privateInt field directly. Long term we can define our own
  jk_channel_socket_t structure and use the _private field, etc - but we 
  just need to store an int.

  XXX We could also use properties or 'notes'
*/

int JK_METHOD jk_channel_socket_factory(jk_env_t *env, jk_pool_t *pool,
                                        void **result,
					const char *type, const char *name);

static int JK_METHOD jk_channel_socket_resolve(char *host, short port,
				     struct sockaddr_in *rc);

static int JK_METHOD jk_channel_socket_close(jk_channel_t *_this,
                                      jk_endpoint_t *endpoint);

static int JK_METHOD jk_channel_socket_getProperty(jk_channel_t *_this, 
					 char *name, char **value)
{
    return JK_FALSE;
}

static int JK_METHOD jk_channel_socket_setProperty(jk_channel_t *_this, 
                                                   char *name, char *value)
{
    jk_channel_socket_private_t *socketInfo=
	(jk_channel_socket_private_t *)(_this->_privatePtr);

    if( strcmp( "host", name ) != 0 ) {
	socketInfo->host=value;
    } else if( strcmp( "defaultPort", name ) != 0 ) {
    } else if( strcmp( "port", name ) != 0 ) {
    } else {
	return JK_FALSE;
    }
    return JK_TRUE;
}

/** resolve the host IP ( jk_resolve ) and initialize the channel.
 */
static int JK_METHOD jk_channel_socket_init(jk_channel_t *_this, 
                                            jk_map_t *props,
                                            char *worker_name, 
                                            jk_worker_t *worker, 
                                            jk_logger_t *l )
{
    int err;
    jk_channel_socket_private_t *socketInfo=
	(jk_channel_socket_private_t *)(_this->_privatePtr);
    char *host=socketInfo->host;
    short port=socketInfo->port;
    struct sockaddr_in *rc=&socketInfo->addr;

    port = map_getIntProp( props, "worker", worker_name, "port", port );
    host = map_getStrProp( props, "worker", worker_name, "host", host);

    _this->worker=worker;
    _this->properties=props;

    if( host==NULL )
        host=DEFAULT_HOST;
    
    err=jk_channel_socket_resolve( host, port, rc );
    if( err!= JK_TRUE ) {
	l->jkLog(l, JK_LOG_ERROR, "jk_channel_socket_init: "
	       "can't resolve %s:%d errno=%d\n", host, port, errno );
    }
    l->jkLog(l, JK_LOG_INFO, "channel_socket.init(): %s:%d for %s\n", host,
             port, worker->name );

    return err;
}

/** private: resolve the address on init
 */
static int JK_METHOD jk_channel_socket_resolve(char *host, short port,
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

static int jk_close_socket(int s)
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
static int JK_METHOD jk_channel_socket_open(jk_channel_t *_this,
                                            jk_endpoint_t *endpoint)
{
    jk_logger_t *l=_this->worker->workerEnv->l;
    int err;
    jk_channel_socket_private_t *socketInfo=
	(jk_channel_socket_private_t *)(_this->_privatePtr);

    struct sockaddr_in *addr=&socketInfo->addr;
    int ndelay=socketInfo->ndelay;

    int sock;
    int ret;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
#ifdef WIN32
        if(SOCKET_ERROR == ret) { 
            errno = WSAGetLastError() - WSABASEERR;
        }
#endif /* WIN32 */
        l->jkLog(l, JK_LOG_ERROR,
                 "channelSocket.open(): can't create socket %d %s\n",
                 errno, strerror( errno ) );
        return JK_FALSE;
    }

    /* Tries to connect to JServ (continues trying while error is EINTR) */
    do {
        l->jkLog(l, JK_LOG_INFO, "channelSocket.open() connect on %d\n",sock);
        ret = connect(sock,(struct sockaddr *)addr,
                      sizeof(struct sockaddr_in));
        
#ifdef WIN32
        if(SOCKET_ERROR == ret) { 
            errno = WSAGetLastError() - WSABASEERR;
        }
#endif /* WIN32 */

    } while (-1 == ret && EINTR == errno);

    /* Check if we connected */
    if(ret != 0 ) {
        jk_close_socket(sock);
        l->jkLog(l, JK_LOG_ERROR,
                 "channelSocket.connect() connect failed %d %s\n",
                 errno, strerror( errno ) );
        return JK_FALSE;
    }

    if(ndelay) {
        int set = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,(char *)&set,sizeof(set));
    }   
        
    l->jkLog(l, JK_LOG_INFO, "channelSocket.connect(), sock = %d\n", sock);

    {
        jk_channel_socket_data_t *sd=endpoint->channelData;
        if( sd==NULL ) {
            sd=(jk_channel_socket_data_t *)
                endpoint->pool->calloc( endpoint->pool,
                                        sizeof( jk_channel_socket_data_t ));
            endpoint->channelData=sd;
        }
        sd->sock=sock;
    }
    return JK_TRUE;
}


/** close the socket  ( was: jk_close_socket )
*/
static int JK_METHOD jk_channel_socket_close(jk_channel_t *_this,
                                             jk_endpoint_t *endpoint)
{
    int sd;
    jk_channel_socket_data_t *chD=endpoint->channelData;
    if( chD==NULL ) 
	return JK_FALSE;

    sd=chD->sock;
    chD->sock=-1;
    /* nothing else to clean, the socket_data was allocated ouf of
     *  endpoint's pool
     */
    return jk_close_socket(sd);
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
static int JK_METHOD jk_channel_socket_send(jk_channel_t *_this,
				  jk_endpoint_t *endpoint,
				  char *b,
                                  int len) 
{
    int sd;
    int  sent=0;

    jk_channel_socket_data_t *chD=endpoint->channelData;
    if( chD==NULL ) 
	return JK_FALSE;
    sd=chD->sock;

    while(sent < len) {
        int this_time = send(sd, (char *)b + sent , len - sent,  0);
	    
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
static int JK_METHOD jk_channel_socket_recv( jk_channel_t *_this,
                                             jk_endpoint_t *endpoint,
                                             char *b, int len ) 
{
    jk_channel_socket_data_t *chD=endpoint->channelData;
    int sd;
    int rdlen;

    if( chD==NULL ) 
	return JK_FALSE;
    sd=chD->sock;
    rdlen = 0;
    
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



int JK_METHOD jk_channel_socket_factory(jk_env_t *env, jk_pool_t *pool, 
                                        void **result,
					const char *type, const char *name)
{
    jk_channel_t *_this;
    
    if( strcmp( "channel", type ) != 0 ) {
	/* Wrong type  XXX throw */
	*result=NULL;
	return JK_FALSE;
    }
    _this=(jk_channel_t *)pool->calloc(pool, sizeof( jk_channel_t));
    
    _this->_privatePtr= (jk_channel_socket_private_t *)
	pool->calloc( pool, sizeof( jk_channel_socket_private_t));

    _this->recv= &jk_channel_socket_recv; 
    _this->send= &jk_channel_socket_send; 
    _this->init= &jk_channel_socket_init; 
    _this->open= &jk_channel_socket_open; 
    _this->close= &jk_channel_socket_close; 
    _this->getProperty= &jk_channel_socket_getProperty; 
    _this->setProperty= &jk_channel_socket_setProperty; 

    _this->supportedProperties=( char ** )pool->alloc( pool, 4 * sizeof( char * ));
    _this->supportedProperties[0]="host";
    _this->supportedProperties[1]="port";
    _this->supportedProperties[2]="defaultPort";
    _this->supportedProperties[3]="\0";

    _this->name="file";

    *result= _this;
    
    return JK_TRUE;
}
