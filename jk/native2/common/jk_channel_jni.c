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
#define JAVA_BRIDGE_CLASS_NAME ("org/apache/jk/common/ChannelJni")


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
    
    jobject epJ;
    jobject msgJ;
} jk_ch_jni_ep_private_t;


int JK_METHOD jk2_channel_jni_factory(jk_env_t *env, jk_pool_t *pool,
                                      void **result,
                                      const char *type, const char *name);


static int JK_METHOD jk2_channel_jni_setProperty(jk_env_t *env,
                                                   jk_channel_t *_this, 
                                                   char *name, char *value)
{
    return JK_TRUE;
}

static int JK_METHOD jk2_channel_jni_init(jk_env_t *env,
                                          jk_channel_t *_this)
{
    /* the channel is init-ed during a worker validation. If a jni worker
       is not already defined... well, not good. But on open we should
       have it.
    */
    env->l->jkLog(env, env->l, JK_LOG_INFO,"channel_jni.init():  %s\n", 
                  _this->worker->name );

    return JK_TRUE;
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

    jk_channel_jni_private_t *jniCh=_this->_privatePtr;

    if( endpoint->channelData != NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() already open, nothing else to do\n"); 
        return JK_TRUE;
    }
    
    jniCh->vm=(jk_vm_t *)we->vm;

    jniEnv = (JNIEnv *)jniCh->vm->attach( env, jniCh->vm );
    if( jniEnv == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() can't attach\n" ); 
        return JK_FALSE;
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
        return JK_FALSE;
    }

    jmethod=(*jniEnv)->GetStaticMethodID(jniEnv, jniCh->jniBridge,
                 "createEndpointStatic", "(JJ)Lorg/apache/jk/core/Endpoint;");
    if( jmethod == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() can't find createEndpointStatic\n"); 
        return JK_FALSE;
    }
    jobj=(*jniEnv)->CallStaticObjectMethod( jniEnv, jniCh->jniBridge,
                                                   jmethod,
                                                   (jlong)(long)(void *)env,
                                               (jlong)(long)(void *)endpoint );
    epData->epJ=(*jniEnv)->NewGlobalRef( jniEnv, jobj );

    jmethod=(*jniEnv)->GetStaticMethodID(jniEnv, jniCh->jniBridge,
                                         "createMessage",
                                         "()Lorg/apache/jk/common/MsgAjp;");
    if( jmethod == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() can't find createMessage\n"); 
        return JK_FALSE;
    }
    jobj=(*jniEnv)->CallStaticObjectMethod( jniEnv, jniCh->jniBridge,
                                            jmethod );
    epData->msgJ=(*jniEnv)->NewGlobalRef( jniEnv, jobj );

    /* XXX Destroy them in close */
    
    jmethod=(*jniEnv)->GetStaticMethodID(jniEnv, jniCh->jniBridge,
                                         "getBuffer",
                                         "(Lorg/apache/jk/common/MsgAjp;)[B");
    if( jmethod == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() can't find getBuffer\n"); 
        return JK_FALSE;
    }
    epData->jarray=(*jniEnv)->CallStaticObjectMethod( jniEnv, jniCh->jniBridge,
                                                   jmethod, epData->msgJ );
    /*epData->jarray=(*jniEnv)->NewByteArray(jniEnv, 10000 ); */
    epData->jarray=(*jniEnv)->NewGlobalRef( jniEnv, epData->jarray );

    epData->arrayLen = (*jniEnv)->GetArrayLength( jniEnv, epData->jarray );
    
    /* XXX > ajp buffer size. Don't know how to fragment or reallocate
       yet */
    epData->carray=(char *)endpoint->pool->calloc( env, endpoint->pool,
                                                   epData->arrayLen);

    jniCh->writeMethod =
        (*jniEnv)->GetStaticMethodID(jniEnv, jniCh->jniBridge,
                                     "receiveRequest",
                                     "(JJLorg/apache/jk/core/Endpoint;"
                                     "Lorg/apache/jk/common/MsgAjp;)I");
    
    if( jniCh->writeMethod == NULL ) {
	env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "channel_jni.open() can't find write method\n"); 
        return JK_FALSE;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channel_jni.open() found write method, open ok\n" ); 

    
    /* Don't detach ( XXX Need to find out when the thread is
     *  closing in order for this to work )
     */
    /*     jniCh->vm->detach( env, jniCh->vm ); */
    return JK_TRUE;
}


