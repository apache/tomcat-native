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
 *  Channel using jni calls ( assuming in-process tomcat ).
 *
 * @author:  Gal Shachor <shachor@il.ibm.com>                           
 * @author: Costin Manolache
 */

#include "jk_workerEnv.h"
#include "jk_env.h"
#include "jk_bean.h"

#ifdef HAVE_JNI

#include "jk_map.h"
#include "jk_env.h"
#include "jk_channel.h"
#include "jk_global.h"

#include <string.h>
#include "jk_registry.h"
#include <jni.h>

/* default only, is configurable now */

#define JAVA_BRIDGE_CLASS_NAME ("org/apache/jk/apr/AprImpl")

#define JNI_TOMCAT_STARTED 2
extern int jk_jni_status_code;

/** Information specific for the socket channel
 */
typedef struct {
    jk_vm_t *vm;
    
    char *className;
    jclass jniBridge;

    jmethodID writeMethod;
    int status;
} jk_channel_jni_private_t;

typedef struct {
    JNIEnv *jniEnv;

    int len;
    jbyteArray jarray;
    char *carray;
    int arrayLen;
    
    jobject jniJavaContext;
/*     jobject msgJ; */
} jk_ch_jni_ep_private_t;


/*
   Duplicate string and convert it to ASCII on EBDIC based systems
   Needed for at least AS/400 and BS2000 but what about other EBDIC systems ?
*/
static void *strdup_ascii(jk_env_t *env, 
                          char *s)
{
#if defined(AS400) || defined(_OSD_POSIX)
	return (env->tmpPool->pstrdup2ascii(env, env->tmpPool, s));
#else
	return (env->tmpPool->pstrdup(env, env->tmpPool, s));
#endif
}

static int JK_METHOD jk2_channel_jni_init(jk_env_t *env,
                                          jk_bean_t *jniWB)
{
    jk_channel_t *jniW=jniWB->object;
    jk_workerEnv_t *wEnv=jniW->workerEnv;

    if (wEnv->childId != 0) {
        if( jniW->worker != NULL )
            jniW->worker->mbean->disabled=JK_TRUE;
        return JK_ERR;
    }
    if( wEnv->vm == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.init() no VM found\n" );
        if( jniW->worker != NULL ) {
            jniW->worker->mbean->disabled=JK_TRUE;
        }
        return JK_ERR;
    }
    return JK_OK;
}

/** Assume the jni-worker or someone else started
 *  tomcat and initialized what is needed.
 */
