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
#include "jk_logger.h"

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

#ifdef HAVE_SIGNAL
#include "signal.h"
#endif

/* jniAprPool is the 'master pool' for standalone use ( i.e. not inprocess )
   All resources are allocated in this pool
*/
static apr_pool_t *jniAprPool=NULL;

/** Access to the jk workerEnv. This field and jk_env_globalEnv are used
    to interact with the jk components 
 */
static jk_workerEnv_t *workerEnv;

static int jniDebug=0;

#define JK_GET_REGION 1
#define JK_GET_BYTE_ARRAY_ELEMENTS 2
#define JK_DIRECT_BUFFER_NIO 3

static int arrayAccessMethod=JK_GET_REGION;

JNIEXPORT void JNICALL 
Java_org_apache_jk_apr_AprImpl_setArrayAccessMode(JNIEnv *jniEnv, jobject _jthis, jint mode)
{
    arrayAccessMethod=mode;
}

/* -------------------- Apr initialization and pools -------------------- */
     
JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_initialize(JNIEnv *jniEnv, jobject _jthis)
{
    jk_env_t *env;

    /* For in-process the env is initialized already */
    if( jk_env_globalEnv == NULL ) {
        jk_pool_t *globalPool;

        apr_initialize(); 
        apr_pool_create( &jniAprPool, NULL );

        if( jniAprPool==NULL ) {
            return JK_ERR;
        }
        jk2_pool_apr_create( NULL, &globalPool, NULL, jniAprPool );
        /* Create the global env */
        env=jk2_env_getEnv( NULL, globalPool );
        env->setAprPool(env, gPool);
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

        env->l=jkb->object;
        env->l->name="stderr";
        env->l->level=JK_LOG_INFO_LEVEL;
        env->alias( env, "logger.file:", "logger");

        jkb=env->createBean2( env, env->globalPool,"workerEnv", "");
        env->alias( env, "workerEnv:", "workerEnv");
        if( jkb==NULL ) {
            fprintf(stderr, "Error creating workerEnv ");
            return JK_ERR;
        }

        workerEnv=jkb->object;
        
        workerEnv->init( env, workerEnv );
    }

    return JK_OK;
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_terminate(JNIEnv *jniEnv, jobject _jthis)
{
    if ( jniAprPool!=NULL ) {
        apr_pool_destroy(jniAprPool);
        jniAprPool = NULL;
/*     apr_terminate(); */
    }
    return 0;
}

/* -------------------- Signals -------------------- */
/* XXX Move it to jk_signal.c */

#ifdef HAVE_SIGNALS
static struct sigaction jkAction;

/* We use a jni channel to send the notification to java
 */
static jk_channel_t *jniChannel;

/* XXX we should sync or use multiple endpoints if multiple signals
   can be concurent
*/
static jk_endpoint_t *signalEndpoint;

static void jk2_SigAction(int sig) {
    jk_env_t *env;
    
    /* Make a callback using the jni channel */
    fprintf(stderr, "Signal %d\n", sig );

    if( jk_env_globalEnv == NULL ) {
        return;
    }

    env=jk_env_globalEnv->getEnv( jk_env_globalEnv );

    if( jniChannel==NULL ) {
        jniChannel=env->getByName( env, "channel.jni:jni" );
        fprintf(stderr, "Got jniChannel %p\n", jniChannel );
    }
    if( jniChannel==NULL ) {
        return;
    }
    if( signalEndpoint == NULL ) {
        jk_bean_t *component=env->createBean2( env, NULL, "endpoint", NULL );
        if( component == NULL ) {
            fprintf(stderr, "Can't create endpoint\n" );
            return;
        }
        component->init( env, component );
        fprintf(stderr, "Create endpoint %p\n", component->object );
        signalEndpoint=component->object;
    }

    /* Channel:jni should be initialized by the caller */

    /* XXX make the callback */
    
    jk_env_globalEnv->releaseEnv( jk_env_globalEnv, env );

}


/* XXX We need to: - preserve the old signal ( or get them ) - either
     implement "waitSignal" or use invocation in jk2_SigAction

     Probably waitSignal() is better ( we can have a thread that waits )
 */

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_signal(JNIEnv *jniEnv, jobject _jthis, jint signalNr )
{
    memset(& jkAction, 0, sizeof(jkAction));
    jkAction.sa_handler=jk2_SigAction;
    sigaction((int)signalNr, &jkAction, (void *) NULL);
    return 0;
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_sendSignal(JNIEnv *jniEnv, jobject _jthis, jint target ,
                                          jint signo)
{
    return kill( (pid_t)target, (int)signo );
}

#else /* ! HAVE_SIGNALS */

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_signal(JNIEnv *jniEnv, jobject _jthis, jint signalNr )
{
    return 0;
}

JNIEXPORT void JNICALL 
Java_org_apache_jk_apr_AprImpl_sendSignal(JNIEnv *jniEnv, jobject _jthis, jint signo,
                                          jint target)
{

}

#endif


/* -------------------- User related functions -------------------- */
/* XXX move it to jk_user.c */

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_getPid(JNIEnv *jniEnv, jobject _jthis)
{
  return (jint) getpid();
}


JNIEXPORT jint JNICALL 
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

    return (jint)rc;
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
  Recycle the jk endpoint
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
  Create a jk handler XXX It should be createJkBean
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
    int rc=JK_OK;
    
    if( component->setAttribute ==NULL )
        return JK_OK;

    if( component->setAttribute!=NULL ) {
        rc=component->setAttribute( env, component, name,
                                    component->pool->pstrdup( env, component->pool, value ) );
    }

    (*jniEnv)->ReleaseStringUTFChars(jniEnv, nameJ, name);
    (*jniEnv)->ReleaseStringUTFChars(jniEnv, valueJ, value);
    
    return rc;
}

/*
*/
JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_jkInit
  (JNIEnv *jniEnv, jobject o, jlong xEnv, jlong componentP )
{
    jk_env_t *env=(jk_env_t *)(void *)(long)xEnv;
    jk_bean_t *component=(jk_bean_t *)(void *)(long)componentP;
    int rc;

    if( component->init ==NULL )
        return JK_OK;
    
    rc=component->init( env, component );
    return rc;
}

/*
*/
JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_jkDestroy
  (JNIEnv *jniEnv, jobject o, jlong xEnv, jlong componentP )
{
    jk_env_t *env=(jk_env_t *)(void *)(long)xEnv;
    jk_bean_t *component=(jk_bean_t *)(void *)(long)componentP;
    int rc;
    
    if( component->destroy ==NULL )
        return JK_OK;
    
    rc=component->destroy( env, component );

    /* XXX component->pool->reset( env, component->pool ); */
    
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
Java_org_apache_jk_apr_AprImpl_jkInvoke
  (JNIEnv *jniEnv, jobject o, jlong envJ, jlong componentP, jlong endpointP, jint code,
   jbyteArray data, jint off, jint len,
   jint raw)
{
    jk_env_t *env = (jk_env_t *)(void *)(long)envJ;
    jk_bean_t *compCtx=(jk_bean_t *)(void *)(long)endpointP;
    void *target=(void *)(long)componentP;
    jk_bean_t *bean=(jk_bean_t *)target;
    jk_endpoint_t *ep;

    jbyte *nbuf;
    jboolean iscopy;

    int cnt=0;
    jint rc = 0;
    unsigned acc = 0;
    unsigned char *oldBuf;

    if( compCtx==NULL || data==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,"jni.jkInvoke() NPE\n");
        return JK_ERR;
    }

    ep = compCtx->object;

    if( ep==NULL || ep->reply==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,"jni.jkInvoke() NPE ep==null\n");
        return JK_ERR;
    }
        
    if( arrayAccessMethod == JK_GET_BYTE_ARRAY_ELEMENTS ) {
        nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, data, &iscopy);
        if( iscopy )
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "aprImpl.jkInvoke() get java bytes iscopy %d\n", iscopy);
        
        if(nbuf==NULL) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                          "jkInvoke() NullPointerException 2\n");
            return -1;
        }
        if( raw==0 ) {
            ep->reply->reset(env, ep->reply);
        }
        
        oldBuf=ep->reply->buf;
        ep->reply->buf = (unsigned char *)nbuf;
    } else if ( arrayAccessMethod == JK_GET_REGION ) {
        (*jniEnv)->GetByteArrayRegion( jniEnv, data, off, len, (jbyte *)ep->reply->buf );
    }
        
    
    if( raw == 0 ) {
        rc=ep->reply->checkHeader( env, ep->reply, ep );
    } else {
        ep->reply->len = len;
        ep->reply->pos= off;
    }

    /* ep->reply->dump( env, ep->reply ,"MESSAGE"); */
    if( rc < 0  ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "jkInvoke() invalid data\n");
        /* we just can't recover, unset recover flag */
        (*jniEnv)->ReleaseByteArrayElements(jniEnv, data, nbuf, 0);
        ep->reply->buf=oldBuf;
        return JK_ERR;
    }

    if( bean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jkInvoke() component dispatch %d %d %p\n", rc, code, bean->invoke);
    
    if( bean->invoke != NULL ) {
        rc=bean->invoke( env, bean, ep, code, ep->reply, raw );
    } else {
        /* Backward compat for AJP13 messages, where the code is used to
         locate a handler. Deprecated, use the invoke() method  ! */
        rc=workerEnv->dispatch( env, workerEnv, target, ep, code, ep->reply ); 
    }

    /* Copy back the response, if any */

    if( arrayAccessMethod == JK_GET_BYTE_ARRAY_ELEMENTS ) {
        if( rc == JK_INVOKE_WITH_RESPONSE ) {
            (*jniEnv)->ReleaseByteArrayElements(jniEnv, data, nbuf, JNI_ABORT );
            rc=JK_OK;
        } else {
            (*jniEnv)->ReleaseByteArrayElements(jniEnv, data, nbuf, 0);
        }
        ep->reply->buf=oldBuf;
    } else if ( arrayAccessMethod == JK_GET_REGION ) {
        if( rc == JK_INVOKE_WITH_RESPONSE ) {
            /*   env->l->jkLog(env, env->l, JK_LOG_INFO, */
            /*                "jkInvoke() release %d %d %p\n", */
            /*                ep->reply->pos, ep->reply->len , ep->reply->buf ); */
            (*jniEnv)->SetByteArrayRegion( jniEnv, data, 0, ep->reply->len + 4 , (jbyte *)ep->reply->buf );
            rc=JK_OK;
        }
    } 

    if( (*jniEnv)->ExceptionCheck( jniEnv ) ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jkInvoke() component dispatch %d %d %p\n", rc, code, bean->invoke);
        (*jniEnv)->ExceptionDescribe( jniEnv );
        /* Not needed if Describe is used.
            (*jniEnv)->ExceptionClear( jniEnv ) */
    }
    
    /*     env->l->jkLog(env, env->l, JK_LOG_INFO, "jkInvoke() done\n"); */

    return rc;
}


