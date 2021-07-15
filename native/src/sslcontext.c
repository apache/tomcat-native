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

/** SSL Context wrapper
 */

#include "tcn.h"

#include "apr_file_io.h"
#include "apr_thread_mutex.h"
#include "apr_poll.h"

#ifdef HAVE_OPENSSL
#include "ssl_private.h"

static jclass byteArrayClass;
static jclass stringClass;

static apr_status_t ssl_context_cleanup(void *data)
{
    tcn_ssl_ctxt_t *c = (tcn_ssl_ctxt_t *)data;
    if (c) {
        int i;
        c->crl = NULL;
        if (c->ctx)
            SSL_CTX_free(c->ctx);
        c->ctx = NULL;
        for (i = 0; i < SSL_AIDX_MAX; i++) {
            if (c->certs[i]) {
                X509_free(c->certs[i]);
                c->certs[i] = NULL;
            }
            if (c->keys[i]) {
                EVP_PKEY_free(c->keys[i]);
                c->keys[i] = NULL;
            }
        }
        if (c->bio_is) {
            SSL_BIO_close(c->bio_is);
            c->bio_is = NULL;
        }
        if (c->bio_os) {
            SSL_BIO_close(c->bio_os);
            c->bio_os = NULL;
        }

        if (c->verifier) {
            JNIEnv *e;
            tcn_get_java_env(&e);
            (*e)->DeleteGlobalRef(e, c->verifier);
            c->verifier = NULL;
        }
        c->verifier_method = NULL;

        if (c->next_proto_data) {
            free(c->next_proto_data);
            c->next_proto_data = NULL;
        }
        c->next_proto_len = 0;

        if (c->alpn_proto_data) {
            free(c->alpn_proto_data);
            c->alpn_proto_data = NULL;
        }
        c->alpn_proto_len = 0;
    }
    return APR_SUCCESS;
}

static jclass    ssl_context_class;
static jmethodID sni_java_callback;

/* Callback used when OpenSSL receives a client hello with a Server Name
 * Indication extension.
 */
int ssl_callback_ServerNameIndication(SSL *ssl, int *al, tcn_ssl_ctxt_t *c)
{
    /* TODO: Is it better to cache the JNIEnv* during the call to handshake? */

    /* Get the JNI environment for this callback */
    JavaVM *javavm = tcn_get_java_vm();
    JNIEnv *env;
    const char *servername;
    jstring hostname;
    jlong original_ssl_context, new_ssl_context;
    tcn_ssl_ctxt_t *new_c;

    // Continue only if the static method exists
    if (sni_java_callback == NULL) {
        return SSL_TLSEXT_ERR_OK;
    }

    (*javavm)->AttachCurrentThread(javavm, (void **)&env, NULL);

    // Get the host name presented by the client
    servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);

    // Convert to Java compatible parameters ready for the method call
    hostname = (*env)->NewStringUTF(env, servername);
    original_ssl_context = P2J(c);

    new_ssl_context = (*env)->CallStaticLongMethod(env,
                                                   ssl_context_class,
                                                   sni_java_callback,
                                                   original_ssl_context,
                                                   hostname);

    // Delete the local reference as this method is called via callback.
    // Otherwise local references are only freed once jni method returns.
    (*env)->DeleteLocalRef(env, hostname);

    if (new_ssl_context != 0 && new_ssl_context != original_ssl_context) {
        new_c = J2P(new_ssl_context, tcn_ssl_ctxt_t *);
        SSL_set_SSL_CTX(ssl, new_c->ctx);
    }

    return SSL_TLSEXT_ERR_OK;
}

#if OPENSSL_VERSION_NUMBER >= 0x10101000L && !defined(LIBRESSL_VERSION_NUMBER)
/*
 * This callback function is called when the ClientHello is received.
 */
int ssl_callback_ClientHello(SSL *ssl, int *al, void *arg)
{
    JavaVM *javavm = tcn_get_java_vm();
    JNIEnv *env;
    char *servername = NULL;
    const unsigned char *pos;
    size_t len, remaining;
    tcn_ssl_ctxt_t *c = (tcn_ssl_ctxt_t *) arg;
 
    (*javavm)->AttachCurrentThread(javavm, (void **)&env, NULL);
    // Continue only if the static method exists
    if (sni_java_callback == NULL) {
        return SSL_CLIENT_HELLO_SUCCESS;
    }

    /* We can't use SSL_get_servername() at this earliest OpenSSL connection
     * stage, and there is no SSL_client_hello_get0_servername() provided as
     * of OpenSSL 1.1.1. So the code below, that extracts the SNI from the
     * ClientHello's TLS extensions, is taken from some test code in OpenSSL,
     * i.e. client_hello_select_server_ctx() in "test/handshake_helper.c".
     */

    /*
     * The server_name extension was given too much extensibility when it
     * was written, so parsing the normal case is a bit complex.
     */
    if (!SSL_client_hello_get0_ext(ssl, TLSEXT_TYPE_server_name, &pos,
                                   &remaining)
            || remaining <= 2) 
        goto give_up;

    /* Extract the length of the supplied list of names. */
    len = (*(pos++) << 8);
    len += *(pos++);
    if (len + 2 != remaining)
        goto give_up;
    remaining = len;

    /*
     * The list in practice only has a single element, so we only consider
     * the first one.
     */
    if (remaining <= 3 || *pos++ != TLSEXT_NAMETYPE_host_name)
        goto give_up;
    remaining--;

    /* Now we can finally pull out the byte array with the actual hostname. */
    len = (*(pos++) << 8);
    len += *(pos++);
    if (len + 2 != remaining)
        goto give_up;

    /* Use the SNI to switch to the relevant vhost, should it differ from
     * c->base_server.
     */
    servername = apr_pstrmemdup(c->pool, (const char *)pos, len);

give_up:
    if (servername != NULL) {
        jstring hostname;
        jlong original_ssl_context, new_ssl_context;
        tcn_ssl_ctxt_t *new_c;

        hostname = (*env)->NewStringUTF(env, servername);
        original_ssl_context = P2J(c);
        new_ssl_context = (*env)->CallStaticLongMethod(env,
                                                   ssl_context_class,
                                                   sni_java_callback,
                                                   original_ssl_context,
                                                   hostname);
        (*env)->DeleteLocalRef(env, hostname);
        if (new_ssl_context != 0 && new_ssl_context != original_ssl_context) {
            SSL_CTX *ctx;
            new_c = J2P(new_ssl_context, tcn_ssl_ctxt_t *);
            ctx = SSL_set_SSL_CTX(ssl, new_c->ctx);

            /* Copied from httpd (modules/ssl/ssl_engine_kernel.c) */
            SSL_set_options(ssl, SSL_CTX_get_options(ctx));
            SSL_set_min_proto_version(ssl, SSL_CTX_get_min_proto_version(ctx));
            SSL_set_max_proto_version(ssl, SSL_CTX_get_max_proto_version(ctx));
            if ((SSL_get_verify_mode(ssl) == SSL_VERIFY_NONE) ||
                (SSL_num_renegotiations(ssl) == 0)) {
                SSL_set_verify(ssl, SSL_CTX_get_verify_mode(ctx), SSL_CTX_get_verify_callback(ctx));
            }
            if (SSL_num_renegotiations(ssl) == 0) {
                SSL_set_session_id_context(ssl,  &(c->context_id[0]), sizeof c->context_id);
            }
        }
 
    }
    return SSL_CLIENT_HELLO_SUCCESS;
}
#endif /* OPENSSL_VERSION_NUMBER < 0x10101000L */

