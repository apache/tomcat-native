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
 * JNI utils. This is now organized as a class with virtual methods.
 * This will allow a future split based on VM type, and simplification
 * of the code ( no more #ifdefs, but different objects for different VMs).
 *
 * It should also allow to better work around VMs and support more
 * ( I assume kaffe will need some special tricks, 1.4 will be different
 *  than 1.1 and 1.2, IBM vms have their own paths, etc ).
 *
 * That's future - for now we use the current ugly code (  result
 *  of few years of workarounds ). The refactoring should be done before adding
 * anything new.
 *
 * @author:  Gal Shachor <shachor@il.ibm.com>
 * @author: Costin Manolache
 */

#include "jk_vm.h"

#if !defined(WIN32) && !defined(NETWARE)
#include <dlfcn.h>
#endif
#if defined LINUX && defined APACHE2_SIGHACK
#include <pthread.h>
#include <signal.h>
#include <bits/signum.h>
#endif

#ifdef NETWARE
#include <nwthread.h>
#include <nwadv.h>
#endif

#include <jni.h>

#ifndef JNI_VERSION_1_1
#define JNI_VERSION_1_1 0x00010001
#endif

#define null_check(e) if ((e) == 0) return JK_FALSE


static int open_jvm1(jk_env_t *env, jk_vm_t *p);

#ifdef JNI_VERSION_1_2
static int detect_jvm_version(jk_env_t *env);
static int open_jvm2(jk_env_t *env, jk_vm_t *p);
#endif

jint (JNICALL *jni_get_default_java_vm_init_args)(void *) = NULL;
jint (JNICALL *jni_create_java_vm)(JavaVM **, JNIEnv **, void *) = NULL;
jint (JNICALL *jni_get_created_java_vms)(JavaVM **, int, int *) = NULL;

/* Guessing - try all those to find the right dll
 */
static const char *defaultVM_PATH[]={
    "$(JAVA_HOME)$(fs)jre$(fs)bin$(fs)classic$(fs)libjvm.$(so)",
    "$(JAVA_HOME)$(fs)jre$(fs)lib$(fs)$(arch)$(fs)classic$(fs)libjvm.$(so)",
    "$(JAVA_HOME)$(fs)jre$(fs)bin$(fs)classic$(fs)jvm.$(so)",
    NULL
};

/** Where to try to find jk jars ( if user doesn't specify it explicitely ) */
static const char *defaultJK_PATH[]={
    "$(tomcat.home)$(fs)modules$(fs)jk$(fs)WEB-INF$(fs)lib$(fs)tomcat-jk2.jar",
 "$(tomcat.home)$(fs)modules$(fs)jk$(fs)WEB-INF$(fs)lib$(fs)tomcat-utils.jar",
    "$(tomcat.home)$(fs)webapps(fs)jk$(fs)WEB-INF$(fs)lib$(fs)tomcat-jk2.jar",
 "$(tomcat.home)$(fs)webapps(fs)jk$(fs)WEB-INF$(fs)lib$(fs)tomcat-utils.jar",
    NULL
};

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



int jk_vm_loadJvm(jk_env_t *env, jk_vm_t *p)
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

        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "jni.loadJvmDll()\n");

        if(jni_create_java_vm &&
           jni_get_default_java_vm_init_args &&
           jni_get_created_java_vms) {
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
        jni_get_created_java_vms = ImportSymbol(GetNLMHandle(),
                                                "JNI_GetCreatedJavaVMs");
        jni_get_default_java_vm_init_args =
            ImportSymbol(GetNLMHandle(), "JNI_GetDefaultJavaVMInitArgs");
    }
    if(jni_create_java_vm &&
       jni_get_default_java_vm_init_args &&
       jni_get_created_java_vms) {
        return JK_TRUE;
    }
    return JK_TRUE;