static JNINativeMethod org_apache_jk_apr_AprImpl_native_methods[] = {
    { 
        "initialize", "()I", 
        Java_org_apache_jk_apr_AprImpl_initialize 
    },
    { 
        "terminate", "()I",
        Java_org_apache_jk_apr_AprImpl_terminate
    },
    { 
        "getJkEnv", "()J",
        Java_org_apache_jk_apr_AprImpl_getJkEnv
    },
    {
        "releaseJkEnv", "(J)V",
        Java_org_apache_jk_apr_AprImpl_releaseJkEnv
    },
    { 
        "getJkHandler", "(JLjava/lang/String;)J",
        Java_org_apache_jk_apr_AprImpl_getJkHandler
    },
    {
        "createJkHandler", "(JLjava/lang/String;)J",
        Java_org_apache_jk_apr_AprImpl_createJkHandler
    },
    {
        "jkSetAttribute", "(JJLjava/lang/String;Ljava/lang/String;)I",
        Java_org_apache_jk_apr_AprImpl_jkSetAttribute
    },
    {
        "jkGetAttribute", "(JJLjava/lang/String;)Ljava/lang/String;",
        Java_org_apache_jk_apr_AprImpl_jkGetAttribute
    },
    {
        "jkInit", "(JJ)I",
        Java_org_apache_jk_apr_AprImpl_jkInit
    },
    {
        "jkDestroy", "(JJ)I",
        Java_org_apache_jk_apr_AprImpl_jkDestroy
    },
    {
        "jkInvoke", "(JJJI[BIII)I",
        Java_org_apache_jk_apr_AprImpl_jkInvoke
    },
    {
        "jkRecycle", "(JJ)V",
        Java_org_apache_jk_apr_AprImpl_jkRecycle
    },
    {
        "setUser", "(Ljava/lang/String;Ljava/lang/String;)I",
        Java_org_apache_jk_apr_AprImpl_setUser
    },
    {
        "signal", "(I)I",
        Java_org_apache_jk_apr_AprImpl_signal
    },
    {
        "sendSignal", "(II)V",
        Java_org_apache_jk_apr_AprImpl_sendSignal
    }
};

/*
  Register Native methods returning the total number of
  native functions
*/
jint jk_jni_aprImpl_registerNatives(JNIEnv *jniEnv, jclass bridgeClass)
{
  
   return (*jniEnv)->RegisterNatives(jniEnv, bridgeClass,
                        org_apache_jk_apr_AprImpl_native_methods,
                        sizeof(org_apache_jk_apr_AprImpl_native_methods) / 
                        sizeof(JNINativeMethod));
}


