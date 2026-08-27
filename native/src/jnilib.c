/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tcn.h"
#include "apr_version.h"
#include "apr_file_io.h"
#include "apr_mmap.h"
#include "apr_atomic.h"
#include "apr_poll.h"

#include "tcn_version.h"

#if defined(WIN32)
#include <Windows.h>
#elif defined(DARWIN)
/* Included intentionally for the sake of completeness */
#include <pthread.h>
#elif defined(__FreeBSD__)
#include <pthread_np.h>
#elif defined(__linux__)
#include <sys/syscall.h>
#elif defined(__hpux)
#include <sys/lwp_id.h>
#else
#include <pthread.h>
#endif

apr_pool_t *tcn_global_pool = NULL;
static JavaVM     *tcn_global_vm = NULL;

static jclass    jString_class;
static jmethodID jString_init;
static jmethodID jString_getBytes;

int tcn_parent_pid = 0;

/* Called by the JVM when APR_JAVA is loaded */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    void   *ppe;
    apr_version_t apv;
    int apvn;

    UNREFERENCED(reserved);
    if ((*vm)->GetEnv(vm, &ppe, JNI_VERSION_1_4)) {
        return JNI_ERR;
    }
    tcn_global_vm = vm;
    env           = (JNIEnv *)ppe;
    /* Before doing anything else check if we have a valid
     * APR version. We need version 1.7.0 as minimum.
     */
    apr_version(&apv);
    apvn = apv.major * 1000 + apv.minor * 100 + apv.patch;
    if (apvn < 1700) {
        tcn_Throw(env, "Unsupported APR version %s: this tcnative requires at least 1.7.0",
                  apr_version_string());
        return JNI_ERR;
    }

    /* Initialize global java.lang.String class */
    TCN_LOAD_CLASS(env, jString_class, "java/lang/String", JNI_ERR);
    TCN_GET_METHOD(env, jString_class, jString_init,
                   "<init>", "([B)V", JNI_ERR);
    TCN_GET_METHOD(env, jString_class, jString_getBytes,
                   "getBytes", "()[B", JNI_ERR);

#ifdef WIN32
    {
        char *ppid = getenv(TCN_PARENT_IDE);
        if (ppid)
            tcn_parent_pid = atoi(ppid);
    }
#else
    tcn_parent_pid = getppid();
#endif

    return  JNI_VERSION_1_4;
}


/* Called by the JVM before the APR_JAVA is unloaded */
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    void   *ppe;

    UNREFERENCED(reserved);

    if ((*vm)->GetEnv(vm, &ppe, JNI_VERSION_1_2)) {
        return;
    }
    if (tcn_global_pool) {
        env  = (JNIEnv *)ppe;
        TCN_UNLOAD_CLASS(env, jString_class);
        apr_terminate();
    }
}

jstring tcn_new_stringn(JNIEnv *env, const char *str, size_t l)
{
    jstring result;
    jbyteArray bytes = 0;

    if (!str)
        return NULL;
    if ((*env)->EnsureLocalCapacity(env, 2) < 0) {
        return NULL; /* out of memory error */
    }
    bytes = (*env)->NewByteArray(env, l);
    if (bytes != NULL) {
        (*env)->SetByteArrayRegion(env, bytes, 0, l, (jbyte *)str);
        result = (*env)->NewObject(env, jString_class, jString_init, bytes);
        (*env)->DeleteLocalRef(env, bytes);
        return result;
    } /* else fall through */
    return NULL;
}

jstring tcn_new_string(JNIEnv *env, const char *str)
{
    if (!str) {
        return NULL;
    } else {
        if ((*env)->EnsureLocalCapacity(env, 1) < 0) {
            return NULL; /* out of memory error */
        }
        return (*env)->NewStringUTF(env, str);
    }
}

char *tcn_get_string(JNIEnv *env, jstring jstr)
{
    jbyteArray bytes = NULL;
    jthrowable exc;
    char *result = NULL;

    if ((*env)->EnsureLocalCapacity(env, 2) < 0) {
        return NULL; /* out of memory error */
    }
    bytes = (*env)->CallObjectMethod(env, jstr, jString_getBytes);
    exc = (*env)->ExceptionOccurred(env);
    if (!exc) {
        jint len = (*env)->GetArrayLength(env, bytes);
        result = (char *)malloc(len + 1);
        if (result == NULL) {
            TCN_THROW_OS_ERROR(env);
            (*env)->DeleteLocalRef(env, bytes);
            return 0;
        }
        (*env)->GetByteArrayRegion(env, bytes, 0, len, (jbyte *)result);
        result[len] = '\0'; /* NULL-terminate */
    }
    else {
        (*env)->DeleteLocalRef(env, exc);
    }
    (*env)->DeleteLocalRef(env, bytes);

    return result;
}