/* Initialize server context */
TCN_IMPLEMENT_CALL(jlong, SSLContext, make)(TCN_STDARGS, jlong pool,
                                            jint protocol, jint mode)
{
    apr_pool_t *p = J2P(pool, apr_pool_t *);
    tcn_ssl_ctxt_t *c = NULL;
    SSL_CTX *ctx = NULL;
    jclass clazz;
    jclass sClazz;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    jint prot;
#endif

    UNREFERENCED(o);
    if (protocol == SSL_PROTOCOL_NONE) {
        tcn_Throw(e, "No SSL protocols requested");
        goto init_failed;
    }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    if (protocol == SSL_PROTOCOL_TLSV1_3) {
#ifdef HAVE_TLSV1_3
        if (mode == SSL_MODE_CLIENT)
            ctx = SSL_CTX_new(TLSv1_3_client_method());
        else if (mode == SSL_MODE_SERVER)
            ctx = SSL_CTX_new(TLSv1_3_server_method());
        else
            ctx = SSL_CTX_new(TLSv1_3_method());
#endif
    } else if (protocol == SSL_PROTOCOL_TLSV1_2) {
#ifdef HAVE_TLSV1_2
        if (mode == SSL_MODE_CLIENT)
            ctx = SSL_CTX_new(TLSv1_2_client_method());
        else if (mode == SSL_MODE_SERVER)
            ctx = SSL_CTX_new(TLSv1_2_server_method());
        else
            ctx = SSL_CTX_new(TLSv1_2_method());
#endif
    } else if (protocol == SSL_PROTOCOL_TLSV1_1) {
#ifdef HAVE_TLSV1_1
        if (mode == SSL_MODE_CLIENT)
            ctx = SSL_CTX_new(TLSv1_1_client_method());
        else if (mode == SSL_MODE_SERVER)
            ctx = SSL_CTX_new(TLSv1_1_server_method());
        else
            ctx = SSL_CTX_new(TLSv1_1_method());
#endif
    } else if (protocol == SSL_PROTOCOL_TLSV1) {
        if (mode == SSL_MODE_CLIENT)
            ctx = SSL_CTX_new(TLSv1_client_method());
        else if (mode == SSL_MODE_SERVER)
            ctx = SSL_CTX_new(TLSv1_server_method());
        else
            ctx = SSL_CTX_new(TLSv1_method());
    } else if (protocol == SSL_PROTOCOL_SSLV3) {
        if (mode == SSL_MODE_CLIENT)
            ctx = SSL_CTX_new(SSLv3_client_method());
        else if (mode == SSL_MODE_SERVER)
            ctx = SSL_CTX_new(SSLv3_server_method());
        else
            ctx = SSL_CTX_new(SSLv3_method());
    } else if (protocol == SSL_PROTOCOL_SSLV2) {
        /* requested but not supported */
#ifndef HAVE_TLSV1_3
    } else if (protocol & SSL_PROTOCOL_TLSV1_3) {
        /* requested but not supported */
#endif
#ifndef HAVE_TLSV1_2
    } else if (protocol & SSL_PROTOCOL_TLSV1_2) {
        /* requested but not supported */
#endif
#ifndef HAVE_TLSV1_1
    } else if (protocol & SSL_PROTOCOL_TLSV1_1) {
        /* requested but not supported */
#endif
    } else {
#endif /* if OPENSSL_VERSION_NUMBER < 0x10100000L */
        if (mode == SSL_MODE_CLIENT)
                ctx = SSL_CTX_new(TLS_client_method());
        else if (mode == SSL_MODE_SERVER)
                ctx = SSL_CTX_new(TLS_server_method());
        else
                ctx = SSL_CTX_new(TLS_method());
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    }
#endif

    if (!ctx) {
        char err[256];
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Invalid Server SSL Protocol (%s)", err);
        goto init_failed;
    }
    if ((c = apr_pcalloc(p, sizeof(tcn_ssl_ctxt_t))) == NULL) {
        tcn_ThrowAPRException(e, apr_get_os_error());
        goto init_failed;
    }

#ifdef HAVE_KEYLOG_CALLBACK
    SSL_callback_add_keylog(ctx);
#endif

    c->protocol = protocol;
    c->mode     = mode;
    c->ctx      = ctx;
    c->pool     = p;
    c->bio_os   = BIO_new(BIO_s_file());
    if (c->bio_os != NULL)
        BIO_set_fp(c->bio_os, stderr, BIO_NOCLOSE | BIO_FP_TEXT);
    SSL_CTX_set_options(c->ctx, SSL_OP_ALL);

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
    /* always disable SSLv2, as per RFC 6176 */
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
    if (!(protocol & SSL_PROTOCOL_SSLV3))
        SSL_CTX_set_options(c->ctx, SSL_OP_NO_SSLv3);
    if (!(protocol & SSL_PROTOCOL_TLSV1))
        SSL_CTX_set_options(c->ctx, SSL_OP_NO_TLSv1);
#ifdef HAVE_TLSV1_1
    if (!(protocol & SSL_PROTOCOL_TLSV1_1))
        SSL_CTX_set_options(c->ctx, SSL_OP_NO_TLSv1_1);
#endif
#ifdef HAVE_TLSV1_2
    if (!(protocol & SSL_PROTOCOL_TLSV1_2))
        SSL_CTX_set_options(c->ctx, SSL_OP_NO_TLSv1_2);
#endif
#ifdef HAVE_TLSV1_3
    if (!(protocol & SSL_PROTOCOL_TLSV1_3))
        SSL_CTX_set_options(c->ctx, SSL_OP_NO_TLSv1_3);
#endif

#else /* if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER) */
    /* We first determine the maximum protocol version we should provide */
#ifdef HAVE_TLSV1_3
    if (protocol & SSL_PROTOCOL_TLSV1_3) {
        prot = TLS1_3_VERSION;
    } else
/* NOTE the dangling else above: take care to preserve it */
#endif
    if (protocol & SSL_PROTOCOL_TLSV1_2) {
        prot = TLS1_2_VERSION;
    } else if (protocol & SSL_PROTOCOL_TLSV1_1) {
        prot = TLS1_1_VERSION;
    } else if (protocol & SSL_PROTOCOL_TLSV1) {
        prot = TLS1_VERSION;
    } else if (protocol & SSL_PROTOCOL_SSLV3) {
        prot = SSL3_VERSION;
    } else {
        SSL_CTX_free(ctx);
        tcn_Throw(e, "Invalid Server SSL Protocol (%d)", protocol);
        goto init_failed;
    }
    SSL_CTX_set_max_proto_version(ctx, prot);

    /* Next we scan for the minimal protocol version we should provide,
     * but we do not allow holes between max and min */
#ifdef HAVE_TLSV1_3
    if (prot == TLS1_3_VERSION && protocol & SSL_PROTOCOL_TLSV1_2) {
        prot = TLS1_2_VERSION;
    }
#endif
    if (prot == TLS1_2_VERSION && protocol & SSL_PROTOCOL_TLSV1_1) {
        prot = TLS1_1_VERSION;
    }
    if (prot == TLS1_1_VERSION && protocol & SSL_PROTOCOL_TLSV1) {
        prot = TLS1_VERSION;
    }
    if (prot == TLS1_VERSION && protocol & SSL_PROTOCOL_SSLV3) {
        prot = SSL3_VERSION;
    }
    SSL_CTX_set_min_proto_version(ctx, prot);
#endif /* if OPENSSL_VERSION_NUMBER < 0x10100000L */

    /*
     * Configure additional context ingredients
     */
    SSL_CTX_set_options(c->ctx, SSL_OP_SINGLE_DH_USE);
#ifdef HAVE_ECC
    SSL_CTX_set_options(c->ctx, SSL_OP_SINGLE_ECDH_USE);
#endif
#ifdef SSL_OP_NO_COMPRESSION
    /* Disable SSL compression to be safe */
    SSL_CTX_set_options(c->ctx, SSL_OP_NO_COMPRESSION);
#endif


    /** To get back the tomcat wrapper from CTX */
    SSL_CTX_set_app_data(c->ctx, (char *)c);

#ifdef SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
    /*
     * Disallow a session from being resumed during a renegotiation,
     * so that an acceptable cipher suite can be negotiated.
     */
    SSL_CTX_set_options(c->ctx, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
    /* Release idle buffers to the SSL_CTX free list */
    SSL_CTX_set_mode(c->ctx, SSL_MODE_RELEASE_BUFFERS);
#endif
    /* Default session context id and cache size */
    SSL_CTX_sess_set_cache_size(c->ctx, SSL_DEFAULT_CACHE_SIZE);
    /* Session cache is disabled by default */
    SSL_CTX_set_session_cache_mode(c->ctx, SSL_SESS_CACHE_OFF);
    /* Longer session timeout */
    SSL_CTX_set_timeout(c->ctx, 14400);

    EVP_Digest((const unsigned char *)SSL_DEFAULT_VHOST_NAME,
               (unsigned long)((sizeof SSL_DEFAULT_VHOST_NAME) - 1),
               &(c->context_id[0]), NULL, EVP_sha1(), NULL);

    /* Set default Certificate verification level
     * and depth for the Client Authentication
     */
    c->verify_depth  = 1;
    c->verify_mode   = SSL_CVERIFY_UNSET;
    c->shutdown_type = SSL_SHUTDOWN_TYPE_UNSET;

    /* Set default password callback */
    SSL_CTX_set_default_passwd_cb(c->ctx, (pem_password_cb *)SSL_password_callback);
    SSL_CTX_set_default_passwd_cb_userdata(c->ctx, (void *)(&tcn_password_callback));
    SSL_CTX_set_info_callback(c->ctx, SSL_callback_handshake);

    /* Cache Java side SNI callback if not already cached */
    if (ssl_context_class == NULL) {
        ssl_context_class = (*e)->NewGlobalRef(e, o);
        sni_java_callback = (*e)->GetStaticMethodID(e, ssl_context_class,
                                                    "sniCallBack", "(JLjava/lang/String;)J");
        /* Older Tomcat versions may not have this static method */
        if ( (*e)->ExceptionCheck(e) ) {
            (*e)->ExceptionClear(e);
        }
    }

    /* Set up OpenSSL call back if SNI is provided by the client */
    SSL_CTX_set_tlsext_servername_callback(c->ctx, ssl_callback_ServerNameIndication);
    SSL_CTX_set_tlsext_servername_arg(c->ctx, c);

#if OPENSSL_VERSION_NUMBER >= 0x10101000L && !defined(LIBRESSL_VERSION_NUMBER)
    /*
     * The ClientHello callback also allows to retrieve the SNI, but since it
     * runs at the earliest possible connection stage we can even set the TLS
     * protocol version(s) according to the selected (name-based-)vhost, which
     * is not possible at the SNI callback stage (due to OpenSSL internals).
     */
    SSL_CTX_set_client_hello_cb(c->ctx, ssl_callback_ClientHello, c);
#endif

    /*
     * Let us cleanup the ssl context when the pool is destroyed
     */
    apr_pool_cleanup_register(p, (const void *)c,
                              ssl_context_cleanup,
                              apr_pool_cleanup_null);

    /* Cache the byte[].class for performance reasons */
    if (stringClass == NULL) {
        clazz = (*e)->FindClass(e, "[B");
        byteArrayClass = (jclass) (*e)->NewGlobalRef(e, clazz);
        sClazz = (*e)->FindClass(e, "java/lang/String");
        stringClass = (jclass) (*e)->NewGlobalRef(e, sClazz);
    }

    return P2J(c);
init_failed:
    return 0;
}

TCN_IMPLEMENT_CALL(jint, SSLContext, free)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    UNREFERENCED_STDARGS;
    TCN_ASSERT(ctx != 0);
    /* Run and destroy the cleanup callback */
    return apr_pool_cleanup_run(c->pool, c, ssl_context_cleanup);
}