#else 
    void *handle;
    handle = dlopen(p->jvm_dll_path, RTLD_NOW | RTLD_GLOBAL);

    if(handle == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                      "Can't load native library %s : %s\n", p->jvm_dll_path,
                      dlerror());
        return JK_FALSE;
    }

    jni_create_java_vm = dlsym(handle, "JNI_CreateJavaVM");
    jni_get_default_java_vm_init_args =
        dlsym(handle, "JNI_GetDefaultJavaVMInitArgs");
    jni_get_created_java_vms =  dlsym(handle, "JNI_GetCreatedJavaVMs");
        
    if(jni_create_java_vm == NULL ||
       jni_get_default_java_vm_init_args == NULL  ||
       jni_get_created_java_vms == NULL )
    {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                      "jni.loadJvm() Can't resolve symbols %s\n",
                      p->jvm_dll_path );
        dlclose(handle);
        return JK_FALSE;
    }
    /* env->l->jkLog(env, env->l, JK_LOG_INFO,  */
    /*                   "jni.loadJvm() %s symbols resolved\n",
                         p->jvm_dll_path); */
    
    return JK_TRUE;
#endif
}


void *jk_vm_attach(jk_env_t *env, jk_vm_t *p)
{
    JNIEnv *rc = NULL;
    int err;
    JavaVM *jvm = (JavaVM *)p->jvm;
    

#if defined LINUX && defined APACHE2_SIGHACK
    /* [V] This message is important. If there are signal mask issues,    *
     *     the JVM usually hangs when a new thread tries to attach to it  */
    /* XXX do we need to do that on _each_ attach or only when we create
       the vm ??? */
    /*     linux_signal_hack(); */
#endif

    err= (*jvm)->GetEnv( jvm, (void **)&rc, JNI_VERSION_1_2 );
    if( err != 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "vm.attach() GetEnv failed %d\n", err);
    }
    
    err = (*jvm)->AttachCurrentThread(jvm,
#ifdef JNI_VERSION_1_2
                                      (void **)
#endif
                                      &rc, NULL);
    if( err != 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "vm.attach() error %d\n", err);
        return NULL;
    }
    env->l->jkLog(env, env->l, JK_LOG_INFO, "vm.attach() ok\n");
    return rc;
}


void jk_vm_detach(jk_env_t *env, jk_vm_t *p)
{
    int err;
    JavaVM *jvm = (JavaVM *)p->jvm;
    
    if( jvm == NULL ) {
        return;
    }

    err= (*jvm)->DetachCurrentThread(jvm);
    if(err == 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "vm.detach() ok\n");
    } else {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "vm.detach() cannot detach from JVM.\n");
    }
}

static int jk_file_exists(jk_env_t *env, const char *f)
{
    if(f) {
        struct stat st;
        if((0 == stat(f, &st)) && (st.st_mode & S_IFREG)) {
            return JK_TRUE;
        }
    }
    return JK_FALSE;
}

/* Add more guessing, based on various vm layouts */

/* Some guessing - to spare the user ( who might know less
   than we do ).
*/
char* jk_vm_guessJvmDll( jk_env_t *env, jk_map_t *props,
                              jk_vm_t *jniw, char *prefix )
{
    char *jvm;
    jk_pool_t *p=props->pool;
    const char **current=defaultVM_PATH;
    char *libp;
    
    /* Maybe he knows more */
    jvm =jk_map_getStrProp( env, props, NULL, prefix,
                            "jvm_lib", NULL );
    if( jvm!=NULL && jk_file_exists(env, jvm)) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.guessJvmDll() - user specified %s\n", jvm);
        return jvm;
    }

    /* We need at least JAVA_HOME ( either env or in settings )
     */
    while( *current != NULL ) {
        jvm = jk_map_replaceProperties(env, props, p,
                                   (char *)p->pstrdup( env, p, *current ) );

        if( jvm!=NULL && jk_file_exists(env, jvm)) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "jni.guessJvmDll() %s\n", jvm);
            return jvm;
        }

        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.guessJvmDll() failed %s\n", *current);
        current++;
    }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "jni.guessJvmDll() failed\n");
        
    return NULL;
}


