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

#include "jk_workerEnv.h"
#include "jk_env.h"
#include "jk_bean.h"

#ifdef HAVE_JNI

#include "jk_global.h"
#include "jk_vm.h"
#include "jk_config.h"


#if defined LINUX && defined APACHE2_SIGHACK
#include <pthread.h>
#include <signal.h>
#include <bits/signum.h>
#endif

#if !defined(WIN32) && !defined(NETWARE)
#include <dlfcn.h>
#endif
#ifdef NETWARE
#include <nwthread.h>
#include <nwadv.h>
#endif

#include <jni.h>

#ifdef APR_HAS_DSO
#include "apr_dso.h"
#endif


#ifndef JNI_VERSION_1_2

#warning -------------------------------------------------------
#warning JAVA 1.1 IS NO LONGER SUPPORTED 
#warning -------------------------------------------------------

int JK_METHOD jk2_vm_factory(jk_env_t *env, jk_pool_t *pool,
                   jk_bean_t *result,
                   char *type, char *name)
{
    return JK_ERR;
}

#else


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

static int jk2_detect_jvm_version(jk_env_t *env);
static int jk2_open_jvm2(jk_env_t *env, jk_vm_t *p);

jint (JNICALL *jni_get_default_java_vm_init_args)(void *) = NULL;
jint (JNICALL *jni_create_java_vm)(JavaVM **, JNIEnv **, void *) = NULL;
jint (JNICALL *jni_get_created_java_vms)(JavaVM **, int, int *) = NULL;

/* Guessing - try all those to find the right dll
 */
static const char *defaultVM_PATH[]={
    "${JAVA_HOME}${fs}jre${fs}bin${fs}classic${fs}libjvm.${so}",
    "${JAVA_HOME}${fs}jre${fs}bin${fs}client${fs}jvm.${so}",
    "${JAVA_HOME}${fs}jre${fs}lib${fs}${arch}${fs}classic${fs}libjvm.${so}",
    "${JAVA_HOME}${fs}jre${fs}lib${fs}${arch}${fs}client${fs}libjvm.${so}",
    "${JAVA_HOME}${fs}jre${fs}bin${fs}classic${fs}jvm.${so}",
    NULL
};

#if defined LINUX && defined APACHE2_SIGHACK
static void jk2_linux_signal_hack() 
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

static void jk2_print_signals( sigset_t *sset) {
    int sig;
    for (sig = 1; sig < 20; sig++) 
	{ if (sigismember(sset, sig)) {printf( " %d", sig);} }
    printf( "\n");
}
#endif


/** Load the VM. Must be called after init.
 */
static int jk2_vm_loadJvm(jk_env_t *env, jk_vm_t *jkvm)
{

#if defined(HAS_APR) && defined(APR_HAS_DSO)
    apr_dso_handle_t *dsoHandle;
    apr_status_t rc;
    apr_pool_t *aprPool;

    aprPool= (apr_pool_t *)env->getAprPool( env );

    if( aprPool==NULL )
        return JK_FALSE;

    /* XXX How do I specify RTLD_NOW and RTLD_GLOBAL ? */
    rc=apr_dso_load( &dsoHandle, jkvm->jvm_dll_path, aprPool );
    
    if(rc == APR_SUCCESS ) {
        rc= apr_dso_sym( (apr_dso_handle_sym_t *)&jni_create_java_vm, dsoHandle, "JNI_CreateJavaVM");
    }

    if( rc == APR_SUCCESS ) {
        rc=apr_dso_sym( (apr_dso_handle_sym_t *)&jni_get_default_java_vm_init_args, dsoHandle,
                        "JNI_GetDefaultJavaVMInitArgs");
    }
    
    if( rc == APR_SUCCESS ) {
        rc=apr_dso_sym( (apr_dso_handle_sym_t *)&jni_get_created_java_vms,
                        dsoHandle, "JNI_GetCreatedJavaVMs");
    }
        
    if( rc!= APR_SUCCESS ) {
        char buf[256];
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "Can't load native library %s : %s\n", jkvm->jvm_dll_path,
                      apr_dso_error(dsoHandle, buf, 256));
        return JK_ERR;
    }
    
    if(jni_create_java_vm == NULL ||
       jni_get_default_java_vm_init_args == NULL  ||
       jni_get_created_java_vms == NULL )
    {
        env->l->jkLog(env, env->l, JK_LOG_EMERG, 
                      "jni.loadJvm() Can't resolve symbols %s\n",
                      jkvm->jvm_dll_path );
        apr_dso_unload( dsoHandle);
        return JK_ERR;
    }

    if( jkvm->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_INFO,  
                      "jni.loadJvm() %s symbols resolved\n",
                      jkvm->jvm_dll_path); 
    
    return JK_OK;
