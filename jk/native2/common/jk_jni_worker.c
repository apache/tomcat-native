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
 * In process JNI worker                                      
 * @author:  Gal Shachor <shachor@il.ibm.com>
 * @author: Costin Manolache
 */

#include "jk_vm.h"
#include "jk_registry.h"
#include "jni.h"

/* default only, is configurable now */
#define JAVA_BRIDGE_CLASS_NAME ("org/apache/jk/common/ChannelJni")

struct jni_worker_data {

    jk_vm_t *vm;
    
    jclass      jk_java_bridge_class;

    jmethodID   jk_startup_method;
    jmethodID   jk_service_method;
    jmethodID   jk_shutdown_method;

    char *className;
    char *tomcat_cmd_line;
    char *stdout_name;
    char *stderr_name;
};

typedef struct jni_worker_data jni_worker_data_t;


/** Static methods - much easier...
 */
static int get_method_ids(jk_env_t *env, jni_worker_data_t *p, JNIEnv *jniEnv )
{

    p->jk_startup_method =
        (*jniEnv)->GetStaticMethodID(jniEnv, p->jk_java_bridge_class,
                                     "startup", 
               "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I");
    
    if(!p->jk_startup_method) {
	env->l->jkLog(env, env->l, JK_LOG_EMERG, "Can't find startup()\n"); 
	return JK_FALSE;
    }

    p->jk_service_method = (*jniEnv)->GetStaticMethodID(jniEnv,
                                                  p->jk_java_bridge_class, 
                                                  "service", 
                                               "(JJ)I");   
    if(!p->jk_service_method) {
	env->l->jkLog(env, env->l, JK_LOG_EMERG, "Can't find service()\n"); 
        return JK_FALSE;
    }
    
    p->jk_shutdown_method = (*jniEnv)->GetStaticMethodID(jniEnv,
                                                   p->jk_java_bridge_class, 
                                                   "shutdown", 
                                                   "()V");   
    if(!p->jk_shutdown_method) {
	env->l->jkLog(env, env->l, JK_LOG_EMERG, "Can't find shutdown()\n"); 
        return JK_FALSE;
    }    
    
    return JK_TRUE;
}

static int JK_METHOD jni_worker_service(jk_env_t *env, jk_endpoint_t *e,
                                        jk_ws_service_t *s,
                                        int *is_recoverable_error)
{
    JNIEnv *jniEnv;
    jint rc;
    jni_worker_data_t *jniWorker;
    
    if(!e || !s) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "jni.service() NullPointerException\n");
        return JK_FALSE;
    }

    if(!is_recoverable_error) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "service() unrecoverable error status\n");
        return JK_FALSE;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO, "service() attaching to vm\n");

    jniWorker=(jni_worker_data_t *)e->worker->worker_private;
    
    jniEnv=(JNIEnv *)e->endpoint_private;
    if(jniEnv==NULL) { /*! attached */
        /* Try to attach */
        jniEnv = jniWorker->vm->attach(env, jniWorker->vm);
            
        if(jniEnv == NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR, "Attach failed\n");  
            /*   Is it recoverable ?? */
            *is_recoverable_error = JK_TRUE;
            return JK_FALSE;
        } 
        e->endpoint_private = jniEnv;
    }

    /* we are attached now */

    /* 
     * When we call the JVM we cannot know what happens
     * So we can not recover !!!
     */
    *is_recoverable_error = JK_FALSE;
	    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "jni.service() calling Tomcat...\n");

    /* [V] For some reason gcc likes this pointer -> int -> jlong
       conversion, but not the direct pointer -> jlong conversion.
       I hope it's okay.  */
    rc = (*jniEnv)->CallStaticIntMethod( jniEnv,
                                         jniWorker->jk_java_bridge_class, 
                                         jniWorker->jk_service_method,
                                         (jlong)(int)s,
                                         (jlong)(int)env);

    /* [V] Righ now JNIEndpoint::service() only returns 1 or 0 */
    if(rc) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "service()  Tomcat returned OK, done\n");
        return JK_TRUE;
    } else {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "service() Tomcat FAILED!\n");
        return JK_FALSE;
    }
}

static int JK_METHOD jni_worker_done(jk_env_t *env, jk_endpoint_t *e)
{
    jni_worker_data_t *jniWorker;
    
    if(e==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "jni.done() NullPointerException\n");
        return JK_FALSE;
    }

    jniWorker=(jni_worker_data_t *)e->worker->worker_private;
    
    /* XXX Don't detach if worker is reused per thread */
    e->endpoint_private=NULL;
    jniWorker->vm->detach( env, jniWorker->vm ); 
    
    e->pool->close( env, e->pool );
    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "jni.done() ok\n");
    return JK_TRUE;
}


