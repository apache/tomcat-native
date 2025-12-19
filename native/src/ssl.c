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
#include "apr_file_io.h"
#include "apr_thread_mutex.h"
#include "apr_atomic.h"
#include "apr_poll.h"

#include "ssl_private.h"

static int ssl_initialized = 0;
static char *ssl_global_rand_file = NULL;
extern apr_pool_t *tcn_global_pool;

tcn_pass_cb_t tcn_password_callback;

static BIO *key_log_file = NULL;

static void ssl_keylog_callback(const SSL *ssl, const char *line)
{
    if (key_log_file && line && *line) {
        BIO_puts(key_log_file, line);
        BIO_puts(key_log_file, "\n");
    }
}

/* From netty-tcnative */
static jclass byteArrayClass;
static jclass stringClass;

void SSL_callback_add_keylog(SSL_CTX *ctx)
{
    if (key_log_file) {
        SSL_CTX_set_keylog_callback(ctx, ssl_keylog_callback);
    }
}

static void init_bio_methods(void);
static void free_bio_methods(void);

TCN_IMPLEMENT_CALL(jint, SSL, version)(TCN_STDARGS)
{
    UNREFERENCED_STDARGS;
    return OpenSSL_version_num();
}

TCN_IMPLEMENT_CALL(jstring, SSL, versionString)(TCN_STDARGS)
{
    UNREFERENCED(o);
    return AJP_TO_JSTRING(OpenSSL_version(OPENSSL_VERSION));
}

/*
 *  the various processing hooks
 */
static apr_status_t ssl_init_cleanup(void *data)
{
    UNREFERENCED(data);

    if (!ssl_initialized)
        return APR_SUCCESS;
    ssl_initialized = 0;

    free_bio_methods();

    /* Openssl v1.1+ handles all termination automatically. */

    if (key_log_file) {
        BIO_free(key_log_file);
        key_log_file = NULL;
    }

    /* Don't call ERR_free_strings here; ERR_load_*_strings only
     * actually load the error strings once per process due to static
     * variable abuse in OpenSSL. */

    /*
     * TODO: determine somewhere we can safely shove out diagnostics
     *       (when enabled) at this late stage in the game:
     * CRYPTO_mem_leaks_fp(stderr);
     */
    return APR_SUCCESS;
}

/*
 * To ensure thread-safetyness in LibreSSL
 */

static unsigned long ssl_thread_id(void)
{
    return (unsigned long)tcn_get_thread_id();
}

static int ssl_rand_choosenum(int l, int h)
{
    int i;
    char buf[50];

    apr_snprintf(buf, sizeof(buf), "%.0f",
                 (((double)(rand()%RAND_MAX)/RAND_MAX)*(h-l)));
    i = atoi(buf)+1;
    if (i < l) i = l;
    if (i > h) i = h;
    return i;
}

static int ssl_rand_load_file(const char *file)
{
    char buffer[APR_PATH_MAX];
    int n;

    if (file == NULL)
        file = ssl_global_rand_file;
    if (file && (strcmp(file, "builtin") == 0))
        return -1;
    if (file == NULL)
        file = RAND_file_name(buffer, sizeof(buffer));
    if (file) {
        if (strncmp(file, "egd:", 4) == 0) {
#ifndef OPENSSL_NO_EGD
            if ((n = RAND_egd(file + 4)) > 0)
                return n;
            else
#endif
                return -1;
        }
        if ((n = RAND_load_file(file, -1)) > 0)
            return n;
    }
    return -1;
}

int SSL_rand_seed(const char *file)
{
    unsigned char stackdata[256];
    static volatile apr_uint32_t counter = 0;

    if (ssl_rand_load_file(file) < 0) {
        int n;
        struct {
            apr_time_t    t;
            pid_t         p;
            unsigned long i;
            apr_uint32_t  u;
        } _ssl_seed;
        if (counter == 0) {
            apr_generate_random_bytes(stackdata, 256);
            RAND_seed(stackdata, 128);
        }
        _ssl_seed.t = apr_time_now();
        _ssl_seed.p = getpid();
        _ssl_seed.i = ssl_thread_id();
        apr_atomic_inc32(&counter);
        _ssl_seed.u = counter;
        RAND_seed((unsigned char *)&_ssl_seed, sizeof(_ssl_seed));
        /*
         * seed in some current state of the run-time stack (128 bytes)
         */
        n = ssl_rand_choosenum(0, sizeof(stackdata)-128-1);
        RAND_seed(stackdata + n, 128);
    }
    return RAND_status();
}

