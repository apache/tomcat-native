/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2003 The Apache Software Foundation.          *
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
 * In process JNI worker. It'll call an arbitrary java class's main method
 * with given parameters and set a pointer to the java vm in the workerEnv.
 * ( XXX and an optional set method to pass env, workerEnv pointers )
 * 
 * @author:  Gal Shachor <shachor@il.ibm.com>
 * @author: Costin Manolache
 */
#include "jk_workerEnv.h"
#include "jk_env.h"
#include "jk_bean.h"

#ifdef HAVE_JNI

#include "apr_thread_proc.h"

#include "jk_vm.h"
#include "jk_registry.h"
#include <jni.h>

extern jint jk_jni_aprImpl_registerNatives(JNIEnv *, jclass);
extern int jk_jni_status_code;

/* default only, will be  configurable  */
#define JAVA_BRIDGE_CLASS_NAME ("org/apache/jk/apr/TomcatStarter")
#define JAVA_BRIDGE_CLASS_APRI ("org/apache/jk/apr/AprImpl")

/* The class will be executed when the connector is started */
#define JK2_WORKER_HOOK_STARTUP     0
#define JK2_WORKER_HOOK_INIT        1
#define JK2_WORKER_HOOK_CLOSE       2
/* The class will be executed when the connector is about to be destroyed */
#define JK2_WORKER_HOOK_SHUTDOWN    3

struct jni_worker_data {
    jclass      jk_java_bridge_class;
    jclass      jk_java_bridge_apri_class;
    jmethodID   jk_main_method;
    jmethodID   jk_setout_method;
    jmethodID   jk_seterr_method;
    char *className;
    char *stdout_name;
    char *stderr_name;
    /* Hack to allow multiple 'options' for the class name */
    char **classNameOptions;
    char **args;
    int nArgs;
    int hook;
};

typedef struct jni_worker_data jni_worker_data_t;

/** -------------------- Startup -------------------- */

/** Static methods - much easier...
 */
static int jk2_get_method_ids(jk_env_t *env, jni_worker_data_t *p, JNIEnv *jniEnv )
{
    p->jk_main_method =
        (*jniEnv)->GetStaticMethodID(jniEnv, p->jk_java_bridge_class,
                                     "main", 
                                     "([Ljava/lang/String;)V");
    if(!p->jk_main_method) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, "Can't find main(String [])\n"); 
        return JK_ERR;
    }
    /* Only the startup hook can redirect the stdout/stderr */
    if (p->hook == JK2_WORKER_HOOK_STARTUP) {
        p->jk_setout_method =
        (*jniEnv)->GetStaticMethodID(jniEnv, p->jk_java_bridge_apri_class,
                                     "setOut", 
                                     "(Ljava/lang/String;)V");
        if(!p->jk_setout_method) {
            env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                          "Can't find AprImpl.setOut(String)"); 
            return JK_ERR;
        }

        p->jk_seterr_method =
        (*jniEnv)->GetStaticMethodID(jniEnv, p->jk_java_bridge_apri_class,
                                     "setErr", 
                                     "(Ljava/lang/String;)V");
        if(!p->jk_seterr_method) {
            env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                          "Can't find AprImpl.setErr(String)\n"); 
            return JK_ERR;
        }
    }

    return JK_OK;
}

static int JK_METHOD jk2_jni_worker_service(jk_env_t *env,
                                            jk_worker_t *w,
                                            jk_ws_service_t *s)
{
    return JK_ERR;
}