static int JK_METHOD jk2_channel_jni_open(jk_env_t *env,
                                          jk_channel_t *_this,
                                          jk_endpoint_t *endpoint)
{
    jk_workerEnv_t *we=endpoint->worker->workerEnv;
    JNIEnv *jniEnv;
    jk_ch_jni_ep_private_t *epData;
    jmethodID jmethod;
    jobject jobj;
    jstring jstr;

    jk_channel_jni_private_t *jniCh=_this->_privatePtr;

    if( endpoint->channelData != NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() already open, nothing else to do\n"); 
        return JK_OK;
    }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,"channel_jni.open():  \n" );

    /* It is useless to continue if the channel worker 
       does not exist.
     */
    if( _this->worker == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channel_jni.open()  NullPointerException, no channel worker found\n"); 
        return JK_ERR;
    }
    
    jniCh->vm=(jk_vm_t *)we->vm;
    if( jniCh->vm == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channel_jni.open() no VM found\n" ); 
        _this->worker->mbean->disabled=JK_TRUE;
        return JK_ERR;
    }

    jniEnv = (JNIEnv *)jniCh->vm->attach( env, jniCh->vm );
    if( jniEnv == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channel_jni.open() can't attach\n" ); 
        _this->worker->mbean->disabled=JK_TRUE;
        return JK_ERR;
    }
    /* Create the buffers used by the write method. We allocate a
       byte[] and jbyte[] - I have no idea what's more expensive,
       to copy a buffer or to 'pin' the jbyte[] for copying.

       This will be tuned if needed, for now it seems the easiest
       solution
    */
    epData=(jk_ch_jni_ep_private_t *)
        endpoint->mbean->pool->calloc( env,endpoint->mbean->pool,
                                       sizeof( jk_ch_jni_ep_private_t ));
    
    endpoint->channelData=epData;

    /* AS400/BS2000 need EBCDIC to ASCII conversion for JNI */
    jniCh->jniBridge = (*jniEnv)->FindClass(jniEnv, strdup_ascii(env, jniCh->className) );
    
    if( jniCh->jniBridge == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channel_jni.open() can't find %s\n",jniCh->className ); 
        _this->worker->mbean->disabled=JK_TRUE;
        return JK_ERR;
    }

    jniCh->jniBridge=(*jniEnv)->NewGlobalRef( jniEnv, jniCh->jniBridge);

    if( jniCh->jniBridge == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channel_jni.open() Unable to allocate globalref for %s\n",jniCh->className ); 
        _this->worker->mbean->disabled=JK_TRUE;
        return JK_ERR;
    }

    /* Interface to the callback mechansim. The idea is simple ( is it ? ) - we
       use a similar pattern with java, trying to do as little as possible
       in C and pass minimal information to allow this.

       The pattern used for callback works for our message forwarding but also for
       other things - like singnals, etc
    */

    /* AS400/BS2000 need EBCDIC to ASCII conversion for JNI */
    jmethod=(*jniEnv)->GetStaticMethodID(jniEnv, jniCh->jniBridge,
                 strdup_ascii(env, "createJavaContext"), 
                 strdup_ascii(env, "(Ljava/lang/String;J)Ljava/lang/Object;"));

    if( jmethod == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channel_jni.open() can't find createJavaContext\n"); 
        _this->worker->mbean->disabled=JK_TRUE;

        if( (*jniEnv)->ExceptionCheck( jniEnv ) ) {
            (*jniEnv)->ExceptionClear( jniEnv );
        }
        return JK_ERR;
    }
    
    /* AS400/BS2000 need EBCDIC to ASCII conversion for JNI */
    jstr=(*jniEnv)->NewStringUTF(jniEnv, strdup_ascii(env, "channelJni" ));
    
    jobj=(*jniEnv)->CallStaticObjectMethod( jniEnv, jniCh->jniBridge,
                                            jmethod,
                                            jstr, 
                                            (jlong)(long)(void *)endpoint->mbean );

    if( jobj  == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channel_jni.open() Can't create java context\n" ); 
        epData->jniJavaContext=NULL;
        _this->worker->mbean->disabled=JK_TRUE;
        if( (*jniEnv)->ExceptionCheck( jniEnv ) ) {
            (*jniEnv)->ExceptionClear( jniEnv );
        }
        return JK_ERR;
    }
    epData->jniJavaContext=(*jniEnv)->NewGlobalRef( jniEnv, jobj );

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channel_jni.open() Got ep %#lx %#lx\n", jobj, epData->jniJavaContext ); 

    /* XXX Destroy them in close */
    
    /* AS400/BS2000 need EBCDIC to ASCII conversion for JNI */
    jmethod=(*jniEnv)->GetStaticMethodID(jniEnv, jniCh->jniBridge,
                                         strdup_ascii(env, "getBuffer"),
                                         strdup_ascii(env, "(Ljava/lang/Object;I)[B"));
    if( jmethod == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channel_jni.open() can't find getBuffer\n"); 
        _this->worker->mbean->disabled=JK_TRUE;
        return JK_ERR;
    }

    epData->jarray=(*jniEnv)->CallStaticObjectMethod( jniEnv, jniCh->jniBridge,
                                                      jmethod,
                                                      epData->jniJavaContext, 0);

    epData->jarray=(*jniEnv)->NewGlobalRef( jniEnv, epData->jarray );

    epData->arrayLen = (*jniEnv)->GetArrayLength( jniEnv, epData->jarray );
    
    /* XXX > ajp buffer size. Don't know how to fragment or reallocate
       yet */
    epData->carray=(char *)endpoint->mbean->pool->calloc( env, endpoint->mbean->pool,
                                                          epData->arrayLen);

    /* AS400/BS2000 need EBCDIC to ASCII conversion for JNI */
    jniCh->writeMethod =
        (*jniEnv)->GetStaticMethodID(jniEnv, jniCh->jniBridge,
                                     strdup_ascii(env, "jniInvoke"),
                                     strdup_ascii(env, "(JLjava/lang/Object;)I"));
    
    if( jniCh->writeMethod == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "channel_jni.open() can't find jniInvoke\n"); 
        
        _this->worker->mbean->disabled=JK_TRUE;
        return JK_ERR;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channel_jni.open() found write method, open ok\n" ); 

    _this->worker->mbean->disabled=JK_FALSE;
    
    /* Don't detach ( XXX Need to find out when the thread is
     *  closing in order for this to work )
     */
    /*     jniCh->vm->detach( env, jniCh->vm ); */
    return JK_OK;
}