#else
    env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                  "Can't load jvm, no apr support %s\n");

    return JK_ERR;
#endif
    
/* #ifdef WIN32 */
/*     HINSTANCE hInst = LoadLibrary(jkvm->jvm_dll_path); */
/*     if(hInst) { */
/*         (FARPROC)jni_create_java_vm =  */
/*             GetProcAddress(hInst, "JNI_CreateJavaVM"); */

/*         (FARPROC)jni_get_created_java_vms =  */
/*             GetProcAddress(hInst, "JNI_GetCreatedJavaVMs"); */

/*         (FARPROC)jni_get_default_java_vm_init_args =  */
/*             GetProcAddress(hInst, "JNI_GetDefaultJavaVMInitArgs"); */

/*         env->l->jkLog(env, env->l, JK_LOG_INFO,  */
/*                       "jni.loadJvmDll()\n"); */

/*         if(jni_create_java_vm && */
/*            jni_get_default_java_vm_init_args && */
/*            jni_get_created_java_vms) { */
/*             return JK_OK; */
/*         } */

/*         FreeLibrary(hInst); */
/*     } */
/*     return JK_OK; */
/* #elif defined(NETWARE) */
/*     int javaNlmHandle = FindNLMHandle("JVM"); */
/*     if (0 == javaNlmHandle) { */
        /* if we didn't get a handle, try to load java and retry getting the */
        /* handle */
/*         spawnlp(P_NOWAIT, "JVM.NLM", NULL); */
/*         ThreadSwitchWithDelay(); */
/*         javaNlmHandle = FindNLMHandle("JVM"); */
/*         if (0 == javaNlmHandle) */
/*             printf("Error loading Java."); */

/*     } */
/*     if (0 != javaNlmHandle) { */
/*         jni_create_java_vm = ImportSymbol(GetNLMHandle(), "JNI_CreateJavaVM"); */
/*         jni_get_created_java_vms = ImportSymbol(GetNLMHandle(), */
/*                                                 "JNI_GetCreatedJavaVMs"); */
/*         jni_get_default_java_vm_init_args = */
/*             ImportSymbol(GetNLMHandle(), "JNI_GetDefaultJavaVMInitArgs"); */
/*     } */
/*     if(jni_create_java_vm && */
/*        jni_get_default_java_vm_init_args && */
/*        jni_get_created_java_vms) { */
/*         return JK_OK; */
/*     } */
/*     return JK_OK; */
/* #else  */
/*     void *handle; */
/*     handle = dlopen(jkvm->jvm_dll_path, RTLD_NOW | RTLD_GLOBAL); */

/*     if(handle == NULL ) { */
/*         env->l->jkLog(env, env->l, JK_LOG_EMERG,  */
/*                       "Can't load native library %s : %s\n", jkvm->jvm_dll_path, */
/*                       dlerror()); */
/*         return JK_ERR; */
/*     } */

/*     jni_create_java_vm = dlsym(handle, "JNI_CreateJavaVM"); */
/*     jni_get_default_java_vm_init_args = */
/*         dlsym(handle, "JNI_GetDefaultJavaVMInitArgs"); */
/*     jni_get_created_java_vms =  dlsym(handle, "JNI_GetCreatedJavaVMs"); */
        