int jk_vm_openVM(jk_env_t *env, jk_vm_t *p)
{
    int jvm_version;

#if defined LINUX && defined APACHE2_SIGHACK
    /* [V] This message is important. If there are signal mask issues,    *
     *     the JVM usually hangs when a new thread tries to attach to it  */
    /* XXX do we need to do that on _each_ attach or only when we create
       the vm ??? */
    linux_signal_hack(); 
#endif

    /* That's kind of strange - if we have JNI_VERSION_1_2 I assume
       we also have 1.2, what do we detect ???? */
#ifdef JNI_VERSION_1_2
    jvm_version= detect_jvm_version(env);

    switch(jvm_version) {
    case JNI_VERSION_1_1:
        return open_jvm1(env, p);
    case JNI_VERSION_1_2:
        return open_jvm2(env, p);
    default:
        return JK_FALSE;
    }
#else
    /* [V] Make sure this is _really_ visible */
    #warning -------------------------------------------------------
    #warning NO JAVA 2 HEADERS! SUPPORT FOR JAVA 2 FEATURES DISABLED
    #warning -------------------------------------------------------
    return open_jvm1(env, p, jniEnv);
#endif
}

static int open_jvm1(jk_env_t *env, jk_vm_t *p)
{
    JDK1_1InitArgs vm_args;  
    int err;
    JavaVM *jvm = (JavaVM *)p->jvm;
    JNIEnv *penv;
    
    env->l->jkLog(env, env->l, JK_LOG_INFO, "vm.open1()\n");

    vm_args.version = JNI_VERSION_1_1;

    err= jni_get_default_java_vm_init_args(&vm_args);
    if(0 != err ) {
    	env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "vm.open1() Fail-> can't get default vm init args\n"); 
        return JK_FALSE;
    }
    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "vm.open1() got default jvm args\n"); 

    if(vm_args.classpath) {
        unsigned len = strlen(vm_args.classpath) + 
            strlen(p->tomcat_classpath) +  3;
        char *tmp = p->pool->alloc(env, p->pool, len);
        if(tmp==NULL) { 
            env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                          "Fail-> allocation error for classpath\n"); 
            return JK_FALSE;
        }
        sprintf(tmp, "%s%c%s", p->tomcat_classpath, 
                PATH_SEPERATOR, vm_args.classpath);
        p->tomcat_classpath = tmp;
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

    err=jni_create_java_vm(&jvm, &penv, &vm_args);

    if (JNI_EEXIST == err) {
        int vmCount;
        
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "vm.open1() try to attach to existing vm\n");
        err=jni_get_created_java_vms(&jvm, 1, &vmCount);

        if (NULL == jvm) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "vm.open1() error attaching %d\n", err);
            return JK_FALSE;
        }

        p->jvm=jvm;
            
    } else if( err != 0 ) { 
        env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                      "Fail-> could not create JVM, code: %d \n", err); 
        return JK_FALSE;
    }

    p->jvm=jvm;
    
    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "vm.open1() done\n");
    return JK_TRUE;
}

#ifdef JNI_VERSION_1_2
static int detect_jvm_version(jk_env_t *env)
{
    JDK1_1InitArgs vm_args;

    /* [V] Idea: ask for 1.2. If the JVM is 1.1 it will return 1.1 instead  */
    /*     Note: asking for 1.1 won't work, 'cause 1.2 JVMs will return 1.1 */
    vm_args.version = JNI_VERSION_1_2;

    if(0 != jni_get_default_java_vm_init_args(&vm_args)) {
    	env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "vm.detect() Fail-> can't get default vm init args\n"); 
        return JK_FALSE;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO, 
                  "vm.detect() found: %X\n", vm_args.version);

    return vm_args.version;
}

static char* build_opt_str(jk_env_t *env, jk_pool_t *pool, 
                           char* opt_name, char* opt_value )
{
    unsigned len = strlen(opt_name) + strlen(opt_value) + 2;

    /* [V] IMHO, these should not be deallocated as long as the JVM runs */
    char *tmp = pool->alloc(env, pool, len);

    if(tmp) {
	    sprintf(tmp, "%s%s", opt_name, opt_value);
	    return tmp;
    } else {
	    env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                          "Fail-> build_opt_str allocation error for %s\n",
                          opt_name);
	    return NULL;
    }
}

