/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/***************************************************************************
 * Description: JNI callbacks implementation for the JNI in process adapter*
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_jnicb.h"
#include "jk_service.h"
#include "jk_util.h"
#include "jk_pool.h"

/*
 * Class:     org_apache_tomcat_modules_server_JNIConnectionHandler
 * Method:    getNumberOfHeaders
 * Signature: (JJ)I
 */
JNIEXPORT jint JNICALL 
Java_org_apache_tomcat_modules_server_JNIConnectionHandler_getNumberOfHeaders
  (JNIEnv *env, jobject o, jlong s, jlong l)
{
    /* [V] Convert indirectly from jlong -> int -> pointer to shut up gcc */
    /*     I hope it's okay on other compilers and/or machines...         */
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_logger_t *pl = (jk_logger_t *)(int)l;

    jk_log(pl, JK_LOG_DEBUG, "Into JNIConnectionHandler::getNumberOfHeaders\n");

    if(!ps) {
	jk_log(pl, JK_LOG_ERROR, 
               "In JNIConnectionHandler::getNumberOfHeaders, NULL ws service object\n");
	/* [V] JNIConnectionHandler doesn't handle this */
	return -1;
    }

    jk_log(pl, JK_LOG_DEBUG,
	   "Done JNIConnectionHandler::getNumberOfHeaders, found %d headers\n",
	   ps->num_headers);
    return (jint)ps->num_headers;
}

/*
 * Class:     org_apache_tomcat_modules_server_JNIConnectionHandler
 * Method:    read
 * Signature: (JJ[BII)I
 */
JNIEXPORT jint JNICALL 
Java_org_apache_tomcat_modules_server_JNIConnectionHandler_read
  (JNIEnv *env, jobject o, jlong s, jlong l, jbyteArray buf, jint from, jint cnt)
{
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_logger_t *pl = (jk_logger_t *)(int)l;
    jint rc = -1;
    jboolean iscommit;
    jbyte *nbuf;
    unsigned nfrom = (unsigned)from;
    unsigned ncnt = (unsigned)cnt;
    unsigned acc = 0;

    jk_log(pl, JK_LOG_DEBUG, "Into JNIConnectionHandler::read\n");

    if(!ps) {
        jk_log(pl, JK_LOG_ERROR, 
               "In JNIConnectionHandler::read, NULL ws service object\n");
	return -1;
    }

    nbuf = (*env)->GetByteArrayElements(env, buf, &iscommit);

    if(!nbuf) {
        jk_log(pl, JK_LOG_ERROR, 
               "In JNIConnectionHandler::read, GetByteArrayElements error\n");
	return -1;
    }
    
    if(!ps->read(ps, nbuf + nfrom, ncnt, &acc)) {
        jk_log(pl, JK_LOG_ERROR, 
               "In JNIConnectionHandler::read, failed to read from web server\n");
    } else {
        rc = (jint)acc;
    }

    (*env)->ReleaseByteArrayElements(env, buf, nbuf, 0);

    jk_log(pl, JK_LOG_DEBUG, "Done JNIConnectionHandler::read\n");
    return rc;
}