static int JK_METHOD jni_worker_validate(jk_env_t *env, jk_worker_t *pThis,
                                         jk_map_t *props, jk_workerEnv_t *we)
{
    jni_worker_data_t *jniWorker;
    int mem_config = 0;
    char *str_config = NULL;
    int rc;
    JNIEnv *jniEnv;
    char *prefix;

    if(! pThis || ! pThis->worker_private) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "In validate, assert failed - invalid parameters\n");
        return JK_FALSE;
    }

    jniWorker = pThis->worker_private;

    prefix=(char *)props->pool->alloc( env, props->pool,
                                       strlen( pThis->name ) + 10 );
    strcpy( prefix, "worker." );
    strcat( prefix, pThis->name );
    fprintf(stderr, "Prefix= %s\n", prefix );
    
    rc=jniWorker->vm->init(env, jniWorker->vm, props, prefix );
    if( rc!=JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "jni.validate() failed to load vm init params\n");
        return JK_FALSE;
    }
    
    jniWorker->className = jk_map_getStrProp( env, props, "worker",
                                              pThis->name,
                                              "class", JAVA_BRIDGE_CLASS_NAME);

    jniWorker->tomcat_cmd_line = jk_map_getStrProp( env, props, "worker",
                                                    pThis->name,
                                                    "cmd_line", NULL ); 

    jniWorker->stdout_name= jk_map_getStrProp( env, props, "worker",
                                               pThis->name, "stdout", NULL ); 
    jniWorker->stderr_name= jk_map_getStrProp( env, props, "worker",
                                               pThis->name, "stderr", NULL );
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "jni.validate() cmd: %s %s %s %s\n",
                  jniWorker->className, jniWorker->tomcat_cmd_line,
                  jniWorker->stdout_name, jniWorker->stderr_name);
    
    /* Verify if we can load the vm XXX do we want this now ? */
    
    rc= jniWorker->vm->load(env, jniWorker->vm );
    
    if( !rc ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "jni.validated() Error - can't load jvm dll\n");
        /* [V] no detach needed here */
        return JK_FALSE;
    }

    rc = jniWorker->vm->open(env, jniWorker->vm );
    
    if( rc== JK_FALSE ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, "Fail-> can't open jvm\n");
        /* [V] no detach needed here */
        return JK_FALSE;
    }

    jniEnv = (JNIEnv *)jniWorker->vm->attach( env, jniWorker->vm );
    
    jniWorker->jk_java_bridge_class =
        (*jniEnv)->FindClass(jniEnv, jniWorker->className );

    if( jniWorker->jk_java_bridge_class == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "Can't find class %s\n", str_config);
        /* [V] the detach here may segfault on 1.1 JVM... */
        jniWorker->vm->detach(env,  jniWorker->vm);
        return JK_FALSE;
    }
    
    env->l->jkLog(env, env->l, JK_LOG_EMERG,
                  "Loaded %s\n", jniWorker->className);

    rc=get_method_ids(env, jniWorker, jniEnv);
    if( !rc ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "Fail-> can't get method ids\n");
        /* [V] the detach here may segfault on 1.1 JVM... */
        jniWorker->vm->detach(env, jniWorker->vm);
        return JK_FALSE;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "jni.validate() ok\n");

    return JK_TRUE;
}

static int JK_METHOD jni_worker_init(jk_env_t *env, jk_worker_t *pThis,
                          jk_map_t *props, jk_workerEnv_t *we)
{
    jni_worker_data_t *jniWorker;
    JNIEnv *jniEnv;
    jstring cmd_line = NULL;
    jstring stdout_name = NULL;
    jstring stderr_name = NULL;
    jint rc = 0;
    

    if(! pThis || ! pThis->worker_private) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "In init, assert failed - invalid parameters\n");
        return JK_FALSE;
    }

    jniWorker = pThis->worker_private;

    if(pThis->workerEnv->vm != NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "jni.init(), done (been here!)\n");
        return JK_TRUE;
    }

    if(!jniWorker->vm ||
       !jniWorker->jk_service_method     ||
       !jniWorker->jk_startup_method     ||
       !jniWorker->jk_shutdown_method) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "Fail-> worker not set completely\n");
        return JK_FALSE;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO, "jni.init()\n");

    jniEnv=jniWorker->vm->attach(env, jniWorker->vm );
    if(jniEnv == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "jni.init() can't attach to vm\n");
        return JK_FALSE;
    }

    if(jniWorker->tomcat_cmd_line) {
        cmd_line = (*jniEnv)->NewStringUTF(jniEnv, jniWorker->tomcat_cmd_line);
    }
    if(jniWorker->stdout_name) {
        stdout_name = (*jniEnv)->NewStringUTF(jniEnv, jniWorker->stdout_name);
    }
    if(jniWorker->stdout_name) {
        stderr_name = (*jniEnv)->NewStringUTF(jniEnv, jniWorker->stderr_name);
    }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "jni.init() calling Tomcat to intialize itself...\n");
    
    rc = (*jniEnv)->CallStaticIntMethod(jniEnv,
                                        jniWorker->jk_java_bridge_class,
                                        jniWorker->jk_startup_method,
                                        cmd_line,
                                        stdout_name,
                                        stderr_name);
    
    jniWorker->vm->detach(env, jniWorker->vm); 

    pThis->workerEnv->vm= jniWorker->vm;
        
    if(rc) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "jni.init() Tomcat initialized OK, done\n");
        return JK_TRUE;
    } else {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                      "Fail-> could not initialize Tomcat\n");
        return JK_FALSE;
    }
}

