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
 * Implementation for org.apache.jk.apr.AprImpl
 *
 * @author Costin Manolache
 */

#include <jni.h>
#include "apr.h"
#include "apr_pools.h"

#include "apr_network_io.h"
#include "apr_errno.h"
#include "apr_general.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "org_apache_jk_apr_AprImpl.h"

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_initialize(JNIEnv *jniEnv, jobject _jthis)
{
/*     apr_initialize(); */
    return 0;
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_terminate(JNIEnv *jniEnv, jobject _jthis)
{
/*     apr_terminate(); */
    return 0;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_poolCreate(JNIEnv *jniEnv, jobject _jthis, jlong parentP)
{
    apr_pool_t *parent;
    apr_pool_t *child;

    parent=(apr_pool_t *)(void *)(long)parentP;
    apr_pool_create( &child, parent );
    return (jlong)(long)child;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_poolClear(JNIEnv *jniEnv, jobject _jthis,
                                         jlong poolP)
{
    apr_pool_t *pool;

    pool=(apr_pool_t *)(void *)(long)poolP;
    apr_pool_clear( pool );
    return 0;
}

static void jk2_SigAction(int signal) {

}

static struct sigaction jkAction;

/* XXX We need to: - preserve the old signal ( or get them ) - either
     implement "waitSignal" or use invocation in jk2_SigAction

     Probably waitSignal() is better ( we can have a thread that waits )
*/

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_signal(JNIEnv *jniEnv, jobject _jthis, jint bitMask,
                                      jobject func)
{
    memset(& jkAction, 0, sizeof(jkAction));
    jkAction.sa_handler=jk2_SigAction;
    sigaction((int)bitMask, &jkAction, (void *) NULL);
    return 0;
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_sendSignal(JNIEnv *jniEnv, jobject _jthis, jint signo,
                                          jlong target)
{
    
    return 0;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_userId(JNIEnv *jniEnv, jobject _jthis, jlong pool)
{
    
    return 0;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_shmInit(JNIEnv *jniEnv, jobject _jthis, jlong pool,
                                       jlong size, jstring file)
{

    return 0;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_shmDestroy(JNIEnv *jniEnv, jobject _jthis, jlong pool,
                                          jlong size, jstring file)
{

    return 0;
}


/* ==================== Unix sockets ==================== */
/* It seems apr doesn't support them yet, so this code will use the
   'native' calls. For 'tcp' sockets we just use what java provides.
*/

   
JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_unSocketClose(JNIEnv *jniEnv, jobject _jthis, 
                                             jlong poolJ, jlong socketJ, jint typeJ )
{
    int socket=(int)socketJ;
    int type=(int)typeJ;
    /*     shutdown( socket, type ); */
    close(socket);
    return 0L;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_unSocketListen(JNIEnv *jniEnv, jobject _jthis, 
                                              jlong poolJ, jstring hostJ, jint backlog )
{
    apr_pool_t *pool=(apr_pool_t *)(void *)(long)poolJ;
    const char *host;
    int status;
    int unixSocket;
    struct sockaddr_un unixAddr;
    mode_t omask;

    memset(& unixAddr, 0, sizeof(struct sockaddr_un));
    unixAddr.sun_family=AF_UNIX;

    host=(*jniEnv)->GetStringUTFChars(jniEnv, hostJ, 0);
    strcpy(unixAddr.sun_path, host);
    (*jniEnv)->ReleaseStringUTFChars(jniEnv, hostJ, host);

    /* remove the exist socket. (it had been moved in ChannelUn.java).
    if (unlink(unixAddr.sun_path) < 0 && errno != ENOENT) {
        // The socket cannot be remove... Well I hope that no problems ;-)
    }
     */

    unixSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (unixSocket<0) {
        return 0L;
    }

    omask = umask(0117); /* so that only Apache can use socket */
        
    status=bind(unixSocket,
                (struct sockaddr *)& unixAddr,
                strlen( unixAddr.sun_path ) +
                sizeof( unixAddr.sun_family) );

    umask(omask); /* can't fail, so can't clobber errno */
    if (status<0)
        return -errno;

    listen( unixSocket, (int)backlog );
    
/*     fprintf(stderr, "Listening on %d \n", */
/*             unixSocket); */
    return (jlong)unixSocket;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_unSocketConnect(JNIEnv *jniEnv, jobject _jthis, 
                                               jlong poolJ, jstring hostJ )
{
    apr_pool_t *pool=(apr_pool_t *)(void *)(long)poolJ;
    char *host;
    int status;
    int unixSocket;
    struct sockaddr_un unixAddr;

    memset(& unixAddr, 0, sizeof(struct sockaddr_un));
    unixAddr.sun_family=AF_UNIX;

    host==(*jniEnv)->GetStringUTFChars(jniEnv, hostJ, 0);
    strcpy(unixAddr.sun_path, host);
    (*jniEnv)->ReleaseStringUTFChars(jniEnv, hostJ, host);
    
    unixSocket = socket(AF_UNIX, SOCK_STREAM, 0);

    if (unixSocket<0) {
        return 0L;
    }

    status=connect(unixSocket,
                   (struct sockaddr *)& unixAddr,
                   strlen( unixAddr.sun_path ) +
                   sizeof( unixAddr.sun_family) );
    
    if( status < 0 ) {
        // Return error
        return -1;
    }

    return (jlong)(long)(void *)unixSocket;
}


JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_unAccept(JNIEnv *jniEnv, jobject _jthis, 
                                      jlong poolJ, jlong unSocketJ)
{
    int listenUnSocket=(int)unSocketJ;
    struct sockaddr_un client;
    int clientlen;
    
    /* What to do with the address ? We could return an object, or do more.
       For now we'll ignore it */
    
    while( 1 ) {
        int connfd;

        clientlen=sizeof( client );
        connfd=accept( listenUnSocket, (struct sockaddr *)&client, &clientlen );
        /* XXX Should we return EINTR ? This would allow us to stop
         */
        if( connfd < 0 ) {
            if( errno==EINTR ) {
                fprintf(stderr, "EINTR\n");
                continue;
            } else {
                fprintf(stderr, "Error accepting %d %d %s\n",
                        listenUnSocket, errno, strerror(errno));
                return -errno;
            }
        }
        return (jlong)connfd;
    }
    return 0L;
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_unRead(JNIEnv *jniEnv, jobject _jthis, 
                                      jlong poolJ, jlong unSocketJ,
                                      jbyteArray bufJ, jint from, jint cnt)
{
    apr_pool_t *pool=(apr_pool_t *)(void *)(long)poolJ;
    jbyte *nbuf;
    int rd;
    jboolean iscommit;

    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, bufJ, &iscommit);
    if( ! nbuf ) {
        return -1;
    }

    while( 1 ) {
        /* Read */
        rd=read( (int)unSocketJ, nbuf + from, cnt );
        if( rd < 0 ) {
            if( errno==EINTR ) {
                fprintf(stderr, "EINTR\n");
                continue;
            } else {
                fprintf(stderr, "Error reading %d %d %s\n",
                        (int)unSocketJ, errno, strerror(errno));
                (*jniEnv)->ReleaseByteArrayElements(jniEnv, bufJ, nbuf, 0);
                return -1;
            }
        }
/*         fprintf(stderr, "Read %d from %d\n", */
/*                 rd, unSocketJ); */
    
        (*jniEnv)->ReleaseByteArrayElements(jniEnv, bufJ, nbuf, 0);
        return (jint)rd;
    }
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_unWrite(JNIEnv *jniEnv, jobject _jthis, 
                                     jlong poolJ, jlong unSocketJ, jbyteArray bufJ, jint from, jint cnt)
{
    apr_status_t status;
    apr_pool_t *pool=(apr_pool_t *)(void *)(long)poolJ;
    jbyte *nbuf;
    int rd;
    jboolean iscommit;

    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, bufJ, &iscommit);
    if( ! nbuf ) {
        return -1;
    }

    /* write */
    write( (int) unSocketJ, nbuf + from, cnt );
    
    (*jniEnv)->ReleaseByteArrayElements(jniEnv, bufJ, nbuf, 0);
    return (jint)rd;
}

/**
 * setSoLinger
 */
JNIEXPORT jint JNICALL
Java_org_apache_jk_apr_AprImpl_unSetSoLingerNative (
    JNIEnv      *env,
    jobject     ignored,
    jint        sd,
    jint        l_onoff,
    jint        l_linger)
{
    struct linger {
        int   l_onoff;    /* linger active */
        int   l_linger;   /* how many seconds to linger for */
    } lin;
    int rc;

    lin.l_onoff = l_onoff;
    lin.l_linger = l_linger;
    rc=setsockopt(sd, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin));
    if( rc < 0) {
        return -errno;
    }
    return 0;
}