TCN_IMPLEMENT_CALL(void, SSLContext, setContextId)(TCN_STDARGS, jlong ctx,
                                                   jstring id)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    TCN_ALLOC_CSTRING(id);

    TCN_ASSERT(ctx != 0);
    UNREFERENCED(o);
    if (J2S(id)) {
        EVP_Digest((const unsigned char *)J2S(id),
                   (unsigned long)strlen(J2S(id)),
                   &(c->context_id[0]), NULL, EVP_sha1(), NULL);
    }
    TCN_FREE_CSTRING(id);
}

TCN_IMPLEMENT_CALL(void, SSLContext, setBIO)(TCN_STDARGS, jlong ctx,
                                             jlong bio, jint dir)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    BIO *bio_handle   = J2P(bio, BIO *);

    UNREFERENCED_STDARGS;
    TCN_ASSERT(ctx != 0);
    if (dir == 0) {
        if (c->bio_os && c->bio_os != bio_handle)
            SSL_BIO_close(c->bio_os);
        c->bio_os = bio_handle;
    }
    else if (dir == 1) {
        if (c->bio_is && c->bio_is != bio_handle)
            SSL_BIO_close(c->bio_is);
        c->bio_is = bio_handle;
    }
    else
        return;
    SSL_BIO_doref(bio_handle);
}

TCN_IMPLEMENT_CALL(void, SSLContext, setOptions)(TCN_STDARGS, jlong ctx,
                                                 jint opt)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);

    UNREFERENCED_STDARGS;
    TCN_ASSERT(ctx != 0);
#ifndef SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
    /* Clear the flag if not supported */
    if (opt & 0x00040000)
        opt &= ~0x00040000;
#endif
    SSL_CTX_set_options(c->ctx, opt);
}

TCN_IMPLEMENT_CALL(jint, SSLContext, getOptions)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);

    UNREFERENCED_STDARGS;
    TCN_ASSERT(ctx != 0);

    return SSL_CTX_get_options(c->ctx);
}

TCN_IMPLEMENT_CALL(void, SSLContext, clearOptions)(TCN_STDARGS, jlong ctx,
                                                   jint opt)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);

    UNREFERENCED_STDARGS;
    TCN_ASSERT(ctx != 0);
    SSL_CTX_clear_options(c->ctx, opt);
}

TCN_IMPLEMENT_CALL(void, SSLContext, setQuietShutdown)(TCN_STDARGS, jlong ctx,
                                                       jboolean mode)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);

    UNREFERENCED_STDARGS;
    TCN_ASSERT(ctx != 0);
    SSL_CTX_set_quiet_shutdown(c->ctx, mode ? 1 : 0);
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCipherSuite)(TCN_STDARGS, jlong ctx,
                                                         jstring ciphers)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    TCN_ALLOC_CSTRING(ciphers);
    jboolean rv = JNI_TRUE;
#ifndef HAVE_EXPORT_CIPHERS
    size_t len;
    char *buf;
#endif

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);
    if (!J2S(ciphers))
        return JNI_FALSE;

#ifndef HAVE_EXPORT_CIPHERS
    /*
     *  Always disable NULL and export ciphers,
     *  no matter what was given in the config.
     */
    len = strlen(J2S(ciphers)) + strlen(SSL_CIPHERS_ALWAYS_DISABLED) + 1;
    buf = malloc(len * sizeof(char *));
    if (buf == NULL)
        return JNI_FALSE;
    memcpy(buf, SSL_CIPHERS_ALWAYS_DISABLED, strlen(SSL_CIPHERS_ALWAYS_DISABLED));
    memcpy(buf + strlen(SSL_CIPHERS_ALWAYS_DISABLED), J2S(ciphers), strlen(J2S(ciphers)));
    buf[len - 1] = '\0';
    if (!SSL_CTX_set_cipher_list(c->ctx, buf)) {
#else
    if (!SSL_CTX_set_cipher_list(c->ctx, J2S(ciphers))) {
#endif
        char err[256];
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Unable to configure permitted SSL ciphers (%s)", err);
        rv = JNI_FALSE;
    }
#ifndef HAVE_EXPORT_CIPHERS
    free(buf);
#endif
    TCN_FREE_CSTRING(ciphers);
    return rv;
}

TCN_IMPLEMENT_CALL(jobjectArray, SSLContext, getCiphers)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    STACK_OF(SSL_CIPHER) *sk;
    int len;
    jobjectArray array;
    SSL_CIPHER *cipher;
    const char *name;
    int i;
    jstring c_name;
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
    SSL *ssl;
#endif

    UNREFERENCED_STDARGS;

    if (c->ctx == NULL) {
        tcn_ThrowException(e, "ssl context is null");
        return NULL;
    }

    /* Before OpenSSL 1.1.0, get_ciphers() was only available
     * on an SSL, not for an SSL_CTX. */
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
    ssl = SSL_new(c->ctx);
    if (ssl == NULL) {
        tcn_ThrowException(e, "could not create temporary ssl from ssl context");
        return NULL;
    }

    sk = SSL_get_ciphers(ssl);
#else
    sk = SSL_CTX_get_ciphers(c->ctx);
#endif
    len = sk_SSL_CIPHER_num(sk);

    if (len <= 0) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
        SSL_free(ssl);
#endif
        return NULL;
    }

    array = (*e)->NewObjectArray(e, len, stringClass, NULL);

    for (i = 0; i < len; i++) {
        cipher = (SSL_CIPHER*) sk_SSL_CIPHER_value(sk, i);
        name = SSL_CIPHER_get_name(cipher);

        c_name = (*e)->NewStringUTF(e, name);
        (*e)->SetObjectArrayElement(e, array, i, c_name);
    }
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
    SSL_free(ssl);
#endif
    return array;
}


TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCARevocation)(TCN_STDARGS, jlong ctx,
                                                          jstring file,
                                                          jstring path)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    TCN_ALLOC_CSTRING(file);
    TCN_ALLOC_CSTRING(path);
    jboolean rv = JNI_FALSE;
    X509_LOOKUP *lookup;
    char err[256];

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);
    if (J2S(file) == NULL && J2S(path) == NULL) {
        return JNI_FALSE;
    }

    if (!c->crl) {
        if ((c->crl = SSL_CTX_get_cert_store(c->ctx)) == NULL)
            goto cleanup;
    }
    if (J2S(file)) {
        lookup = X509_STORE_add_lookup(c->crl, X509_LOOKUP_file());
        if (lookup == NULL) {
            ERR_error_string(SSL_ERR_get(), err);
            c->crl = NULL;
            tcn_Throw(e, "Lookup failed for file %s (%s)", J2S(file), err);
            goto cleanup;
        }
        if (!X509_LOOKUP_load_file(lookup, J2S(file), X509_FILETYPE_PEM)) {
            ERR_error_string(SSL_ERR_get(), err);
            c->crl = NULL;
            tcn_Throw(e, "Load failed for file %s (%s)", J2S(file), err);
            goto cleanup;
        }
    }
    if (J2S(path)) {
        lookup = X509_STORE_add_lookup(c->crl, X509_LOOKUP_hash_dir());
        if (lookup == NULL) {
            ERR_error_string(SSL_ERR_get(), err);
            c->crl = NULL;
            tcn_Throw(e, "Lookup failed for path %s (%s)", J2S(file), err);
            goto cleanup;
        }
        if (!X509_LOOKUP_add_dir(lookup, J2S(path), X509_FILETYPE_PEM)) {
            ERR_error_string(SSL_ERR_get(), err);
            c->crl = NULL;
            tcn_Throw(e, "Load failed for path %s (%s)", J2S(file), err);
            goto cleanup;
        }
    }
    X509_STORE_set_flags(c->crl, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
    rv = JNI_TRUE;
cleanup:
    TCN_FREE_CSTRING(file);
    TCN_FREE_CSTRING(path);
    return rv;
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCertificateChainFile)(TCN_STDARGS, jlong ctx,
                                                                  jstring file,
                                                                  jboolean skipfirst)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jboolean rv = JNI_FALSE;
    TCN_ALLOC_CSTRING(file);

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);
    if (!J2S(file))
        return JNI_FALSE;
    if (SSL_CTX_use_certificate_chain(c->ctx, J2S(file), skipfirst) > 0)
        rv = JNI_TRUE;
    TCN_FREE_CSTRING(file);
    return rv;
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCACertificate)(TCN_STDARGS,
                                                           jlong ctx,
                                                           jstring file,
                                                           jstring path)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jboolean rv = JNI_TRUE;
    TCN_ALLOC_CSTRING(file);
    TCN_ALLOC_CSTRING(path);

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);
    if (file == NULL && path == NULL)
        return JNI_FALSE;

   /*
     * Configure Client Authentication details
     */
    if (!SSL_CTX_load_verify_locations(c->ctx,
                                       J2S(file), J2S(path))) {
        char err[256];
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Unable to configure locations "
                  "for client authentication (%s)", err);
        rv = JNI_FALSE;
        goto cleanup;
    }
    c->store = SSL_CTX_get_cert_store(c->ctx);
    if (c->mode) {
        STACK_OF(X509_NAME) *ca_certs;
        c->ca_certs++;
        ca_certs = SSL_CTX_get_client_CA_list(c->ctx);
        if (ca_certs == NULL) {
            ca_certs = SSL_load_client_CA_file(J2S(file));
            if (ca_certs != NULL)
                SSL_CTX_set_client_CA_list(c->ctx, ca_certs);
        }
        else {
            if (!SSL_add_file_cert_subjects_to_stack(ca_certs, J2S(file)))
                ca_certs = NULL;
        }
        if (ca_certs == NULL && c->verify_mode == SSL_CVERIFY_REQUIRE) {
            /*
             * Give a warning when no CAs were configured but client authentication
             * should take place. This cannot work.
             */
            if (c->bio_os) {
                BIO_printf(c->bio_os,
                            "[WARN] Oops, you want to request client "
                            "authentication, but no CAs are known for "
                            "verification!?");
            }
            else {
                fprintf(stderr,
                        "[WARN] Oops, you want to request client "
                        "authentication, but no CAs are known for "
                        "verification!?");
            }

        }
    }
