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
 *  Channel using jni calls ( assuming in-process tomcat ).
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
#include <jni.h>

/* default only, is configurable now */
#define JAVA_BRIDGE_CLASS_NAME ("org/apache/jk/apr/AprImpl")


/** Information specific for the socket channel
 */
typedef struct {
    jk_vm_t *vm;
    
    char *className;
    jclass jniBridge;

    jmethodID writeMethod;
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



static int JK_METHOD jk2_channel_jni_setProperty(jk_env_t *env,
                                                 jk_bean_t *mbean, 
                                                 char *name, void *value)
{
    return JK_OK;
}

static int JK_METHOD jk2_channel_jni_init(jk_env_t *env,
                                          jk_channel_t *_this)
{

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
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,"channel_jni.init():  \n" );

    jniCh->vm=(jk_vm_t *)we->vm;
    if( jniCh->vm == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() no VM found\n" ); 
        return JK_ERR;
    }

    jniEnv = (JNIEnv *)jniCh->vm->attach( env, jniCh->vm );
    if( jniEnv == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() can't attach\n" ); 
        return JK_ERR;
    }
    /* Create the buffers used by the write method. We allocate a
       byte[] and jbyte[] - I have no idea what's more expensive,
       to copy a buffer or to 'pin' the jbyte[] for copying.

       This will be tuned if needed, for now it seems the easiest
       solution
    */
    epData=(jk_ch_jni_ep_private_t *)
        endpoint->pool->calloc( env,endpoint->pool,
                                sizeof( jk_ch_jni_ep_private_t ));
    
    endpoint->channelData=epData;
    /** XXX make it customizable */
    jniCh->className=JAVA_BRIDGE_CLASS_NAME;

    jniCh->jniBridge =
        (*jniEnv)->FindClass(jniEnv, jniCh->className );

    jniCh->jniBridge=(*jniEnv)->NewGlobalRef( jniEnv, jniCh->jniBridge);
    
    if( jniCh->jniBridge == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() can't find %s\n",jniCh->className ); 
        return JK_ERR;
    }


    /* Interface to the callback mechansim. The idea is simple ( is it ? ) - we
       use a similar pattern with java, trying to do as little as possible
       in C and pass minimal information to allow this.

       The pattern used for callback works for our message forwarding but also for
       other things - like singnals, etc
    */

    jmethod=(*jniEnv)->GetStaticMethodID(jniEnv, jniCh->jniBridge,
                 "createJavaContext", "(Ljava/lang/String;J)Ljava/lang/Object;");
    if( jmethod == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() can't find createJavaContext\n"); 
        return JK_ERR;
    }
    
    jstr=(*jniEnv)->NewStringUTF(jniEnv, "channelJni" );
    
    jobj=(*jniEnv)->CallStaticObjectMethod( jniEnv, jniCh->jniBridge,
                                            jmethod,
                                            jstr, 
                                            (jlong)(long)(void *)endpoint->mbean );

    if( jobj  == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "channel_jni.open() Can't create java context\n" ); 
        epData->jniJavaContext=NULL;
        return JK_ERR;
    }
    epData->jniJavaContext=(*jniEnv)->NewGlobalRef( jniEnv, jobj );

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channel_jni.open() Got ep %p %p\n", jobj, epData->jniJavaContext ); 

    /* XXX Destroy them in close */
    
    jmethod=(*jniEnv)->GetStaticMethodID(jniEnv, jniCh->jniBridge,
                                         "getBuffer",
                                         "(Ljava/lang/Object;I)[B");
    if( jmethod == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() can't find getBuffer\n"); 
        return JK_ERR;
    }

    epData->jarray=(*jniEnv)->CallStaticObjectMethod( jniEnv, jniCh->jniBridge,
                                                      jmethod,
                                                      epData->jniJavaContext, 0);

    epData->jarray=(*jniEnv)->NewGlobalRef( jniEnv, epData->jarray );

    epData->arrayLen = (*jniEnv)->GetArrayLength( jniEnv, epData->jarray );
    
    /* XXX > ajp buffer size. Don't know how to fragment or reallocate
       yet */
    epData->carray=(char *)endpoint->pool->calloc( env, endpoint->pool,
                                                   epData->arrayLen);

    jniCh->writeMethod =
        (*jniEnv)->GetStaticMethodID(jniEnv, jniCh->jniBridge,
                                     "jniInvoke",
                                     "(JLjava/lang/Object;)I");
    
    if( jniCh->writeMethod == NULL ) {
	env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "channel_jni.open() can't find jniInvoke\n"); 
        return JK_ERR;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channel_jni.open() found write method, open ok\n" ); 

    
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

    epData=(jk_ch_jni_ep_private_t *)endpoint->channelData;
    
    /* (*jniEnv)->DeleteGlobalRef( jniEnv, epData->msgJ ); */
    /*     (*jniEnv)->DeleteGlobalRef( jniEnv, epData->jniJavaContext ); */
    
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
//    int sd;
    int  sent=0;
    char *b;
    int len;
    jbyte *nbuf;
    jbyteArray jbuf;
    int jlen;
    jboolean iscommit=0;
    JNIEnv *jniEnv;
    jk_channel_jni_private_t *jniCh=_this->_privatePtr;
    jk_ch_jni_ep_private_t *epData=
        (jk_ch_jni_ep_private_t *)endpoint->channelData;;

    env->l->jkLog(env, env->l, JK_LOG_INFO,"channel_jni.send() %p\n", epData ); 

    if( epData == NULL ) {
        jk2_channel_jni_open( env, _this, endpoint );
        epData=(jk_ch_jni_ep_private_t *)endpoint->channelData;
    }
    if( epData == NULL || epData->jniJavaContext == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,"channel_jni.send() no java context\n" ); 
        
        return JK_ERR;
    }

    msg->end( env, msg );
    len=msg->len;
    b=msg->buf;

    env->l->jkLog(env, env->l, JK_LOG_INFO,"channel_jni.send() (1) %p\n", epData ); 

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

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channel_jni.send() getting byte array \n" );
    
    /* Copy the data in the ( recycled ) jbuf, then call the
     *  write method. XXX We could try 'pining' if the vm supports
     *  it, this is a looong lived object.
     */
    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, jbuf, &iscommit);

    if(nbuf==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelJni.send() Can't get java bytes");
        return JK_ERR;
    }

    if( len > jlen ) {
        /* XXX Reallocate the buffer */
        len=jlen;
    }

    memcpy( nbuf, b, len );

    (*jniEnv)->ReleaseByteArrayElements(jniEnv, jbuf, nbuf, 0);
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channel_jni.send() before send %p\n",
                  (void *)(long)epData->jniJavaContext); 
    
    sent=(*jniEnv)->CallStaticIntMethod( jniEnv,
                                         jniCh->jniBridge, 
                                         jniCh->writeMethod,
                                         (jlong)(long)(void *)env,
                                         epData->jniJavaContext );
    env->l->jkLog(env, env->l, JK_LOG_INFO,"channel_jni.send() result %d\n",
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
//    jbyte *nbuf;
//    jbyteArray jbuf;
//    int jlen;
//    jboolean iscommit;    
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
    JNIEnv *jniEnv;
    jint rc;
    jk_workerEnv_t *we=worker->workerEnv;

    env->l->jkLog(env, env->l, JK_LOG_INFO, "service() attaching to vm\n");


    jniEnv=(JNIEnv *)endpoint->endpoint_private;
    if(jniEnv==NULL) { /*! attached */
        /* Try to attach */
        if( we->vm == NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR, "No VM to use\n");
            return JK_ERR;
        }
        jniEnv = we->vm->attach(env, we->vm);
            
        if(jniEnv == NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR, "Attach failed\n");  
            /*   Is it recoverable ?? - yes, don't change the previous value*/
            /*   r->is_recoverable_error = JK_OK; */
            return JK_ERR;
        } 
        endpoint->endpoint_private = jniEnv;
    }
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

    /* XXX Don't detach if worker is reused per thread */
    endpoint->endpoint_private=NULL;
    if( we==NULL || we->vm==NULL ) {
        return JK_OK;
    }
    /* we->vm->detach( env, we->vm );  */
    
    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "channelJni.afterRequest() ok\n");
    return JK_OK;
}

