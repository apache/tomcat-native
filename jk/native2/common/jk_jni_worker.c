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
 * Description: In process JNI worker                                      *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Based on:                                                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#if !defined(WIN32) && !defined(NETWARE)
#include <dlfcn.h>
#endif

#include <jni.h>

#include "jk_pool.h"
#include "jk_env.h"

#if defined LINUX && defined APACHE2_SIGHACK
#include <pthread.h>
#include <signal.h>
#include <bits/signum.h>
#endif

#ifdef NETWARE
#include <nwthread.h>
#include <nwadv.h>
#endif
#include "jk_logger.h"
#include "jk_service.h"

#ifndef JNI_VERSION_1_1
#define JNI_VERSION_1_1 0x00010001
#endif

#define null_check(e) if ((e) == 0) return JK_FALSE

jint (JNICALL *jni_get_default_java_vm_init_args)(void *) = NULL;
jint (JNICALL *jni_create_java_vm)(JavaVM **, JNIEnv **, void *) = NULL;
jint (JNICALL *jni_get_created_java_vms)(JavaVM **, int, int *) = NULL;

#define JAVA_BRIDGE_CLASS_NAME ("org/apache/tomcat/modules/server/JNIEndpoint")
/* #define JAVA_BRIDGE_CLASS_NAME ("org/apache/tomcat/service/JNIEndpoint")
 */

static jk_worker_t *the_singleton_jni_worker = NULL;

struct jni_worker {

    int was_verified;
    int was_initialized;

    jk_pool_t *pool;

    /*
     * JVM Object pointer.
     */
    JavaVM      *jvm;   

    /*
     * [V] JNIEnv used for boostraping from validate -> init w/o an attach
     */
    JNIEnv	*tmp_env;

    /*
     * Web Server to Java bridge, instance and class.
     */
    jobject     jk_java_bridge_object;
    jclass      jk_java_bridge_class;

    /*
     * Java methods ids, to jump into the JVM
     */
    jmethodID   jk_startup_method;
    jmethodID   jk_service_method;
    jmethodID   jk_shutdown_method;

    /*
     * Command line for tomcat startup
     */
    char *tomcat_cmd_line;

    /*
     * Classpath
     */
    char *tomcat_classpath;

    /*
     * Full path to the jni javai/jvm dll
     */
    char *jvm_dll_path;

    /*
     * Initial Java heap size
     */
    unsigned tomcat_ms;

    /*
     * Max Java heap size
     */
    unsigned tomcat_mx;

    /*
     * Java system properties
     */
    char **sysprops;

#ifdef JNI_VERSION_1_2
    /*
     * Java 2 initialization options (-X... , -verbose etc.)
     */
    char **java2opts;

    /*
     * Java 2 lax/strict option checking (bool)
     */    
    int java2lax;
#endif

    /*
     * stdout and stderr file names for Java
     */
    char *stdout_name;
    char *stderr_name;

    char *name; 
    jk_worker_t worker;
};
typedef struct jni_worker jni_worker_t;

struct jni_endpoint {    
    int attached;
    JNIEnv *env;
    jni_worker_t *worker;
    
    jk_endpoint_t endpoint;
};
typedef struct jni_endpoint jni_endpoint_t;

int JK_METHOD jk_worker_jni_factory(jk_env_t *env, jk_pool_t *pool, void **result,
                                    char *type, char *name);

static int load_jvm_dll(jni_worker_t *p,
                        jk_logger_t *l);

static int open_jvm(jni_worker_t *p,
                    JNIEnv **env,
                    jk_logger_t *l);

static int open_jvm1(jni_worker_t *p,
                    JNIEnv **env,
                    jk_logger_t *l);

#ifdef JNI_VERSION_1_2
static int detect_jvm_version(jk_logger_t *l);

static int open_jvm2(jni_worker_t *p,
                    JNIEnv **env,
                    jk_logger_t *l);
#endif


static int get_bridge_object(jni_worker_t *p,
                             JNIEnv *env,
                             jk_logger_t *l);

static int get_method_ids(jni_worker_t *p,
                          JNIEnv *env,
                          jk_logger_t *l);

static JNIEnv *attach_to_jvm(jni_worker_t *p, 
                             jk_logger_t *l);

static void detach_from_jvm(jni_worker_t *p, 
                            jk_logger_t *l);

static char **jk_parse_sysprops(jk_pool_t *p, 
                                const char *sysprops);

static int jk_file_exists(const char *f);

static void jk_append_libpath(jk_pool_t *p, 
                              const char *libpath);


#if defined LINUX && defined APACHE2_SIGHACK
static void linux_signal_hack() 
{
    sigset_t newM;
    sigset_t old;
    
    sigemptyset(&newM);
    pthread_sigmask( SIG_SETMASK, &newM, &old );
    
    sigdelset(&old, SIGUSR1 );
    sigdelset(&old, SIGUSR2 );
    sigdelset(&old, SIGUNUSED );
    sigdelset(&old, SIGRTMIN );
    sigdelset(&old, SIGRTMIN + 1 );
    sigdelset(&old, SIGRTMIN + 2 );
    pthread_sigmask( SIG_SETMASK, &old, NULL );
}