cleanup:
    TCN_FREE_CSTRING(file);
    TCN_FREE_CSTRING(path);
    return rv;
}

TCN_IMPLEMENT_CALL(void, SSLContext, setTmpDH)(TCN_STDARGS, jlong ctx,
                                                                  jstring file)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    BIO *bio = NULL;
    DH *dh = NULL;
    TCN_ALLOC_CSTRING(file);
    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);
    TCN_ASSERT(file);

    if (!J2S(file)) {
        tcn_Throw(e, "Error while configuring DH: no dh param file given");
        return;
    }

    bio = BIO_new_file(J2S(file), "r");
    if (!bio) {
        char err[256];
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error while configuring DH using %s: %s", J2S(file), err);
        TCN_FREE_CSTRING(file);
        return;
    }

    dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (!dh) {
        char err[256];
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error while configuring DH: no DH parameter found in %s (%s)", J2S(file), err);
        TCN_FREE_CSTRING(file);
        return;
    }

    if (1 != SSL_CTX_set_tmp_dh(c->ctx, dh)) {
        char err[256];
        DH_free(dh);
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error while configuring DH with file %s: %s", J2S(file), err);
        TCN_FREE_CSTRING(file);
        return;
    }

    DH_free(dh);
    TCN_FREE_CSTRING(file);
}

TCN_IMPLEMENT_CALL(void, SSLContext, setTmpECDHByCurveName)(TCN_STDARGS, jlong ctx,
                                                                  jstring curveName)
{
#ifdef HAVE_ECC
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    int i;
    EC_KEY  *ecdh;
    TCN_ALLOC_CSTRING(curveName);
    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);
    TCN_ASSERT(curveName);

    /* First try to get curve by name */
    i = OBJ_sn2nid(J2S(curveName));
    if (!i) {
        tcn_Throw(e, "Can't configure elliptic curve: unknown curve name %s", J2S(curveName));
        TCN_FREE_CSTRING(curveName);
        return;
    }

    ecdh = EC_KEY_new_by_curve_name(i);
    if (!ecdh) {
        tcn_Throw(e, "Can't configure elliptic curve: unknown curve name %s", J2S(curveName));
        TCN_FREE_CSTRING(curveName);
        return;
    }

    /* Setting found curve to context */
    if (1 != SSL_CTX_set_tmp_ecdh(c->ctx, ecdh)) {
        char err[256];
        EC_KEY_free(ecdh);
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error while configuring elliptic curve %s: %s", J2S(curveName), err);
        TCN_FREE_CSTRING(curveName);
        return;
    }
    EC_KEY_free(ecdh);
    TCN_FREE_CSTRING(curveName);
#else
	tcn_Throw(e, "Cant't configure elliptic curve: unsupported by this OpenSSL version");
	return;
#endif
}

TCN_IMPLEMENT_CALL(void, SSLContext, setShutdownType)(TCN_STDARGS, jlong ctx,
                                                      jint type)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);

    UNREFERENCED_STDARGS;
    TCN_ASSERT(ctx != 0);
    c->shutdown_type = type;
}

TCN_IMPLEMENT_CALL(void, SSLContext, setVerify)(TCN_STDARGS, jlong ctx,
                                                jint level, jint depth)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    int verify = SSL_VERIFY_NONE;

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);
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
    if (!c->store) {
        if (SSL_CTX_set_default_verify_paths(c->ctx)) {
            c->store = SSL_CTX_get_cert_store(c->ctx);
            X509_STORE_set_flags(c->store, 0);
        }
        else {
            /* XXX: See if this is fatal */
        }
    }

    SSL_CTX_set_verify(c->ctx, verify, SSL_callback_SSL_verify);
}

static EVP_PKEY *load_pem_key(tcn_ssl_ctxt_t *c, const char *file)
{
    BIO *bio = NULL;
    EVP_PKEY *key = NULL;
    tcn_pass_cb_t *cb_data = c->cb_data;
    int i;

    if ((bio = BIO_new(BIO_s_file())) == NULL) {
        return NULL;
    }
    if (BIO_read_filename(bio, file) <= 0) {
        BIO_free(bio);
        return NULL;
    }
    if (!cb_data)
        cb_data = &tcn_password_callback;
    for (i = 0; i < 3; i++) {
        key = PEM_read_bio_PrivateKey(bio, NULL,
                    (pem_password_cb *)SSL_password_callback,
                    (void *)cb_data);
        if (key)
            break;
        cb_data->password[0] = '\0';
        BIO_ctrl(bio, BIO_CTRL_RESET, 0, NULL);
    }
    BIO_free(bio);
    return key;
}

static X509 *load_pem_cert(tcn_ssl_ctxt_t *c, const char *file)
{
    BIO *bio = NULL;
    X509 *cert = NULL;
    tcn_pass_cb_t *cb_data = c->cb_data;

    if ((bio = BIO_new(BIO_s_file())) == NULL) {
        return NULL;
    }
    if (BIO_read_filename(bio, file) <= 0) {
        BIO_free(bio);
        return NULL;
    }
    if (!cb_data)
        cb_data = &tcn_password_callback;
    cert = PEM_read_bio_X509_AUX(bio, NULL,
                (pem_password_cb *)SSL_password_callback,
                (void *)cb_data);
    if (cert == NULL &&
       (ERR_GET_REASON(ERR_peek_last_error()) == PEM_R_NO_START_LINE)) {
        SSL_ERR_clear();
        BIO_ctrl(bio, BIO_CTRL_RESET, 0, NULL);
        cert = d2i_X509_bio(bio, NULL);
    }
    BIO_free(bio);
    return cert;
}

static int ssl_load_pkcs12(tcn_ssl_ctxt_t *c, const char *file,
                           EVP_PKEY **pkey, X509 **cert, STACK_OF(X509) **ca)
{
    const char *pass;
    char        buff[PEM_BUFSIZE];
    int         len, rc = 0;
    PKCS12     *p12;
    BIO        *in;
    tcn_pass_cb_t *cb_data = c->cb_data;

    if ((in = BIO_new(BIO_s_file())) == 0)
        return 0;
    if (BIO_read_filename(in, file) <= 0) {
        BIO_free(in);
        return 0;
    }
    p12 = d2i_PKCS12_bio(in, 0);
    if (p12 == 0) {
        /* Error loading PKCS12 file */
        goto cleanup;
    }
    /* See if an empty password will do */
    if (PKCS12_verify_mac(p12, "", 0) || PKCS12_verify_mac(p12, 0, 0)) {
        pass = "";
    }
    else {
        if (!cb_data)
            cb_data = &tcn_password_callback;
        len = SSL_password_callback(buff, PEM_BUFSIZE, 0, cb_data);
        if (len < 0) {
            /* Passpharse callback error */
            goto cleanup;
        }
        if (!PKCS12_verify_mac(p12, buff, len)) {
            /* Mac verify error (wrong password?) in PKCS12 file */
            goto cleanup;
        }
        pass = buff;
    }
    rc = PKCS12_parse(p12, pass, pkey, cert, ca);
cleanup:
    if (p12 != 0)
        PKCS12_free(p12);
    BIO_free(in);
    return rc;
}