static int JK_METHOD jk2_jni_worker_setProperty(jk_env_t *env, jk_bean_t *mbean,
                                                char *name, void *valueP)
{
    jk_worker_t *_this=mbean->object;
    char *value=valueP;
    jni_worker_data_t *jniWorker;
    int mem_config = 0;
    
    if(! _this || ! _this->worker_private) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "In validate, assert failed - invalid parameters\n");
        return JK_ERR;
    }

    jniWorker = _this->worker_private;
    if (strcmp(mbean->name, "worker.jni:onStartup") == 0)
        jniWorker->hook = JK2_WORKER_HOOK_STARTUP;
    else if (strncmp(mbean->name, "worker.jni:onInit",
        sizeof("worker.jni:onInit") -1)== 0)
        jniWorker->hook = JK2_WORKER_HOOK_INIT;
    else if (strncmp(mbean->name, "worker.jni:onClose",
        sizeof("worker.jni:onClose") -1)== 0)
        jniWorker->hook = JK2_WORKER_HOOK_CLOSE;
    else if (strcmp(mbean->name, "worker.jni:onShutdown") == 0)
        jniWorker->hook = JK2_WORKER_HOOK_SHUTDOWN;

    if( strcmp( name, "stdout" )==0 ) {
        jniWorker->stdout_name = value;
    } else if( strcmp( name, "stderr" )==0 ) {
        jniWorker->stderr_name = value;
    } else if( strcmp( name, "class" )==0 ) {
        if( jniWorker->className != NULL ) {
            int i;
            for( i=0; i<4; i++ ) {
                if( jniWorker->classNameOptions[i]==NULL ) {
                    jniWorker->classNameOptions[i]=value;
                }
            } 
        } else {
            jniWorker->className = value;
        }
    } else if( strcmp( name, "ARG" )==0 ) {
        jniWorker->args[jniWorker->nArgs]=value;
        jniWorker->nArgs++;
    } else {
        return JK_ERR;
    }

    return JK_OK;
}

static int JK_METHOD jk2_jni_worker_init(jk_env_t *env, jk_bean_t *bean)
{
    jk_worker_t *_this=bean->object;
    jni_worker_data_t *jniWorker;
    JNIEnv *jniEnv;
    jstring cmd_line = NULL;
    jstring stdout_name = NULL;
    jstring stderr_name = NULL;
    jint rc = 0;
    char *str_config = NULL;
    jk_map_t *props;
    jk_vm_t *vm;
    jclass jstringClass;
    jarray jargs;
    int i=0;
    if(! _this || ! _this->worker_private) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "In init, assert failed - invalid parameters\n");
        return JK_ERR;
    }

    vm = _this->workerEnv->vm;
    if( vm == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerJni.init() No VM found\n");
        return JK_ERR;
    }
    
    /* Allow only the first child to execute the worker */
    if (_this->workerEnv->childId != 0) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "workerJni.Init() Skipping initialization for the %d %d\n",
                      _this->workerEnv->childId, 
                      _this->workerEnv->childProcessId);
        return JK_ERR;
    }

    props=_this->workerEnv->initData;
    jniWorker = _this->worker_private;
    
    if( jniWorker->className==NULL )
        jniWorker->className=JAVA_BRIDGE_CLASS_NAME;
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "jni.validate() class= %s \n",
                  jniWorker->className);

    jniEnv = (JNIEnv *)vm->attach( env, vm );

    if( jniEnv==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "workerJni.init() Can't attach to VM\n");
        return JK_ERR;
    }

    if(jniWorker->stdout_name) {
        stdout_name = (*jniEnv)->NewStringUTF(jniEnv, jniWorker->stdout_name);
    }
    if(jniWorker->stderr_name) {
        stderr_name = (*jniEnv)->NewStringUTF(jniEnv, jniWorker->stderr_name);
    }
    
    jniWorker->jk_java_bridge_class =
        (*jniEnv)->FindClass(jniEnv, jniWorker->className );

    if( jniWorker->jk_java_bridge_class == NULL ) {
        int i;
        for( i=0;i<4; i++ ) {
            if( jniWorker->classNameOptions[i] != NULL ) {
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                              "jni.validate() try %s \n",
                              jniWorker->classNameOptions[i]);
                jniWorker->jk_java_bridge_class =
                    (*jniEnv)->FindClass(jniEnv, jniWorker->classNameOptions[i] );
                if(jniWorker->jk_java_bridge_class != NULL )
                    break;
            }
        }
    }
    
    if( jniWorker->jk_java_bridge_class == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "Can't find class %s\n", jniWorker->className );
        /* [V] the detach here may segfault on 1.1 JVM... */
        vm->detach(env, vm);
        return JK_ERR;
    }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "Loaded %s\n", jniWorker->className);