static char* build_opt_int(jk_env_t *env, jk_pool_t *pool, 
                           char* opt_name, int opt_value )
{
    /* [V] this should suffice even for 64-bit int */
    unsigned len = strlen(opt_name) + 20 + 2;
    /* [V] IMHO, these should not be deallocated as long as the JVM runs */
    char *tmp = pool->alloc(env, pool, len);

    if(tmp) {
        sprintf(tmp, "%s%d", opt_name, opt_value);
        return tmp;
    } else {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                      "Fail-> build_opt_int allocation error for %s\n",
                      opt_name);
        return NULL;
    }
}


static int open_jvm2(jk_env_t *env, jk_vm_t *p)
{
    JavaVMInitArgs vm_args;
    JNIEnv *penv;
    JavaVMOption options[100];
    JavaVM *jvm;
    int optn = 0, err;
    char* tmp;
    
    vm_args.version = JNI_VERSION_1_2;
    vm_args.options = options;

    if(p->tomcat_classpath) {
    	env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.openJvm2() CLASSPATH=%s\n", p->tomcat_classpath);
        tmp = build_opt_str(env, p->pool, "-Djava.class.path=",
                            p->tomcat_classpath);
        null_check(tmp);
        options[optn++].optionString = tmp;
    }

    if(p->tomcat_mx) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.openJvm2() -Xmx %d\n", p->tomcat_mx);
    	tmp = build_opt_int(env, p->pool, "-Xmx", p->tomcat_mx);
        null_check(tmp);
        options[optn++].optionString = tmp;
    }

    if(p->tomcat_ms) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.openJvm2() -Xms %d\n", p->tomcat_ms);
        tmp = build_opt_int(env, p->pool, "-Xms", p->tomcat_ms);
        null_check(tmp);
        options[optn++].optionString = tmp;
    }

    if(p->sysprops) {
        int i = 0;
        while(p->sysprops[i]) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "jni.openJvm2() -D%s\n", p->sysprops[i]);
            tmp = build_opt_str(env, p->pool, "-D", p->sysprops[i]);
            null_check(tmp);
            options[optn++].optionString = tmp;
            i++;
        }
    }

    if(p->java2opts) {
        int i=0;
        
        while(p->java2opts[i]) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "jni.openJvm2() java2opts: %s\n", p->java2opts[i]);
            /* Pass it "as is" */
            options[optn++].optionString = p->java2opts[i++];
        }
    }

    vm_args.nOptions = optn;
    
    if(p->java2lax) {
    	env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.openJvm2()  ignore unknown options\n");
        vm_args.ignoreUnrecognized = JNI_TRUE;
    } else {
        vm_args.ignoreUnrecognized = JNI_FALSE;
    }

    err=jni_create_java_vm(&jvm, &penv, &vm_args);
    
    if (JNI_EEXIST == err) {
        int vmCount;
        
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "vm.open2() try to attach to existing vm.\n");

        err=jni_get_created_java_vms(&jvm, 1, &vmCount);
        
        if (NULL == jvm) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "vm.open1() error attaching %d\n", err);
            return JK_FALSE;
        }

        p->jvm=jvm;
        return JK_TRUE;
    } else if( err!=0 ) {
    	env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "Fail-> could not create JVM, code: %d \n", err); 
        return JK_FALSE;
    }

    p->jvm=jvm;

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "vm.open2() done\n");

    return JK_TRUE;
}
#endif

/** 'Guess' tomcat.home from properties or
    env. Set it as 'tomcat.home'. ( XXX try 'standard'
    locations, relative to apache home, etc )
*/
static char *guessTomcatHome(jk_env_t *env, jk_map_t *props )
{
    /* TOMCAT_HOME or CATALINA_HOME */
    char *tomcat_home;

    tomcat_home=props->get( env, props, "TOMCAT_HOME" );
    if( tomcat_home==NULL ) {
        tomcat_home=props->get( env, props, "CATALINA_HOME" );
    }
    if( tomcat_home == NULL ) {
        tomcat_home=getenv( "TOMCAT_HOME" );
    }
    if( tomcat_home == NULL ) {
        tomcat_home=getenv( "CATALINA_HOME" );
    }
    if( tomcat_home == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO, "Can't find tomcat\n");
        return NULL;
    }

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "jni.guessTomcatHome() %s\n", tomcat_home);
    
    props->put(env, props, "tomcat.home",
               props->pool->pstrdup( env, props->pool, tomcat_home ), NULL);
    
    return tomcat_home;
}