/*     if(jni_create_java_vm == NULL || */
/*        jni_get_default_java_vm_init_args == NULL  || */
/*        jni_get_created_java_vms == NULL ) */
/*     { */
/*         env->l->jkLog(env, env->l, JK_LOG_EMERG,  */
/*                       "jni.loadJvm() Can't resolve symbols %s\n", */
/*                       jkvm->jvm_dll_path ); */
/*         dlclose(handle); */
/*         return JK_ERR; */
/*     } */
    /* env->l->jkLog(env, env->l, JK_LOG_INFO,  */
    /*                   "jni.loadJvm() %s symbols resolved\n",
                         jkvm->jvm_dll_path); */
    
/*     return JK_OK; */
/* #endif */

}


static void *jk2_vm_attach(jk_env_t *env, jk_vm_t *jkvm)
{
    JNIEnv *rc = NULL;
    int err;
    JavaVM *jvm = (JavaVM *)jkvm->jvm;
    
     if( jvm == NULL ) return NULL;
    
#if defined LINUX && defined APACHE2_SIGHACK
    /* [V] This message is important. If there are signal mask issues,    *
     *     the JVM usually hangs when a new thread tries to attach to it  */
    /* XXX do we need to do that on _each_ attach or only when we create
       the vm ??? */
    /*     jk2_linux_signal_hack(); */
#endif

    err= (*jvm)->GetEnv( jvm, (void **)&rc, JNI_VERSION_1_2 );
    if( ( err != 0 ) &&
        ( err != JNI_EDETACHED) ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "vm.attach() GetEnv failed %d\n", err);
    }
    
    err = (*jvm)->AttachCurrentThread(jvm,
                                      (void **)
                                      &rc, NULL);
    if( err != 0 ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "vm.attach() error %d\n", err);
        return NULL;
    }
    if( jkvm->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_INFO, "vm.attach() ok\n");
    return rc;
}


static void jk2_vm_detach(jk_env_t *env, jk_vm_t *jkvm)
{
    int err;
    JavaVM *jvm = (JavaVM *)jkvm->jvm;
    
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

static int jk2_file_exists(jk_env_t *env, const char *f)
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
static char* jk2_vm_guessJvmDll(jk_env_t *env, jk_map_t *props,
                         jk_vm_t *jkvm)
{
    char *jvm;
    jk_pool_t *p=props->pool;
    const char **current=defaultVM_PATH;
    
    /* We need at least JAVA_HOME ( either env or in settings )
     */
    while( *current != NULL ) {
        jvm = jk2_config_replaceProperties(env, props, p,
                                           (char *)p->pstrdup( env, p, *current ) );

        if( jvm!=NULL && jk2_file_exists(env, jvm)) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "jni.guessJvmDll() %s\n", jvm);
            return jvm;
        }

        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "jni.guessJvmDll() failed %s\n", jvm);
        current++;
    }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "jni.guessJvmDll() failed\n");
        
    return NULL;
}


static int jk2_vm_initVM(jk_env_t *env, jk_vm_t *jkvm)
{
    int jvm_version;
    JDK1_1InitArgs vm_args11;
    jk_map_t *props=jkvm->properties;
    JavaVMInitArgs vm_args;
    JNIEnv *penv;
    JavaVMOption options[100];
    JavaVM *jvm;
    int optn = 0, err;
    
    /** Make sure we have the vm dll */
    if( jkvm->jvm_dll_path ==NULL ||
        ! jk2_file_exists(env, jkvm->jvm_dll_path )) {
        jkvm->jvm_dll_path=jk2_vm_guessJvmDll( env, props, jkvm  );
    }
    
    if( jkvm->jvm_dll_path==NULL ) {
        jkvm->jvm_dll_path=
            jk2_config_replaceProperties(env, props, props->pool,"libjvm.${so}");
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "vm.init(): no jvm_dll_path, will use LD_LIBRARY_PATH %s\n",
                      jkvm->jvm_dll_path);
    } else {
        env->l->jkLog(env, env->l, JK_LOG_INFO, "vm.init(): Jni lib: %s\n",
                      jkvm->jvm_dll_path );
    }

    err=jk2_vm_loadJvm(env, jkvm );

    if( err!=JK_OK ) {
        env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "jni.loadJvm() Error - can't load jvm dll\n");
        /* [V] no detach needed here */
        return JK_ERR;
    }

    
