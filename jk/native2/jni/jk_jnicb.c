


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

/***************************************************************************
 * Description: JNI callbacks implementation for the JNI in process adapter*
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_jnicb.h"
#include "jk_service.h"
#include "jk_pool.h"
#include "jk_env.h"

/*
 * Class:     org_apache_tomcat_modules_server_JNIConnectionHandler
 * Method:    getNumberOfHeaders
 * Signature: (JJ)I
 */
JNIEXPORT jint JNICALL 
Java_org_apache_tomcat_modules_server_JNIConnectionHandler_getNumberOfHeaders
  (JNIEnv *jniEnv, jobject o, jlong s, jlong l)
{
    /* [V] Convert indirectly from jlong -> int -> pointer to shut up gcc */
    /*     I hope it's okay on other compilers and/or machines...         */
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_env_t *env = (jk_env_t *)(int)l;
    int cnt;
    
    if(!ps) {
	env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "JNIConnectionHandler.getNumberOfHeaders() NullPointerException\n");
	/* [V] JNIConnectionHandler doesn't handle this */
	return -1;
    }

    cnt=ps->headers_in->size( env, ps->headers_in );
    
    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "JNIConnectionHandler.getNumberOfHeaders() %d headers\n", cnt);
    return (jint)cnt;
}

/*
 * Class:     org_apache_tomcat_modules_server_JNIConnectionHandler
 * Method:    read
 * Signature: (JJ[BII)I
 */
JNIEXPORT jint JNICALL 
Java_org_apache_tomcat_modules_server_JNIConnectionHandler_read
  (JNIEnv *jniEnv, jobject o, jlong s, jlong l, jbyteArray buf, jint from, jint cnt)
{
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_env_t *env = (jk_env_t *)(int)l;
    jint rc = -1;
    jboolean iscommit;
    jbyte *nbuf;
    unsigned nfrom = (unsigned)from;
    unsigned ncnt = (unsigned)cnt;
    unsigned acc = 0;

    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "Into JNIConnectionHandler::read\n");

    if(!ps) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "jkjni.read() NULL ws service object\n");
	return -1;
    }

    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, buf, &iscommit);

    if(!nbuf) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "In JNIConnectionHandler::read, GetByteArrayElements error\n");
	return -1;
    }
    
    if(!ps->read(env, ps, nbuf + nfrom, ncnt, &acc)) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "jkjni.read() failed to read from web server\n");
    } else {
        rc = (jint)acc;
    }

    (*jniEnv)->ReleaseByteArrayElements(jniEnv, buf, nbuf, 0);

    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "Done JNIConnectionHandler::read\n");
    return rc;
}