static void print_signals( sigset_t *sset) {
    int sig;
    for (sig = 1; sig < 20; sig++) 
	{ if (sigismember(sset, sig)) {printf( " %d", sig);} }
    printf( "\n");
}
#endif

static char **jk_parse_sysprops(jk_pool_t *pool, 
                                const char *sysprops)
{
    char **rc = NULL;

    if(pool && sysprops) {
        char *prps = pool->pstrdup(pool, sysprops);
        if(prps && strlen(prps)) {
            unsigned num_of_prps;

            for(num_of_prps = 1; *sysprops ; sysprops++) {
                if('*' == *sysprops) {
                    num_of_prps++;
                }
            }            

            rc = pool->alloc(pool, (num_of_prps + 1) * sizeof(char *));
            if(rc) {
                unsigned i = 0;
                char *tmp = strtok(prps, "*");

                while(tmp && i < num_of_prps) {
                    rc[i] = tmp;
                    tmp = strtok(NULL, "*");
                    i++;
                }
                rc[i] = NULL;
            }
        }
    }

    return rc;
}

static int jk_file_exists(const char *f)
{
    if(f) {
        struct stat st;
        if((0 == stat(f, &st)) && (st.st_mode & S_IFREG)) {
            return JK_TRUE;
        }
    }
    return JK_FALSE;
}


static void jk_append_libpath(jk_pool_t *pool, 
                              const char *libpath)
{
    char *env = NULL;
    char *current = getenv(PATH_ENV_VARIABLE);

    if(current) {
        env = pool->alloc(pool, strlen(PATH_ENV_VARIABLE) + 
                          strlen(current) + 
                          strlen(libpath) + 5);
        if(env) {
            sprintf(env, "%s=%s%c%s", 
                    PATH_ENV_VARIABLE, 
                    libpath, 
                    PATH_SEPERATOR, 
                    current);
        }
    } else {
        env = pool->alloc(pool, strlen(PATH_ENV_VARIABLE) +
                          strlen(libpath) + 5);
        if(env) {
            sprintf(env, "%s=%s", PATH_ENV_VARIABLE, libpath);
        }
    }

    if(env) {
        putenv(env);
    }
}