static char *guessClassPath(jk_env_t *env, jk_map_t *props )
{
    /* Guess 3.3, 4.0, 4.1 'standard' locations */
    char *jkJar;
    jk_pool_t *p=props->pool;
    const char **current=defaultJK_PATH;

    guessTomcatHome( env, props );
    
    while( *current != NULL ) {
        jkJar = jk_map_replaceProperties(env, props, p,
                                       (char *)p->pstrdup( env, p, *current ));
        
        if( jkJar!=NULL && jk_file_exists(env, jkJar)) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "jni.guessJkJar() %s\n", jkJar);
            return jkJar;
        }

        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.guessJkJar() failed %s\n", *current);
        current++;
    }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "jni.guessJkJar() failed\n");
        
    return NULL;
}


/** Initialize the vm properties
 */
int jk_vm_init(jk_env_t *env, jk_vm_t *_this,
                    jk_map_t *props, char *prefix)
{
    char *str_config;
    int int_config;
    
    _this->tomcat_mx= jk_map_getIntProp( env, props, NULL, prefix, "mx", 0 );
    _this->tomcat_ms= jk_map_getIntProp( env, props, NULL, prefix, "ms", 0 );

    _this->tomcat_classpath= jk_map_getStrProp( env, props, NULL, prefix,
                                                "class_path", NULL );

    if(_this->tomcat_classpath == NULL ) {
        _this->tomcat_classpath = guessClassPath( env, props );
    }
    
    if(_this->tomcat_classpath == NULL ) {
        char *cp=getenv( "CLASSPATH" );
        if( cp!=NULL ) {
            _this->tomcat_classpath=props->pool->pstrdup( env, props->pool,cp);
            env->l->jkLog(env, env->l, JK_LOG_INFO, "Using CLASSPATH %s\n",cp);
        }
    }
    if( _this->tomcat_classpath == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, "Fail-> no classpath\n");
        return JK_FALSE;
    }

    _this->jvm_dll_path=jk_vm_guessJvmDll( env, props, _this, prefix  );

    if(!_this->jvm_dll_path ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "Fail-> no jvm_dll_path\n");
        return JK_FALSE;
    }
    env->l->jkLog(env, env->l, JK_LOG_INFO, "Jni lib: %s\n",
                  _this->jvm_dll_path);

    str_config=  jk_map_getStrProp( env, props, NULL, prefix,
                                    "sysprops", NULL ); 
    if(str_config!= NULL ) {
        _this->sysprops  = jk_map_split( env, NULL, _this->pool,
                                         str_config, "*", NULL);
    }

#ifdef JNI_VERSION_1_2
    str_config= jk_map_getStrProp(env, props, NULL,prefix ,"java2opts",NULL );
    if( str_config != NULL ) {
    	env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.validate() java2opts %s\n", str_config); 
        _this->java2opts = jk_map_split( env, NULL, _this->pool,
                                     str_config, "*", NULL);
    }
    int_config= jk_map_getIntProp( env, props, NULL, prefix, "java2lax", -1 );
    if(int_config != -1 ) {
        _this->java2lax = int_config ? JK_TRUE : JK_FALSE;
    }
#endif
    return JK_TRUE;
}
     