/** 
 */
static int JK_METHOD jk2_channel_jni_close(jk_env_t *env,jk_channel_t *_this,
                                           jk_endpoint_t *endpoint)
{
    jk_ch_jni_ep_private_t *epData;
    JNIEnv *jniEnv;
    jk_channel_jni_private_t *jniCh=_this->_privatePtr;
    epData=(jk_ch_jni_ep_private_t *)endpoint->channelData;

    if (epData == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.close() no channel data\n" ); 
        return JK_ERR;
    }
    jniEnv = (JNIEnv *)jniCh->vm->attach( env, jniCh->vm );

    if( jniEnv == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.close() can't attach\n" ); 
        return JK_ERR;
    }
    if( epData->jarray != NULL ){
        (*jniEnv)->DeleteGlobalRef( jniEnv, epData->jarray );
    }
    if( epData->jniJavaContext != NULL){
        (*jniEnv)->DeleteGlobalRef( jniEnv, epData->jniJavaContext );
    }
    
    jniCh->vm->detach( env, jniCh->vm );
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channel_jni.close() ok\n" ); 

    endpoint->channelData=NULL;
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
static int JK_METHOD jk2_channel_jni_send(jk_env_t *env, jk_channel_t *_this,
                                          jk_endpoint_t *endpoint,
                                          jk_msg_t *msg) 
{
/*    int sd; */
    int  sent=0;
    char *b;
    int len;
    jbyte *nbuf;
    jbyteArray jbuf;
    int jlen=0;
    jboolean iscopy=0;
    JNIEnv *jniEnv;
    jk_channel_jni_private_t *jniCh=_this->_privatePtr;
    jk_ch_jni_ep_private_t *epData=
        (jk_ch_jni_ep_private_t *)endpoint->channelData;

    if( _this->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,"channel_jni.send() %#lx\n", epData ); 

    if( epData == NULL ) {
        jk2_channel_jni_open( env, _this, endpoint );
        epData=(jk_ch_jni_ep_private_t *)endpoint->channelData;
    }
    if( epData == NULL ){
        env->l->jkLog(env, env->l, JK_LOG_ERROR,"channel_jni.send() error opening channel\n" ); 
        return JK_ERR;
    }
    if( epData->jniJavaContext == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,"channel_jni.send() no java context\n" ); 
        jk2_channel_jni_close( env, _this, endpoint );
        return JK_ERR;
    }

    msg->end( env, msg );
    len=msg->len;
    b=msg->buf;

    if( _this->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,"channel_jni.send() (1) %#lx\n", epData ); 

    jniEnv=NULL; /* epData->jniEnv; */
    jbuf=epData->jarray;

    if( jniCh->writeMethod == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "channel_jni.send() no write method\n" ); 
        return JK_ERR;
    }
    if( jniEnv==NULL ) {
        /* Try first getEnv, then attach */
        jniEnv = (JNIEnv *)jniCh->vm->attach( env, jniCh->vm );
        if( jniEnv == NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "channel_jni.send() can't attach\n" ); 
            return JK_ERR;
        }
    }

    if( _this->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "channel_jni.send() getting byte array \n" );
    
    /* Copy the data in the ( recycled ) jbuf, then call the
     *  write method. XXX We could try 'pining' if the vm supports
     *  it, this is a looong lived object.
     */