/** 
 */
static int JK_METHOD jk2_channel_jni_close(jk_env_t *env,jk_channel_t *_this,
                                           jk_endpoint_t *endpoint)
{
    jk_ch_jni_ep_private_t *epData;

    epData=(jk_ch_jni_ep_private_t *)endpoint->channelData;
    
    /* (*jniEnv)->DeleteGlobalRef( jniEnv, epData->msgJ ); */
    /*     (*jniEnv)->DeleteGlobalRef( jniEnv, epData->epJ ); */
    
    return JK_TRUE;

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
    int sd;
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

    env->l->jkLog(env, env->l, JK_LOG_INFO,"channel_jni.send()\n" ); 

    if( epData == NULL ) {
        jk2_channel_jni_open( env, _this, endpoint );
        epData=(jk_ch_jni_ep_private_t *)endpoint->channelData;
    }

    msg->end( env, msg );
    len=msg->len;
    b=msg->buf;

    jniEnv=NULL; /* epData->jniEnv; */
    jbuf=epData->jarray;

    if( jniCh->writeMethod == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "channel_jni.send() no write method\n" ); 
        return JK_FALSE;
    }
    if( jniEnv==NULL ) {
        /* Try first getEnv, then attach */
        jniEnv = (JNIEnv *)jniCh->vm->attach( env, jniCh->vm );
        if( jniEnv == NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "channel_jni.send() can't attach\n" ); 
            return JK_FALSE;
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
        return JK_FALSE;
    }

    if( len > jlen ) {
        /* XXX Reallocate the buffer */
        len=jlen;
    }

    memcpy( nbuf, b, len );

    (*jniEnv)->ReleaseByteArrayElements(jniEnv, jbuf, nbuf, 0);
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channel_jni.send() before send %p %p\n",
                  (void *)(long)epData->epJ, 
                  (void *)(long)epData->msgJ ); 

    sent=(*jniEnv)->CallStaticIntMethod( jniEnv,
                                         jniCh->jniBridge, 
                                         jniCh->writeMethod,
                                         (jlong)(long)(void *)env,
                             (jlong)(long)(void *)endpoint->currentRequest,
                                         epData->epJ,
                                         epData->msgJ);
    env->l->jkLog(env, env->l, JK_LOG_INFO,"channel_jni.send() result %d\n",
                  sent); 
    return JK_TRUE;
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
    jbyte *nbuf;
    jbyteArray jbuf;
    int jlen;
    jboolean iscommit;    
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

/* Process a message from java. We return in all cases,
   with the response message if any. 
*/
static int jk2_channel_jni_processMsg(jk_env_t *env, jk_endpoint_t *e,
                                      jk_ws_service_t *r)
{
    int code;
    jk_handler_t *handler;
    int rc;
    jk_handler_t **handlerTable=e->worker->workerEnv->handlerTable;
    int maxHandler=e->worker->workerEnv->lastMessageId;

    rc=-1;
    handler=NULL;

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channelJniNative.processMsg()\n");
    
    /* e->reply->dump(env, e->reply, "Received ");  */

    rc = e->worker->workerEnv->dispatch( env, e->worker->workerEnv, e, r );

    /* Process the status code returned by handler */
    switch( rc ) {
    case JK_HANDLER_RESPONSE:
        e->recoverable = JK_FALSE; 
        /* XXX response must be put back into the buffer
           rc = e->post->send(env, e->post, e ); */
        if (rc < 0) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                 "jni.processCallbacks() error sending response data\n");
            return JK_FALSE;
        }
        return JK_TRUE;
    case JK_HANDLER_ERROR:
        /*
         * we won't be able to gracefully recover from this so
         * set recoverable to false and get out.
         */
        e->recoverable = JK_FALSE;
        return JK_FALSE;
    case JK_HANDLER_FATAL:
        /*
         * Client has stop talking to us, so get out.
         * We assume this isn't our fault, so just a normal exit.
         * In most (all?)  cases, the ajp13_endpoint::reuse will still be
         * false here, so this will be functionally the same as an
         * un-recoverable error.  We just won't log it as such.
         */
        return JK_FALSE;
    default:
        /* All other cases */
        return JK_TRUE;
    }     
    
    /* not reached */
    return JK_FALSE;
}