TCN_IMPLEMENT_CALL(jint, SSL, initialize)(TCN_STDARGS, jstring engine)
{
    jclass clazz;
    jclass sClazz;

    TCN_ALLOC_CSTRING(engine);

    UNREFERENCED(o);
    if (!tcn_global_pool) {
        TCN_FREE_CSTRING(engine);
        tcn_ThrowAPRException(e, APR_EINVAL);
        return (jint)APR_EINVAL;
    }
    /* Check if already initialized */
    if (ssl_initialized++) {
        TCN_FREE_CSTRING(engine);
        return (jint)APR_SUCCESS;
    }

    memset(&tcn_password_callback, 0, sizeof(tcn_pass_cb_t));
    /* Initialize PRNG
     * This will in most cases call the builtin
     * low entropy seed.
     */
    SSL_rand_seed(NULL);
    /* For SSL_get_app_data2(), SSL_get_app_data3() and SSL_get_app_data4() at request time */
    SSL_init_app_data_idx();

    init_bio_methods();

    /*
     * Let us cleanup the ssl library when the library is unloaded
     */
    apr_pool_cleanup_register(tcn_global_pool, NULL,
                              ssl_init_cleanup,
                              apr_pool_cleanup_null);
    TCN_FREE_CSTRING(engine);

    /* Cache the byte[].class for performance reasons */
    clazz = (*e)->FindClass(e, "[B");
    byteArrayClass = (jclass) (*e)->NewGlobalRef(e, clazz);

    /* Cache the String.class for performance reasons */
    sClazz = (*e)->FindClass(e, "java/lang/String");
    stringClass = (jclass) (*e)->NewGlobalRef(e, sClazz);

    if (!key_log_file) {
        char *key_log_file_name = getenv("SSLKEYLOGFILE");
        if (key_log_file_name) {
            FILE *file = fopen(key_log_file_name, "a");
            if (file) {
                if (setvbuf(file, NULL, _IONBF, 0)) {
                    fclose(file);
                } else {
                    key_log_file = BIO_new_fp(file, BIO_CLOSE);
                }
            }
        }
    }

    return (jint)APR_SUCCESS;
}

TCN_IMPLEMENT_CALL(void, SSL, randSet)(TCN_STDARGS, jstring file)
{
    TCN_ALLOC_CSTRING(file);
    UNREFERENCED(o);
    if (J2S(file)) {
        ssl_global_rand_file = apr_pstrdup(tcn_global_pool, J2S(file));
    }
    TCN_FREE_CSTRING(file);
}

TCN_IMPLEMENT_CALL(jint, SSL, fipsModeGet)(TCN_STDARGS)
{
#if defined(LIBRESSL_VERSION_NUMBER)
    UNREFERENCED(o);
    /* LibreSSL doesn't support FIPS */
    return 0;
#else
    EVP_MD              *md;
    const OSSL_PROVIDER *provider;
    const char          *name;
    UNREFERENCED(o);

    // Maps the OpenSSL 3.x onwards behaviour to the OpenSSL 1.x API

    // Checks that FIPS is the default provider
    md = EVP_MD_fetch(NULL, "SHA-512", NULL);
    provider = EVP_MD_get0_provider(md);
    name = OSSL_PROVIDER_get0_name(provider);
    // Clean up
    EVP_MD_free(md);

    if (strcmp("fips", name)) {
        return 0;
    } else {
        return 1;
    }
#endif
}

TCN_IMPLEMENT_CALL(jint, SSL, fipsModeSet)(TCN_STDARGS, jint mode)
{
    int r = 0;
    UNREFERENCED(o);

    /* This method should never be called when using Tomcat Native 2.x onwards */
    tcn_ThrowException(e, "fipsModeSet is not supported in Tomcat Native 2.x onwards.");

    return r;
}

/* OpenSSL Java Stream BIO */

typedef struct  {
    int            refcount;
    apr_pool_t     *pool;
    tcn_callback_t cb;
} BIO_JAVA;


static apr_status_t generic_bio_cleanup(void *data)
{
    BIO *b = (BIO *)data;

    if (b) {
        BIO_free(b);
    }
    return APR_SUCCESS;
}

void SSL_BIO_close(BIO *bi)
{
    BIO_JAVA *j;
    if (bi == NULL)
        return;
    j = (BIO_JAVA *)BIO_get_data(bi);
    if (j != NULL && BIO_test_flags(bi, SSL_BIO_FLAG_CALLBACK)) {
        j->refcount--;
        if (j->refcount == 0) {
            if (j->pool)
                apr_pool_cleanup_run(j->pool, bi, generic_bio_cleanup);
            else
                BIO_free(bi);
        }
    }
    else
        BIO_free(bi);
}

void SSL_BIO_doref(BIO *bi)
{
    BIO_JAVA *j;
    if (bi == NULL)
        return;
    j = (BIO_JAVA *)BIO_get_data(bi);
    if (j != NULL && BIO_test_flags(bi, SSL_BIO_FLAG_CALLBACK)) {
        j->refcount++;
    }
}


static int jbs_new(BIO *bi)
{
    BIO_JAVA *j;

    if ((j = OPENSSL_malloc(sizeof(BIO_JAVA))) == NULL)
        return 0;
    j->pool      = NULL;
    j->refcount  = 1;
    BIO_set_shutdown(bi, 1);
    BIO_set_init(bi, 0);
    BIO_set_data(bi, (void *)j);

    return 1;
}