/*
 * Class:     org_apache_tomcat_modules_server_JNIConnectionHandler
 * Method:    readEnvironment
 * Signature: (JJ[Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL 
Java_org_apache_tomcat_modules_server_JNIConnectionHandler_readEnvironment
  (JNIEnv *jniEnv, jobject o, jlong s, jlong l, jobjectArray envbuf)
{
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_env_t *env = (jk_env_t *)(int)l;
    char port[10];

    env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                  "Into JNIConnectionHandler::readEnvironment. Environment follows --->\n");

    if(!ps) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
               "In JNIConnectionHandler::readEnvironment, NULL ws service object\n");
	return JK_FALSE;
    }

    sprintf(port, "%d", ps->server_port);
        
    if(ps->method) {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                         envbuf, 
                                         0, 
                                         (*jniEnv)->NewStringUTF(jniEnv, ps->method));
    }
    if(ps->req_uri) {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                      envbuf, 
                                      1, 
                                      (*jniEnv)->NewStringUTF(jniEnv, ps->req_uri));
	env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "---> req_uri: %s\n", ps->req_uri);
    }
    if(ps->query_string) {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                      envbuf, 
                                      2, 
                                      (*jniEnv)->NewStringUTF(jniEnv, ps->query_string));
    }
    if(ps->remote_addr) {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                      envbuf, 
                                      3, 
                                      (*jniEnv)->NewStringUTF(jniEnv, ps->remote_addr));
    }
    if(ps->remote_host) {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                      envbuf, 
                                      4, 
                                      (*jniEnv)->NewStringUTF(jniEnv, ps->remote_host));
    }
    if(ps->server_name) {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                      envbuf, 
                                      5, 
                                      (*jniEnv)->NewStringUTF(jniEnv, ps->server_name));
    }

    (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                     envbuf, 
                                     6, 
                                     (*jniEnv)->NewStringUTF(jniEnv, port));

    if(ps->auth_type) {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                      envbuf, 
                                      7, 
                                      (*jniEnv)->NewStringUTF(jniEnv, ps->auth_type));
    }
    if(ps->remote_user) {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                         envbuf, 
                                         8, 
                                         (*jniEnv)->NewStringUTF(jniEnv, ps->remote_user));
    }
    if(ps->is_ssl) {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                         envbuf, 
                                         9, 
                                         (*jniEnv)->NewStringUTF(jniEnv, "https"));
    } else {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                         envbuf, 
                                         9, 
                                         (*jniEnv)->NewStringUTF(jniEnv, "http"));
    }

    if(ps->protocol) {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                      envbuf, 
                                      10, 
                                      (*jniEnv)->NewStringUTF(jniEnv, ps->protocol));
    }
    if(ps->server_software) {
        (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                      envbuf, 
                                      11, 
                                      (*jniEnv)->NewStringUTF(jniEnv, ps->server_software));
    }
    if(ps->is_ssl) {
        if(ps->ssl_cert) {
            (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                          envbuf, 
                                          12, 
                                          (*jniEnv)->NewStringUTF(jniEnv, ps->ssl_cert));
        }
        
        if(ps->ssl_cipher) {
            (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                          envbuf, 
                                          13, 
                                          (*jniEnv)->NewStringUTF(jniEnv, ps->ssl_cipher));
        }

        if(ps->ssl_session) {
            (*jniEnv)->SetObjectArrayElement(jniEnv, 
                                          envbuf, 
                                          14, 
                                          (*jniEnv)->NewStringUTF(jniEnv, ps->ssl_session));
        }
    }

    env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                  "Done JNIConnectionHandler::readEnvironment\n");


    return JK_TRUE;
}

/*
 * Class:     org_apache_tomcat_modules_server_JNIConnectionHandler
 * Method:    readHeaders
 * Signature: (JJ[Ljava/lang/String;[Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL 
Java_org_apache_tomcat_modules_server_JNIConnectionHandler_readHeaders
  (JNIEnv *jniEnv, jobject o, jlong s, jlong l, jobjectArray hnames, jobjectArray hvalues)
{
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_env_t *env = (jk_env_t *)(int)l;
    int i;
    int cnt;

    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "Into JNIConnectionHandler::readHeaders\n");

    if(!ps) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "In JNIConnectionHandler::readHeaders, NULL ws service object\n");
        return JK_FALSE;
    }

    cnt=ps->headers_in->size( env, ps->headers_in );
    
    for(i = 0 ; i < cnt ; i++) {
        char *name= ps->headers_in->nameAt( env, ps->headers_in, i);
        char *val=ps->headers_in->valueAt( env, ps->headers_in, i);
        (*jniEnv)->SetObjectArrayElement(jniEnv, hnames, i, 
                                         (*jniEnv)->NewStringUTF(jniEnv, name));
        (*jniEnv)->SetObjectArrayElement(jniEnv, hvalues, i, 
                                         (*jniEnv)->NewStringUTF(jniEnv, val));
    }
    env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                  "JNIConnectionHandler.readHeaders() created %d headers\n",
                  cnt);

    return JK_TRUE;
}

/*
 * Class:     org_apache_tomcat_modules_server_JNIConnectionHandler
 * Method:    startReasponse
 * Signature: (JJILjava/lang/String;[Ljava/lang/String;[Ljava/lang/String;I)I
 */