/*
  
 */
int jk2_channel_jni_javaSendPacket(JNIEnv *jniEnv, jobject o,
                                   jlong envJ, jlong eP, jlong s,
                                   jbyteArray data, jint dataLen)
{
    /* [V] Convert indirectly from jlong -> int -> pointer to shut up gcc */
    /*     I hope it's okay on other compilers and/or machines...         */
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_env_t *env = (jk_env_t *)(long)envJ;
    jk_endpoint_t *e = (jk_endpoint_t *)(long)eP;
    int cnt=0;
    jint rc = -1;
    jboolean iscommit;
    jbyte *nbuf;
    unsigned acc = 0;
    int msgLen=(int)dataLen;

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channelJniNative.sendPacket()\n");
        
    if(!ps) {
	env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "channelJniNative.sendPacket() NullPointerException\n");
	return -1;
    }

    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, data, &iscommit);

    if(nbuf==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                    "channelJniNative.sendPacket() NullPointerException 2\n");
	return -1;
    }

    /* Simulate a receive on the incoming packet. e->reply is what's
     used when receiving data from java. This method is JAVA.sendPacket()
    and corresponds to CHANNELJNI.receive */
    e->currentData = nbuf;
    e->currentOffset=0;
    /* This was an workaround, no longer used ! */
    
    e->reply->reset(env, e->reply);

    memcpy( e->reply->buf, nbuf , msgLen );
    
    rc=e->reply->checkHeader( env, e->reply, e );
    if( rc < 0  ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "ajp14.service() Error reading reply\n");
        /* we just can't recover, unset recover flag */
        return JK_FALSE;
    }

    /* XXX check if the len in header matches our len */

    /* Now execute it */
    jk2_channel_jni_processMsg( env, e, ps );
    
    (*jniEnv)->ReleaseByteArrayElements(jniEnv, data, nbuf, 0);

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channelJniNative.sendPacket() done\n");

    return (jint)cnt;
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
            return JK_FALSE;
        }
        jniEnv = we->vm->attach(env, we->vm);
            
        if(jniEnv == NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR, "Attach failed\n");  
            /*   Is it recoverable ?? - yes, don't change the previous value*/
            /*   r->is_recoverable_error = JK_TRUE; */
            return JK_FALSE;
        } 
        endpoint->endpoint_private = jniEnv;
    }
    return JK_TRUE;
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
    we->vm->detach( env, we->vm ); 
    
    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "channelJni.afterRequest() ok\n");
    return JK_TRUE;
}



int JK_METHOD jk2_channel_jni_factory(jk_env_t *env,
                                     jk_pool_t *pool, 
                                     void **result,
                                     const char *type, const char *name)
{
    jk_channel_t *_this;
    
    if( strcmp( "channel", type ) != 0 ) {
	/* Wrong type  XXX throw */
	*result=NULL;
	return JK_FALSE;
    }
    _this=(jk_channel_t *)pool->calloc(env, pool, sizeof( jk_channel_t));
    
    _this->recv= jk2_channel_jni_recv;
    _this->setProperty= jk2_channel_jni_setProperty; 
    _this->send= jk2_channel_jni_send; 
    _this->init= jk2_channel_jni_init; 
    _this->open= jk2_channel_jni_open; 
    _this->close= jk2_channel_jni_close; 

    _this->beforeRequest= jk2_channel_jni_beforeRequest;
    _this->afterRequest= jk2_channel_jni_afterRequest;
    
    _this->name="jni";

    _this->_privatePtr=(jk_channel_jni_private_t *)pool->calloc(env, pool,
                    sizeof(jk_channel_jni_private_t));
    _this->is_stream=JK_FALSE;
    
    *result= _this;
    
    return JK_TRUE;
}