#if defined LINUX && defined APACHE2_SIGHACK
    /* [V] This message is important. If there are signal mask issues,    *
     *     the JVM usually hangs when a new thread tries to attach to it  */
    /* XXX do we need to do that on _each_ attach or only when we create
       the vm ??? */
    jk2_linux_signal_hack(); 
#endif

    /* That's kind of strange - if we have JNI_VERSION_1_2 I assume
       we also have 1.2, what do we detect ???? */

    /* [V] Idea: ask for 1.2. If the JVM is 1.1 it will return 1.1 instead  */
    /*     Note: asking for 1.1 won't work, 'cause 1.2 JVMs will return 1.1 */
    vm_args11.version = JNI_VERSION_1_2;

    if(0 != jni_get_default_java_vm_init_args(&vm_args11)) {
    	env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "vm.detect() Fail-> can't get default vm init args\n"); 
        return JK_ERR;
    }

    jvm_version= vm_args11.version;

    if(jvm_version != JNI_VERSION_1_2 ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "vm.detect() found: %X expecting 1.2\n", jvm_version);
        return JK_ERR;
    }

    vm_args.version = JNI_VERSION_1_2;
    vm_args.options = options;

    while(jkvm->options[optn]) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "vm.openJvm2() Option: %s\n", jkvm->options[optn]);
        /* Pass it "as is" */
        options[optn].optionString = jkvm->options[optn];
        optn++;
    }

    vm_args.nOptions = optn;
    
    vm_args.ignoreUnrecognized = JNI_TRUE;

    err=jni_create_java_vm(&jvm, &penv, &vm_args);
    
    if (JNI_EEXIST == err) {
        int vmCount;
        
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "vm.open2() try to attach to existing vm.\n");

        err=jni_get_created_java_vms(&jvm, 1, &vmCount);
        
        if (NULL == jvm) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "vm.open1() error attaching %d\n", err);
            return JK_ERR;
        }

        jkvm->jvm=jvm;
        return JK_OK;
    } else if( err!=0 ) {
    	env->l->jkLog(env, env->l, JK_LOG_EMERG,
                      "Fail-> could not create JVM, code: %d \n", err); 
        return JK_ERR;
    }

    jkvm->jvm=jvm;

    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "vm.open2() done\n");

    return JK_OK;
}

static int JK_METHOD
jk2_jk_vm_setProperty(jk_env_t *env, jk_bean_t *mbean, char *name, void *valueP )
{
    jk_vm_t *jkvm=mbean->object;
    char *value=valueP;
    
    if( strcmp( name, "OPT" )==0 ) {
        jkvm->options[jkvm->nOptions]=value;
        jkvm->nOptions++;
    } else if( strcmp( name, "JVM" )==0 ) {
        jkvm->jvm_dll_path=value;
    } else {
        return JK_ERR;
    }

    return JK_OK;
}


int JK_METHOD jk2_vm_factory(jk_env_t *env, jk_pool_t *pool,
                             jk_bean_t *result, char *type, char *name)
{
    jk_vm_t *jkvm;
    jk_workerEnv_t *workerEnv;
    
    jkvm = (jk_vm_t *)pool->calloc(env, pool, sizeof(jk_vm_t ));

    jkvm->pool=pool;

    jkvm->jvm_dll_path = NULL;
    jkvm->options = pool->calloc( env, pool, 64 * sizeof( char *));
    jkvm->nOptions =0;

    jkvm->init=jk2_vm_initVM;
    jkvm->attach=jk2_vm_attach;
    jkvm->detach=jk2_vm_detach;
    
    result->object=jkvm;
    result->setAttribute=jk2_jk_vm_setProperty;
    jkvm->mbean=result;

    workerEnv=env->getByName( env, "workerEnv" );
    jkvm->properties=workerEnv->initData;

    workerEnv->vm=jkvm;
    
    return JK_OK;
}

#endif /* Java2 */

#else /* HAVE_JNI */

int JK_METHOD jk2_vm_factory(jk_env_t *env, jk_pool_t *pool,
                             jk_bean_t *result, char *type, char *name)
{
    result->disabled=1;
    return JK_OK;
}
#endif