static int jbs_free(BIO *bi)
{
    BIO_JAVA *j;
    if (bi == NULL)
        return 0;
    j = (BIO_JAVA *)BIO_get_data(bi);
    if (j != NULL) {
        if (BIO_get_init(bi)) {
            JNIEnv   *e = NULL;
            BIO_set_init(bi, 0);
            tcn_get_java_env(&e);
            TCN_UNLOAD_CLASS(e, j->cb.obj);
        }
        OPENSSL_free(j);
    }
    BIO_set_data(bi, NULL);
    return 1;
}

static int jbs_write(BIO *b, const char *in, int inl)
{
    jint ret = -1;
    if (BIO_get_init(b) && in != NULL) {
        BIO_JAVA *j = (BIO_JAVA *)BIO_get_data(b);
        JNIEnv   *e = NULL;
        jbyteArray jb;
        tcn_get_java_env(&e);
        jb = (*e)->NewByteArray(e, inl);
        if (!(*e)->ExceptionOccurred(e)) {
            BIO_clear_retry_flags(b);
            (*e)->SetByteArrayRegion(e, jb, 0, inl, (jbyte *)in);
            ret = (*e)->CallIntMethod(e, j->cb.obj,
                                      j->cb.mid[0], jb);
            (*e)->ReleaseByteArrayElements(e, jb, (jbyte *)in, JNI_ABORT);
            (*e)->DeleteLocalRef(e, jb);
        }
    }
    /* From netty-tc-native, in the AF we were returning 0 */
    if (ret == 0) {
        BIO_set_retry_write(b);
        ret = -1;
    }
    return ret;
}

static int jbs_read(BIO *b, char *out, int outl)
{
    jint ret = 0;
    if (BIO_get_init(b) && out != NULL) {
        BIO_JAVA *j = (BIO_JAVA *)BIO_get_data(b);
        JNIEnv   *e = NULL;
        jbyteArray jb;
        tcn_get_java_env(&e);
        jb = (*e)->NewByteArray(e, outl);
        if (!(*e)->ExceptionOccurred(e)) {
            BIO_clear_retry_flags(b);
            ret = (*e)->CallIntMethod(e, j->cb.obj,
                                      j->cb.mid[1], jb);
            if (ret > 0) {
                jbyte *jout = (*e)->GetPrimitiveArrayCritical(e, jb, NULL);
                memcpy(out, jout, ret);
                (*e)->ReleasePrimitiveArrayCritical(e, jb, jout, 0);
            } else if (outl != 0) {
                ret = -1;
                BIO_set_retry_read(b);
            }
            (*e)->DeleteLocalRef(e, jb);
        }
    }
    return ret;
}

static int jbs_puts(BIO *b, const char *in)
{
    int ret = 0;
    if (BIO_get_init(b) && in != NULL) {
        BIO_JAVA *j = (BIO_JAVA *)BIO_get_data(b);
        JNIEnv   *e = NULL;
        tcn_get_java_env(&e);
        ret = (*e)->CallIntMethod(e, j->cb.obj,
                                  j->cb.mid[2],
                                  tcn_new_string(e, in));
    }
    return ret;
}

static int jbs_gets(BIO *b, char *out, int outl)
{
    int ret = 0;
    if (BIO_get_init(b) && out != NULL) {
        BIO_JAVA *j = (BIO_JAVA *)BIO_get_data(b);
        JNIEnv   *e = NULL;
        jobject  o;
        tcn_get_java_env(&e);
        if ((o = (*e)->CallObjectMethod(e, j->cb.obj,
                            j->cb.mid[3], (jint)(outl - 1)))) {
            TCN_ALLOC_CSTRING(o);
            if (J2S(o)) {
                int l = (int)strlen(J2S(o));
                if (l < outl) {
                    strcpy(out, J2S(o));
                    ret = outl;
                }
            }
            TCN_FREE_CSTRING(o);
        }
    }
    return ret;
}

static long jbs_ctrl(BIO *b, int cmd, long num, void *ptr)
{
    int ret = 0;
    switch (cmd) {
        case BIO_CTRL_FLUSH:
            ret = 1;
            break;
        default:
            ret = 0;
            break;
    }
    return ret;
}

static BIO_METHOD *jbs_methods = NULL;

static void init_bio_methods(void)
{
    jbs_methods = BIO_meth_new(BIO_TYPE_FILE, "Java Callback");
    BIO_meth_set_write(jbs_methods, &jbs_write);
    BIO_meth_set_read(jbs_methods, &jbs_read);
    BIO_meth_set_puts(jbs_methods, &jbs_puts);
    BIO_meth_set_gets(jbs_methods, &jbs_gets);
    BIO_meth_set_ctrl(jbs_methods, &jbs_ctrl);
    BIO_meth_set_create(jbs_methods, &jbs_new);
    BIO_meth_set_destroy(jbs_methods, &jbs_free);
}