TCN_IMPLEMENT_CALL(void, SSLContext, setRandom)(TCN_STDARGS, jlong ctx,
                                                jstring file)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    TCN_ALLOC_CSTRING(file);

    TCN_ASSERT(ctx != 0);
    UNREFERENCED(o);
    if (J2S(file))
        c->rand_file = apr_pstrdup(c->pool, J2S(file));
    TCN_FREE_CSTRING(file);
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCertificate)(TCN_STDARGS, jlong ctx,
                                                         jstring cert, jstring key,
                                                         jstring password, jint idx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jboolean rv = JNI_TRUE;
    TCN_ALLOC_CSTRING(cert);
    TCN_ALLOC_CSTRING(key);
    TCN_ALLOC_CSTRING(password);
    const char *key_file, *cert_file;
    const char *p;
    char err[256];
#ifdef HAVE_ECC
    EC_GROUP *ecparams = NULL;
    int nid;
    EC_KEY *eckey = NULL;
#endif
    DH *dhparams;

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);

    if (idx < 0 || idx >= SSL_AIDX_MAX) {
        /* TODO: Throw something */
        rv = JNI_FALSE;
        goto cleanup;
    }
    if (J2S(password)) {
        if (!c->cb_data)
            c->cb_data = &tcn_password_callback;
        strncpy(c->cb_data->password, J2S(password), SSL_MAX_PASSWORD_LEN - 1);
        c->cb_data->password[SSL_MAX_PASSWORD_LEN-1] = '\0';
    }
    key_file  = J2S(key);
    cert_file = J2S(cert);
    if (!key_file)
        key_file = cert_file;
    if (!key_file || !cert_file) {
        tcn_Throw(e, "No Certificate file specified or invalid file format");
        rv = JNI_FALSE;
        goto cleanup;
    }
    if ((p = strrchr(cert_file, '.')) != NULL && strcmp(p, ".pkcs12") == 0) {
        if (!ssl_load_pkcs12(c, cert_file, &c->keys[idx], &c->certs[idx], 0)) {
            ERR_error_string(SSL_ERR_get(), err);
            tcn_Throw(e, "Unable to load certificate %s (%s)",
                      cert_file, err);
            rv = JNI_FALSE;
            goto cleanup;
        }
    }
    else {
        if ((c->keys[idx] = load_pem_key(c, key_file)) == NULL
#ifndef OPENSSL_NO_ENGINE
                && (tcn_ssl_engine == NULL ||
                (c->keys[idx] = ENGINE_load_private_key(tcn_ssl_engine, key_file,
                                                        NULL, NULL)) == NULL)
#endif
                ) {
            ERR_error_string(SSL_ERR_get(), err);
            tcn_Throw(e, "Unable to load certificate key %s (%s)",
                      key_file, err);
            rv = JNI_FALSE;
            goto cleanup;
        }
        if ((c->certs[idx] = load_pem_cert(c, cert_file)) == NULL) {
            ERR_error_string(SSL_ERR_get(), err);
            tcn_Throw(e, "Unable to load certificate %s (%s)",
                      cert_file, err);
            rv = JNI_FALSE;
            goto cleanup;
        }
    }
    if (SSL_CTX_use_certificate(c->ctx, c->certs[idx]) <= 0) {
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error setting certificate (%s)", err);
        rv = JNI_FALSE;
        goto cleanup;
    }
    if (SSL_CTX_use_PrivateKey(c->ctx, c->keys[idx]) <= 0) {
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error setting private key (%s)", err);
        rv = JNI_FALSE;
        goto cleanup;
    }
    if (SSL_CTX_check_private_key(c->ctx) <= 0) {
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Private key does not match the certificate public key (%s)",
                  err);
        rv = JNI_FALSE;
        goto cleanup;
    }

    /*
     * Try to read DH parameters from the (first) SSLCertificateFile
     */
    /* XXX Does this also work for pkcs12 or only for PEM files?
     * If only for PEM files move above to the PEM handling */
    if ((idx == 0) && (dhparams = SSL_dh_GetParamFromFile(cert_file))) {
        SSL_CTX_set_tmp_dh(c->ctx, dhparams);
        DH_free(dhparams);
    }

#ifdef HAVE_ECC
    /*
     * Similarly, try to read the ECDH curve name from SSLCertificateFile...
     */
    /* XXX Does this also work for pkcs12 or only for PEM files?
     * If only for PEM files move above to the PEM handling */
    if ((ecparams = SSL_ec_GetParamFromFile(cert_file)) &&
        (nid = EC_GROUP_get_curve_name(ecparams)) &&
        (eckey = EC_KEY_new_by_curve_name(nid))) {
        SSL_CTX_set_tmp_ecdh(c->ctx, eckey);
    }
    /*
     * ...otherwise, enable auto curve selection (OpenSSL 1.0.2)
     * or configure NIST P-256 (required to enable ECDHE for earlier versions)
     * ECDH is always enabled in 1.1.0 unless excluded from SSLCipherList
     */
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    else {
#if defined(SSL_CTX_set_ecdh_auto)
        SSL_CTX_set_ecdh_auto(c->ctx, 1);
#else
        eckey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
        SSL_CTX_set_tmp_ecdh(c->ctx, eckey);
#endif
    }
#endif
    /* OpenSSL assures us that _free() is NULL-safe */
    EC_KEY_free(eckey);
    EC_GROUP_free(ecparams);
#endif
    SSL_CTX_set_tmp_dh_callback(c->ctx, SSL_callback_tmp_DH);

cleanup:
    TCN_FREE_CSTRING(cert);
    TCN_FREE_CSTRING(key);
    TCN_FREE_CSTRING(password);
    return rv;
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCertificateRaw)(TCN_STDARGS, jlong ctx,
                                                         jbyteArray javaCert, jbyteArray javaKey, jint idx)
{
#ifdef HAVE_ECC
#ifndef SSL_CTX_set_ecdh_auto
    EC_KEY *eckey = NULL;
#endif
#endif
    jsize lengthOfCert;
    unsigned char* cert;
    X509 * certs;
    EVP_PKEY * evp;
    const unsigned char *tmp;
    BIO * bio;

    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jboolean rv = JNI_TRUE;
    char err[256];

    /* we get the key contents into a byte array */
    jbyte* bufferPtr = (*e)->GetByteArrayElements(e, javaKey, NULL);
    jsize lengthOfKey = (*e)->GetArrayLength(e, javaKey);
    unsigned char* key = malloc(lengthOfKey);
    memcpy(key, bufferPtr, lengthOfKey);
    (*e)->ReleaseByteArrayElements(e, javaKey, bufferPtr, 0);

    bufferPtr = (*e)->GetByteArrayElements(e, javaCert, NULL);
    lengthOfCert = (*e)->GetArrayLength(e, javaCert);
    cert = malloc(lengthOfCert);
    memcpy(cert, bufferPtr, lengthOfCert);
    (*e)->ReleaseByteArrayElements(e, javaCert, bufferPtr, 0);

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);

    if (idx < 0 || idx >= SSL_AIDX_MAX) {
        tcn_Throw(e, "Invalid key type");
        rv = JNI_FALSE;
        goto cleanup;
    }

    tmp = (const unsigned char *)cert;
    certs = d2i_X509(NULL, &tmp, lengthOfCert);
    if (certs == NULL) {
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error reading certificate (%s)", err);
        rv = JNI_FALSE;
        goto cleanup;
    }
    if(c->certs[idx] != NULL) {
        free(c->certs[idx]);
    }
    c->certs[idx] = certs;

    bio = BIO_new(BIO_s_mem());
    BIO_write(bio, key, lengthOfKey);

    evp = PEM_read_bio_PrivateKey(bio, NULL, 0, NULL);
    if (evp == NULL) {
        BIO_free(bio);
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error reading private key (%s)", err);
        rv = JNI_FALSE;
        goto cleanup;
    }
    BIO_free(bio);
    if(c->keys[idx] != NULL) {
        free(c->keys[idx]);
    }
    c->keys[idx] = evp;

    if (SSL_CTX_use_certificate(c->ctx, c->certs[idx]) <= 0) {
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error setting certificate (%s)", err);
        rv = JNI_FALSE;
        goto cleanup;
    }
    if (SSL_CTX_use_PrivateKey(c->ctx, c->keys[idx]) <= 0) {
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error setting private key (%s)", err);
        rv = JNI_FALSE;
        goto cleanup;
    }
    if (SSL_CTX_check_private_key(c->ctx) <= 0) {
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Private key does not match the certificate public key (%s)",
                  err);
        rv = JNI_FALSE;
        goto cleanup;
    }

    /*
     * TODO Try to read DH parameters from somewhere...
     */

#ifdef HAVE_ECC
    /*
     * TODO try to read the ECDH curve name from somewhere...
     */
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#if defined(SSL_CTX_set_ecdh_auto)
    SSL_CTX_set_ecdh_auto(c->ctx, 1);
#else
    eckey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    SSL_CTX_set_tmp_ecdh(c->ctx, eckey);
    EC_KEY_free(eckey);
#endif
#endif
#endif
    SSL_CTX_set_tmp_dh_callback(c->ctx, SSL_callback_tmp_DH);
cleanup:
    free(key);
    free(cert);
    return rv;
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, addChainCertificateRaw)(TCN_STDARGS, jlong ctx,
                                                                 jbyteArray javaCert)
{
    jsize lengthOfCert;
    unsigned char* cert;
    X509 * certs;
    const unsigned char *tmp;

    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jboolean rv = JNI_TRUE;
    char err[256];

    /* we get the cert contents into a byte array */
    jbyte* bufferPtr = (*e)->GetByteArrayElements(e, javaCert, NULL);
    lengthOfCert = (*e)->GetArrayLength(e, javaCert);
    cert = malloc(lengthOfCert);
    memcpy(cert, bufferPtr, lengthOfCert);
    (*e)->ReleaseByteArrayElements(e, javaCert, bufferPtr, 0);

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);

    tmp = (const unsigned char *)cert;
    certs = d2i_X509(NULL, &tmp, lengthOfCert);
    if (certs == NULL) {
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error reading certificate (%s)", err);
        rv = JNI_FALSE;
#if defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x20901000L
    } else {
        tcn_Throw(e, "Unable to use Java keystores with LibreSSL");
#else
    } else if (SSL_CTX_add0_chain_cert(c->ctx, certs) <= 0) {
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error adding certificate to chain (%s)", err);
#endif
        rv = JNI_FALSE;
    }

    free(cert);
    return rv;
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, addClientCACertificateRaw)(TCN_STDARGS, jlong ctx,
                                                                    jbyteArray javaCert)
{
    jsize lengthOfCert;
    unsigned char *charCert;
    X509 *cert;
    const unsigned char *tmp;

    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jboolean rv = JNI_TRUE;
    char err[256];

    /* we get the cert contents into a byte array */
    jbyte* bufferPtr = (*e)->GetByteArrayElements(e, javaCert, NULL);
    lengthOfCert = (*e)->GetArrayLength(e, javaCert);
    charCert = malloc(lengthOfCert);
    memcpy(charCert, bufferPtr, lengthOfCert);
    (*e)->ReleaseByteArrayElements(e, javaCert, bufferPtr, 0);

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);

    tmp = (const unsigned char *)charCert;
    cert = d2i_X509(NULL, &tmp, lengthOfCert);
    if (cert == NULL) {
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error encoding allowed peer CA certificate (%s)", err);
        rv = JNI_FALSE;
    } else if (SSL_CTX_add_client_CA(c->ctx, cert) <= 0) {
        ERR_error_string(SSL_ERR_get(), err);
        tcn_Throw(e, "Error adding allowed peer CA certificate (%s)", err);
        rv = JNI_FALSE;
    }

    free(charCert);
    return rv;
}

