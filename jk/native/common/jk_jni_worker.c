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

#if !defined(WIN32) && !defined(NETWARE) && !defined(AS400)
#include <dlfcn.h>
#endif

#include <jni.h>

#include "jk_pool.h"
#include "jk_jni_worker.h"
#include "jk_util.h"

#if defined LINUX && defined APACHE2_SIGHACK
#include <pthread.h>
#include <signal.h>
#include <bits/signum.h>
#endif

#ifdef NETWARE
#ifdef __NOVELL_LIBC__
#include <dlfcn.h>
#else
#include <nwthread.h>
#include <nwadv.h>
#endif
#endif

#ifndef JNI_VERSION_1_1
#define JNI_VERSION_1_1 0x00010001
#endif

/* probably on an older system that doesn't support RTLD_NOW or RTLD_LAZY.
 * The below define is a lie since we are really doing RTLD_LAZY since the
 * system doesn't support RTLD_NOW.
 */
#ifndef RTLD_NOW
#define RTLD_NOW 1
#endif

#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 0
#endif

#define null_check(e) if ((e) == 0) return JK_FALSE

jint (JNICALL *jni_get_default_java_vm_init_args)(void *) = NULL;
jint (JNICALL *jni_create_java_vm)(JavaVM **, JNIEnv **, void *) = NULL;
#ifdef AS400
jint (JNICALL *jni_get_created_java_vms)(JavaVM **, long, long *) = NULL;
#else
jint (JNICALL *jni_get_created_java_vms)(JavaVM **, int, int *) = NULL;
#endif

#define TC33_JAVA_BRIDGE_CLASS_NAME ("org/apache/tomcat/modules/server/JNIEndpoint")
#define TC32_JAVA_BRIDGE_CLASS_NAME ("org/apache/tomcat/service/JNIEndpoint")

static jk_worker_t *the_singleton_jni_worker = NULL;

struct jni_worker {

    int was_verified;
    int was_initialized;