/* Instead of loading mod_jk2.so from java, use the JNI RegisterGlobals.
 * XXX Need the way to customize JAVA_BRIDGE_CLASS_APRI, but since
 * it's hardcoded in JniHandler.java doesn't matter for now.
 */
    jniWorker->jk_java_bridge_apri_class =
        (*jniEnv)->FindClass(jniEnv, JAVA_BRIDGE_CLASS_APRI );

    if( jniWorker->jk_java_bridge_apri_class == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "Can't find class %s\n", JAVA_BRIDGE_CLASS_APRI );
        /* [V] the detach here may segfault on 1.1 JVM... */
        vm->detach(env, vm);
        return JK_ERR;
    }

    if (jniWorker->hook == JK2_WORKER_HOOK_STARTUP) {
        rc = jk_jni_aprImpl_registerNatives( jniEnv, jniWorker->jk_java_bridge_apri_class);
        if( rc != 0) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "Can't register native functions for %s \n", JAVA_BRIDGE_CLASS_APRI ); 
            vm->detach(env, vm);
            return JK_ERR;
        }
    }

    rc=jk2_get_method_ids(env, jniWorker, jniEnv);
    if( rc!=JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "jniWorker: Fail-> can't get method ids\n");
        /* [V] the detach here may segfault on 1.1 JVM... */
        vm->detach(env, vm);
        return rc;
    }
    /* Set out and err stadard files */ 
    if (jniWorker->stdout_name && jniWorker->jk_setout_method) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.init() setting stdout=%s...\n",jniWorker->stdout_name);
        (*jniEnv)->CallStaticVoidMethod(jniEnv,
                                        jniWorker->jk_java_bridge_apri_class,
                                        jniWorker->jk_setout_method,
                                        stdout_name);
    }
    
    if (jniWorker->stderr_name && jniWorker->jk_seterr_method) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.init() setting stderr=%s...\n",jniWorker->stderr_name);
        (*jniEnv)->CallStaticVoidMethod(jniEnv,
                                        jniWorker->jk_java_bridge_apri_class,
                                        jniWorker->jk_seterr_method,
                                        stderr_name);
    }
    
    if (jniWorker->hook < JK2_WORKER_HOOK_CLOSE) {
        jstringClass=(*jniEnv)->FindClass(jniEnv, "java/lang/String" );

        jargs=(*jniEnv)->NewObjectArray(jniEnv, jniWorker->nArgs, jstringClass, NULL);

        for( i=0; i<jniWorker->nArgs; i++ ) {
            jstring arg=NULL;
            if( jniWorker->args[i] != NULL ) {
                arg=(*jniEnv)->NewStringUTF(jniEnv, jniWorker->args[i] );
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                              "jni.init() ARG %s\n", jniWorker->args[i]);
            }
            (*jniEnv)->SetObjectArrayElement(jniEnv, jargs, i, arg );        
        }
    
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.init() calling main()...\n");
        (*jniEnv)->CallStaticVoidMethod(jniEnv,
                                        jniWorker->jk_java_bridge_class,
                                        jniWorker->jk_main_method,
                                        jargs);
    }
    else {
        /* XXX Is it right thing to disable all non init hooks 
         * or make that customizable.
         */
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.init() disabling the non init hook worker\n");
        _this->lb_disabled = JK_TRUE;
    }
#if 0
    vm->detach(env, vm);
#endif
    /* XXX create a jni channel */
    return JK_OK;
}