static int JK_METHOD jni_worker_getEndpoint(jk_env_t *env, jk_worker_t *pThis,
                                            jk_endpoint_t **pend)
{
    /* [V] This slow, needs replacement */
    jk_pool_t *endpointPool;
    jk_endpoint_t *e;
    
    endpointPool=pThis->pool->create( env, pThis->pool, HUGE_POOL_SIZE );
    
    e = (jk_endpoint_t *)endpointPool->calloc(env, endpointPool,
                                              sizeof(jk_endpoint_t));

    if(!pThis || ! pThis->worker_private || !pend) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                      "jniWorker.getEndpoint() NullPointerException\n");
        return JK_FALSE;
    }
    
    if(e==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "jniWorker.getEndpoint() OutOfMemoryException\n");
        return JK_FALSE;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO, "jni.get_endpoint()\n");
    /** XXX Fix the spaghetti */

    e->worker = pThis;
    e->endpoint_private = NULL;
    e->service = jni_worker_service;
    e->done = jni_worker_done;
    e->channelData = NULL;
    e->pool=endpointPool;
    *pend = e;
    
    return JK_TRUE;
}

static int JK_METHOD jni_worker_destroy(jk_env_t *env, jk_worker_t *_this)
{
    jni_worker_data_t *jniWorker;
    JNIEnv *jniEnv;

    if(!_this  || ! _this->worker_private) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "In destroy, assert failed - invalid parameters\n");
        return JK_FALSE;
    }

    jniWorker = _this->worker_private;

    if(!jniWorker->vm) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "In destroy, JVM not intantiated\n");
        return JK_FALSE;
    }
    
    if(! jniWorker->jk_shutdown_method) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "In destroy, Tomcat not intantiated\n");
        return JK_FALSE;
    }

    if((jniEnv = jniWorker->vm->attach(env, jniWorker->vm))) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "jni.destroy(), shutting down Tomcat...\n");
        (*jniEnv)->CallStaticVoidMethod(jniEnv,
                                  jniWorker->jk_java_bridge_class,
                                  jniWorker->jk_shutdown_method);
        jniWorker->vm->detach(env, jniWorker->vm);
    }

    _this->pool->close(env, _this->pool);

    env->l->jkLog(env, env->l, JK_LOG_INFO, "jni.destroy() done\n");

    return JK_TRUE;
}

int JK_METHOD jk_worker_jni_factory(jk_env_t *env, jk_pool_t *pool,
                                    void **result,
                                    const char *type, const char *name)
{
    jk_worker_t *_this;
    jni_worker_data_t *jniData;

    if(name==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                      "jni.factory() NullPointerException name==null\n");
        return JK_FALSE;
    }

    /* No singleton - you can have multiple jni workers,
     running different bridges or starting different programs inprocess*/

    _this=(jk_worker_t *)pool->calloc(env, pool, sizeof(jk_worker_t));
    
    jniData = (jni_worker_data_t *)pool->calloc(env, pool,
                                                sizeof(jni_worker_data_t ));
    if(_this==NULL || jniData==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "jni.factory() OutOfMemoryException \n");
        return JK_FALSE;
    }

    _this->worker_private=jniData;

    _this->pool=pool;
    _this->name = _this->pool->pstrdup(env, _this->pool, name);

    /* XXX split it in VM11 and VM12 util */
    jk_vm_factory( env, pool, &jniData->vm, "vm", "default" );
    
    jniData->jk_java_bridge_class  = NULL;
    jniData->jk_startup_method     = NULL;
    jniData->jk_service_method     = NULL;
    jniData->jk_shutdown_method    = NULL;

    jniData->tomcat_cmd_line       = NULL;
    jniData->stdout_name           = NULL;
    jniData->stderr_name           = NULL;

    _this->validate       = jni_worker_validate;
    _this->init           = jni_worker_init;
    _this->get_endpoint   = jni_worker_getEndpoint;
    _this->destroy        = jni_worker_destroy;

    *result = _this;

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "jni.worker_factory() done\n");

    return JK_TRUE;
}