JNIEXPORT jint JNICALL 
Java_org_apache_tomcat_modules_server_JNIConnectionHandler_startReasponse
  (JNIEnv *jniEnv, jobject o, jlong s, jlong l, 
   jint sc, jstring msg, jobjectArray hnames, jobjectArray hvalues, jint hcnt)
{
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_env_t *env = (jk_env_t *)(int)l;
    const char *nmsg = NULL;
    char **nhnames = NULL;
    char **nhvalues = NULL;
    jstring *shnames = NULL;
    jstring *shvalues = NULL;
    int i = 0;
    int ok = JK_TRUE;

    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "Into JNIConnectionHandler::startReasponse\n");

    if(!ps) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "In JNIConnectionHandler::startReasponse, NULL ws service object\n");
	return JK_FALSE;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "jkJni.head() status=%d headers=%d --->\n",
                  sc, hcnt);

    if(hcnt) {
        ok = JK_FALSE;

        for( ; i < hcnt ; i++) {
            jboolean iscommit;
            
            char *name;
            char *val;
            char *name1;
            char *val1;
            jstring jname;
            jstring jval;
            
            shvalues[i] = shnames[i] = NULL;
            nhnames[i] = nhvalues[i] = NULL;
            
            jname = (*jniEnv)->GetObjectArrayElement(jniEnv, hnames, i);
            jval = (*jniEnv)->GetObjectArrayElement(jniEnv, hvalues, i);

            if(jname==NULL || jval==NULL) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                              "In JNIConnectionHandler::startReasponse, GetObjectArrayElement error\n");
                break;
            }

            name = (char *)(*jniEnv)->GetStringUTFChars(jniEnv, jname, &iscommit);
            val = (char *)(*jniEnv)->GetStringUTFChars(jniEnv, jval, &iscommit);

            /* Take ownership, the Java object will not be valid long */
            name1 = ps->pool->pstrdup( env, ps->pool, name );
            val1 = ps->pool->pstrdup( env, ps->pool, val );
            
            (*jniEnv)->ReleaseStringUTFChars(jniEnv, jname, name);
            (*jniEnv)->ReleaseStringUTFChars(jniEnv, jval, val);

            if(name==NULL || val==NULL ) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                              "In JNIConnectionHandler::startReasponse, GetStringUTFChars error\n");
                break;
            }
		
            ps->headers_out->add(env, ps->headers_out, name1, val1 );
        }
        if(i == hcnt) {
            ok = JK_TRUE;
            env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                          "In JNIConnectionHandler::startReasponse. ----- End headers.\n",
                          hcnt);
        }
    }			        

    if(msg && ok) {
        jboolean iscommit;
        nmsg = (*jniEnv)->GetStringUTFChars(jniEnv, msg, &iscommit);
        if(!nmsg) {
            ok = JK_FALSE;
        }
        
        ps->msg=ps->pool->pstrdup( env, ps->pool, nmsg );
        
        if(nmsg) {
            (*jniEnv)->ReleaseStringUTFChars(jniEnv, msg, nmsg);
        }
    }

    ps->status=(int)sc;
    
    if(ok) {
        if(!ps->head(env, ps)) {
            ok = JK_FALSE;
            env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                          "In JNIConnectionHandler::startReasponse, servers startReasponse failed\n");
        }
    }

    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                  "Done JNIConnectionHandler::startReasponse.\n");

    return ok;
}

/*
 * Class:     org_apache_tomcat_modules_server_JNIConnectionHandler
 * Method:    write
 * Signature: (JJ[BII)I
 */
JNIEXPORT jint JNICALL 
Java_org_apache_tomcat_modules_server_JNIConnectionHandler_write
  (JNIEnv *jniEnv, jobject o, jlong s, jlong l, jbyteArray buf, jint from, jint cnt)
{
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_env_t *env = (jk_env_t *)(int)l;
    jint rc = JK_FALSE;   
    jboolean iscommit;
    jbyte *nbuf;
    unsigned nfrom = (unsigned)from;
    unsigned ncnt = (unsigned)cnt;

    if(!ps) {
    	env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "In JNIConnectionHandler::write, NULL ws service object\n");
	return JK_FALSE;
    }
    
    env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                  "In JNIConnectionHandler::write %d\n", cnt);

    nbuf = (*jniEnv)->GetByteArrayElements(jniEnv, buf, &iscommit);

    if(!nbuf) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "In JNIConnectionHandler::write, GetByteArrayElements error\n");
	return JK_FALSE;
    }

    if(!ps->write(env, ps, nbuf + nfrom, ncnt)) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "In JNIConnectionHandler::write, failed to write to the web server\n");
    } else {
        rc = (jint)JK_TRUE;
    }

    (*jniEnv)->ReleaseByteArrayElements(jniEnv, buf, nbuf, JNI_ABORT);

    return rc; 
}