static void free_bio_methods(void)
{
    BIO_meth_free(jbs_methods);
}

/*** Begin Twitter 1:1 API addition ***/
TCN_IMPLEMENT_CALL(jint, SSL, getLastErrorNumber)(TCN_STDARGS) {
    UNREFERENCED_STDARGS;
    return SSL_ERR_get();
}

static void ssl_info_callback(const SSL *ssl, int where, int ret) {
    int *handshakeCount = NULL;
    if (0 != (where & SSL_CB_HANDSHAKE_DONE)) {
        handshakeCount = (int*) SSL_get_app_data3(ssl);
        if (handshakeCount != NULL) {
            ++(*handshakeCount);
        }
    }
}

static apr_status_t ssl_con_pool_cleanup(void *data)
{
    SSL *ssl = (SSL*) data;
    int *destroyCount;

    TCN_ASSERT(ssl != 0);

    destroyCount = SSL_get_app_data4(ssl);
    if (destroyCount != NULL) {
        ++(*destroyCount);
    }

    return APR_SUCCESS;
}

TCN_IMPLEMENT_CALL(jlong /* SSL * */, SSL, newSSL)(TCN_STDARGS,
                                                   jlong ctx /* tcn_ssl_ctxt_t * */,
                                                   jboolean server) {
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    int *handshakeCount = malloc(sizeof(int));
    int *destroyCount = malloc(sizeof(int));
    SSL *ssl;
    apr_pool_t *p = NULL;
    tcn_ssl_conn_t *con;

    UNREFERENCED_STDARGS;

    TCN_ASSERT(ctx != 0);

    ssl = SSL_new(c->ctx);
    if (ssl == NULL) {
        free(handshakeCount);
        free(destroyCount);
        tcn_ThrowException(e, "cannot create new ssl");
        return 0;
    }

    apr_pool_create(&p, c->pool);
    if (p == NULL) {
        free(handshakeCount);
        free(destroyCount);
        SSL_free(ssl);
        tcn_ThrowAPRException(e, apr_get_os_error());
        return 0;
    }

    if ((con = apr_pcalloc(p, sizeof(tcn_ssl_conn_t))) == NULL) {
        free(handshakeCount);
        free(destroyCount);
        SSL_free(ssl);
        apr_pool_destroy(p);
        tcn_ThrowAPRException(e, apr_get_os_error());
        return 0;
    }
    con->pool = p;
    con->ctx  = c;
    con->ssl  = ssl;
    con->shutdown_type = c->shutdown_type;

    /* Store the handshakeCount in the SSL instance. */
    *handshakeCount = 0;
    SSL_set_app_data3(ssl, handshakeCount);

    /* Store the destroyCount in the SSL instance. */
    *destroyCount = 0;
    SSL_set_app_data4(ssl, destroyCount);

    /* Add callback to keep track of handshakes. */
    SSL_CTX_set_info_callback(c->ctx, ssl_info_callback);

    if (server) {
        SSL_set_accept_state(ssl);
    } else {
        SSL_set_connect_state(ssl);
    }

    /* Setup verify and seed */
    SSL_set_verify_result(ssl, X509_V_OK);
    SSL_rand_seed(c->rand_file);

    /* Store for later usage in SSL_callback_SSL_verify */
    SSL_set_app_data2(ssl, c);
    SSL_set_app_data(ssl, con);
    /* Register cleanup that prevent double destruction */
    apr_pool_cleanup_register(con->pool, (const void *)ssl,
                              ssl_con_pool_cleanup,
                              apr_pool_cleanup_null);

    return P2J(ssl);
}

/* How much did SSL write into this BIO? */
TCN_IMPLEMENT_CALL(jint /* nbytes */, SSL, pendingWrittenBytesInBIO)(TCN_STDARGS,
                                                                     jlong bio /* BIO * */) {
    UNREFERENCED_STDARGS;

    return BIO_ctrl_pending(J2P(bio, BIO *));
}

/* How much is available for reading in the given SSL struct? */
TCN_IMPLEMENT_CALL(jint, SSL, pendingReadableBytesInSSL)(TCN_STDARGS, jlong ssl /* SSL * */) {
    UNREFERENCED_STDARGS;

    return SSL_pending(J2P(ssl, SSL *));
}

/* Write wlen bytes from wbuf into bio */
TCN_IMPLEMENT_CALL(jint /* status */, SSL, writeToBIO)(TCN_STDARGS,
                                                       jlong bio /* BIO * */,
                                                       jlong wbuf /* char* */,
                                                       jint wlen /* sizeof(wbuf) */) {
    UNREFERENCED_STDARGS;

    return BIO_write(J2P(bio, BIO *), J2P(wbuf, void *), wlen);

}