/*
 * Class:     org_apache_tomcat_modules_server_JNIConnectionHandler
 * Method:    readEnvironment
 * Signature: (JJ[Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL 
Java_org_apache_tomcat_modules_server_JNIConnectionHandler_readEnvironment
  (JNIEnv *env, jobject o, jlong s, jlong l, jobjectArray envbuf)
{
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_logger_t *pl = (jk_logger_t *)(int)l;
    char port[10];

    jk_log(pl, JK_LOG_DEBUG, 
           "Into JNIConnectionHandler::readEnvironment. Environment follows --->\n");

    if(!ps) {
        jk_log(pl, JK_LOG_ERROR, 
               "In JNIConnectionHandler::readEnvironment, NULL ws service object\n");
	return JK_FALSE;
    }

    sprintf(port, "%d", ps->server_port);
        
    if(ps->method) {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      0, 
                                      (*env)->NewStringUTF(env, ps->method));
	jk_log(pl, JK_LOG_DEBUG, "---> method: %s\n", ps->method);
    }
    if(ps->req_uri) {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      1, 
                                      (*env)->NewStringUTF(env, ps->req_uri));
	jk_log(pl, JK_LOG_DEBUG, "---> req_uri: %s\n", ps->req_uri);
    }
    if(ps->query_string) {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      2, 
                                      (*env)->NewStringUTF(env, ps->query_string));
        jk_log(pl, JK_LOG_DEBUG, "---> query_string: %s\n", ps->query_string);
    }
    if(ps->remote_addr) {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      3, 
                                      (*env)->NewStringUTF(env, ps->remote_addr));
	jk_log(pl, JK_LOG_DEBUG, "---> remote_addr: %s\n", ps->remote_addr);
    }
    if(ps->remote_host) {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      4, 
                                      (*env)->NewStringUTF(env, ps->remote_host));
	jk_log(pl, JK_LOG_DEBUG, "---> remote_host: %s\n", ps->remote_host);
    }
    if(ps->server_name) {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      5, 
                                      (*env)->NewStringUTF(env, ps->server_name));
	jk_log(pl, JK_LOG_DEBUG, "---> server_name: %s\n", ps->server_name);
    }

    (*env)->SetObjectArrayElement(env, 
                                  envbuf, 
                                  6, 
                                  (*env)->NewStringUTF(env, port));
    jk_log(pl, JK_LOG_DEBUG, "---> server_port: %s\n", port);

    if(ps->auth_type) {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      7, 
                                      (*env)->NewStringUTF(env, ps->auth_type));
	jk_log(pl, JK_LOG_DEBUG, "---> auth_type: %s\n", ps->auth_type);
    }
    if(ps->remote_user) {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      8, 
                                      (*env)->NewStringUTF(env, ps->remote_user));
	jk_log(pl, JK_LOG_DEBUG, "---> remote_user: %s\n", ps->remote_user);
    }
    if(ps->is_ssl) {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      9, 
                                      (*env)->NewStringUTF(env, "https"));
    } else {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      9, 
                                      (*env)->NewStringUTF(env, "http"));
    }
    jk_log(pl, JK_LOG_DEBUG, "---> is_ssl: %s\n", ps->is_ssl ? "yes" : "no");

    if(ps->protocol) {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      10, 
                                      (*env)->NewStringUTF(env, ps->protocol));
        jk_log(pl, JK_LOG_DEBUG, "---> protocol: %s\n", ps->protocol);
    }
    if(ps->server_software) {
        (*env)->SetObjectArrayElement(env, 
                                      envbuf, 
                                      11, 
                                      (*env)->NewStringUTF(env, ps->server_software));
        jk_log(pl, JK_LOG_DEBUG, "---> server_software: %s\n", ps->server_software);
    }
    if(ps->is_ssl) {
        if(ps->ssl_cert) {
            (*env)->SetObjectArrayElement(env, 
                                          envbuf, 
                                          12, 
                                          (*env)->NewStringUTF(env, ps->ssl_cert));
            jk_log(pl, JK_LOG_DEBUG, "---> ssl_cert: %s\n", ps->ssl_cert);
        }
        
        if(ps->ssl_cipher) {
            (*env)->SetObjectArrayElement(env, 
                                          envbuf, 
                                          13, 
                                          (*env)->NewStringUTF(env, ps->ssl_cipher));
            jk_log(pl, JK_LOG_DEBUG, "---> ssl_cipher: %s\n", ps->ssl_cipher);
        }

        if(ps->ssl_session) {
            (*env)->SetObjectArrayElement(env, 
                                          envbuf, 
                                          14, 
                                          (*env)->NewStringUTF(env, ps->ssl_session));
            jk_log(pl, JK_LOG_DEBUG, "---> ssl_session: %s\n", ps->ssl_session);
        }
    }

    jk_log(pl, JK_LOG_DEBUG, 
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
  (JNIEnv *env, jobject o, jlong s, jlong l, jobjectArray hnames, jobjectArray hvalues)
{
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_logger_t *pl = (jk_logger_t *)(int)l;
    unsigned i;

    jk_log(pl, JK_LOG_DEBUG, "Into JNIConnectionHandler::readHeaders\n");

    if(!ps) {
        jk_log(pl, JK_LOG_ERROR, 
           "In JNIConnectionHandler::readHeaders, NULL ws service object\n");
	   return JK_FALSE;
    }

    jk_log(pl, JK_LOG_DEBUG, "In JNIConnectionHandler::readHeaders, %d headers follow --->\n",
	   ps->num_headers);

    for(i = 0 ; i < ps->num_headers ; i++) {
        (*env)->SetObjectArrayElement(env, 
                                      hnames, 
                                      i, 
                                      (*env)->NewStringUTF(env, ps->headers_names[i]));
        (*env)->SetObjectArrayElement(env, 
                                      hvalues, 
                                      i, 
                                      (*env)->NewStringUTF(env, ps->headers_values[i]));
	jk_log(pl, JK_LOG_DEBUG, "---> %s = %s\n",
	       ps->headers_names[i], ps->headers_values[i]);
    }
    jk_log(pl, JK_LOG_DEBUG, 
	   "Done JNIConnectionHandler::readHeaders\n");

    return JK_TRUE;
}

/*
 * Class:     org_apache_tomcat_modules_server_JNIConnectionHandler
 * Method:    startReasponse
 * Signature: (JJILjava/lang/String;[Ljava/lang/String;[Ljava/lang/String;I)I
 */