    jk_pool_t p;
    jk_pool_atom_t buf[TINY_POOL_SIZE];

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
	 * Bridge Type, Tomcat 32/33/40/41/5
	 */
	unsigned	bridge_type;
	
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


/*
   Duplicate string and convert it to ASCII on EBDIC based systems
   Needed for at least AS/400 and BS2000 but what about other EBDIC systems ?
*/
static void *strdup_ascii(jk_pool_t *p, 
                          char *s)
{
	char * rc;	
	rc = jk_pool_strdup(p, s);

#if defined(AS400) || defined(_OSD_POSIX)
	jk_xlate_to_ascii(rc, strlen(rc));
#endif

    return rc;
}

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

static int JK_METHOD service(jk_endpoint_t *e,
                             jk_ws_service_t *s,
                             jk_logger_t *l,
                             int *is_recoverable_error)
{
    jni_endpoint_t *p;
    jint rc;

    jk_log(l, JK_LOG_DEBUG, "Into service\n");
    if(!e || !e->endpoint_private || !s) {
	    jk_log(l, JK_LOG_EMERG, "In service, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    p = e->endpoint_private;

    if(!is_recoverable_error) {
	    return JK_FALSE;
    }

    if(!p->attached) { 
        /* Try to attach */
        if(!(p->env = attach_to_jvm(p->worker, l))) {
	        jk_log(l, JK_LOG_EMERG, "Attach failed\n");  
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
	    
    jk_log(l, JK_LOG_DEBUG, "In service, calling Tomcat...\n");

    rc = (*(p->env))->CallIntMethod(p->env,
                                    p->worker->jk_java_bridge_object,
                                    p->worker->jk_service_method,
    /* [V] For some reason gcc likes this pointer -> int -> jlong conversion, */
    /*     but not the direct pointer -> jlong conversion. I hope it's okay.  */
#ifdef AS400
                                    s,
                                    l
#else
                                    (jlong)(int)s,
                                    (jlong)(int)l
#endif
);

    /* [V] Righ now JNIEndpoint::service() only returns 1 or 0 */
    if(rc) {
	    jk_log(l, JK_LOG_DEBUG, 
               "In service, Tomcat returned OK, done\n");
	    return JK_TRUE;
    } else {
	    jk_log(l, JK_LOG_ERROR, 
               "In service, Tomcat FAILED!\n");
	    return JK_FALSE;
    }
}

static int JK_METHOD done(jk_endpoint_t **e,
                          jk_logger_t *l)
{
    jni_endpoint_t *p;

    jk_log(l, JK_LOG_DEBUG, "Into done\n");
    if(!e || !*e || !(*e)->endpoint_private) {
	    jk_log(l, JK_LOG_EMERG, 
               "In done, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    p = (*e)->endpoint_private;

    if(p->attached) {
        detach_from_jvm(p->worker,l);
    }

    free(p);
    *e = NULL;
    jk_log(l, JK_LOG_DEBUG, 
           "Done ok\n");
    return JK_TRUE;
}

static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t *props,
                              jk_worker_env_t *we,
                              jk_logger_t *l)
{
    jni_worker_t *p;
    int mem_config = 0;
    int	btype      = 0;
    char *str_config = NULL;
    JNIEnv *env;

    jk_log(l, JK_LOG_DEBUG, "Into validate\n");

    if(! pThis || ! pThis->worker_private) {
	    jk_log(l, JK_LOG_EMERG, "In validate, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    p = pThis->worker_private;

    if(p->was_verified) {
    	jk_log(l, JK_LOG_DEBUG, "validate, been here before, done\n");
        return JK_TRUE;
    }

    if(jk_get_worker_mx(props, p->name, (unsigned int *)&mem_config)) {
        p->tomcat_mx = mem_config;
    }

    if(jk_get_worker_ms(props, p->name, (unsigned int *)&mem_config)) {
        p->tomcat_ms = mem_config;
    }

    if(jk_get_worker_classpath(props, p->name, &str_config)) {
        p->tomcat_classpath = jk_pool_strdup(&p->p, str_config);
    }

    if(!p->tomcat_classpath) {
        jk_log(l, JK_LOG_EMERG, "Fail-> no classpath\n");
        return JK_FALSE;
    }

    if(jk_get_worker_bridge_type(props, p->name, (unsigned int *)&btype)) {
    	p->bridge_type = btype;
    }	

    if(jk_get_worker_jvm_path(props, p->name, &str_config)) {
        p->jvm_dll_path  = jk_pool_strdup(&p->p, str_config);
    }

    if(!p->jvm_dll_path || !jk_file_exists(p->jvm_dll_path)) {
        jk_log(l, JK_LOG_EMERG, "Fail-> no jvm_dll_path\n");
        return JK_FALSE;
    }

    if(jk_get_worker_cmd_line(props, p->name, &str_config)) {
        p->tomcat_cmd_line  = jk_pool_strdup(&p->p, str_config);
    }

    if(jk_get_worker_stdout(props, p->name, &str_config)) {
        p->stdout_name  = jk_pool_strdup(&p->p, str_config);
    }

    if(jk_get_worker_stderr(props, p->name, &str_config)) {
        p->stderr_name  = jk_pool_strdup(&p->p, str_config);
    }

    if(jk_get_worker_sysprops(props, p->name, &str_config)) {
        p->sysprops  = jk_parse_sysprops(&p->p, str_config);
    }

#ifdef JNI_VERSION_1_2
    if(jk_get_worker_str_prop(props, p->name, "java2opts", &str_config)) {
    	/* jk_log(l, JK_LOG_DEBUG, "Got opts: %s\n", str_config); */
	    p->java2opts = jk_parse_sysprops(&p->p, str_config);
    }
    if(jk_get_worker_int_prop(props, p->name, "java2lax", &mem_config)) {
        p->java2lax = mem_config ? JK_TRUE : JK_FALSE;
    }
#endif

    if(jk_get_worker_libpath(props, p->name, &str_config)) {
        jk_append_libpath(&p->p, str_config);
    }

    if(!load_jvm_dll(p, l)) {
	    jk_log(l, JK_LOG_EMERG, "Fail-> can't load jvm dll\n");
	    /* [V] no detach needed here */
	    return JK_FALSE;
    }

    if(!open_jvm(p, &env, l)) {
	    jk_log(l, JK_LOG_EMERG, "Fail-> can't open jvm\n");
	    /* [V] no detach needed here */
	    return JK_FALSE;
    }

    if(!get_bridge_object(p, env, l)) {
        jk_log(l, JK_LOG_EMERG, "Fail-> can't get bridge object\n");
        /* [V] the detach here may segfault on 1.1 JVM... */
        detach_from_jvm(p, l);
        return JK_FALSE;
    }

    if(!get_method_ids(p, env, l)) {
        jk_log(l, JK_LOG_EMERG, "Fail-> can't get method ids\n");
        /* [V] the detach here may segfault on 1.1 JVM... */
        detach_from_jvm(p, l);
        return JK_FALSE;
    }

    p->was_verified = JK_TRUE;
    p->tmp_env = env;

    jk_log(l, JK_LOG_DEBUG, 
           "Done validate\n");

    return JK_TRUE;
}

static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t *props,
                          jk_worker_env_t *we,
                          jk_logger_t *l)
{
    jni_worker_t *p;
    JNIEnv *env;

    jk_log(l, JK_LOG_DEBUG, "Into init\n");

    if(! pThis || ! pThis->worker_private) {
	    jk_log(l, JK_LOG_EMERG, "In init, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    p = pThis->worker_private;

    if(p->was_initialized) {
	    jk_log(l, JK_LOG_DEBUG, "init, done (been here!)\n");
        return JK_TRUE;
    }

    if(!p->jvm ||
       !p->jk_java_bridge_object ||
       !p->jk_service_method     ||
       !p->jk_startup_method     ||
       !p->jk_shutdown_method) {
	    jk_log(l, JK_LOG_EMERG, "Fail-> worker not set completely\n");
	    return JK_FALSE;
    }

    /* [V] init is called from the same thread that called validate */
    /* there is no need to attach to the JVM, just get the env */

    /* if(env = attach_to_jvm(p,l)) { */
    if((env = p->tmp_env)) {
        jstring cmd_line = (jstring)NULL;
        jstring stdout_name = (jstring)NULL;
        jstring stderr_name = (jstring)NULL;
        jint rc = 0;

		/* AS400/BS2000 need EBCDIC to ASCII conversion for JNI */
		
        if(p->tomcat_cmd_line) {
            cmd_line = (*env)->NewStringUTF(env, strdup_ascii(&p->p, p->tomcat_cmd_line));
        }
        if(p->stdout_name) {
            stdout_name = (*env)->NewStringUTF(env, strdup_ascii(&p->p, p->stdout_name));
        }
        if(p->stderr_name) {
            stderr_name = (*env)->NewStringUTF(env, strdup_ascii(&p->p, p->stderr_name));
        }

	    jk_log(l, JK_LOG_DEBUG, "In init, calling Tomcat to intialize itself...\n");
        rc = (*env)->CallIntMethod(env,
                                   p->jk_java_bridge_object,
                                   p->jk_startup_method,
                                   cmd_line,
                                   stdout_name,
                                   stderr_name);

        detach_from_jvm(p, l); 

	    if(rc) {
	        p->was_initialized = JK_TRUE;
	        jk_log(l, JK_LOG_DEBUG, 
                   "In init, Tomcat initialized OK, done\n");
	        return JK_TRUE;
	    } else {
	        jk_log(l, JK_LOG_EMERG, 
                   "Fail-> could not initialize Tomcat\n");
	        return JK_FALSE;
	    }
    } else {
	    jk_log(l, JK_LOG_ERROR, 
               "In init, FIXME: init didn't gen env from validate!\n");
	    return JK_FALSE;
    }
}

static int JK_METHOD get_endpoint(jk_worker_t *pThis,
                                  jk_endpoint_t **pend,
                                  jk_logger_t *l)
{
    /* [V] This slow, needs replacement */
    jni_endpoint_t *p = (jni_endpoint_t *)malloc(sizeof(jni_endpoint_t));

    jk_log(l, JK_LOG_DEBUG, 
           "Into get_endpoint\n");

    if(!pThis || ! pThis->worker_private || !pend) {
	    jk_log(l, JK_LOG_EMERG, 
               "In get_endpoint, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    if(p) {
        p->attached = JK_FALSE;
        p->env = NULL;
        p->worker = pThis->worker_private;
        p->endpoint.endpoint_private = p;
        p->endpoint.service = service;
        p->endpoint.done = done;
        *pend = &p->endpoint;
	
        return JK_TRUE;
    } else {
	    jk_log(l, JK_LOG_ERROR, "In get_endpoint, could not allocate endpoint\n");
	    return JK_FALSE;
    }
}

static int JK_METHOD destroy(jk_worker_t **pThis,
                             jk_logger_t *l)
{
    jni_worker_t *p;
    JNIEnv *env;

    jk_log(l, JK_LOG_DEBUG, 
           "Into destroy\n");

    if(!pThis || !*pThis || ! (*pThis)->worker_private) {
	    jk_log(l, JK_LOG_EMERG, "In destroy, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    p = (*pThis)->worker_private;

    if(!p->jvm) {
	    jk_log(l, JK_LOG_DEBUG, "In destroy, JVM not intantiated\n");
	    return JK_FALSE;
    }

    if(!p->jk_java_bridge_object || ! p->jk_shutdown_method) {
        jk_log(l, JK_LOG_DEBUG, "In destroy, Tomcat not intantiated\n");
	    return JK_FALSE;
    }

    if((env = attach_to_jvm(p,l))) {
	    jk_log(l, JK_LOG_DEBUG, 
               "In destroy, shutting down Tomcat...\n");
        (*env)->CallVoidMethod(env,
                               p->jk_java_bridge_object,
                               p->jk_shutdown_method);
        detach_from_jvm(p, l);
    }

    jk_close_pool(&p->p);
    free(p);

    jk_log(l, JK_LOG_DEBUG, "Done destroy\n");

    return JK_TRUE;
}

int JK_METHOD jni_worker_factory(jk_worker_t **w,
                                 const char *name,
                                 jk_logger_t *l)
{
    jni_worker_t *private_data;

    jk_log(l, JK_LOG_DEBUG, "Into jni_worker_factory\n");

    if(!name || !w) {
	    jk_log(l, JK_LOG_EMERG, 
               "In jni_worker_factory, assert failed - invalid parameters\n");
	    return JK_FALSE;
    }

    if(the_singleton_jni_worker) {
	    jk_log(l, JK_LOG_DEBUG, 
               "In jni_worker_factory, instance already created\n");
        *w = the_singleton_jni_worker;
	    return JK_TRUE;
    }

    private_data = (jni_worker_t *)malloc(sizeof(jni_worker_t ));

    if(!private_data) {
	    jk_log(l, JK_LOG_ERROR, 
               "In jni_worker_factory, memory allocation error\n");
	    return JK_FALSE;
    }

    jk_open_pool(&private_data->p,
	             private_data->buf,
                 sizeof(jk_pool_atom_t) * TINY_POOL_SIZE);

    private_data->name = jk_pool_strdup(&private_data->p, name);

    if(!private_data->name) {
        jk_log(l, JK_LOG_ERROR, 
               "In jni_worker_factory, memory allocation error\n");
	    jk_close_pool(&private_data->p);
        free(private_data);
        return JK_FALSE;
    }

    private_data->was_verified          = JK_FALSE;
    private_data->was_initialized       = JK_FALSE;
    private_data->jvm                   = NULL;
    private_data->tmp_env               = NULL;
    private_data->jk_java_bridge_object = (jobject)NULL;
    private_data->jk_java_bridge_class  = (jclass)NULL;
    private_data->jk_startup_method     = (jmethodID)NULL;
    private_data->jk_service_method     = (jmethodID)NULL;
    private_data->jk_shutdown_method    = (jmethodID)NULL;
    private_data->tomcat_cmd_line       = NULL;
    private_data->tomcat_classpath      = NULL;
    private_data->bridge_type      		= TC33_BRIDGE_TYPE;
    private_data->jvm_dll_path          = NULL;
    private_data->tomcat_ms             = 0;
    private_data->tomcat_mx             = 0;
    private_data->sysprops              = NULL;
#ifdef JNI_VERSION_1_2
    private_data->java2opts             = NULL;
    private_data->java2lax              = JK_TRUE;
#endif
    private_data->stdout_name           = NULL;
    private_data->stderr_name           = NULL;

    private_data->worker.worker_private = private_data;
    private_data->worker.validate       = validate;
    private_data->worker.init           = init;
    private_data->worker.get_endpoint   = get_endpoint;
    private_data->worker.destroy        = destroy;

    *w = &private_data->worker;
    the_singleton_jni_worker = &private_data->worker;

    jk_log(l, JK_LOG_DEBUG, "Done jni_worker_factory\n");
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

        jk_log(l, JK_LOG_DEBUG, 
               "Loaded all JNI procs\n");

        if(jni_create_java_vm && jni_get_default_java_vm_init_args && jni_get_created_java_vms) {
            return JK_TRUE;
        }

        FreeLibrary(hInst);
    }
#elif defined(NETWARE) && !defined(__NOVELL_LIBC__)
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
#elif defined(AS400)
    jk_log(l, JK_LOG_DEBUG,
           "Direct reference to JNI entry points (no SRVPGM)\n");
    jni_create_java_vm = &JNI_CreateJavaVM;
    jni_get_default_java_vm_init_args = &JNI_GetDefaultJavaVMInitArgs;
    jni_get_created_java_vms = &JNI_GetCreatedJavaVMs;
#else
    void *handle;
    jk_log(l, JK_LOG_DEBUG, 
           "Into load_jvm_dll, load %s\n", p->jvm_dll_path);

    handle = dlopen(p->jvm_dll_path, RTLD_NOW | RTLD_GLOBAL);

    if(!handle) {
        jk_log(l, JK_LOG_EMERG, 
               "Can't load native library %s : %s\n", p->jvm_dll_path,
               dlerror());
    } else {
        jni_create_java_vm = dlsym(handle, "JNI_CreateJavaVM");
        jni_get_default_java_vm_init_args = dlsym(handle, "JNI_GetDefaultJavaVMInitArgs");
        jni_get_created_java_vms =  dlsym(handle, "JNI_GetCreatedJavaVMs");

        if(jni_create_java_vm && jni_get_default_java_vm_init_args &&
           jni_get_created_java_vms) {
    	    jk_log(l, JK_LOG_DEBUG, 
                   "In load_jvm_dll, symbols resolved, done\n");
            return JK_TRUE;
        }
	    jk_log(l, JK_LOG_EMERG, 
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
    JNIEnv *penv;
    int err;
    *env = NULL;

    jk_log(l, JK_LOG_DEBUG, 
           "Into open_jvm1\n");

    vm_args.version = JNI_VERSION_1_1;

    if(0 != jni_get_default_java_vm_init_args(&vm_args)) {
    	jk_log(l, JK_LOG_EMERG, "Fail-> can't get default vm init args\n"); 
        return JK_FALSE;
    }
    jk_log(l, JK_LOG_DEBUG, 
           "In open_jvm_dll, got default jvm args\n"); 

    if(vm_args.classpath) {
        unsigned len = strlen(vm_args.classpath) + 
                       strlen(p->tomcat_classpath) + 
                       3;
        char *tmp = jk_pool_alloc(&p->p, len);
        if(tmp) {
            sprintf(tmp, "%s%c%s", 
                    strdup_ascii(&p->p, p->tomcat_classpath), 
                    PATH_SEPERATOR,
                    vm_args.classpath);
            p->tomcat_classpath = tmp;
        } else {
	        jk_log(l, JK_LOG_EMERG, 
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
        /* No EBCDIC to ASCII conversion here for AS/400 - later */
	vm_args.properties = p->sysprops;
    }

    jk_log(l, JK_LOG_DEBUG, "In open_jvm1, about to create JVM...\n");
    if((err=jni_create_java_vm(&(p->jvm), 
                               &penv, 
                               &vm_args)) != 0) {
	    jk_log(l, JK_LOG_EMERG, 
               "Fail-> could not create JVM, code: %d \n", err); 
        return JK_FALSE;
    }
    jk_log(l, JK_LOG_DEBUG, 
           "In open_jvm1, JVM created, done\n");

    *env = penv;

    return JK_TRUE;
}

#ifdef JNI_VERSION_1_2
static int detect_jvm_version(jk_logger_t *l)
{
    JNIEnv *env = NULL;
    JDK1_1InitArgs vm_args;

    jk_log(l, JK_LOG_DEBUG, 
           "Into detect_jvm_version\n");

    /* [V] Idea: ask for 1.2. If the JVM is 1.1 it will return 1.1 instead  */
    /*     Note: asking for 1.1 won't work, 'cause 1.2 JVMs will return 1.1 */
    vm_args.version = JNI_VERSION_1_2;

    if(0 != jni_get_default_java_vm_init_args(&vm_args)) {
    	jk_log(l, JK_LOG_EMERG, "Fail-> can't get default vm init args\n"); 
        return JK_FALSE;
    }
    jk_log(l, JK_LOG_DEBUG, 
           "In detect_jvm_version, found: %X, done\n", vm_args.version);

    return vm_args.version;
}

static char* build_opt_str(jk_pool_t *p, 
                           char* opt_name, 
                           char* opt_value, 
                           jk_logger_t *l)
{
    unsigned len = strlen(opt_name) + strlen(opt_value) + 2;

    /* [V] IMHO, these should not be deallocated as long as the JVM runs */
    char *tmp = jk_pool_alloc(p, len);

    if(tmp) {
	    sprintf(tmp, "%s%s", opt_name, opt_value);
	    return tmp;
    } else {
	    jk_log(l, JK_LOG_EMERG, 
               "Fail-> build_opt_str allocation error for %s\n", opt_name);
	    return NULL;
    }
}

static char* build_opt_int(jk_pool_t *p, 
                           char* opt_name, 
                           int opt_value, 
                           jk_logger_t *l)
{
    /* [V] this should suffice even for 64-bit int */
    unsigned len = strlen(opt_name) + 20 + 2;
    /* [V] IMHO, these should not be deallocated as long as the JVM runs */
    char *tmp = jk_pool_alloc(p, len);

    if(tmp) {
	    sprintf(tmp, "%s%d", opt_name, opt_value);
	    return tmp;
    } else {
	    jk_log(l, JK_LOG_EMERG, 
               "Fail-> build_opt_int allocation error for %s\n", opt_name);
	    return NULL;
    }
}

static int open_jvm2(jni_worker_t *p,
                    JNIEnv **env,
                    jk_logger_t *l)
{
    JavaVMInitArgs vm_args;
    JNIEnv *penv = NULL;
    JavaVMOption options[100];
    int optn = 0, err;
    char* tmp;

    *env = NULL;

    jk_log(l, JK_LOG_DEBUG, 
           "Into open_jvm2\n");

    vm_args.version = JNI_VERSION_1_2;
    vm_args.options = options;

/* AS/400 need EBCDIC to ASCII conversion to parameters passed to JNI */
/* No conversion for ASCII based systems (what about BS2000 ?) */

    if(p->tomcat_classpath) {
    	jk_log(l, JK_LOG_DEBUG, "In open_jvm2, setting classpath to %s\n", p->tomcat_classpath);
	    tmp = build_opt_str(&p->p, "-Djava.class.path=", p->tomcat_classpath, l);
	    null_check(tmp);
        options[optn++].optionString = strdup_ascii(&p->p, tmp);
    }

    if(p->tomcat_mx) {
	    jk_log(l, JK_LOG_DEBUG, "In open_jvm2, setting max heap to %d\n", p->tomcat_mx);
    	tmp = build_opt_int(&p->p, "-Xmx", p->tomcat_mx, l);
	    null_check(tmp);
        options[optn++].optionString = strdup_ascii(&p->p, tmp);
    }

    if(p->tomcat_ms) {
    	jk_log(l, JK_LOG_DEBUG, "In open_jvm2, setting start heap to %d\n", p->tomcat_ms);
        tmp = build_opt_int(&p->p, "-Xms", p->tomcat_ms, l);
	    null_check(tmp);
        options[optn++].optionString = strdup_ascii(&p->p, tmp);
    }

    if(p->sysprops) {
	    int i = 0;
	    while(p->sysprops[i]) {
	        jk_log(l, JK_LOG_DEBUG, "In open_jvm2, setting %s\n", p->sysprops[i]);
	        tmp = build_opt_str(&p->p, "-D", p->sysprops[i], l);
	        null_check(tmp);
	        options[optn++].optionString = strdup_ascii(&p->p, tmp);
	        i++;
	    }
    }

    if(p->java2opts) {
	    int i=0;

	    while(p->java2opts[i]) {
	        jk_log(l, JK_LOG_DEBUG, "In open_jvm2, using option: %s\n", p->java2opts[i]);
	        /* Pass it "as is" */
	        options[optn++].optionString = strdup_ascii(&p->p, p->java2opts[i++]);
	    }
    }

    vm_args.nOptions = optn;
    
    if(p->java2lax) {
    	jk_log(l, JK_LOG_DEBUG, "In open_jvm2, the JVM will ignore unknown options\n");
	    vm_args.ignoreUnrecognized = JNI_TRUE;
    } else {
    	jk_log(l, JK_LOG_DEBUG, "In open_jvm2, the JVM will FAIL if it finds unknown options\n");
	    vm_args.ignoreUnrecognized = JNI_FALSE;
    }

    jk_log(l, JK_LOG_DEBUG, "In open_jvm2, about to create JVM...\n");

    err=jni_create_java_vm(&(p->jvm), &penv, &vm_args);

    if (JNI_EEXIST == err) {
#ifdef AS400
        long vmCount;
#else
        int  vmCount;
#endif
       jk_log(l, JK_LOG_DEBUG, "JVM alread instantiated."
              "Trying to attach instead.\n");

        jni_get_created_java_vms(&(p->jvm), 1, &vmCount);
        if (NULL != p->jvm)
            penv = attach_to_jvm(p, l);

        if (NULL != penv)
            err = 0;
    }

    if(err != 0) {
    	jk_log(l, JK_LOG_EMERG, "Fail-> could not create JVM, code: %d \n", err); 
        return JK_FALSE;
    }
    jk_log(l, JK_LOG_DEBUG, "In open_jvm2, JVM created, done\n");

    *env = penv;

    return JK_TRUE;
}
#endif

static int get_bridge_object(jni_worker_t *p,
                             JNIEnv *env,
                             jk_logger_t *l)
{
	char *	btype;
	char *	ctype;
	
    jmethodID  constructor_method_id;

    jk_log(l, JK_LOG_DEBUG, 
           "Into get_bridge_object\n");

	switch (p->bridge_type)
	{
		case TC32_BRIDGE_TYPE :
			btype = TC32_JAVA_BRIDGE_CLASS_NAME;
			break;
			
		case TC33_BRIDGE_TYPE :
			btype = TC33_JAVA_BRIDGE_CLASS_NAME;
			break;
			
		case TC40_BRIDGE_TYPE :
		case TC41_BRIDGE_TYPE :
		case TC50_BRIDGE_TYPE :
	    	jk_log(l, JK_LOG_EMERG, "Bridge type %d not supported\n", p->bridge_type);
	    	return JK_FALSE;
	}
	
/* AS400/BS2000 need conversion from EBCDIC to ASCII before passing to JNI */
/* for others, strdup_ascii is just jk_pool_strdup */

    ctype = strdup_ascii(&p->p, btype);

    p->jk_java_bridge_class = (*env)->FindClass(env, ctype);

    if(!p->jk_java_bridge_class) {
	    jk_log(l, JK_LOG_EMERG, "Can't find class %s\n", btype);
	    return JK_FALSE;
    }
    jk_log(l, JK_LOG_DEBUG, 
           "In get_bridge_object, loaded %s bridge class\n", btype);

    constructor_method_id = (*env)->GetMethodID(env,
                                                p->jk_java_bridge_class,
                                                strdup_ascii(&p->p, "<init>"), /* method name */
                                                strdup_ascii(&p->p, "()V"));   /* method sign */

    if(!constructor_method_id) {
	    p->jk_java_bridge_class = (jclass)NULL;
	    jk_log(l, JK_LOG_EMERG, 
               "Can't find constructor\n");
	    return JK_FALSE;
    }

    p->jk_java_bridge_object = (*env)->NewObject(env,
                                                 p->jk_java_bridge_class,
                                                 constructor_method_id);
    if(! p->jk_java_bridge_object) {
	    p->jk_java_bridge_class = (jclass)NULL;
	    jk_log(l, JK_LOG_EMERG, 
               "Can't create new bridge object\n");
	    return JK_FALSE;
    }

    p->jk_java_bridge_object = (jobject)(*env)->NewGlobalRef(env, p->jk_java_bridge_object);
    if(! p->jk_java_bridge_object) {
	    jk_log(l, JK_LOG_EMERG, "Can't create global ref to bridge object\n");
	    p->jk_java_bridge_class = (jclass)NULL;
        p->jk_java_bridge_object = (jobject)NULL;
	    return JK_FALSE;
    }

    jk_log(l, JK_LOG_DEBUG, 
           "In get_bridge_object, bridge built, done\n");
    return JK_TRUE;
}

static int get_method_ids(jni_worker_t *p,
                          JNIEnv *env,
                          jk_logger_t *l)
{

/* AS400/BS2000 need conversion from EBCDIC to ASCII before passing to JNI */

    p->jk_startup_method = (*env)->GetMethodID(env,
                                               p->jk_java_bridge_class, 
                                               strdup_ascii(&p->p, "startup"), 
                                               strdup_ascii(&p->p, "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I"));  

    if(!p->jk_startup_method) {
	jk_log(l, JK_LOG_EMERG, "Can't find startup()\n"); 
	return JK_FALSE;
    }

    p->jk_service_method = (*env)->GetMethodID(env,
                                               p->jk_java_bridge_class, 
                                               strdup_ascii(&p->p, "service"), 
                                               strdup_ascii(&p->p, "(JJ)I"));   

    if(!p->jk_service_method) {
	jk_log(l, JK_LOG_EMERG, "Can't find service()\n"); 
        return JK_FALSE;
    }

    p->jk_shutdown_method = (*env)->GetMethodID(env,
                                                p->jk_java_bridge_class, 
                                                strdup_ascii(&p->p, "shutdown"), 
                                                strdup_ascii(&p->p, "()V"));   

    if(!p->jk_shutdown_method) {
	jk_log(l, JK_LOG_EMERG, "Can't find shutdown()\n"); 
        return JK_FALSE;
    }    
    
    return JK_TRUE;
}

static JNIEnv *attach_to_jvm(jni_worker_t *p, jk_logger_t *l)
{
    JNIEnv *rc = NULL;
    /* [V] This message is important. If there are signal mask issues,    *
     *     the JVM usually hangs when a new thread tries to attach to it  */
    jk_log(l, JK_LOG_DEBUG, 
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
	    jk_log(l, JK_LOG_DEBUG, 
               "In attach_to_jvm, attached ok\n");
        return rc;
    }
    jk_log(l, JK_LOG_ERROR, 
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
	    jk_log(l, JK_LOG_ERROR, 
               "In detach_from_jvm, cannot detach from NULL JVM.\n");
    }

    if(0 == (*(p->jvm))->DetachCurrentThread(p->jvm)) {
	    jk_log(l, JK_LOG_DEBUG, 
               "In detach_from_jvm, detached ok\n");
    } else {
	    jk_log(l, JK_LOG_ERROR, 
               "In detach_from_jvm, cannot detach from JVM.\n");
    }
}