#ifdef JK_JNI_CRITICAL
    nbuf = (*jniEnv)->GetPrimitiveArrayCritical(jniEnv, jbuf, &iscopy);
#else
    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, jbuf, &iscopy);
 #endif
    if( iscopy )
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelJni.send() get java bytes iscopy %d\n", iscopy);
    
    if(nbuf==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channelJni.send() Can't get java bytes");
        return JK_ERR;
    }

    memcpy( nbuf, b, len );

#ifdef JK_JNI_CRITICAL
    (*jniEnv)->ReleasePrimitiveArrayCritical(jniEnv, jbuf, nbuf, 0);
#else
    (*jniEnv)->ReleaseByteArrayElements(jniEnv, jbuf, nbuf, 0);
#endif    
    if( _this->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "channel_jni.send() before send %#lx\n",
                      (void *)(long)epData->jniJavaContext); 
    
    sent=(*jniEnv)->CallStaticIntMethod( jniEnv,
                                         jniCh->jniBridge, 
                                         jniCh->writeMethod,
                                         (jlong)(long)(void *)env,
                                         epData->jniJavaContext );
    if( _this->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,"channel_jni.send() result %d\n",
                      sent); 
    return JK_OK;
}


/**
 * Not used - we have a single thread, there is no 'blocking' - the
 * java side will send messages by calling a native method, which will
 * receive and dispatch.
 */
static int JK_METHOD jk2_channel_jni_recv(jk_env_t *env, jk_channel_t *_this,
                                         jk_endpoint_t *endpoint,
                                         jk_msg_t *msg) 
{
/*    jbyte *nbuf; */
/*    jbyteArray jbuf; */
/*    int jlen; */
/*    jboolean iscommit;     */
    jk_channel_jni_private_t *jniCh=_this->_privatePtr;

    env->l->jkLog(env, env->l, JK_LOG_ERROR,
                  "channelJni.recv() method not supported for JNI channel\n");
    return -1;

    /* Old workaround:
       
    nbuf=(jbyte *)endpoint->currentData;
    
    if(nbuf==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelJni.recv() no jbyte[] was received\n");
        return -1;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelJni.recv() receiving %d\n", len);

    memcpy( b, nbuf + endpoint->currentOffset, len );
    endpoint->currentOffset += len;
    
    return len;
    */
}


/** Called before request processing, to initialize resources.
    All following calls will be in the same thread.
*/
int JK_METHOD jk2_channel_jni_beforeRequest(struct jk_env *env,
                                            jk_channel_t *_this,
                                            struct jk_worker *worker,
                                            struct jk_endpoint *endpoint,
                                            struct jk_ws_service *r )
{
    jk_workerEnv_t *we=worker->workerEnv;

    if( worker->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "service() attaching to vm\n");

    return JK_OK;
}

/** Called after request processing. Used to be worker.done()
 */
int JK_METHOD jk2_channel_jni_afterRequest(struct jk_env *env,
                                          jk_channel_t *_this,
                                          struct jk_worker *worker,
                                          struct jk_endpoint *endpoint,
                                          struct jk_ws_service *r )
{
    jk_workerEnv_t *we=worker->workerEnv;

    if( we==NULL || we->vm==NULL) {
        return JK_OK;
    }
    /* 
     * In case of not having the endpoint detach the jvm.
     * XXX Remove calling this function from ajp13 worker?
     */
    if (endpoint == NULL)
        we->vm->detach( env, we->vm );
    if( worker->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                      "channelJni.afterRequest() ok\n");
    return JK_OK;
}