/* Read up to rlen bytes from bio into rbuf */
TCN_IMPLEMENT_CALL(jint /* status */, SSL, readFromBIO)(TCN_STDARGS,
                                                        jlong bio /* BIO * */,
                                                        jlong rbuf /* char * */,
                                                        jint rlen /* sizeof(rbuf) - 1 */) {
    UNREFERENCED_STDARGS;

    return BIO_read(J2P(bio, BIO *), J2P(rbuf, void *), rlen);
}

/* Write up to wlen bytes of application data to the ssl BIO (encrypt) */
TCN_IMPLEMENT_CALL(jint /* status */, SSL, writeToSSL)(TCN_STDARGS,
                                                       jlong ssl /* SSL * */,
                                                       jlong wbuf /* char * */,
                                                       jint wlen /* sizeof(wbuf) */) {
    UNREFERENCED_STDARGS;

    return SSL_write(J2P(ssl, SSL *), J2P(wbuf, void *), wlen);
}

/* Read up to rlen bytes of application data from the given SSL BIO (decrypt) */
TCN_IMPLEMENT_CALL(jint /* status */, SSL, readFromSSL)(TCN_STDARGS,
                                                        jlong ssl /* SSL * */,
                                                        jlong rbuf /* char * */,
                                                        jint rlen /* sizeof(rbuf) - 1 */) {
    UNREFERENCED_STDARGS;

    return SSL_read(J2P(ssl, SSL *), J2P(rbuf, void *), rlen);
}

/* Get the shutdown status of the engine */
TCN_IMPLEMENT_CALL(jint /* status */, SSL, getShutdown)(TCN_STDARGS,
                                                        jlong ssl /* SSL * */) {
    UNREFERENCED_STDARGS;

    return SSL_get_shutdown(J2P(ssl, SSL *));
}

/* Free the SSL * and its associated internal BIO */
TCN_IMPLEMENT_CALL(void, SSL, freeSSL)(TCN_STDARGS,
                                       jlong ssl /* SSL * */) {
    SSL *ssl_ = J2P(ssl, SSL *);
    int *handshakeCount = SSL_get_app_data3(ssl_);
    int *destroyCount = SSL_get_app_data4(ssl_);
    tcn_ssl_conn_t *con = SSL_get_app_data(ssl_);

    UNREFERENCED_STDARGS;

    if (destroyCount != NULL) {
        if (*destroyCount == 0) {
            apr_pool_destroy(con->pool);
        }
        free(destroyCount);
    }
    if (handshakeCount != NULL) {
        free(handshakeCount);
    }
    SSL_free(ssl_);
}

/* Make a BIO pair (network and internal) for the provided SSL * and return the network BIO */
TCN_IMPLEMENT_CALL(jlong, SSL, makeNetworkBIO)(TCN_STDARGS,
                                               jlong ssl /* SSL * */) {
    SSL *ssl_ = J2P(ssl, SSL *);
    BIO *internal_bio;
    BIO *network_bio;

    UNREFERENCED(o);

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        goto fail;
    }

    if (BIO_new_bio_pair(&internal_bio, 0, &network_bio, 0) != 1) {
        tcn_ThrowException(e, "BIO_new_bio_pair failed");
        goto fail;
    }

    SSL_set_bio(ssl_, internal_bio, internal_bio);

    return P2J(network_bio);
 fail:
    return 0;
}

/* Free a BIO * (typically, the network BIO) */
TCN_IMPLEMENT_CALL(void, SSL, freeBIO)(TCN_STDARGS,
                                       jlong bio /* BIO * */) {
    BIO *bio_;
    UNREFERENCED_STDARGS;

    bio_ = J2P(bio, BIO *);
    BIO_free(bio_);
}

/* Send CLOSE_NOTIFY to peer */
TCN_IMPLEMENT_CALL(jint /* status */, SSL, shutdownSSL)(TCN_STDARGS,
                                                        jlong ssl /* SSL * */) {
    UNREFERENCED_STDARGS;

    return SSL_shutdown(J2P(ssl, SSL *));
}

/* Read which cipher was negotiated for the given SSL *. */
TCN_IMPLEMENT_CALL(jstring, SSL, getCipherForSSL)(TCN_STDARGS,
                                                  jlong ssl /* SSL * */)
{
    UNREFERENCED_STDARGS;

    return AJP_TO_JSTRING(SSL_get_cipher(J2P(ssl, SSL*)));
}

/* Read which protocol was negotiated for the given SSL *. */
TCN_IMPLEMENT_CALL(jstring, SSL, getVersion)(TCN_STDARGS,
                                                  jlong ssl /* SSL * */)
{
    UNREFERENCED_STDARGS;

    return AJP_TO_JSTRING(SSL_get_version(J2P(ssl, SSL*)));
}

/* Is the handshake over yet? */
TCN_IMPLEMENT_CALL(jint, SSL, isInInit)(TCN_STDARGS,
                                        jlong ssl /* SSL * */) {
    SSL *ssl_ = J2P(ssl, SSL *);

    UNREFERENCED(o);

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return 0;
    } else {
        return SSL_in_init(ssl_);
    }
}