static int JK_METHOD jk2_jni_worker_destroy(jk_env_t *env, jk_bean_t *bean)
{
    jk_worker_t *_this=bean->object;
    jni_worker_data_t *jniWorker;
    jk_vm_t *vm;
    JNIEnv *jniEnv;
    jstring cmd_line = NULL;
    jstring stdout_name = NULL;
    jstring stderr_name = NULL;
    jclass jstringClass;
    jarray jargs;
    jstring arg=NULL;

    if(!_this  || ! _this->worker_private) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "In destroy, assert failed - invalid parameters\n");
        return JK_ERR;
    }
    vm = _this->workerEnv->vm;
    
    if( vm == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "jni.destroy() No VM found\n");
        return JK_ERR;
    }
    
    jniWorker = _this->worker_private;
    
    if (jniWorker->hook < JK2_WORKER_HOOK_CLOSE) {
        if( bean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "jni.destroy(), done...worker is not hooked for close\n");
        return JK_OK;
    }
    if (jniWorker->jk_java_bridge_class == NULL ||
        jniWorker->jk_main_method == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                       "jni.destroy(), done...worker does not have java methods\n");
        return JK_OK;
    }
    if((jniEnv = vm->attach(env, vm))) {
        int i;
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                       "jni.destroy(), shutting down Tomcat...\n");

        jstringClass=(*jniEnv)->FindClass(jniEnv, "java/lang/String" );
        jargs=(*jniEnv)->NewObjectArray(jniEnv, jniWorker->nArgs, jstringClass, NULL);
        for( i=0; i<jniWorker->nArgs; i++ ) {
            jstring arg=NULL;
            if( jniWorker->args[i] != NULL ) {
                arg=(*jniEnv)->NewStringUTF(jniEnv, jniWorker->args[i] );
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                                "jni.init() ARG %s\n", jniWorker->args[i]);
            }
            (*jniEnv)->SetObjectArrayElement(jniEnv, jargs, i, arg );        
        }

        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.destroy() calling main()...\n");

        jk_jni_status_code =0;
        (*jniEnv)->CallStaticVoidMethod(jniEnv,
                                    jniWorker->jk_java_bridge_class,
                                    jniWorker->jk_main_method,
                                    jargs);
        if (jniWorker->hook == JK2_WORKER_HOOK_SHUTDOWN) {
#if APR_HAS_THREADS
            while (jk_jni_status_code != 2) {
                apr_thread_yield();
            }
#endif
            vm->detach(env, vm);
        }
    }
    env->l->jkLog(env, env->l, JK_LOG_INFO, "jni.destroy() done\n");

    return JK_OK;
}

int JK_METHOD jk2_worker_jni_factory(jk_env_t *env, jk_pool_t *pool,
                                     jk_bean_t *result,
                                     const char *type, const char *name)
{
    jk_worker_t *_this;
    jni_worker_data_t *jniData;
    jk_workerEnv_t *wEnv;

    if(name==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                      "jni.factory() NullPointerException name==null\n");
        return JK_ERR;
    }

    wEnv = env->getByName( env, "workerEnv" );

    /* No singleton - you can have multiple jni workers,
     running different bridges or starting different programs inprocess*/

    _this=(jk_worker_t *)pool->calloc(env, pool, sizeof(jk_worker_t));
    
    jniData = (jni_worker_data_t *)pool->calloc(env, pool,
                                                sizeof(jni_worker_data_t ));
    if(_this==NULL || jniData==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "jni.factory() OutOfMemoryException \n");
        return JK_ERR;
    }

    _this->worker_private=jniData;

    jniData->jk_java_bridge_class  = NULL;
    jniData->classNameOptions=(char **)pool->calloc(env, pool, 4 * sizeof(char *));

    jniData->args = pool->calloc( env, pool, 64 * sizeof( char *));
    jniData->nArgs =0;
    jniData->stdout_name= NULL;
    jniData->stderr_name= NULL;

    result->init           = jk2_jni_worker_init;
    result->destroy        = jk2_jni_worker_destroy;
    _this->service = jk2_jni_worker_service;

    result->object = _this;
    result->setAttribute = jk2_jni_worker_setProperty;
    _this->mbean=result;

    _this->workerEnv = wEnv;
    _this->workerEnv->addWorker( env, _this->workerEnv, _this );

    return JK_OK;
}
#else

int JK_METHOD jk2_worker_jni_factory(jk_env_t *env, jk_pool_t *pool,
                                     jk_bean_t *result,
                                     const char *type, const char *name)
{
    result->disabled=1;
    
    return JK_OK;
}
     
#endif