JNIEXPORT jint JNICALL 
Java_org_apache_tomcat_modules_server_JNIConnectionHandler_startReasponse
  (JNIEnv *env, jobject o, jlong s, jlong l, 
   jint sc, jstring msg, jobjectArray hnames, jobjectArray hvalues, jint hcnt)
{
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_logger_t *pl = (jk_logger_t *)(int)l;
    const char *nmsg = NULL;
    char **nhnames = NULL;
    char **nhvalues = NULL;
    jstring *shnames = NULL;
    jstring *shvalues = NULL;
    int i = 0;
    int ok = JK_TRUE;

    jk_log(pl, JK_LOG_DEBUG, "Into JNIConnectionHandler::startReasponse\n");

    if(!ps) {
        jk_log(pl, JK_LOG_ERROR, 
	       "In JNIConnectionHandler::startReasponse, NULL ws service object\n");
	return JK_FALSE;
    }

    jk_log(pl, JK_LOG_DEBUG, "In JNIConnectionHandler::startReasponse, %d headers follow --->\n",
	   hcnt);

    if(hcnt) {
        ok = JK_FALSE;

        nhnames = (char **)jk_pool_alloc(ps->pool, hcnt * sizeof(char *));
        nhvalues = (char **)jk_pool_alloc(ps->pool, hcnt * sizeof(char *));            
        shnames = (jstring *)jk_pool_alloc(ps->pool, hcnt * sizeof(jstring));
        shvalues = (jstring *)jk_pool_alloc(ps->pool, hcnt * sizeof(jstring));

        if(nhvalues && nhnames && shnames && shnames) {	    
            for( ; i < hcnt ; i++) {
                jboolean iscommit;

                shvalues[i] = shnames[i] = NULL;
                nhnames[i] = nhvalues[i] = NULL;

                shnames[i] = (*env)->GetObjectArrayElement(env, hnames, i);
                shvalues[i] = (*env)->GetObjectArrayElement(env, hvalues, i);

                if(!shvalues[i] || !shnames[i]) {
                    jk_log(pl, JK_LOG_ERROR, 
                           "In JNIConnectionHandler::startReasponse, GetObjectArrayElement error\n");
                    break;
                }

                nhnames[i] = (char *)(*env)->GetStringUTFChars(env, shnames[i], &iscommit);
                nhvalues[i] = (char *)(*env)->GetStringUTFChars(env, shvalues[i], &iscommit);

                if(!nhvalues[i] || !nhnames[i]) {
                    jk_log(pl, JK_LOG_ERROR, 
                           "In JNIConnectionHandler::startReasponse, GetStringUTFChars error\n");
                    break;
                }
		
		jk_log(pl, JK_LOG_DEBUG, "---> %s=%s\n", nhnames[i], nhvalues[i]);	
            }
            if(i == hcnt) {
                ok = JK_TRUE;
		jk_log(pl, JK_LOG_DEBUG, 
                       "In JNIConnectionHandler::startReasponse. ----- End headers.\n", hcnt);
            }
	} else {
	    jk_log(pl, JK_LOG_ERROR,
	           "In JNIConnectionHandler::startReasponse, memory allocation error\n");
	}
    }			        

    if(msg && ok) {
        jboolean iscommit;
        nmsg = (*env)->GetStringUTFChars(env, msg, &iscommit);
        if(!nmsg) {
            ok = JK_FALSE;
        }
    }

    if(ok) {
        if(!ps->start_response(ps, sc, nmsg, (const char**)nhnames, (const char**)nhvalues, hcnt)) {
            ok = JK_FALSE;
            jk_log(pl, JK_LOG_ERROR, 
                   "In JNIConnectionHandler::startReasponse, servers startReasponse failed\n");
        }
    }

    if(nmsg) {
        (*env)->ReleaseStringUTFChars(env, msg, nmsg);
    }
        
    if(i < hcnt) {
        i++;
    }

    if(nhvalues) {
        int j;

        for(j = 0 ; j < i ; j++) {
            if(nhvalues[j]) {
                (*env)->ReleaseStringUTFChars(env, shvalues[j], nhvalues[j]);
            }
        }
    }

    if(nhnames) {
        int j;

        for(j = 0 ; j < i ; j++) {
            if(nhnames[j]) {
                (*env)->ReleaseStringUTFChars(env, shnames[j], nhnames[j]);
            }
        }
    }
    jk_log(pl, JK_LOG_DEBUG, "Done JNIConnectionHandler::startReasponse.\n");

    return ok;
}