TCN_IMPLEMENT_CALL(jint, SSL, doHandshake)(TCN_STDARGS,
                                           jlong ssl /* SSL * */) {
    SSL *ssl_ = J2P(ssl, SSL *);
    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return 0;
    }

    UNREFERENCED(o);

    return SSL_do_handshake(ssl_);
}

TCN_IMPLEMENT_CALL(jint, SSL, renegotiate)(TCN_STDARGS,
                                           jlong ssl /* SSL * */) {
    SSL *ssl_ = J2P(ssl, SSL *);
    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return 0;
    }

    UNREFERENCED(o);

    return SSL_renegotiate(ssl_);
}

TCN_IMPLEMENT_CALL(jint, SSL, renegotiatePending)(TCN_STDARGS,
                                                  jlong ssl /* SSL * */) {
    SSL *ssl_ = J2P(ssl, SSL *);
    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return 0;
    }

    UNREFERENCED(o);

    return SSL_renegotiate_pending(ssl_);
}

TCN_IMPLEMENT_CALL(jint, SSL, verifyClientPostHandshake)(TCN_STDARGS,
                                                         jlong ssl /* SSL * */) {
#if defined(SSL_OP_NO_TLSv1_3)
    SSL *ssl_ = J2P(ssl, SSL *);
    tcn_ssl_conn_t *con;

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return 0;
    }

    UNREFERENCED(o);

    con = (tcn_ssl_conn_t *)SSL_get_app_data(ssl_);
    con->pha_state = PHA_STARTED;

    return SSL_verify_client_post_handshake(ssl_);
#else
    return 0;
#endif
}

TCN_IMPLEMENT_CALL(jint, SSL, getPostHandshakeAuthInProgress)(TCN_STDARGS,
                                                              jlong ssl /* SSL * */) {
#if defined(SSL_OP_NO_TLSv1_3)
    SSL *ssl_ = J2P(ssl, SSL *);
    tcn_ssl_conn_t *con;

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return 0;
    }

    UNREFERENCED(o);

    con = (tcn_ssl_conn_t *)SSL_get_app_data(ssl_);

    return (con->pha_state == PHA_STARTED);
#else
    return 0;
#endif
}

/*** End Twitter API Additions ***/

/*** Apple API Additions ***/

TCN_IMPLEMENT_CALL(jstring, SSL, getAlpnSelected)(TCN_STDARGS,
                                                         jlong ssl /* SSL * */) {
    /* Looks fishy we have the same in sslnetwork.c, it set by socket/connection */
    SSL *ssl_ = J2P(ssl, SSL *);
    const unsigned char *proto;
    unsigned int proto_len;

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return NULL;
    }

    UNREFERENCED(o);

    SSL_get0_alpn_selected(ssl_, &proto, &proto_len);
    return tcn_new_stringn(e, (const char *) proto, (size_t) proto_len);
}

TCN_IMPLEMENT_CALL(jobjectArray, SSL, getPeerCertChain)(TCN_STDARGS,
                                                  jlong ssl /* SSL * */)
{
    STACK_OF(X509) *sk;
    int len;
    int i;
    X509 *cert;
    int length;
    unsigned char *buf;
    jobjectArray array;
    jbyteArray bArray;

    SSL *ssl_ = J2P(ssl, SSL *);

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return NULL;
    }

    UNREFERENCED(o);

    // Get a stack of all certs in the chain.
    sk = SSL_get_peer_cert_chain(ssl_);

    len = sk_X509_num(sk);
    if (len <= 0) {
        /* No peer certificate chain as no auth took place yet, or the auth was not successful. */
        return NULL;
    }
    /* Create the byte[][] array that holds all the certs */
    array = (*e)->NewObjectArray(e, len, byteArrayClass, NULL);

    for(i = 0; i < len; i++) {
        cert = (X509*) sk_X509_value(sk, i);

        buf = NULL;
        length = i2d_X509(cert, &buf);
        if (length < 0) {
            OPENSSL_free(buf);
            /* In case of error just return an empty byte[][] */
            return (*e)->NewObjectArray(e, 0, byteArrayClass, NULL);
        }
        bArray = (*e)->NewByteArray(e, length);
        (*e)->SetByteArrayRegion(e, bArray, 0, length, (jbyte*) buf);
        (*e)->SetObjectArrayElement(e, array, i, bArray);

        /*
         * Delete the local reference as we not know how long the chain is and local references are otherwise
         * only freed once jni method returns.
         */
        (*e)->DeleteLocalRef(e, bArray);

        OPENSSL_free(buf);
    }
    return array;
}