static int ssl_array_index(apr_array_header_t *array,
                           const char *s)
{
    int i;
    for (i = 0; i < array->nelts; i++) {
        const char *p = APR_ARRAY_IDX(array, i, const char*);
        if (!strcmp(p, s)) {
            return i;
        }
    }
    return -1;
}

static int ssl_cmp_alpn_protos(apr_array_header_t *array,
                               const char *proto1,
                               const char *proto2)
{
    int index1 = ssl_array_index(array, proto1);
    int index2 = ssl_array_index(array, proto2);
    if (index2 > index1) {
        return (index1 >= 0)? 1 : -1;
    }
    else if (index1 > index2) {
        return (index2 >= 0)? -1 : 1;
    }

    /* Both have the same index (-1 so neither listed by cient) compare
     * the names so that spdy3 gets precedence over spdy2. That makes
     * the outcome at least deterministic. */
    return strcmp((const char *)proto1, (const char *)proto2);
}

/*
 * This callback function is executed when the TLS Application Layer
 * Protocol Negotiate Extension (ALPN, RFC 7301) is triggered by the client
 * hello, giving a list of desired protocol names (in descending preference)
 * to the server.
 * The callback has to select a protocol name or return an error if none of
 * the clients preferences is supported.
 * The selected protocol does not have to be on the client list, according
 * to RFC 7301, so no checks are performed.
 * The client protocol list is serialized as length byte followed by ascii
 * characters (not null-terminated), followed by the next protocol name.
 */
int cb_server_alpn(SSL *ssl,
                   const unsigned char **out, unsigned char *outlen,
                   const unsigned char *in, unsigned int inlen, void *arg)
{
    tcn_ssl_ctxt_t *tcsslctx = (tcn_ssl_ctxt_t *)arg;
    tcn_ssl_conn_t *con = (tcn_ssl_conn_t *)SSL_get_app_data(ssl);
    apr_array_header_t *client_protos;
    apr_array_header_t *proposed_protos;
    int i;
    size_t len;

    if (inlen == 0) {
        // Client specified an empty protocol list. Nothing to negotiate.
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    client_protos = apr_array_make(con->pool , 0, sizeof(char *));
    for (i = 0; i < inlen; /**/) {
        /* Grab length of next item from leading length byte */
        unsigned int plen = in[i++];
        if (plen + i > inlen) {
            // The protocol name extends beyond the declared length
            // of the protocol list.
            return SSL_TLSEXT_ERR_ALERT_FATAL;
        }
        APR_ARRAY_PUSH(client_protos, char*) = apr_pstrndup(con->pool, (const char *)in+i, plen);
        i += plen;
    }

    if (tcsslctx->alpn == NULL) {
        // Server supported protocol names not set.
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    if (tcsslctx->alpnlen == 0) {
        // Server supported protocols is an empty list
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    proposed_protos = apr_array_make(con->pool, 0, sizeof(char *));
    for (i = 0; i < tcsslctx->alpnlen; /**/) {
        /* Grab length of next item from leading length byte */
        unsigned int plen = tcsslctx->alpn[i++];
        if (plen + i > tcsslctx->alpnlen) {
            // The protocol name extends beyond the declared length
            // of the protocol list.
            return SSL_TLSEXT_ERR_ALERT_FATAL;
        }
        APR_ARRAY_PUSH(proposed_protos, char*) = apr_pstrndup(con->pool, (const char *)tcsslctx->alpn+i, plen);
        i += plen;
    }

    if (proposed_protos->nelts <= 0) {
        // Should never happen. The server did not specify any protocols.
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    /* Now select the most preferred protocol from the proposals. */
    *out = APR_ARRAY_IDX(proposed_protos, 0, const unsigned char *);
    for (i = 1; i < proposed_protos->nelts; ++i) {
        const char *proto = APR_ARRAY_IDX(proposed_protos, i, const char*);
        /* Do we prefer it over existing candidate? */
        if (ssl_cmp_alpn_protos(client_protos, (const char *)*out, proto) < 0) {
            *out = (const unsigned char*)proto;
        }
    }

    len = strlen((const char*)*out);
    if (len > 255) {
        // Agreed protocol name too long
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    *outlen = (unsigned char)len;

    return SSL_TLSEXT_ERR_OK;
}

TCN_IMPLEMENT_CALL(jint, SSLContext, setALPN)(TCN_STDARGS, jlong ctx,
                                              jbyteArray buf, jint len)
{
    tcn_ssl_ctxt_t *sslctx = J2P(ctx, tcn_ssl_ctxt_t *);

    sslctx->alpn = apr_pcalloc(sslctx->pool, len);
    (*e)->GetByteArrayRegion(e, buf, 0, len, (jbyte *)sslctx->alpn);
    sslctx->alpnlen = len;

    if (sslctx->mode == SSL_MODE_SERVER) {
        SSL_CTX_set_alpn_select_cb(sslctx->ctx, cb_server_alpn, sslctx);
    } else {
        /*
         * TODO: Implement client side call-back
         * SSL_CTX_set_next_proto_select_cb(sslctx->ctx, cb_request_alpn, sslctx);
         */
        return APR_ENOTIMPL;
    }
    return 0;
}

/* Start of netty-tc-native add */

/* Convert protos to wire format */
static int initProtocols(JNIEnv *e, const tcn_ssl_ctxt_t *c, unsigned char **proto_data,
            unsigned int *proto_len, jobjectArray protos) {
    int i;
    unsigned char *p_data;
    /*
     * We start with allocate 128 bytes which should be good enough for most use-cases while still be pretty low.
     * We will call realloc to increate this if needed.
     */
    size_t p_data_size = 128;
    size_t p_data_len = 0;
    jstring proto_string;
    const char *proto_chars;
    size_t proto_chars_len;
    int cnt;

    if (protos == NULL) {
        // Guard against NULL protos.
        return -1;
    }

    cnt = (*e)->GetArrayLength(e, protos);

    if (cnt == 0) {
        // if cnt is 0 we not need to continue and can just fail fast.
        return -1;
    }

    p_data = (unsigned char *) malloc(p_data_size);
    if (p_data == NULL) {
        // Not enough memory?
        return -1;
    }

    for (i = 0; i < cnt; ++i) {
         proto_string = (jstring) (*e)->GetObjectArrayElement(e, protos, i);
         proto_chars = (*e)->GetStringUTFChars(e, proto_string, 0);

         proto_chars_len = strlen(proto_chars);
         if (proto_chars_len > 0 && proto_chars_len <= MAX_ALPN_NPN_PROTO_SIZE) {
            // We need to add +1 as each protocol is prefixed by it's length (unsigned char).
            // For all except of the last one we already have the extra space as everything is
            // delimited by ','.
            p_data_len += 1 + proto_chars_len;
            if (p_data_len > p_data_size) {
                // double size
                p_data_size <<= 1;
                p_data = realloc(p_data, p_data_size);
                if (p_data == NULL) {
                    // Not enough memory?
                    (*e)->ReleaseStringUTFChars(e, proto_string, proto_chars);
                    break;
                }
            }
            // Write the length of the protocol and then increment before memcpy the protocol itself.
            *p_data = proto_chars_len;
            ++p_data;
            memcpy(p_data, proto_chars, proto_chars_len);
            p_data += proto_chars_len;
         }

         // Release the string to prevent memory leaks
         (*e)->ReleaseStringUTFChars(e, proto_string, proto_chars);
    }

    if (p_data == NULL) {
        // Something went wrong so update the proto_len and return -1
        *proto_len = 0;
        return -1;
    } else {
        if (*proto_data != NULL) {
            // Free old data
            free(*proto_data);
        }
        // Decrement pointer again as we incremented it while creating the protocols in wire format.
        p_data -= p_data_len;
        *proto_data = p_data;
        *proto_len = p_data_len;
        return 0;
    }
}

TCN_IMPLEMENT_CALL(void, SSLContext, setNpnProtos)(TCN_STDARGS, jlong ctx, jobjectArray next_protos,
        jint selectorFailureBehavior)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);

    TCN_ASSERT(ctx != 0);
    UNREFERENCED(o);

    if (initProtocols(e, c, &c->next_proto_data, &c->next_proto_len, next_protos) == 0) {
        c->next_selector_failure_behavior = selectorFailureBehavior;

        // depending on if it's client mode or not we need to call different functions.
        if (c->mode == SSL_MODE_CLIENT)  {
            SSL_CTX_set_next_proto_select_cb(c->ctx, SSL_callback_select_next_proto, (void *)c);
        } else {
            SSL_CTX_set_next_protos_advertised_cb(c->ctx, SSL_callback_next_protos, (void *)c);
        }
    }
}

TCN_IMPLEMENT_CALL(void, SSLContext, setAlpnProtos)(TCN_STDARGS, jlong ctx, jobjectArray alpn_protos,
        jint selectorFailureBehavior)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);

    TCN_ASSERT(ctx != 0);
    UNREFERENCED(o);

    if (initProtocols(e, c, &c->alpn_proto_data, &c->alpn_proto_len, alpn_protos) == 0) {
        c->alpn_selector_failure_behavior = selectorFailureBehavior;

        // depending on if it's client mode or not we need to call different functions.
        if (c->mode == SSL_MODE_CLIENT)  {
            SSL_CTX_set_alpn_protos(c->ctx, c->alpn_proto_data, c->alpn_proto_len);
        } else {
            SSL_CTX_set_alpn_select_cb(c->ctx, SSL_callback_alpn_select_proto, (void *) c);

        }
    }
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, setSessionCacheMode)(TCN_STDARGS, jlong ctx, jlong mode)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    return SSL_CTX_set_session_cache_mode(c->ctx, mode);
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, getSessionCacheMode)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    return SSL_CTX_get_session_cache_mode(c->ctx);
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, setSessionCacheTimeout)(TCN_STDARGS, jlong ctx, jlong timeout)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_set_timeout(c->ctx, timeout);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, getSessionCacheTimeout)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    return SSL_CTX_get_timeout(c->ctx);
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, setSessionCacheSize)(TCN_STDARGS, jlong ctx, jlong size)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = 0;

    // Also allow size of 0 which is unlimited
    if (size >= 0) {
      SSL_CTX_set_session_cache_mode(c->ctx, SSL_SESS_CACHE_SERVER);
      rv = SSL_CTX_sess_set_cache_size(c->ctx, size);
    }

    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, getSessionCacheSize)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    return SSL_CTX_sess_get_cache_size(c->ctx);
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionNumber)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_number(c->ctx);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionConnect)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_connect(c->ctx);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionConnectGood)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_connect_good(c->ctx);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionConnectRenegotiate)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_connect_renegotiate(c->ctx);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionAccept)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_accept(c->ctx);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionAcceptGood)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_accept_good(c->ctx);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionAcceptRenegotiate)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_accept_renegotiate(c->ctx);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionHits)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_hits(c->ctx);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionCbHits)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_cb_hits(c->ctx);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionMisses)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_misses(c->ctx);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionTimeouts)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_timeouts(c->ctx);
    return rv;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionCacheFull)(TCN_STDARGS, jlong ctx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jlong rv = SSL_CTX_sess_cache_full(c->ctx);
    return rv;
}

