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
#include "apr_strings.h"
#include "apr_portable.h"
#include "apr_lib.h"

#include "org_apache_jk_apr_AprImpl.h"

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"

#ifndef WIN32
#include <unistd.h>
#include <pwd.h>
#endif

#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if APR_HAS_SHARED_MEMORY
#include "apr_shm.h"
#endif

#include "signal.h"

#include "apr_proc_mutex.h"

static apr_pool_t *jniAprPool;
static jk_workerEnv_t *workerEnv;
static int jniDebug=0;

/* -------------------- Apr initialization and pools -------------------- */

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_initialize(JNIEnv *jniEnv, jobject _jthis)
{
    jk_env_t *env;
    
    apr_initialize(); 
    apr_pool_create( &jniAprPool, NULL );

    if( jk_env_globalEnv == NULL ) {
        jk_pool_t *globalPool;

        if( jniAprPool==NULL ) {
            return 0;
        }
        jk2_pool_apr_create( NULL, &globalPool, NULL, jniAprPool );
        /* Create the global env */
        env=jk2_env_getEnv( NULL, globalPool );
    }
    env=jk_env_globalEnv;

    workerEnv=env->getByName( env, "workerEnv" );
    if( workerEnv==NULL ) {
        jk_bean_t *jkb;

        jkb=env->createBean2( env, env->globalPool, "logger.file", "");
        if( jkb==NULL ) {
            fprintf(stderr, "Error creating logger ");
            return JK_ERR;
        }

        env->l=jkb->object;;
        env->alias( env, "logger.file:", "logger");

        jkb=env->createBean2( env, env->globalPool,"workerEnv", "");
        env->alias( env, "workerEnv:", "workerEnv");
        if( jkb==NULL ) {
            fprintf(stderr, "Error creating workerEnv ");
            return JK_ERR;
        }

        workerEnv=jkb->object;
    }
    /* fprintf( stderr, "XXX aprImpl: %p %p\n", env, workerEnv); */
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

/* -------------------- Signals -------------------- */
#ifdef HAVE_SIGNALS
static struct sigaction jkAction;

static void jk2_SigAction(int sig) {
    fprintf(stderr, "Signal %d\n", sig );
    signal( sig, jk2_SigAction );
}
#endif

/* XXX We need to: - preserve the old signal ( or get them ) - either
     implement "waitSignal" or use invocation in jk2_SigAction

     Probably waitSignal() is better ( we can have a thread that waits )
*/

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_signal(JNIEnv *jniEnv, jobject _jthis, jint bitMask,
                                      jobject func)
{
#ifdef HAVE_SIGNALS
    memset(& jkAction, 0, sizeof(jkAction));
    jkAction.sa_handler=jk2_SigAction;
    sigaction((int)bitMask, &jkAction, (void *) NULL);
#endif
    return 0;
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_sendSignal(JNIEnv *jniEnv, jobject _jthis, jint signo,
                                          jlong target)
{
    
    return 0;
}

/* -------------------- User related functions -------------------- */

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_userId(JNIEnv *jniEnv, jobject _jthis)
{
    
    return 0;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_getPid(JNIEnv *jniEnv, jobject _jthis)
{
  return (jlong) getpid();
}


JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_setUser(JNIEnv *jniEnv, jobject _jthis,
                                       jstring userJ, jstring groupJ)
{
    int rc=0;
#ifndef WIN32
    const char *user;
    char *group;
    struct passwd *passwd;
    int uid;
    int gid;

    user = (*jniEnv)->GetStringUTFChars(jniEnv, userJ, 0);
    
    passwd = getpwnam(user);

    (*jniEnv)->ReleaseStringUTFChars(jniEnv, userJ, user);

    if (passwd == NULL ) {
        return -1;
    }
    uid = passwd->pw_uid;
    gid = passwd->pw_gid;

    if (uid < 0 || gid < 0 ) 
        return -2;

    rc = setuid(uid);

#endif

    return rc;
}

/* -------------------- interprocess mutexes -------------------- */

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_mutexCreate(JNIEnv *jniEnv, jobject _jthis, jlong poolP,
                                           jstring fileJ,
                                           jint mechJ )
{
    apr_proc_mutex_t *mutex;
    apr_lockmech_e mech=(apr_lockmech_e)mechJ;
    apr_pool_t *pool=(apr_pool_t *)(void *)(long)poolP;
    apr_status_t  st;
    char *fname=(char *)(*jniEnv)->GetStringUTFChars(jniEnv, fileJ, 0);

    st=apr_proc_mutex_create( &mutex, fname, mech, pool );

    (*jniEnv)->ReleaseStringUTFChars(jniEnv, fileJ, fname);
    
    return (jlong)(long)(void *)mutex;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_mutexLock(JNIEnv *jniEnv, jobject _jthis, jlong poolP,
                                         jlong mutexP )
{
    apr_proc_mutex_t *mutex=(apr_proc_mutex_t *)(void *)(long)mutexP;
    apr_status_t  st;
    
    st=apr_proc_mutex_lock( mutex );
    
    return (jlong)(long)st;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_mutexTryLock(JNIEnv *jniEnv, jobject _jthis, jlong poolP,
                                         jlong mutexP )
{
    apr_proc_mutex_t *mutex=(apr_proc_mutex_t *)(void *)(long)mutexP;
    apr_status_t  st;
    
    st=apr_proc_mutex_trylock( mutex );
    
    return (jlong)(long)st;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_mutexUnLock(JNIEnv *jniEnv, jobject _jthis, jlong poolP,
                                         jlong mutexP )
{
    apr_proc_mutex_t *mutex=(apr_proc_mutex_t *)(void *)(long)mutexP;
    apr_status_t  st;
    
    st=apr_proc_mutex_unlock( mutex );
    
    return (jlong)(long)st;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_mutexDestroy(JNIEnv *jniEnv, jobject _jthis, jlong poolP,
                                            jlong mutexP )
{
    apr_proc_mutex_t *mutex=(apr_proc_mutex_t *)(void *)(long)mutexP;
    apr_status_t  st;
    
    st=apr_proc_mutex_destroy( mutex );
    
    return (jlong)(long)st;
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
    int unixSocket=-1L;
#ifdef HAVE_UNIXSOCKETS
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
    
    fprintf(stderr, "Listening on %d \n",
            unixSocket);
#endif
    return (jlong)unixSocket;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_unSocketConnect(JNIEnv *jniEnv, jobject _jthis, 
                                               jlong poolJ, jstring hostJ )
{
    apr_pool_t *pool=(apr_pool_t *)(void *)(long)poolJ;
    const char *host;
    int status;
    int unixSocket=-1L;
#ifdef HAVE_UNIXSOCKETS
    struct sockaddr_un unixAddr;

    memset(& unixAddr, 0, sizeof(struct sockaddr_un));
    unixAddr.sun_family=AF_UNIX;

    host=(*jniEnv)->GetStringUTFChars(jniEnv, hostJ, 0);
    if( host==NULL )
        return -1;
    
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

#endif
    return (jlong)unixSocket;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_unAccept(JNIEnv *jniEnv, jobject _jthis, 
                                      jlong poolJ, jlong unSocketJ)
{
#ifdef HAVE_UNIXSOCKETS
    int listenUnSocket=(int)unSocketJ;
    struct sockaddr_un client;
    int clientlen;
    
    /* What to do with the address ? We could return an object, or do more.
       For now we'll ignore it */
    
    while( 1 ) {
        int connfd;

        fprintf(stderr, "unAccept %d\n", listenUnSocket );

#ifdef HAVE_SIGNALS
/*         signal( SIGCHLD, SIG_IGN ); */
/*         signal( SIGIO, jk2_SigAction ); */
#endif        
        clientlen=sizeof( client );
        
        connfd=accept( listenUnSocket, (struct sockaddr *)&client, &clientlen );
        /* XXX Should we return EINTR ? This would allow us to stop
         */
        if( connfd < 0 ) {
            fprintf(stderr, "unAccept: error %d\n", connfd);
            if( errno==EINTR ) {
                fprintf(stderr, "EINTR\n");
                continue;
            } else {
                fprintf(stderr, "Error accepting %d %d %s\n",
                        listenUnSocket, errno, strerror(errno));
                return (jlong)-errno;
            }
        }
        fprintf(stderr, "unAccept: accepted %d\n", connfd);
        return (jlong)connfd;
    }
#endif
    return 0L;
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_unRead(JNIEnv *jniEnv, jobject _jthis, 
                                      jlong poolJ, jlong unSocketJ,
                                      jbyteArray jbuf, jint from, jint cnt)
{
#ifdef HAVE_UNIXSOCKETS
    apr_pool_t *pool=(apr_pool_t *)(void *)(long)poolJ;
    jbyte *nbuf;
    int rd;
    jboolean iscopy;

    /* We can't use Critical with blocking ops. 
     */
    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, jbuf, &iscopy);
    if( ! nbuf ) {
        return -1;
    }

    if( iscopy==JNI_TRUE )
        fprintf( stderr, "aprImpl.unRead() get java bytes iscopy %d\n", iscopy);

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
                (*jniEnv)->ReleaseByteArrayElements(jniEnv, jbuf, nbuf, 0);
                return -1;
            }
        }
/*         fprintf(stderr, "Read %d from %d\n", */
/*                 rd, unSocketJ); */
    
        (*jniEnv)->ReleaseByteArrayElements(jniEnv, jbuf, nbuf, 0);
        return (jint)rd;
    }
#endif
    return (jint)0;
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_unWrite(JNIEnv *jniEnv, jobject _jthis, 
                                     jlong poolJ, jlong unSocketJ, jbyteArray jbuf, jint from, jint cnt)
{
    apr_status_t status;
    apr_pool_t *pool=(apr_pool_t *)(void *)(long)poolJ;
    jbyte *nbuf;
    int rd=0;
#ifdef HAVE_UNIXSOCKETS
    jboolean iscopy;

    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, jbuf, &iscopy);
    if( ! nbuf ) {
        return -1;
    }

    /* write */
    write( (int) unSocketJ, nbuf + from, cnt );
    
    (*jniEnv)->ReleaseByteArrayElements(jniEnv, jbuf, nbuf, 0);
#endif
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
#ifdef HAVE_UNIXSOCKETS
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
#endif
    return 0;
}

/* -------------------- Access jk components -------------------- */

/*
 * Get a jk_env_t * from the pool
 *
 * XXX We should use per thread data or per jniEnv data ( the jniEnv and jk_env are
 * serving the same purpose )
 */
JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_getJkEnv
  (JNIEnv *jniEnv, jobject o )
{
    jk_env_t *env;

    if( jk_env_globalEnv == NULL )
        return 0;

    /* fprintf(stderr, "Get env %p\n", jk_env_globalEnv); */
    env=jk_env_globalEnv->getEnv( jk_env_globalEnv );
    /*     if( env!=NULL) */
    /*         env->l->jkLog(env, env->l, JK_LOG_INFO,  */
    /*                       "aprImpl.getJkEnv()  %p\n", env); */
    return (jlong)(long)(void *)env;
}


/*
  Release the jk env 
*/
JNIEXPORT void JNICALL 
Java_org_apache_jk_apr_AprImpl_releaseJkEnv
  (JNIEnv *jniEnv, jobject o, jlong xEnv )
{
    jk_env_t *env=(jk_env_t *)(void *)(long)xEnv;

    if( jk_env_globalEnv != NULL ) 
        jk_env_globalEnv->releaseEnv( jk_env_globalEnv, env );

    if( jniDebug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "aprImpl.releaseJkEnv()  %p\n", env);
}

/*
  Recycle the jk env 
*/
JNIEXPORT void JNICALL 
Java_org_apache_jk_apr_AprImpl_jkRecycle
  (JNIEnv *jniEnv, jobject o, jlong xEnv, jlong endpointP )
{
    jk_env_t *env=(jk_env_t *)(void *)(long)xEnv;
    jk_bean_t *compCtx=(jk_bean_t *)(void *)(long)endpointP;
    jk_endpoint_t *ep = (compCtx==NULL ) ? NULL : compCtx->object;

    if( env == NULL )
        return;

    if( ep!=NULL ) {
        ep->reply->reset( env, ep->reply );
    }

    env->recycleEnv( env );

    /*     env->l->jkLog(env, env->l, JK_LOG_INFO,  */
    /*                   "aprImpl.releaseJkEnv()  %p\n", env); */
}



/*
  Find a jk component. 
*/
JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_getJkHandler
  (JNIEnv *jniEnv, jobject o, jlong xEnv, jstring compNameJ)
{
    jk_env_t *env=(jk_env_t *)(void *)(long)xEnv;
    jk_bean_t *component;
    char *cname=(char *)(*jniEnv)->GetStringUTFChars(jniEnv, compNameJ, 0);

    component=env->getBean( env, cname );
    
    /*     env->l->jkLog(env, env->l, JK_LOG_INFO,  */
    /*                   "aprImpl.getJkHandler()  %p %s\n", component, cname ); */
    
    (*jniEnv)->ReleaseStringUTFChars(jniEnv, compNameJ, cname);

    return (jlong)(long)(void *)component;
}

/*
  Create a jk handler
*/
JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_createJkHandler
  (JNIEnv *jniEnv, jobject o, jlong xEnv, jstring compNameJ)
{
    jk_env_t *env=(jk_env_t *)(void *)(long)xEnv;
    jk_bean_t *component;
    char *cname=(char *)(*jniEnv)->GetStringUTFChars(jniEnv, compNameJ, 0);

    component=env->createBean( env, NULL, cname );
    
    (*jniEnv)->ReleaseStringUTFChars(jniEnv, compNameJ, cname);

    return (jlong)(long)(void *)component;
}

/*
*/
JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_jkSetAttribute
  (JNIEnv *jniEnv, jobject o, jlong xEnv, jlong componentP, jstring nameJ, jstring valueJ )
{
    jk_env_t *env=(jk_env_t *)(void *)(long)xEnv;
    jk_bean_t *component=(jk_bean_t *)(void *)(long)componentP;
    char *name=(char *)(*jniEnv)->GetStringUTFChars(jniEnv, nameJ, 0);
    char *value=(char *)(*jniEnv)->GetStringUTFChars(jniEnv, valueJ, 0);
    int rc;
    
    if( component->setAttribute ==NULL )
        return JK_OK;
    
    rc=component->setAttribute( env, component, name, value );

    (*jniEnv)->ReleaseStringUTFChars(jniEnv, nameJ, name);
    (*jniEnv)->ReleaseStringUTFChars(jniEnv, valueJ, value);
    
    return rc;
}

/*
*/
JNIEXPORT jstring JNICALL 
Java_org_apache_jk_apr_AprImpl_jkGetAttribute
  (JNIEnv *jniEnv, jobject o, jlong xEnv, jlong componentP, jstring nameJ)
{
    jk_env_t *env=(jk_env_t *)(void *)(long)xEnv;
    jk_bean_t *component=(jk_bean_t *)(void *)(long)componentP;
    char *name=(char *)(*jniEnv)->GetStringUTFChars(jniEnv, nameJ, 0);
    char *value;
    jstring valueJ=NULL;
    int rc;
    
    if( component->setAttribute ==NULL )
        return JK_OK;
    
    value=component->getAttribute( env, component, name );
    if( value!=NULL )
        valueJ=(*jniEnv)->NewStringUTF(jniEnv, value);
    
    (*jniEnv)->ReleaseStringUTFChars(jniEnv, nameJ, name);
    
    return valueJ;
}

/*
*/
JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_jkGetId
  (JNIEnv *jniEnv, jobject o, jlong xEnv, jstring nsJ, jstring nameJ)
{
    jk_env_t *env=(jk_env_t *)(void *)(long)xEnv;
    char *name=(char *)(*jniEnv)->GetStringUTFChars(jniEnv, nameJ, 0);
    char *value;
    int rc=-1;
    int i=0;

    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "aprImpl.getId()  %d %s\n", rc, name );
        
    (*jniEnv)->ReleaseStringUTFChars(jniEnv, nameJ, name);
    
    return rc;
}


/*
*/
JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_jkInvoke
  (JNIEnv *jniEnv, jobject o, jlong envJ, jlong componentP, jlong endpointP, jint code, jbyteArray data, jint len)
{
    jk_env_t *env = (jk_env_t *)(void *)(long)envJ;
    jk_bean_t *compCtx=(jk_bean_t *)(void *)(long)endpointP;
    void *target=(void *)(long)componentP;
    jk_endpoint_t *ep = compCtx->object;

    jbyte *nbuf;
    jboolean iscopy;

    int cnt=0;
    jint rc = -1;
    unsigned acc = 0;

    /*env->l->jkLog(env, env->l, JK_LOG_INFO,"jkInvoke()\n"); */
        
    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, data, &iscopy);

    if( iscopy )
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "aprImpl.jkInvoke() get java bytes iscopy %d\n", iscopy);

    if(nbuf==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "jkInvoke() NullPointerException 2\n");
	return -1;
    }
    
    ep->reply->reset(env, ep->reply);

    memcpy( ep->reply->buf, nbuf , len );
    
    rc=ep->reply->checkHeader( env, ep->reply, ep );
    /* ep->reply->dump( env, ep->reply ,"MESSAGE"); */
    if( rc < 0  ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "jkInvoke() invalid data\n");
        /* we just can't recover, unset recover flag */
        (*jniEnv)->ReleaseByteArrayElements(jniEnv, data, nbuf, 0);
        return JK_ERR;
    }

    /*  env->l->jkLog(env, env->l, JK_LOG_INFO, */
    /*               "jkInvoke() component dispatch %d %d\n", rc, code); */

    rc=workerEnv->dispatch( env, workerEnv, target, ep, code, ep->reply );
    
    (*jniEnv)->ReleaseByteArrayElements(jniEnv, data, nbuf, 0);

    /*     env->l->jkLog(env, env->l, JK_LOG_INFO, "jkInvoke() done\n"); */

    return rc;
}