int jk_vm_factory( jk_env_t *env, jk_pool_t *pool,
                        void **result,
                        char *type, char *name)
{
    jk_vm_t *_this;

    _this = (jk_vm_t *)pool->calloc(env, pool, sizeof(jk_vm_t ));

    _this->pool=pool;

    _this->tomcat_classpath      = NULL;
    _this->jvm_dll_path          = NULL;
    _this->tomcat_ms             = 0;
    _this->tomcat_mx             = 0;
    _this->sysprops              = NULL;
#ifdef JNI_VERSION_1_2
    _this->java2opts             = NULL;
    _this->java2lax              = JK_TRUE;
#endif

    _this->open=jk_vm_openVM;
    _this->load=jk_vm_loadJvm;
    _this->init=jk_vm_init;
    _this->attach=jk_vm_attach;
    _this->detach=jk_vm_detach;
    
    *result=_this;
    return JK_TRUE;
}

/* -------------------- Less usefull -------------------- */

/* DEPRECATED */

static const char *defaultLIB_PATH[]={
    "$(JAVA_HOME)$(fs)jre$(fs)lib$(fs)$(arch)$(fs)classic",
    "$(JAVA_HOME)$(fs)jre$(fs)lib$(fs)$(arch)",
    "$(JAVA_HOME)$(fs)jre$(fs)lib$(fs)$(arch)$(fs)native_threads",
    NULL
};

static int jk_dir_exists(jk_env_t *env, const char *f)
{
    if(f) {
        struct stat st;
        if((0 == stat(f, &st)) && (st.st_mode & S_IFDIR)) {
            return JK_TRUE;
        }
    }
    return JK_FALSE;
}

static void jk_vm_appendLibpath(jk_env_t *env, jk_pool_t *pool, 
                                     const char *libpath)
{
    char *envVar = NULL;
    char *current = getenv(PATH_ENV_VARIABLE);

    if(current) {
        envVar = pool->alloc(env, pool, strlen(PATH_ENV_VARIABLE) + 
                          strlen(current) + 
                          strlen(libpath) + 5);
        if(envVar) {
            sprintf(envVar, "%s=%s%c%s", 
                    PATH_ENV_VARIABLE, 
                    libpath, 
                    PATH_SEPERATOR, 
                    current);
        }
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "jni.appendLibPath() %s %s %s\n",
                      current, libpath, envVar);
    } else {
        envVar = pool->alloc(env, pool, strlen(PATH_ENV_VARIABLE) +
                             strlen(libpath) + 5);
        if(envVar) {
            sprintf(envVar, "%s=%s", PATH_ENV_VARIABLE, libpath);
        }
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "jni.appendLibPath() %s %s %s\n",
                      current, libpath, envVar);
    }

    if(envVar) {
        putenv(envVar);
    }
}


static void addDefaultLibPaths(jk_env_t *env, jk_map_t *props,
                               jk_vm_t *jniw, char *prefix )
{
    jk_pool_t *p=props->pool;
    const char **current=defaultLIB_PATH;
    char *libp;

    while( *current != NULL ) {
        libp = jk_map_replaceProperties(env, props, p,
                                        p->pstrdup( env, p, *current ));
        if( libp!=NULL && jk_dir_exists(env, libp)) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "jni.addDefaultLibPaths() %s\n", libp);
            jk_vm_appendLibpath(env, p, libp);
        } else {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "jni.addDefaultLibPaths() failed %s\n", libp);
        }
        current++;
    }
}

jobject jk_vm_newObject(jk_env_t *env, jk_vm_t *p,
                             JNIEnv *jniEnv, char *className, jclass objClass)
{
    jmethodID  constructor_method_id;
    jobject objInst;

    constructor_method_id = (*jniEnv)->GetMethodID(jniEnv,
                                                   objClass,
                                                   "<init>", /* method name */
                                                   "()V");   /* method sign */
    if(!constructor_method_id) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                      "Can't find constructor\n");
        return NULL;
    }

    objInst = (*jniEnv)->NewObject(jniEnv, objClass,
                                   constructor_method_id);
    
    if(objInst == NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                      "Can't create new bridge object\n");
        return NULL;
    }

    objInst =
        (jobject)(*jniEnv)->NewGlobalRef(jniEnv, objInst);
    
    if(objInst==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "Can't create global ref to bridge object\n");
        return NULL;
    }

    env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                  "In get_bridge_object, bridge built, done\n");
    return objInst;
}