/** Called by java. Will take the rest of the message and dispatch again to the real target.
 */
static int JK_METHOD jk2_channel_jni_dispatch(jk_env_t *env, void *target, jk_endpoint_t *ep, jk_msg_t *msg)
{
    jk_bean_t *jniChB=(jk_bean_t *)target;
    jk_channel_t *jniCh=(jk_channel_t *)jniChB->object;
    int code;
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,"channelJni.java2cInvoke() ok\n");

    code = (int)msg->getByte(env, msg);
    return ep->worker->workerEnv->dispatch( env, ep->worker->workerEnv,
                                            ep->currentRequest, ep, code, ep->reply );
}

int JK_METHOD jk2_channel_jni_factory(jk_env_t *env, jk_pool_t *pool, 
                                      jk_bean_t *result,
                                      const char *type, const char *name)
{
    jk_channel_t *ch=result->object;
    jk_workerEnv_t *wEnv;
    
    ch=(jk_channel_t *)pool->calloc(env, pool, sizeof( jk_channel_t));
    
    ch->recv= jk2_channel_jni_recv;
    ch->send= jk2_channel_jni_send; 
    ch->init= jk2_channel_jni_init; 
    ch->open= jk2_channel_jni_open; 
    ch->close= jk2_channel_jni_close; 

    ch->beforeRequest= jk2_channel_jni_beforeRequest;
    ch->afterRequest= jk2_channel_jni_afterRequest;
    
    ch->_privatePtr=(jk_channel_jni_private_t *)pool->calloc(env, pool,
                    sizeof(jk_channel_jni_private_t));
    ch->is_stream=JK_FALSE;

    result->setAttribute= jk2_channel_jni_setProperty;
    ch->mbean=result;
    result->object= ch;

    wEnv=env->getByName( env, "workerEnv" );
    ch->workerEnv=wEnv;
    wEnv->addChannel( env, wEnv, ch );

    wEnv->registerHandler( env, wEnv, type,
                           "sendResponse", JK_HANDLE_JNI_DISPATCH,
                           jk2_channel_jni_dispatch, NULL );

    
    return JK_OK;
}
