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
    JNIEnv *env;
    
} jk_ch_jni_ep_private_t;


int JK_METHOD jk_channel_jni_factory(jk_env_t *env, jk_pool_t *pool,
                                        void **result,
					const char *type, const char *name);


static int JK_METHOD jk_channel_jni_init(jk_env_t *env,
                                         jk_channel_t *_this, 
                                         jk_map_t *props,
                                         char *worker_name, 
                                         jk_worker_t *worker )
{
    int err;
    char *tmp;
    
    _this->worker=worker;
    _this->properties=props;

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "channel_jni.init():  %s\n", 
                  worker->name );

    return err;
}

/** Assume the jni-worker or someone else started
 *  tomcat and initialized what is needed.
 */
static int JK_METHOD jk_channel_jni_open(jk_env_t *env,
                                         jk_channel_t *_this,
                                         jk_endpoint_t *endpoint)
{
    jk_workerEnv_t *we=endpoint->worker->workerEnv;
    JNIEnv *jniEnv;

    jk_channel_jni_private_t *jniCh=_this->_privatePtr;
    
    /** XXX make it customizable */
    jniCh->className=JAVA_BRIDGE_CLASS_NAME;

    jniCh->vm=(jk_vm_t *)we->vm;

    jniEnv = (JNIEnv *)jniCh->vm->attach( env, jniCh->vm );
    if( jniEnv == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() can't attach\n" ); 
        return JK_FALSE;
    }
    
    jniCh->jniBridge =
        (*jniEnv)->FindClass(jniEnv, jniCh->className );

    if( jniCh->jniBridge == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channel_jni.open() can't find %s\n",jniCh->className ); 
        return JK_FALSE;
    }
    
     jniCh->writeMethod =
        (*jniEnv)->GetStaticMethodID(jniEnv, jniCh->jniBridge,
                                     "write", "(JJ)I");
    
    if( jniCh->writeMethod == NULL ) {
	env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "channel_jni.open() can't find write method\n"); 
        return JK_FALSE;
    }

    /* Don't detach ( XXX Need to find out when the thread is
     *  closing in order for this to work )
     */
    jniCh->vm->detach( env, jniCh->vm );
    return JK_TRUE;
}


/** 
 */
static int JK_METHOD jk_channel_jni_close(jk_env_t *env,jk_channel_t *_this,
                                             jk_endpoint_t *endpoint)
{
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
static int JK_METHOD jk_channel_jni_send(jk_env_t *env, jk_channel_t *_this,
                                            jk_endpoint_t *endpoint,
                                            char *b, int len) 
{
    int sd;
    int  sent=0;
    jbyte *nbuf;
    jbyteArray jbuf;
    int jlen;
    jboolean iscommit;
    
    jk_channel_jni_private_t *jniCh=_this->_privatePtr;

    JNIEnv *jniEnv=(JNIEnv *)endpoint->channelData;;
    if( jniEnv==NULL ) {
        /* Try first getEnv, then attach */
        jniEnv = (JNIEnv *)jniCh->vm->attach( env, jniCh->vm );
        if( jniEnv == NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "channel_jni.send() can't attach\n" ); 
            return JK_FALSE;
        }
    }

    /* Copy the data in the ( recycled ) jbuf, then call the
     *  write method. XXX We could try 'pining' if the vm supports
     *  it, this is a looong lived object.
     */
    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, jbuf, &iscommit);

    if(nbuf==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelJni.send() Can't get java bytes");
        return -1;
    }

    if( len > jlen ) {
        /* XXX Reallocate the buffer */
        len=jlen;
    }

    memcpy( nbuf, b, len );

    (*jniEnv)->ReleaseByteArrayElements(jniEnv, jbuf, nbuf, 0);
    
    sent=(*jniEnv)->CallStaticIntMethod( jniEnv,
                                         jniCh->jniBridge, 
                                         jniCh->writeMethod,
                                         jbuf,
                                         len);
    return sent;
}


/** receive len bytes.
 * @param sd  opened socket.
 * @param b   buffer to store the data.
 * @param len length to receive.
 * @return    -1: receive failed or connection closed.
 *            >0: length of the received data.
 * Was: tcp_socket_recvfull
 */
static int JK_METHOD jk_channel_jni_recv( jk_env_t *env, jk_channel_t *_this,
                                          jk_endpoint_t *endpoint,
                                          char *b, int len ) 
{
    jbyte *nbuf;
    jbyteArray jbuf;
    int jlen;
    jboolean iscommit;    
    jk_channel_jni_private_t *jniCh=_this->_privatePtr;

    JNIEnv *jniEnv=(JNIEnv *)endpoint->channelData;;
    if( jniEnv==NULL ) {
        /* Try first getEnv, then attach */
        jniEnv = (JNIEnv *)jniCh->vm->attach( env, jniCh->vm );
        if( jniEnv == NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "channel_jni.send() can't attach\n" ); 
            return JK_FALSE;
        }
    }

    /* Copy the data in the ( recycled ) jbuf, then call the
     *  write method. XXX We could try 'pining' if the vm supports
     *  it, this is a looong lived object.
     */
    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, jbuf, &iscommit);

    if(nbuf==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "channelJni.send() Can't get java bytes");
        return -1;
    }

    if( len > jlen ) {
        /* XXX Reallocate the buffer */
        len=jlen;
    }

    memcpy( b, nbuf, len );

    (*jniEnv)->ReleaseByteArrayElements(jniEnv, jbuf, nbuf, 0);
    
    return len;
}


int JK_METHOD jk_channel_jni_factory(jk_env_t *env,
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
    
    _this->recv= jk_channel_jni_recv; 
    _this->send= jk_channel_jni_send; 
    _this->init= jk_channel_jni_init; 
    _this->open= jk_channel_jni_open; 
    _this->close= jk_channel_jni_close; 

    _this->name="jni";

    _this->_privatePtr=(jk_channel_jni_private_t *)pool->calloc(env, pool,
                    sizeof(jk_channel_jni_private_t));
    
    *result= _this;
    
    return JK_TRUE;
}