TCN_IMPLEMENT_CALL(jbyteArray, SSL, getPeerCertificate)(TCN_STDARGS,
                                                  jlong ssl /* SSL * */)
{
    X509 *cert;
    int length;
    unsigned char *buf = NULL;
    jbyteArray bArray;

    SSL *ssl_ = J2P(ssl, SSL *);

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return NULL;
    }

    UNREFERENCED(o);

    /* Get a stack of all certs in the chain */
    cert = SSL_get_peer_certificate(ssl_);
    if (cert == NULL) {
        return NULL;
    }

    length = i2d_X509(cert, &buf);

    bArray = (*e)->NewByteArray(e, length);
    (*e)->SetByteArrayRegion(e, bArray, 0, length, (jbyte*) buf);

    /*
     * We need to free the cert as the reference count is incremented by one and it is not destroyed when the
     * session is freed.
     * See https://www.openssl.org/docs/ssl/SSL_get_peer_certificate.html
     */
    X509_free(cert);

    OPENSSL_free(buf);

    return bArray;
}

TCN_IMPLEMENT_CALL(jstring, SSL, getErrorString)(TCN_STDARGS, jlong number)
{
    char buf[TCN_OPENSSL_ERROR_STRING_LENGTH];
    UNREFERENCED(o);
    ERR_error_string_n(number, buf, TCN_OPENSSL_ERROR_STRING_LENGTH);
    return tcn_new_string(e, buf);
}

TCN_IMPLEMENT_CALL(jlong, SSL, getTime)(TCN_STDARGS, jlong ssl)
{
    const SSL *ssl_ = J2P(ssl, SSL *);
    const SSL_SESSION *session;

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return 0;
    }

    UNREFERENCED(o);

    session  = SSL_get_session(ssl_);
    if (session) {
        return SSL_get_time(session);
    } else {
        tcn_ThrowException(e, "ssl session is null");
        return 0;
    }
}

TCN_IMPLEMENT_CALL(void, SSL, setVerify)(TCN_STDARGS, jlong ssl,
                                                jint level, jint depth)
{
    tcn_ssl_ctxt_t *c;
    int verify;
    SSL *ssl_ = J2P(ssl, SSL *);

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return;
    }

    c = SSL_get_app_data2(ssl_);

    verify = SSL_VERIFY_NONE;

    UNREFERENCED(o);

    if (c == NULL) {
        tcn_ThrowException(e, "context is null");
        return;
    }
    c->verify_mode = level;

    if (c->verify_mode == SSL_CVERIFY_UNSET)
        c->verify_mode = SSL_CVERIFY_NONE;
    if (depth > 0)
        c->verify_depth = depth;
    /*
     *  Configure callbacks for SSL context
     */
    if (c->verify_mode == SSL_CVERIFY_REQUIRE)
        verify |= SSL_VERIFY_PEER_STRICT;
    if ((c->verify_mode == SSL_CVERIFY_OPTIONAL) ||
        (c->verify_mode == SSL_CVERIFY_OPTIONAL_NO_CA))
        verify |= SSL_VERIFY_PEER;
    if (!c->store)
        c->store = SSL_CTX_get_cert_store(c->ctx);

    SSL_set_verify(ssl_, verify, SSL_callback_SSL_verify);
}

TCN_IMPLEMENT_CALL(void, SSL, setOptions)(TCN_STDARGS, jlong ssl,
                                                 jint opt)
{
    SSL *ssl_ = J2P(ssl, SSL *);

    UNREFERENCED_STDARGS;

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return;
    }

#ifndef SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
    /* Clear the flag if not supported */
    if (opt & 0x00040000) {
        opt &= ~0x00040000;
    }
#endif
    SSL_set_options(ssl_, opt);
}

TCN_IMPLEMENT_CALL(jint, SSL, getOptions)(TCN_STDARGS, jlong ssl)
{
    SSL *ssl_ = J2P(ssl, SSL *);

    UNREFERENCED_STDARGS;

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return 0;
    }

    return SSL_get_options(ssl_);
}

TCN_IMPLEMENT_CALL(jobjectArray, SSL, getCiphers)(TCN_STDARGS, jlong ssl)
{
    STACK_OF(SSL_CIPHER) *sk;
    int len;
    jobjectArray array;
    SSL_CIPHER *cipher;
    const char *name;
    int i;
    jstring c_name;
    SSL *ssl_ = J2P(ssl, SSL *);

    UNREFERENCED_STDARGS;

    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return NULL;
    }

    sk = SSL_get_ciphers(ssl_);
    len = sk_SSL_CIPHER_num(sk);

    if (len <= 0) {
        /* No peer certificate chain as no auth took place yet, or the auth was not successful. */
        return NULL;
    }

    /* Ensure stringClass is initialized (lazy initialization) */
    if (stringClass == NULL) {
        jclass sClazz = (*e)->FindClass(e, "java/lang/String");
        stringClass = (jclass) (*e)->NewGlobalRef(e, sClazz);
    }

    /* Create the byte[][] array that holds all the certs */
    array = (*e)->NewObjectArray(e, len, stringClass, NULL);

    for (i = 0; i < len; i++) {
        cipher = (SSL_CIPHER*) sk_SSL_CIPHER_value(sk, i);
        name = SSL_CIPHER_get_name(cipher);

        c_name = (*e)->NewStringUTF(e, name);
        (*e)->SetObjectArrayElement(e, array, i, c_name);
    }
    return array;
}