#define TICKET_KEYS_SIZE 48
TCN_IMPLEMENT_CALL(void, SSLContext, setSessionTicketKeys)(TCN_STDARGS, jlong ctx, jbyteArray keys)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    jbyte* b;

    if ((*e)->GetArrayLength(e, keys) != TICKET_KEYS_SIZE) {
        if (c->bio_os) {
            BIO_printf(c->bio_os, "[ERROR] Session ticket keys provided were wrong size.");
        }
        else {
            fprintf(stderr, "[ERROR] Session ticket keys provided were wrong size.");
        }
        exit(1);
    }

    b = (*e)->GetByteArrayElements(e, keys, NULL);
    SSL_CTX_set_tlsext_ticket_keys(c->ctx, b, TICKET_KEYS_SIZE);
    (*e)->ReleaseByteArrayElements(e, keys, b, 0);
}


#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)

/*
 * Adapted from OpenSSL:
 * https://github.com/openssl/openssl/blob/OpenSSL_1_0_2-stable/ssl/ssl_locl.h#L318
 */
/* Bits for algorithm_mkey (key exchange algorithm) */
/* RSA key exchange */
# define SSL_kRSA                0x00000001L
/* DH cert, RSA CA cert */
# define SSL_kDHr                0x00000002L
/* DH cert, DSA CA cert */
# define SSL_kDHd                0x00000004L
/* tmp DH key no DH cert */
# define SSL_kEDH                0x00000008L
/* forward-compatible synonym */
# define SSL_kDHE                SSL_kEDH
/* Kerberos5 key exchange */
# define SSL_kKRB5               0x00000010L
/* ECDH cert, RSA CA cert */
# define SSL_kECDHr              0x00000020L
/* ECDH cert, ECDSA CA cert */
# define SSL_kECDHe              0x00000040L
/* ephemeral ECDH */
# define SSL_kEECDH              0x00000080L
/* forward-compatible synonym */
# define SSL_kECDHE              SSL_kEECDH
/* PSK */
# define SSL_kPSK                0x00000100L
/* GOST key exchange */
# define SSL_kGOST               0x00000200L
/* SRP */
# define SSL_kSRP                0x00000400L

/* Bits for algorithm_auth (server authentication) */
/* RSA auth */
# define SSL_aRSA                0x00000001L
/* DSS auth */
# define SSL_aDSS                0x00000002L
/* no auth (i.e. use ADH or AECDH) */
# define SSL_aNULL               0x00000004L
/* Fixed DH auth (kDHd or kDHr) */
# define SSL_aDH                 0x00000008L
/* Fixed ECDH auth (kECDHe or kECDHr) */
# define SSL_aECDH               0x00000010L
/* KRB5 auth */
# define SSL_aKRB5               0x00000020L
/* ECDSA auth*/
# define SSL_aECDSA              0x00000040L
/* PSK auth */
# define SSL_aPSK                0x00000080L
/* GOST R 34.10-94 signature auth */
# define SSL_aGOST94             0x00000100L
/* GOST R 34.10-2001 signature auth */
# define SSL_aGOST01             0x00000200L
/* SRP auth */
# define SSL_aSRP                0x00000400L

/* OpenSSL end */

#define TCN_SSL_kRSA                SSL_kRSA
#define TCN_SSL_kDHr                SSL_kDHr
#define TCN_SSL_kDHd                SSL_kDHd
#define TCN_SSL_kDHE                SSL_kDHE
#define TCN_SSL_kKRB5               SSL_kKRB5
#define TCN_SSL_kECDHr              SSL_kECDHr
#define TCN_SSL_kECDHe              SSL_kECDHe
#define TCN_SSL_kECDHE              SSL_kECDHE

#define TCN_SSL_aRSA                SSL_aRSA
#define TCN_SSL_aDSS                SSL_aDSS
#define TCN_SSL_aNULL               SSL_aNULL
#define TCN_SSL_aDH                 SSL_aDH
#define TCN_SSL_aECDH               SSL_aECDH
#define TCN_SSL_aKRB5               SSL_aKRB5
#define TCN_SSL_aECDSA              SSL_aECDSA

#else

#define TCN_SSL_kRSA                NID_kx_rsa
#define TCN_SSL_kDHE                NID_kx_dhe
#define TCN_SSL_kECDHE              NID_kx_ecdhe

#define TCN_SSL_aRSA                NID_auth_rsa
#define TCN_SSL_aDSS                NID_auth_dss
#define TCN_SSL_aNULL               NID_auth_null
#define TCN_SSL_aECDSA              NID_auth_ecdsa

#endif

/*
 * Adapted from Android:
 * https://android.googlesource.com/platform/external/openssl/+/master/patches/0003-jsse.patch
 */
static const char* SSL_CIPHER_authentication_method(const SSL_CIPHER* cipher){
    int auth;
    int kx;
    if (cipher == NULL) {
        return "UNKNOWN";
    }
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
    kx = cipher->algorithm_mkey;
    auth = cipher->algorithm_auth;
#else
    kx = SSL_CIPHER_get_kx_nid(cipher);
    auth = SSL_CIPHER_get_auth_nid(cipher);
#endif

    switch (kx)
        {
    case TCN_SSL_kRSA:
        return SSL_TXT_RSA;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    case TCN_SSL_kDHr:
        return SSL_TXT_DH "_" SSL_TXT_RSA;
    case TCN_SSL_kDHd:
        return SSL_TXT_DH "_" SSL_TXT_DSS;
#endif
    case TCN_SSL_kDHE:
        switch (auth)
            {
        case TCN_SSL_aDSS:
            return "DHE_" SSL_TXT_DSS;
        case TCN_SSL_aRSA:
            return "DHE_" SSL_TXT_RSA;
        case TCN_SSL_aNULL:
            return SSL_TXT_DH "_anon";
        default:
            return "UNKNOWN";
            }
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    case TCN_SSL_kKRB5:
        return SSL_TXT_KRB5;
    case TCN_SSL_kECDHr:
        return SSL_TXT_ECDH "_" SSL_TXT_RSA;
    case TCN_SSL_kECDHe:
        return SSL_TXT_ECDH "_" SSL_TXT_ECDSA;
#endif
    case TCN_SSL_kECDHE:
        switch (auth)
            {
        case TCN_SSL_aECDSA:
            return "ECDHE_" SSL_TXT_ECDSA;
        case TCN_SSL_aRSA:
            return "ECDHE_" SSL_TXT_RSA;
        case TCN_SSL_aNULL:
            return SSL_TXT_ECDH "_anon";
        default:
            return "UNKNOWN";
            }
    default:
        return "UNKNOWN";
    }
}

static const char* SSL_authentication_method(const SSL* ssl) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
   return SSL_CIPHER_authentication_method(ssl->s3->tmp.new_cipher);
#else
    /* XXX ssl->s3->tmp.new_cipher is no longer available in OpenSSL 1.1.0 */
    /* https://github.com/netty/netty-tcnative/blob/1.1.33/openssl-dynamic/src/main/c/sslcontext.c
     * contains a different method, but I think this is not correct.
     * Instead of choosing the cipher used for the current handshake it simply
     * uses the first cipher available during the handshake. */
    /* Not sure whether SSL_get_current_cipher(ssl) returns something useful
     * at the point in time we call it. */
   return SSL_CIPHER_authentication_method(SSL_get_current_cipher(ssl));
#endif
}
/* Android end */