/*
 * Class:     org_apache_tomcat_modules_server_JNIConnectionHandler
 * Method:    write
 * Signature: (JJ[BII)I
 */
JNIEXPORT jint JNICALL 
Java_org_apache_tomcat_modules_server_JNIConnectionHandler_write
  (JNIEnv *env, jobject o, jlong s, jlong l, jbyteArray buf, jint from, jint cnt)
{
    jk_ws_service_t *ps = (jk_ws_service_t *)(int)s;
    jk_logger_t *pl = (jk_logger_t *)(int)l;
    jint rc = JK_FALSE;   
    jboolean iscommit;
    jbyte *nbuf;
    unsigned nfrom = (unsigned)from;
    unsigned ncnt = (unsigned)cnt;

    jk_log(pl, JK_LOG_DEBUG, 
           "In JNIConnectionHandler::write\n");

    if(!ps) {
    	jk_log(pl, JK_LOG_ERROR, 
	    "In JNIConnectionHandler::write, NULL ws service object\n");
	return JK_FALSE;
    }
    
    nbuf = (*env)->GetByteArrayElements(env, buf, &iscommit);

    if(!nbuf) {
        jk_log(pl, JK_LOG_ERROR, 
               "In JNIConnectionHandler::write, GetByteArrayElements error\n");
	return JK_FALSE;
    }

    if(!ps->write(ps, nbuf + nfrom, ncnt)) {
        jk_log(pl, JK_LOG_ERROR, 
                   "In JNIConnectionHandler::write, failed to write to the web server\n");
    } else {
        rc = (jint)JK_TRUE;
    }

    (*env)->ReleaseByteArrayElements(env, buf, nbuf, JNI_ABORT);

    return rc; 
}