TCN_IMPLEMENT_CALL(jboolean, SSL, setCipherSuites)(TCN_STDARGS, jlong ssl,
                                                         jstring cipherList)
{
    SSL *ssl_ = J2P(ssl, SSL *);
    TCN_ALLOC_CSTRING(cipherList);
    jboolean rv = JNI_TRUE;
    #ifndef HAVE_EXPORT_CIPHERS
        size_t len;
        char *buf;
    #endif
    UNREFERENCED(o);

    if (ssl_ == NULL) {
        TCN_FREE_CSTRING(cipherList);
        tcn_ThrowException(e, "ssl is null");
        return JNI_FALSE;
    }

    if (!J2S(cipherList)) {
        rv = JNI_FALSE;
        goto free_cipherList;
    }

#ifndef HAVE_EXPORT_CIPHERS
    /*
     *  Always disable NULL and export ciphers,
     *  no matter what was given in the config.
     */
    len = strlen(J2S(cipherList)) + strlen(SSL_CIPHERS_ALWAYS_DISABLED) + 1;
    buf = malloc(len * sizeof(char *));
    if (buf == NULL) {
        rv = JNI_FALSE;
        goto free_cipherList;
    }
    memcpy(buf, SSL_CIPHERS_ALWAYS_DISABLED, strlen(SSL_CIPHERS_ALWAYS_DISABLED));
    memcpy(buf + strlen(SSL_CIPHERS_ALWAYS_DISABLED), J2S(cipherList), strlen(J2S(cipherList)));
    buf[len - 1] = '\0';
    if (!SSL_set_cipher_list(ssl_, buf)) {
#else
    if (!SSL_set_cipher_list(ssl_, J2S(cipherList))) {
#endif
        char err[TCN_OPENSSL_ERROR_STRING_LENGTH];
        ERR_error_string_n(SSL_ERR_get(), err, TCN_OPENSSL_ERROR_STRING_LENGTH);
        tcn_Throw(e, "Unable to configure permitted SSL ciphers (%s)", err);
        rv = JNI_FALSE;
    }
#ifndef HAVE_EXPORT_CIPHERS
    free(buf);
#endif
free_cipherList:
    TCN_FREE_CSTRING(cipherList);
    return rv;
}

TCN_IMPLEMENT_CALL(jboolean, SSL, setCipherSuitesEx)(TCN_STDARGS, jlong ssl,
                                                         jstring cipherSuites)
{
    SSL *ssl_ = J2P(ssl, SSL *);
    TCN_ALLOC_CSTRING(cipherSuites);
    jboolean rv = JNI_TRUE;
    UNREFERENCED(o);

    if (ssl_ == NULL) {
        TCN_FREE_CSTRING(cipherSuites);
        tcn_ThrowException(e, "ssl is null");
        return JNI_FALSE;
    }

    if (!J2S(cipherSuites)) {
        rv = JNI_FALSE;
        goto free_cipherSuites;
    }

    if (!SSL_set_ciphersuites(ssl_, J2S(cipherSuites))) {
        char err[TCN_OPENSSL_ERROR_STRING_LENGTH];
        ERR_error_string_n(SSL_ERR_get(), err, TCN_OPENSSL_ERROR_STRING_LENGTH);
        tcn_Throw(e, "Unable to configure permitted SSL cipher suites (%s)", err);
        rv = JNI_FALSE;
    }

free_cipherSuites:
    TCN_FREE_CSTRING(cipherSuites);
    return rv;
}

TCN_IMPLEMENT_CALL(jbyteArray, SSL, getSessionId)(TCN_STDARGS, jlong ssl)
{

    unsigned int len;
    const unsigned char *session_id;
    const SSL_SESSION *session;
    jbyteArray bArray;
    SSL *ssl_ = J2P(ssl, SSL *);
    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return NULL;
    }
    UNREFERENCED(o);
    session = SSL_get_session(ssl_);
    if (NULL == session) {
        return NULL;
    }

    session_id = SSL_SESSION_get_id(session, &len);

    if (len == 0 || session_id == NULL) {
        return NULL;
    }

    bArray = (*e)->NewByteArray(e, len);
    (*e)->SetByteArrayRegion(e, bArray, 0, len, (jbyte*) session_id);
    return bArray;
}

TCN_IMPLEMENT_CALL(jint, SSL, getHandshakeCount)(TCN_STDARGS, jlong ssl)
{
    int *handshakeCount = NULL;
    SSL *ssl_ = J2P(ssl, SSL *);
    if (ssl_ == NULL) {
        tcn_ThrowException(e, "ssl is null");
        return -1;
    }
    UNREFERENCED(o);

    handshakeCount = SSL_get_app_data3(ssl_);
    if (handshakeCount != NULL) {
        return *handshakeCount;
    }
    return 0;
}

/*** End Apple API Additions ***/