char *tcn_strdup(JNIEnv *env, jstring jstr)
{
    char *result = NULL;
    const char *cjstr;

    cjstr = (const char *)((*env)->GetStringUTFChars(env, jstr, 0));
    if (cjstr) {
        result = strdup(cjstr);
        (*env)->ReleaseStringUTFChars(env, jstr, cjstr);
    }
    return result;
}

char *tcn_pstrdup(JNIEnv *env, jstring jstr, apr_pool_t *pool)
{
    char *result = NULL;
    const char *cjstr;

    cjstr = (const char *)((*env)->GetStringUTFChars(env, jstr, 0));
    if (cjstr) {
        result = apr_pstrdup(pool, cjstr);
        (*env)->ReleaseStringUTFChars(env, jstr, cjstr);
    }
    return result;
}

TCN_IMPLEMENT_CALL(jboolean, Library, initialize)(TCN_STDARGS)
{

    UNREFERENCED_STDARGS;
    if (!tcn_global_pool) {
        apr_initialize();
        if (apr_pool_create(&tcn_global_pool, NULL) != APR_SUCCESS) {
            return JNI_FALSE;
        }
        apr_atomic_init(tcn_global_pool);
    }
    return JNI_TRUE;
}

TCN_IMPLEMENT_CALL(void, Library, terminate)(TCN_STDARGS)
{

    UNREFERENCED_STDARGS;
    if (tcn_global_pool) {
        apr_pool_t *p = tcn_global_pool;
        tcn_global_pool = NULL;
        apr_pool_destroy(p);
        apr_terminate();
    }
}

TCN_IMPLEMENT_CALL(jint, Library, version)(TCN_STDARGS, jint what)
{
    apr_version_t apv;

    UNREFERENCED_STDARGS;
    apr_version(&apv);

    switch (what) {
        case 0x01:
            return TCN_MAJOR_VERSION;
        break;
        case 0x02:
            return TCN_MINOR_VERSION;
        break;
        case 0x03:
            return TCN_PATCH_VERSION;
        break;
        case 0x04:
            return TCN_IS_DEV_VERSION;
        break;
        case 0x11:
            return apv.major;
        break;
        case 0x12:
            return apv.minor;
        break;
        case 0x13:
            return apv.patch;
        break;
        case 0x14:
            return apv.is_dev;
        break;
    }
    return 0;
}

TCN_IMPLEMENT_CALL(jstring, Library, versionString)(TCN_STDARGS)
{
    UNREFERENCED(o);
    return AJP_TO_JSTRING(TCN_VERSION_STRING);
}

TCN_IMPLEMENT_CALL(jstring, Library, aprVersionString)(TCN_STDARGS)
{
    UNREFERENCED(o);
    return AJP_TO_JSTRING(apr_version_string());
}

apr_pool_t *tcn_get_global_pool(void)
{
    if (!tcn_global_pool) {
        if (apr_pool_create(&tcn_global_pool, NULL) != APR_SUCCESS) {
            return NULL;
        }
        apr_atomic_init(tcn_global_pool);
    }
    return tcn_global_pool;
}

jclass tcn_get_string_class(void)
{
    return jString_class;
}

JavaVM * tcn_get_java_vm(void)
{
    return tcn_global_vm;
}

jint tcn_get_java_env(JNIEnv **env)
{
    if ((*tcn_global_vm)->GetEnv(tcn_global_vm, (void **)env,
                                 JNI_VERSION_1_4)) {
        return JNI_ERR;
    }
    return JNI_OK;
}

unsigned long tcn_get_thread_id(void)
{
    /* OpenSSL needs this to return an unsigned long.  On OS/390, the pthread
     * id is a structure twice that big.  Use the TCB pointer instead as a
     * unique unsigned long.
     */
#ifdef __MVS__
    struct PSA {
        char unmapped[540];
        unsigned long PSATOLD;
    } *psaptr = 0;

    return psaptr->PSATOLD;
#elif defined(WIN32)
    return (unsigned long)GetCurrentThreadId();
#elif defined(DARWIN)
    uint64_t tid;
    pthread_threadid_np(NULL, &tid);
    return (unsigned long)tid;
#elif defined(__FreeBSD__)
    return (unsigned long)pthread_getthreadid_np();
#elif defined(__linux__)
    return (unsigned long)syscall(SYS_gettid);
#elif defined(__hpux)
    return (unsigned long)_lwp_self();
#else
    return (unsigned long)pthread_self();
#endif
}

