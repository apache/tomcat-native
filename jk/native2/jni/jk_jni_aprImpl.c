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

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_initialize(JNIEnv *env, jobject _jthis)
{
    apr_initialize();
    return 0;
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_terminate(JNIEnv *env, jobject _jthis)
{
    apr_terminate();
    return 0;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_poolCreate(JNIEnv *env, jobject _jthis, jlong parentP)
{
    apr_pool_t *parent;
    apr_pool_t *child;

    parent=(apr_pool_t *)(void *)(long)parentP;
    apr_pool_create( &child, parent );
    return (jlong)(long)child;
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_poolClear(JNIEnv *env, jobject _jthis,
                                         jlong poolP)
{
    apr_pool_t *pool;

    pool=(apr_pool_t *)(void *)poolP;
    apr_pool_clear( pool );
    return 0;
}

JNIEXPORT jint JNICALL 
Java_org_apache_jk_apr_AprImpl_signal(JNIEnv *env, jobject _jthis, jint signo,
                                      jobject func)
{

    return 0;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_userId(JNIEnv *env, jobject _jthis, jlong pool)
{

    return 0;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_shmInit(JNIEnv *env, jobject _jthis, jlong pool,
                                       jlong size, jstring file)
{

    return 0;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_shmDestroy(JNIEnv *env, jobject _jthis, jlong pool,
                                          jlong size, jstring file)
{

    return 0;
}


JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_socketCreate(JNIEnv *env, jobject _jthis, jint fam,
                                            jint type, jlong pool)
{

    return 0;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_socketBind(JNIEnv *env, jobject _jthis,
                                          jlong socket, jlong sa )
{

    return 0;
}

JNIEXPORT jlong JNICALL 
Java_org_apache_jk_apr_AprImpl_socketListen(JNIEnv *env, jobject _jthis,
                                            jlong socket, jint bl )
{

    return 0;
}