static int JK_METHOD service(jk_endpoint_t *e,
                             jk_ws_service_t *s,
                             jk_logger_t *l,
                             int *is_recoverable_error)
{
    jni_endpoint_t *p;
    jint rc;

    l->jkLog(l, JK_LOG_DEBUG, "Into service\n");
    if(!e || !e->endpoint_private || !s) {
	    l->jkLog(l, JK_LOG_EMERG, "In service, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    p = e->endpoint_private;

    if(!is_recoverable_error) {
	    return JK_FALSE;
    }

    if(!p->attached) { 
        /* Try to attach */
        if(!(p->env = attach_to_jvm(p->worker, l))) {
	        l->jkLog(l, JK_LOG_EMERG, "Attach failed\n");  
	        /*   Is it recoverable ?? */
	        *is_recoverable_error = JK_TRUE;
	        return JK_FALSE;
	    } 
        p->attached = JK_TRUE;
    }

    /* we are attached now */

    /* 
     * When we call the JVM we cannot know what happens
     * So we can not recover !!!
     */
    *is_recoverable_error = JK_FALSE;
	    
    l->jkLog(l, JK_LOG_DEBUG, "In service, calling Tomcat...\n");

    rc = (*(p->env))->CallIntMethod(p->env,
                                    p->worker->jk_java_bridge_object,
                                    p->worker->jk_service_method,
    /* [V] For some reason gcc likes this pointer -> int -> jlong conversion, */
    /*     but not the direct pointer -> jlong conversion. I hope it's okay.  */
                                    (jlong)(int)s,
                                    (jlong)(int)l);

    /* [V] Righ now JNIEndpoint::service() only returns 1 or 0 */
    if(rc) {
	    l->jkLog(l, JK_LOG_DEBUG, 
               "In service, Tomcat returned OK, done\n");
	    return JK_TRUE;
    } else {
	    l->jkLog(l, JK_LOG_ERROR, 
               "In service, Tomcat FAILED!\n");
	    return JK_FALSE;
    }
}

static int JK_METHOD done(jk_endpoint_t **e,
                          jk_logger_t *l)
{
    jni_endpoint_t *p;

    if(!e || !*e || !(*e)->endpoint_private) {
        l->jkLog(l, JK_LOG_EMERG,  "jni.done() NullPointerException\n");
        return JK_FALSE;
    }

    l->jkLog(l, JK_LOG_DEBUG, "jni.done()\n");
    
    p = (*e)->endpoint_private;

    if(p->attached) {
        detach_from_jvm(p->worker,l);
    }

    (*e)->pool->close( (*e)->pool );
    *e = NULL;
    l->jkLog(l, JK_LOG_DEBUG, 
           "Done ok\n");
    return JK_TRUE;
}

static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t *props,
                              jk_workerEnv_t *we,
                              jk_logger_t *l)
{
    jni_worker_t *p;
    int mem_config = 0;
    char *str_config = NULL;
    JNIEnv *env;

    l->jkLog(l, JK_LOG_DEBUG, "Into validate\n");

    if(! pThis || ! pThis->worker_private) {
	    l->jkLog(l, JK_LOG_EMERG, "In validate, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    p = pThis->worker_private;

    if(p->was_verified) {
    	l->jkLog(l, JK_LOG_DEBUG, "validate, been here before, done\n");
        return JK_TRUE;
    }

    mem_config= map_getIntProp( props, "worker", p->name, "mx", -1 );
    if( mem_config != -1 ) {
        p->tomcat_mx = mem_config;
    }

    mem_config= map_getIntProp( props, "worker", p->name, "ms", -1 );
    if(mem_config != -1 ) {
        p->tomcat_ms = mem_config;
    }

    str_config= map_getStrProp( props, "worker", p->name, "class_path", NULL );
    if(str_config != NULL ) {
        p->tomcat_classpath = p->pool->pstrdup(p->pool, str_config);
    }

    if(!p->tomcat_classpath) {
        l->jkLog(l, JK_LOG_EMERG, "Fail-> no classpath\n");
        return JK_FALSE;
    }

    str_config= map_getStrProp( props, "worker", p->name, "jvm_lib", NULL );
    if(str_config != NULL ) {
        p->jvm_dll_path  = p->pool->pstrdup(p->pool, str_config);
    }

    if(!p->jvm_dll_path || !jk_file_exists(p->jvm_dll_path)) {
        l->jkLog(l, JK_LOG_EMERG, "Fail-> no jvm_dll_path\n");
        return JK_FALSE;
    }

    str_config= map_getStrProp( props, "worker", p->name, "cmd_line", NULL ); 
    if(str_config != NULL ) {
        p->tomcat_cmd_line  = p->pool->pstrdup(p->pool, str_config);
    }

    str_config=  map_getStrProp( props, "worker", p->name, "stdout", NULL ); 
    if(str_config!= NULL ) {
        p->stdout_name  = p->pool->pstrdup(p->pool, str_config);
    }

    str_config=  map_getStrProp( props, "worker", p->name, "stderr", NULL ); 
    if(str_config!= NULL ) {
        p->stderr_name  = p->pool->pstrdup(p->pool, str_config);
    }

    str_config=  map_getStrProp( props, "worker", p->name, "sysprops", NULL ); 
    if(str_config!= NULL ) {
        p->sysprops  = jk_parse_sysprops(p->pool, str_config);
    }

#ifdef JNI_VERSION_1_2
    str_config= map_getStrProp( props, "worker", p->name, "java2opts", NULL );
    if( str_config != NULL ) {
    	/* l->jkLog(l, JK_LOG_DEBUG, "Got opts: %s\n", str_config); */
        p->java2opts = jk_parse_sysprops(p->pool, str_config);
    }
    mem_config= map_getIntProp( props, "worker", p->name, "java2lax", -1 );
    if(mem_config != -1 ) {
        p->java2lax = mem_config ? JK_TRUE : JK_FALSE;
    }
#endif

    str_config=  map_getStrProp( props, "worker", p->name, "ld_path", NULL ); 
    if(str_config!= NULL ) {
        jk_append_libpath(p->pool, str_config);
    }

    if(!load_jvm_dll(p, l)) {
	    l->jkLog(l, JK_LOG_EMERG, "Fail-> can't load jvm dll\n");
	    /* [V] no detach needed here */
	    return JK_FALSE;
    }

    if(!open_jvm(p, &env, l)) {
	    l->jkLog(l, JK_LOG_EMERG, "Fail-> can't open jvm\n");
	    /* [V] no detach needed here */
	    return JK_FALSE;
    }

    if(!get_bridge_object(p, env, l)) {
        l->jkLog(l, JK_LOG_EMERG, "Fail-> can't get bridge object\n");
        /* [V] the detach here may segfault on 1.1 JVM... */
        detach_from_jvm(p, l);
        return JK_FALSE;
    }

    if(!get_method_ids(p, env, l)) {
        l->jkLog(l, JK_LOG_EMERG, "Fail-> can't get method ids\n");
        /* [V] the detach here may segfault on 1.1 JVM... */
        detach_from_jvm(p, l);
        return JK_FALSE;
    }

    p->was_verified = JK_TRUE;
    p->tmp_env = env;

    l->jkLog(l, JK_LOG_DEBUG, 
           "Done validate\n");

    return JK_TRUE;
}

static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t *props,
                          jk_workerEnv_t *we,
                          jk_logger_t *l)
{
    jni_worker_t *p;
    JNIEnv *env;

    l->jkLog(l, JK_LOG_DEBUG, "Into init\n");

    if(! pThis || ! pThis->worker_private) {
	    l->jkLog(l, JK_LOG_EMERG, "In init, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    p = pThis->worker_private;

    if(p->was_initialized) {
	    l->jkLog(l, JK_LOG_DEBUG, "init, done (been here!)\n");
        return JK_TRUE;
    }

    if(!p->jvm ||
       !p->jk_java_bridge_object ||
       !p->jk_service_method     ||
       !p->jk_startup_method     ||
       !p->jk_shutdown_method) {
	    l->jkLog(l, JK_LOG_EMERG, "Fail-> worker not set completely\n");
	    return JK_FALSE;
    }

    /* [V] init is called from the same thread that called validate */
    /* there is no need to attach to the JVM, just get the env */

    /* if(env = attach_to_jvm(p,l)) { */
    if((env = p->tmp_env)) {
        jstring cmd_line = NULL;
        jstring stdout_name = NULL;
        jstring stderr_name = NULL;
        jint rc = 0;

        if(p->tomcat_cmd_line) {
            cmd_line = (*env)->NewStringUTF(env, p->tomcat_cmd_line);
        }
        if(p->stdout_name) {
            stdout_name = (*env)->NewStringUTF(env, p->stdout_name);
        }
        if(p->stdout_name) {
            stderr_name = (*env)->NewStringUTF(env, p->stderr_name);
        }

	    l->jkLog(l, JK_LOG_DEBUG, "In init, calling Tomcat to intialize itself...\n");
        rc = (*env)->CallIntMethod(env,
                                   p->jk_java_bridge_object,
                                   p->jk_startup_method,
                                   cmd_line,
                                   stdout_name,
                                   stderr_name);

        detach_from_jvm(p, l); 

	    if(rc) {
	        p->was_initialized = JK_TRUE;
	        l->jkLog(l, JK_LOG_DEBUG, 
                   "In init, Tomcat initialized OK, done\n");
	        return JK_TRUE;
	    } else {
	        l->jkLog(l, JK_LOG_EMERG, 
                   "Fail-> could not initialize Tomcat\n");
	        return JK_FALSE;
	    }
    } else {
	    l->jkLog(l, JK_LOG_ERROR, 
               "In init, FIXME: init didn't gen env from validate!\n");
	    return JK_FALSE;
    }
}

static int JK_METHOD get_endpoint(jk_worker_t *pThis,
                                  jk_endpoint_t **pend,
                                  jk_logger_t *l)
{
    /* [V] This slow, needs replacement */
    jk_pool_t *endpointPool;
    jni_endpoint_t *e;
    
    endpointPool=pThis->pool->create( pThis->pool, HUGE_POOL_SIZE );
    
    e = (jni_endpoint_t *)endpointPool->calloc(endpointPool,
                                               sizeof(jni_endpoint_t));

    l->jkLog(l, JK_LOG_DEBUG, 
           "Into get_endpoint\n");

    if(!pThis || ! pThis->worker_private || !pend) {
	    l->jkLog(l, JK_LOG_EMERG, 
               "jniWorker.getEndpoint() NullPointerException\n");
	    return JK_FALSE;
    }

    if(e==NULL) {
        l->jkLog(l, JK_LOG_ERROR,
                 "jniWorker.getEndpoint() OutOfMemoryException\n");
        return JK_FALSE;
    }

    /** XXX Fix the spaghetti */
    e->attached = JK_FALSE;
    e->env = NULL;
    e->worker = pThis->worker_private;
    e->endpoint.endpoint_private = e;
    e->endpoint.service = service;
    e->endpoint.done = done;
    e->endpoint.channelData = NULL;
    e->endpoint.pool=endpointPool;
    *pend = &e->endpoint;
    
    return JK_TRUE;
}

static int JK_METHOD destroy(jk_worker_t **pThis,
                             jk_logger_t *l)
{
    jni_worker_t *p;
    JNIEnv *env;

    l->jkLog(l, JK_LOG_DEBUG, 
           "Into destroy\n");

    if(!pThis || !*pThis || ! (*pThis)->worker_private) {
	    l->jkLog(l, JK_LOG_EMERG,
                     "In destroy, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    p = (*pThis)->worker_private;

    if(!p->jvm) {
	    l->jkLog(l, JK_LOG_DEBUG, "In destroy, JVM not intantiated\n");
	    return JK_FALSE;
    }

    if(!p->jk_java_bridge_object || ! p->jk_shutdown_method) {
        l->jkLog(l, JK_LOG_DEBUG, "In destroy, Tomcat not intantiated\n");
	    return JK_FALSE;
    }

    if((env = attach_to_jvm(p,l))) {
	    l->jkLog(l, JK_LOG_DEBUG, 
               "In destroy, shutting down Tomcat...\n");
        (*env)->CallVoidMethod(env,
                               p->jk_java_bridge_object,
                               p->jk_shutdown_method);
        detach_from_jvm(p, l);
    }

    p->pool->close(p->pool);

    l->jkLog(l, JK_LOG_DEBUG, "Done destroy\n");

    return JK_TRUE;
}

int JK_METHOD jk_worker_jni_factory(jk_env_t *env, jk_pool_t *pool, void **result,
                                    char *type, char *name)
{
    jk_logger_t *l=env->logger;
    jni_worker_t *_this;

    l->jkLog(l, JK_LOG_DEBUG, "Into jni_worker_factory\n");

    if(!name) {
	    l->jkLog(l, JK_LOG_EMERG, 
               "In jni_worker_factory, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    if(the_singleton_jni_worker) {
        l->jkLog(l, JK_LOG_DEBUG, 
                 "In jni_worker_factory, instance already created\n");
        *result = the_singleton_jni_worker;
        return JK_TRUE;
    }

    _this = (jni_worker_t *)pool->calloc(pool, sizeof(jni_worker_t ));

    if(!_this) {
	    l->jkLog(l, JK_LOG_ERROR, 
               "In jni_worker_factory, memory allocation error\n");
	    return JK_FALSE;
    }

    _this->pool=pool;
    
    _this->name = _this->pool->pstrdup(_this->pool, name);

    if(!_this->name) {
        l->jkLog(l, JK_LOG_ERROR, 
                 "In jni_worker_factory, memory allocation error\n");
        _this->pool->close(_this->pool);
        return JK_FALSE;
    }

    _this->was_verified          = JK_FALSE;
    _this->was_initialized       = JK_FALSE;
    _this->jvm                   = NULL;
    _this->tmp_env               = NULL;
    _this->jk_java_bridge_object = NULL;
    _this->jk_java_bridge_class  = NULL;
    _this->jk_startup_method     = NULL;
    _this->jk_service_method     = NULL;
    _this->jk_shutdown_method    = NULL;
    _this->tomcat_cmd_line       = NULL;
    _this->tomcat_classpath      = NULL;
    _this->jvm_dll_path          = NULL;
    _this->tomcat_ms             = 0;
    _this->tomcat_mx             = 0;
    _this->sysprops              = NULL;
#ifdef JNI_VERSION_1_2
    _this->java2opts             = NULL;
    _this->java2lax              = JK_TRUE;
#endif
    _this->stdout_name           = NULL;
    _this->stderr_name           = NULL;

    _this->worker.worker_private = _this;
    _this->worker.validate       = validate;
    _this->worker.init           = init;
    _this->worker.get_endpoint   = get_endpoint;
    _this->worker.destroy        = destroy;

    *result = &_this->worker;
    the_singleton_jni_worker = &_this->worker;
    _this->worker.pool=pool;

    l->jkLog(l, JK_LOG_DEBUG, "Done jni_worker_factory\n");
    return JK_TRUE;
}

static int load_jvm_dll(jni_worker_t *p,
                        jk_logger_t *l)
{
#ifdef WIN32
    HINSTANCE hInst = LoadLibrary(p->jvm_dll_path);
    if(hInst) {
        (FARPROC)jni_create_java_vm = 
            GetProcAddress(hInst, "JNI_CreateJavaVM");

        (FARPROC)jni_get_created_java_vms = 
            GetProcAddress(hInst, "JNI_GetCreatedJavaVMs");

        (FARPROC)jni_get_default_java_vm_init_args = 
            GetProcAddress(hInst, "JNI_GetDefaultJavaVMInitArgs");

        l->jkLog(l, JK_LOG_DEBUG, 
               "Loaded all JNI procs\n");

        if(jni_create_java_vm && jni_get_default_java_vm_init_args && jni_get_created_java_vms) {
            return JK_TRUE;
        }

        FreeLibrary(hInst);
    }
#elif defined(NETWARE)
    int javaNlmHandle = FindNLMHandle("JVM");
    if (0 == javaNlmHandle) {
        /* if we didn't get a handle, try to load java and retry getting the */
        /* handle */
        spawnlp(P_NOWAIT, "JVM.NLM", NULL);
        ThreadSwitchWithDelay();
        javaNlmHandle = FindNLMHandle("JVM");
        if (0 == javaNlmHandle)
            printf("Error loading Java.");

    }
    if (0 != javaNlmHandle) {
        jni_create_java_vm = ImportSymbol(GetNLMHandle(), "JNI_CreateJavaVM");
        jni_get_created_java_vms = ImportSymbol(GetNLMHandle(), "JNI_GetCreatedJavaVMs");
        jni_get_default_java_vm_init_args = ImportSymbol(GetNLMHandle(), "JNI_GetDefaultJavaVMInitArgs");
    }
    if(jni_create_java_vm && jni_get_default_java_vm_init_args && jni_get_created_java_vms) {
        return JK_TRUE;
    }
#else 
    void *handle;
    l->jkLog(l, JK_LOG_DEBUG, 
           "Into load_jvm_dll, load %s\n", p->jvm_dll_path);

    handle = dlopen(p->jvm_dll_path, RTLD_NOW | RTLD_GLOBAL);

    if(!handle) {
        l->jkLog(l, JK_LOG_EMERG, 
               "Can't load native library %s : %s\n", p->jvm_dll_path,
               dlerror());
    } else {
        jni_create_java_vm = dlsym(handle, "JNI_CreateJavaVM");
        jni_get_default_java_vm_init_args = dlsym(handle, "JNI_GetDefaultJavaVMInitArgs");
        jni_get_created_java_vms =  dlsym(handle, "JNI_GetCreatedJavaVMs");
        
        if(jni_create_java_vm && jni_get_default_java_vm_init_args
           && jni_get_created_java_vms) {
    	    l->jkLog(l, JK_LOG_DEBUG, 
                   "In load_jvm_dll, symbols resolved, done\n");
            return JK_TRUE;
        }
	    l->jkLog(l, JK_LOG_EMERG, 
               "Can't resolve JNI_CreateJavaVM or JNI_GetDefaultJavaVMInitArgs\n");
        dlclose(handle);
    }
#endif
    return JK_FALSE;
}

static int open_jvm(jni_worker_t *p,
                    JNIEnv **env,
                    jk_logger_t *l)
{
#ifdef JNI_VERSION_1_2
    int jvm_version = detect_jvm_version(l);

    switch(jvm_version) {
	    case JNI_VERSION_1_1:
	        return open_jvm1(p, env, l);
        case JNI_VERSION_1_2:
	        return open_jvm2(p, env, l);
	    default:
            return JK_FALSE;
    }
#else
    /* [V] Make sure this is _really_ visible */
    #warning -------------------------------------------------------
    #warning NO JAVA 2 HEADERS! SUPPORT FOR JAVA 2 FEATURES DISABLED
    #warning -------------------------------------------------------
    return open_jvm1(p, env, l);
#endif
}

static int open_jvm1(jni_worker_t *p,
                    JNIEnv **env,
                    jk_logger_t *l)
{
    JDK1_1InitArgs vm_args;  
    JNIEnv *penv = NULL;
    int err;
    *env = NULL;

    l->jkLog(l, JK_LOG_DEBUG, 
           "Into open_jvm1\n");

    vm_args.version = JNI_VERSION_1_1;

    if(0 != jni_get_default_java_vm_init_args(&vm_args)) {
    	l->jkLog(l, JK_LOG_EMERG, "Fail-> can't get default vm init args\n"); 
        return JK_FALSE;
    }
    l->jkLog(l, JK_LOG_DEBUG, 
           "In open_jvm_dll, got default jvm args\n"); 

    if(vm_args.classpath) {
        unsigned len = strlen(vm_args.classpath) + 
                       strlen(p->tomcat_classpath) + 
                       3;
        char *tmp = p->pool->alloc(p->pool, len);
        if(tmp) {
            sprintf(tmp, "%s%c%s", 
                    p->tomcat_classpath, 
                    PATH_SEPERATOR,
                    vm_args.classpath);
            p->tomcat_classpath = tmp;
        } else {
	        l->jkLog(l, JK_LOG_EMERG, 
                   "Fail-> allocation error for classpath\n"); 
            return JK_FALSE;
        }
    }
    vm_args.classpath = p->tomcat_classpath;

    if(p->tomcat_mx) {
        vm_args.maxHeapSize = p->tomcat_mx;
    }

    if(p->tomcat_ms) {
        vm_args.minHeapSize = p->tomcat_ms;
    }

    if(p->sysprops) {
        vm_args.properties = p->sysprops;
    }

    l->jkLog(l, JK_LOG_DEBUG, "In open_jvm1, about to create JVM...\n");
        err=jni_create_java_vm(&(p->jvm), &penv, &vm_args);

    if (JNI_EEXIST == err) {
        int vmCount;
       l->jkLog(l, JK_LOG_DEBUG, "JVM alread instantiated."
              "Trying to attach instead.\n");

        jni_get_created_java_vms(&(p->jvm), 1, &vmCount);
        if (NULL != p->jvm)
            penv = attach_to_jvm(p, l);

        if (NULL != penv)
            err = 0;
    }

    if(err != 0) {
	    l->jkLog(l, JK_LOG_EMERG, 
               "Fail-> could not create JVM, code: %d \n", err); 
        return JK_FALSE;
    }
    l->jkLog(l, JK_LOG_DEBUG, 
           "In open_jvm1, JVM created, done\n");

    *env = penv;

    return JK_TRUE;
}

#ifdef JNI_VERSION_1_2
static int detect_jvm_version(jk_logger_t *l)
{
    JNIEnv *env = NULL;
    JDK1_1InitArgs vm_args;

    l->jkLog(l, JK_LOG_DEBUG, 
           "Into detect_jvm_version\n");

    /* [V] Idea: ask for 1.2. If the JVM is 1.1 it will return 1.1 instead  */
    /*     Note: asking for 1.1 won't work, 'cause 1.2 JVMs will return 1.1 */
    vm_args.version = JNI_VERSION_1_2;

    if(0 != jni_get_default_java_vm_init_args(&vm_args)) {
    	l->jkLog(l, JK_LOG_EMERG, "Fail-> can't get default vm init args\n"); 
        return JK_FALSE;
    }
    l->jkLog(l, JK_LOG_DEBUG, 
           "In detect_jvm_version, found: %X, done\n", vm_args.version);

    return vm_args.version;
}

static char* build_opt_str(jk_pool_t *pool, 
                           char* opt_name, 
                           char* opt_value, 
                           jk_logger_t *l)
{
    unsigned len = strlen(opt_name) + strlen(opt_value) + 2;

    /* [V] IMHO, these should not be deallocated as long as the JVM runs */
    char *tmp = pool->alloc(pool, len);

    if(tmp) {
	    sprintf(tmp, "%s%s", opt_name, opt_value);
	    return tmp;
    } else {
	    l->jkLog(l, JK_LOG_EMERG, 
               "Fail-> build_opt_str allocation error for %s\n", opt_name);
	    return NULL;
    }
}

static char* build_opt_int(jk_pool_t *pool, 
                           char* opt_name, 
                           int opt_value, 
                           jk_logger_t *l)
{
    /* [V] this should suffice even for 64-bit int */
    unsigned len = strlen(opt_name) + 20 + 2;
    /* [V] IMHO, these should not be deallocated as long as the JVM runs */
    char *tmp = pool->alloc(pool, len);

    if(tmp) {
	    sprintf(tmp, "%s%d", opt_name, opt_value);
	    return tmp;
    } else {
	    l->jkLog(l, JK_LOG_EMERG, 
               "Fail-> build_opt_int allocation error for %s\n", opt_name);
	    return NULL;
    }
}

static int open_jvm2(jni_worker_t *p,
                    JNIEnv **env,
                    jk_logger_t *l)
{
    JavaVMInitArgs vm_args;
    JNIEnv *penv;
    JavaVMOption options[100];
    int optn = 0, err;
    char* tmp;

    *env = NULL;

    l->jkLog(l, JK_LOG_DEBUG, 
           "Into open_jvm2\n");

    vm_args.version = JNI_VERSION_1_2;
    vm_args.options = options;

    if(p->tomcat_classpath) {
    	l->jkLog(l, JK_LOG_DEBUG, "In open_jvm2, setting classpath to %s\n", p->tomcat_classpath);
	    tmp = build_opt_str(p->pool, "-Djava.class.path=", p->tomcat_classpath, l);
	    null_check(tmp);
        options[optn++].optionString = tmp;
    }

    if(p->tomcat_mx) {
	    l->jkLog(l, JK_LOG_DEBUG, "In open_jvm2, setting max heap to %d\n", p->tomcat_mx);
    	tmp = build_opt_int(p->pool, "-Xmx", p->tomcat_mx, l);
	    null_check(tmp);
        options[optn++].optionString = tmp;
    }

    if(p->tomcat_ms) {
    	l->jkLog(l, JK_LOG_DEBUG, "In open_jvm2, setting start heap to %d\n", p->tomcat_ms);
        tmp = build_opt_int(p->pool, "-Xms", p->tomcat_ms, l);
	    null_check(tmp);
        options[optn++].optionString = tmp;
    }

    if(p->sysprops) {
	    int i = 0;
	    while(p->sysprops[i]) {
	        l->jkLog(l, JK_LOG_DEBUG, "In open_jvm2, setting %s\n", p->sysprops[i]);
	        tmp = build_opt_str(p->pool, "-D", p->sysprops[i], l);
	        null_check(tmp);
	        options[optn++].optionString = tmp;
	        i++;
	    }
    }

    if(p->java2opts) {
	    int i=0;

	    while(p->java2opts[i]) {
	        l->jkLog(l, JK_LOG_DEBUG, "In open_jvm2, using option: %s\n", p->java2opts[i]);
	        /* Pass it "as is" */
	        options[optn++].optionString = p->java2opts[i++];
	    }
    }

    vm_args.nOptions = optn;
    
    if(p->java2lax) {
    	l->jkLog(l, JK_LOG_DEBUG, "In open_jvm2, the JVM will ignore unknown options\n");
	    vm_args.ignoreUnrecognized = JNI_TRUE;
    } else {
    	l->jkLog(l, JK_LOG_DEBUG, "In open_jvm2, the JVM will FAIL if it finds unknown options\n");
	    vm_args.ignoreUnrecognized = JNI_FALSE;
    }

    l->jkLog(l, JK_LOG_DEBUG, "In open_jvm2, about to create JVM...\n");

    if((err=jni_create_java_vm(&(p->jvm), &penv, &vm_args)) != 0) {
    	l->jkLog(l, JK_LOG_EMERG, "Fail-> could not create JVM, code: %d \n", err); 
        return JK_FALSE;
    }
    l->jkLog(l, JK_LOG_DEBUG, "In open_jvm2, JVM created, done\n");

    *env = penv;

    return JK_TRUE;
}
#endif

static int get_bridge_object(jni_worker_t *p,
                             JNIEnv *env,
                             jk_logger_t *l)
{
    jmethodID  constructor_method_id;

    l->jkLog(l, JK_LOG_DEBUG, 
           "Into get_bridge_object\n");

    p->jk_java_bridge_class = (*env)->FindClass(env, JAVA_BRIDGE_CLASS_NAME);
    if(!p->jk_java_bridge_class) {
	    l->jkLog(l, JK_LOG_EMERG, "Can't find class %s\n", JAVA_BRIDGE_CLASS_NAME);
	    return JK_FALSE;
    }
    l->jkLog(l, JK_LOG_DEBUG, 
           "In get_bridge_object, loaded %s bridge class\n", JAVA_BRIDGE_CLASS_NAME);

    constructor_method_id = (*env)->GetMethodID(env,
                                                p->jk_java_bridge_class,
                                                "<init>", /* method name */
                                                "()V");   /* method sign */
    if(!constructor_method_id) {
	    p->jk_java_bridge_class = NULL;
	    l->jkLog(l, JK_LOG_EMERG, 
               "Can't find constructor\n");
	    return JK_FALSE;
    }

    p->jk_java_bridge_object = (*env)->NewObject(env,
                                                 p->jk_java_bridge_class,
                                                 constructor_method_id);
    if(! p->jk_java_bridge_object) {
	    p->jk_java_bridge_class = NULL;
	    l->jkLog(l, JK_LOG_EMERG, 
               "Can't create new bridge object\n");
	    return JK_FALSE;
    }

    p->jk_java_bridge_object = (jobject)(*env)->NewGlobalRef(env, p->jk_java_bridge_object);
    if(! p->jk_java_bridge_object) {
	    l->jkLog(l, JK_LOG_EMERG, "Can't create global ref to bridge object\n");
	    p->jk_java_bridge_class = NULL;
        p->jk_java_bridge_object = NULL;
	    return JK_FALSE;
    }

    l->jkLog(l, JK_LOG_DEBUG, 
           "In get_bridge_object, bridge built, done\n");
    return JK_TRUE;
}

static int get_method_ids(jni_worker_t *p,
                          JNIEnv *env,
                          jk_logger_t *l)
{
    p->jk_startup_method = (*env)->GetMethodID(env,
                                               p->jk_java_bridge_class, 
                                               "startup", 
                                               "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I");  
    if(!p->jk_startup_method) {
	l->jkLog(l, JK_LOG_EMERG, "Can't find startup()\n"); 
	return JK_FALSE;
    }

    p->jk_service_method = (*env)->GetMethodID(env,
                                               p->jk_java_bridge_class, 
                                               "service", 
                                               "(JJ)I");   
    if(!p->jk_service_method) {
	l->jkLog(l, JK_LOG_EMERG, "Can't find service()\n"); 
        return JK_FALSE;
    }

    p->jk_shutdown_method = (*env)->GetMethodID(env,
                                                p->jk_java_bridge_class, 
                                                "shutdown", 
                                                "()V");   
    if(!p->jk_shutdown_method) {
	l->jkLog(l, JK_LOG_EMERG, "Can't find shutdown()\n"); 
        return JK_FALSE;
    }    
    
    return JK_TRUE;
}

static JNIEnv *attach_to_jvm(jni_worker_t *p, jk_logger_t *l)
{
    JNIEnv *rc = NULL;
    /* [V] This message is important. If there are signal mask issues,    *
     *     the JVM usually hangs when a new thread tries to attach to it  */
    l->jkLog(l, JK_LOG_DEBUG, 
           "Into attach_to_jvm\n");

#if defined LINUX && defined APACHE2_SIGHACK
    linux_signal_hack();
#endif

    if(0 == (*(p->jvm))->AttachCurrentThread(p->jvm,
#ifdef JNI_VERSION_1_2
					                        (void **)
#endif
                                             &rc,
                                             NULL)) {
	    l->jkLog(l, JK_LOG_DEBUG, 
               "In attach_to_jvm, attached ok\n");
        return rc;
    }
    l->jkLog(l, JK_LOG_ERROR, 
           "In attach_to_jvm, cannot attach thread to JVM.\n");
    return NULL;
}

/*
static JNIEnv *attach_to_jvm(jni_worker_t *p)
{
    JNIEnv *rc = NULL;

#ifdef LINUX
    linux_signal_hack();
#endif    

    if(0 == (*(p->jvm))->AttachCurrentThread(p->jvm, 
#ifdef JNI_VERSION_1_2 
           (void **)
#endif
                                             &rc, 
                                             NULL)) {
        return rc;
    }

    return NULL;
}
*/
static void detach_from_jvm(jni_worker_t *p, 
                            jk_logger_t *l)
{
    if(!p->jvm || !(*(p->jvm))) {
	    l->jkLog(l, JK_LOG_ERROR, 
               "In detach_from_jvm, cannot detach from NULL JVM.\n");
    }

    if(0 == (*(p->jvm))->DetachCurrentThread(p->jvm)) {
	    l->jkLog(l, JK_LOG_DEBUG, 
               "In detach_from_jvm, detached ok\n");
    } else {
	    l->jkLog(l, JK_LOG_ERROR, 
               "In detach_from_jvm, cannot detach from JVM.\n");
    }
}