static int SSL_cert_verify(X509_STORE_CTX *ctx, void *arg) {
    /* Get Apache context back through OpenSSL context */
    SSL *ssl = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
    tcn_ssl_ctxt_t *c = SSL_get_app_data2(ssl);


    // Get a stack of all certs in the chain
    STACK_OF(X509) *sk = X509_STORE_CTX_get0_untrusted(ctx);

    int len = sk_X509_num(sk);
    unsigned i;
    X509 *cert;
    int length;
    unsigned char *buf;
    JNIEnv *e;
    jbyteArray array;
    jbyteArray bArray;
    const char *authMethod;
    jstring authMethodString;
    jboolean result;
    int r;
    tcn_get_java_env(&e);

    // Create the byte[][] array that holds all the certs
    array = (*e)->NewObjectArray(e, len, byteArrayClass, NULL);

    for(i = 0; i < len; i++) {
        cert = (X509*) sk_X509_value(sk, i);

        buf = NULL;
        length = i2d_X509(cert, &buf);
        if (length < 0) {
            // In case of error just return an empty byte[][]
            array = (*e)->NewObjectArray(e, 0, byteArrayClass, NULL);
            // We need to delete the local references so we not leak memory as this method is called via callback.
            OPENSSL_free(buf);
            break;
        }
        bArray = (*e)->NewByteArray(e, length);
        (*e)->SetByteArrayRegion(e, bArray, 0, length, (jbyte*) buf);
        (*e)->SetObjectArrayElement(e, array, i, bArray);

        // Delete the local reference as we not know how long the chain is and local references are otherwise
        // only freed once jni method returns.
        (*e)->DeleteLocalRef(e, bArray);
        OPENSSL_free(buf);
    }

    authMethod = SSL_authentication_method(ssl);
    authMethodString = (*e)->NewStringUTF(e, authMethod);

    result = (*e)->CallBooleanMethod(e, c->verifier, c->verifier_method, P2J(ssl), array,
            authMethodString);

    r = result == JNI_TRUE ? 1 : 0;

    // We need to delete the local references so we not leak memory as this method is called via callback.
    (*e)->DeleteLocalRef(e, authMethodString);
    (*e)->DeleteLocalRef(e, array);
    return r;
}


TCN_IMPLEMENT_CALL(void, SSLContext, setCertVerifyCallback)(TCN_STDARGS, jlong ctx, jobject verifier)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);

    if (verifier == NULL) {
        SSL_CTX_set_cert_verify_callback(c->ctx, NULL, NULL);
    } else {
        jclass verifier_class = (*e)->GetObjectClass(e, verifier);
        jmethodID method = (*e)->GetMethodID(e, verifier_class, "verify", "(J[[BLjava/lang/String;)Z");

        if (method == NULL) {
            return;
        }
        // Delete the reference to the previous specified verifier if needed.
        if (c->verifier != NULL) {
            (*e)->DeleteLocalRef(e, c->verifier);
        }
        c->verifier = (*e)->NewGlobalRef(e, verifier);
        c->verifier_method = method;

        SSL_CTX_set_cert_verify_callback(c->ctx, SSL_cert_verify, NULL);
    }
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setSessionIdContext)(TCN_STDARGS, jlong ctx, jbyteArray sidCtx)
{
    tcn_ssl_ctxt_t *c = J2P(ctx, tcn_ssl_ctxt_t *);
    int len = (*e)->GetArrayLength(e, sidCtx);
    unsigned char *buf;
    int res;

    UNREFERENCED(o);
    TCN_ASSERT(ctx != 0);

    buf = malloc(len);

    (*e)->GetByteArrayRegion(e, sidCtx, 0, len, (jbyte*) buf);

    res = SSL_CTX_set_session_id_context(c->ctx, buf, len);
    free(buf);

    if (res == 1) {
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

/* End of netty-tc-native add */
#else
/* OpenSSL is not supported.
 * Create empty stubs.
 */

TCN_IMPLEMENT_CALL(jlong, SSLContext, make)(TCN_STDARGS, jlong pool,
                                            jint protocol, jint mode)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(pool);
    UNREFERENCED(protocol);
    UNREFERENCED(mode);
    return 0;
}

TCN_IMPLEMENT_CALL(jint, SSLContext, free)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return APR_ENOTIMPL;
}

TCN_IMPLEMENT_CALL(void, SSLContext, setContextId)(TCN_STDARGS, jlong ctx,
                                                   jstring id)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(id);
}

TCN_IMPLEMENT_CALL(void, SSLContext, setBIO)(TCN_STDARGS, jlong ctx,
                                             jlong bio, jint dir)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(bio);
    UNREFERENCED(dir);
}

TCN_IMPLEMENT_CALL(void, SSLContext, setOptions)(TCN_STDARGS, jlong ctx,
                                                 jint opt)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(opt);
}

TCN_IMPLEMENT_CALL(jint, SSLContext, getOptions)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(void, SSLContext, clearOptions)(TCN_STDARGS, jlong ctx,
                                                   jint opt)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(opt);
}

TCN_IMPLEMENT_CALL(void, SSLContext, setQuietShutdown)(TCN_STDARGS, jlong ctx,
                                                       jboolean mode)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(mode);
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCipherSuite)(TCN_STDARGS, jlong ctx,
                                                         jstring ciphers)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(ciphers);
    return JNI_FALSE;
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCARevocation)(TCN_STDARGS, jlong ctx,
                                                          jstring file,
                                                          jstring path)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(file);
    UNREFERENCED(path);
    return JNI_FALSE;
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCertificateChainFile)(TCN_STDARGS, jlong ctx,
                                                                  jstring file,
                                                                  jboolean skipfirst)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(file);
    UNREFERENCED(skipfirst);
    return JNI_FALSE;
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCACertificate)(TCN_STDARGS,
                                                           jlong ctx,
                                                           jstring file,
                                                           jstring path)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(file);
    UNREFERENCED(path);
    return JNI_FALSE;
}

TCN_IMPLEMENT_CALL(void, SSLContext, setShutdownType)(TCN_STDARGS, jlong ctx,
                                                      jint type)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(type);
}

TCN_IMPLEMENT_CALL(void, SSLContext, setVerify)(TCN_STDARGS, jlong ctx,
                                                jint level, jint depth)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(level);
    UNREFERENCED(depth);
}

TCN_IMPLEMENT_CALL(void, SSLContext, setRandom)(TCN_STDARGS, jlong ctx,
                                                jstring file)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(file);
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCertificate)(TCN_STDARGS, jlong ctx,
                                                         jstring cert, jstring key,
                                                         jstring password, jint idx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(cert);
    UNREFERENCED(key);
    UNREFERENCED(password);
    UNREFERENCED(idx);
    return JNI_FALSE;
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, setCertificateRaw)(TCN_STDARGS, jlong ctx,
                                                         jbyteArray javaCert, jbyteArray javaKey,
                                                         jint idx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(javaCert);
    UNREFERENCED(javaKey);
    UNREFERENCED(idx);
    return JNI_FALSE;
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, addChainCertificateRaw)(TCN_STDARGS, jlong ctx,
                                                                 jbyteArray javaCert)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(javaCert);
    return JNI_FALSE;
}

TCN_IMPLEMENT_CALL(jboolean, SSLContext, addClientCACertificateRaw)(TCN_STDARGS, jlong ctx,
                                                                    jbyteArray javaCert)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(javaCert);
    return JNI_FALSE;
}

TCN_IMPLEMENT_CALL(jint, SSLContext, setALPN)(TCN_STDARGS, jlong ctx,
                                              jbyteArray buf, jint len)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(buf);
    UNREFERENCED(len);
    return APR_ENOTIMPL;
}

/* Start of netty-tc-native add */

TCN_IMPLEMENT_CALL(void, SSLContext, setNpnProtos)(TCN_STDARGS, jlong ctx, jobjectArray next_protos,
        jint selectorFailureBehavior)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(next_protos);
}


TCN_IMPLEMENT_CALL(void, SSLContext, setAlpnProtos)(TCN_STDARGS, jlong ctx, jobjectArray alpn_protos,
        jint selectorFailureBehavior)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(alpn_protos);
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, setSessionCacheMode)(TCN_STDARGS, jlong ctx, jlong mode)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(mode);
    return -1;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, getSessionCacheMode)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return -1;
}


TCN_IMPLEMENT_CALL(jlong, SSLContext, setSessionCacheTimeout)(TCN_STDARGS, jlong ctx, jlong timeout)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(timeout);
    return -1;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, getSessionCacheTimeout)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return -1;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, setSessionCacheSize)(TCN_STDARGS, jlong ctx, jlong size)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(size);
    return -1;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, getSessionCacheSize)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return -1;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionNumber)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionConnect)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionConnectGood)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionConnectRenegotiate)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionAccept)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionAcceptGood)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionAcceptRenegotiate)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionHits)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionCbHits)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionTimeouts)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionCacheFull)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(jlong, SSLContext, sessionMisses)(TCN_STDARGS, jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    return 0;
}

TCN_IMPLEMENT_CALL(void, SSLContext, setSessionTicketKeys)(TCN_STDARGS, jlong ctx, jbyteArray keys)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(keys);
}

TCN_IMPLEMENT_CALL(void, SSLContext, setCertVerifyCallback)(TCN_STDARGS, jlong ctx, jobject verifier)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(verifier);
}
TCN_IMPLEMENT_CALL(jboolean, SSLContext, setSessionIdContext)(TCN_STDARGS, jlong ctx, jbyteArray sidCtx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(ctx);
    UNREFERENCED(sidCtx);
    return JNI_FALSE;
}
/* End of netty-tc-native add */

#endif