static int JK_METHOD jk2_channel_jni_setProperty(jk_env_t *env,
                                                    jk_bean_t *mbean, 
                                                    char *name, void *valueP)
{
    jk_channel_t *ch=(jk_channel_t *)mbean->object;
    char *value=valueP;
    jk_channel_jni_private_t *jniInfo=
        (jk_channel_jni_private_t *)(ch->_privatePtr);

    if( strcmp( "class", name ) == 0 ) {
        jniInfo->className=value;
    }
    /* TODO: apache protocol hooks
    else if( strcmp( "xxxx", name ) == 0 ) {
        jniInfo->xxxx=value;
    } 
    */
    else {
        return jk2_channel_setAttribute( env, mbean, name, valueP );
    }
    return JK_OK;
}

/** Called by java. Will take the msg and dispatch it to workerEnv, as if it would
 *  be if received via socket
 */
int JK_METHOD jk2_channel_jni_invoke(jk_env_t *env, jk_bean_t *bean, jk_endpoint_t *ep, int code,
                                     jk_msg_t *msg, int raw)
{
    jk_channel_t *ch=(jk_channel_t *)bean->object;
    int rc=JK_OK;

    if( ch->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                      "ch.%d() \n", code);
    
    code = (int)msg->getByte(env, msg);

    if( ch->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,"channelJni.java2cInvoke() %d\n", code);

    return ep->worker->workerEnv->dispatch( env, ep->worker->workerEnv,
                                            ep->currentRequest, ep, code, ep->reply );
}

static int JK_METHOD jk2_channel_jni_status(jk_env_t *env,
                                            struct jk_worker *worker,
                                            jk_channel_t *_this)
{

    jk_channel_jni_private_t *jniCh=_this->_privatePtr;
    if ( jniCh->status != JNI_TOMCAT_STARTED) {
        jniCh->status = jk_jni_status_code;
        if (jniCh->status != JNI_TOMCAT_STARTED)
            return JK_ERR;
    }
    return JK_OK;
}


int JK_METHOD jk2_channel_jni_factory(jk_env_t *env, jk_pool_t *pool, 
                                      jk_bean_t *result,
                                      const char *type, const char *name)
{
    jk_channel_t *ch=result->object;
    jk_workerEnv_t *wEnv;
    jk_channel_jni_private_t *jniPrivate;


    wEnv = env->getByName( env, "workerEnv" );
    ch=(jk_channel_t *)pool->calloc(env, pool, sizeof( jk_channel_t));
    
    ch->recv= jk2_channel_jni_recv;
    ch->send= jk2_channel_jni_send; 
    ch->open= jk2_channel_jni_open; 
    ch->close= jk2_channel_jni_close; 

    ch->beforeRequest= jk2_channel_jni_beforeRequest;
    ch->afterRequest= jk2_channel_jni_afterRequest;
    ch->status = jk2_channel_jni_status;

    ch->_privatePtr=jniPrivate=(jk_channel_jni_private_t *)pool->calloc(env, pool,
                                sizeof(jk_channel_jni_private_t));

    jniPrivate->className = JAVA_BRIDGE_CLASS_NAME;
    ch->is_stream=JK_FALSE;

    /* No special attribute */
    result->setAttribute= jk2_channel_jni_setProperty;
    ch->mbean=result;
    result->object= ch;
    result->init= jk2_channel_jni_init; 

    ch->workerEnv=wEnv;
    wEnv->addChannel( env, wEnv, ch );

    result->invoke=jk2_channel_jni_invoke;

    return JK_OK;
}

#else

int JK_METHOD jk2_channel_jni_factory(jk_env_t *env, jk_pool_t *pool, 
                                      jk_bean_t *result,
                                      const char *type, const char *name)
{
    result->disabled=1;
    return JK_OK;
}

#endif
