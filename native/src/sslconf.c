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

#include "ssl_private.h"

#ifdef HAVE_SSL_CONF_CMD

#define SSL_THROW_RETURN -9

#include "apr_file_io.h"

/**
 * Define the Path Checking modes
 */
#define PCM_EXISTS     0x1
#define PCM_ISREG      0x2
#define PCM_ISDIR      0x4
#define PCM_ISNONZERO  0x8

#define FLAGS_CHECK_FILE (PCM_EXISTS|PCM_ISREG|PCM_ISNONZERO)
#define FLAGS_CHECK_DIR  (PCM_EXISTS|PCM_ISDIR)

static int path_check(apr_pool_t *p, const char *path, int pcm)
{
    apr_finfo_t finfo;

    if (path == NULL)
        return 1;
    if (pcm & PCM_EXISTS &&
        apr_stat(&finfo, path, APR_FINFO_TYPE|APR_FINFO_SIZE, p) != 0)
        return 1;
    if (pcm & PCM_ISREG && finfo.filetype != APR_REG)
        return 1;
    if (pcm & PCM_ISDIR && finfo.filetype != APR_DIR)
        return 1;
    if (pcm & PCM_ISNONZERO && finfo.size <= 0)
        return 1;
    return 0;
}


static int check_dir(apr_pool_t *p, const char *dir)
{
    return path_check(p, dir, FLAGS_CHECK_DIR);
}

static int check_file(apr_pool_t *p, const char *file)
{
    return path_check(p, file, FLAGS_CHECK_FILE);
}

static apr_status_t ssl_ctx_config_cleanup(void *data)
{
    tcn_ssl_conf_ctxt_t *c = (tcn_ssl_conf_ctxt_t *)data;
    if (c != NULL && c->cctx != NULL) {
        SSL_CONF_CTX_free(c->cctx);
        c->cctx = NULL;
        c->pool = NULL;
    }
    return APR_SUCCESS;
}

/* Initialize an SSL_CONF context */
TCN_IMPLEMENT_CALL(jlong, SSLConf, make)(TCN_STDARGS, jlong pool,
                                         jint flags)
{
    apr_pool_t *p = J2P(pool, apr_pool_t *);
    tcn_ssl_conf_ctxt_t *c = NULL;
    SSL_CONF_CTX *cctx;
    unsigned long ec;

    UNREFERENCED(o);

    SSL_ERR_clear();
    cctx = SSL_CONF_CTX_new();
    ec = SSL_ERR_get();
    if (!cctx || ec != 0) {
        if (ec != 0) {
            char err[TCN_OPENSSL_ERROR_STRING_LENGTH];
            ERR_error_string_n(ec, err, TCN_OPENSSL_ERROR_STRING_LENGTH);
            tcn_Throw(e, "Could not create SSL_CONF context (%s)", err);
        } else {
            tcn_Throw(e, "Could not create SSL_CONF context");
        }
        return 0;
    }

    SSL_CONF_CTX_set_flags(cctx, flags);

    if ((c = apr_pcalloc(p, sizeof(tcn_ssl_conf_ctxt_t))) == NULL) {
        tcn_ThrowAPRException(e, apr_get_os_error());
        return 0;
    }

    c->cctx = cctx;
    c->pool = p;

    /*
     * Some Tomcat Native specific settings are also set via this representation
     * of the SSL_CONF_CTX. This process is a little bit hacky. The expected
     * call sequence is:
     * - SSLConf.make() - create SSL_CONF_CTX and the associated Tomcat Native
     *   object
     * - SSLConf.check() - MUST be called for each Tomcat specific setting that
     *   needs to be configured. May be called for OpenSSL settings in which
     *   case the setting will be validated.
     * - SSLConf.assign() - this actually *applies* the Tomcat Native specific
     *   configuration to Tomcat Native as well as linking the SSL_CONF_CTX
     *   object with the SSL_CTX object.
     * - SSLConf.apply() - called for each OpenSSL setting. Any Tomcat specific
     *   settings used here will be ignored.
     * - SSLConf.finish() - MUST be called to complete the OpenSSL setting
     *   process.
     */
    /* Initialise Tomcat Native specific OCSP defaults */
    c->no_ocsp_check     = OCSP_NO_CHECK_DEFAULT;
    c->ocsp_soft_fail    = OCSP_SOFT_FAIL_DEFAULT;
    c->ocsp_timeout      = OCSP_TIMEOUT_DEFAULT;
    c->ocsp_verify_flags = OCSP_VERIFY_FLAGS_DEFAULT;
    
    /*
     * Let us cleanup the SSL_CONF context when the pool is destroyed
     */
    apr_pool_cleanup_register(p, (const void *)c,
                              ssl_ctx_config_cleanup,
                              apr_pool_cleanup_null);

    return P2J(c);
}

/* Destroy an SSL_CONF context */
TCN_IMPLEMENT_CALL(void, SSLConf, free)(TCN_STDARGS, jlong cctx)
{
    tcn_ssl_conf_ctxt_t *c = J2P(cctx, tcn_ssl_conf_ctxt_t *);
    UNREFERENCED_STDARGS;
    TCN_ASSERT(c != 0);
    apr_pool_cleanup_run(c->pool, c, ssl_ctx_config_cleanup);
}

/* Check a command for an SSL_CONF context */
TCN_IMPLEMENT_CALL(jint, SSLConf, check)(TCN_STDARGS, jlong cctx,
                                         jstring cmd, jstring value)
{
    tcn_ssl_conf_ctxt_t *c = J2P(cctx, tcn_ssl_conf_ctxt_t *);
    int rc = 1;
    int value_type;
    unsigned long ec;
    TCN_ALLOC_CSTRING(cmd);
    TCN_ALLOC_CSTRING(value);
    UNREFERENCED(o);
    TCN_ASSERT(c != 0);
    TCN_ASSERT(c->cctx != 0);
    if (!J2S(cmd)) {
        tcn_Throw(e, "Can not check null SSL_CONF command");
        rc = SSL_THROW_RETURN;
        goto cleanup;
    }
    /*
     * Although this is the check method, this sets the Tomcat specific
     * settings.
     */
    if (!strcmp(J2S(cmd), "NO_OCSP_CHECK")) {
        if (!strcasecmp(J2S(value), "false"))
            c->no_ocsp_check = 0;
        else
            c->no_ocsp_check = 1;
        rc = 1;
        goto cleanup;
    }

    if (!strcmp(J2S(cmd), "OCSP_SOFT_FAIL")) {
        if (!strcasecmp(J2S(value), "false"))
            c->ocsp_soft_fail = 0;
        else
            c->ocsp_soft_fail = 1;
        rc = 1;
        goto cleanup;
    }

    if (!strcmp(J2S(cmd), "OCSP_TIMEOUT")) {
        int i;
        errno = 0;
        i = (int) strtol(J2S(value), NULL, 10);
        if (!errno) {
            // Tomcat configures timeout is millisecond. APR uses microseconds.
            c->ocsp_timeout = i * 1000;
        }
        rc = 1;
        goto cleanup;
    }

    if (!strcmp(J2S(cmd), "OCSP_VERIFY_FLAGS")) {
        int i;
        errno = 0;
        i = (int) strtol(J2S(value), NULL, 10);
        if (!errno) {
            c->ocsp_verify_flags = i;
        }
        rc = 1;
        goto cleanup;
    }

    SSL_ERR_clear();
    value_type = SSL_CONF_cmd_value_type(c->cctx, J2S(cmd));
    ec = SSL_ERR_get();
    if (ec != 0) {
        char err[TCN_OPENSSL_ERROR_STRING_LENGTH];
        ERR_error_string_n(ec, err, TCN_OPENSSL_ERROR_STRING_LENGTH);
        tcn_Throw(e, "Could not determine SSL_CONF command type for '%s' (%s)", J2S(cmd), err);
        rc = SSL_THROW_RETURN;
        goto cleanup;
    }

    if (value_type == SSL_CONF_TYPE_UNKNOWN) {
        tcn_Throw(e, "Invalid SSL_CONF command '%s', type unknown", J2S(cmd));
        rc = SSL_THROW_RETURN;
        goto cleanup;
    }

    if (value_type == SSL_CONF_TYPE_FILE) {
        if (!J2S(value)) {
            tcn_Throw(e, "SSL_CONF command '%s' needs a non-empty file argument", J2S(cmd));
            rc = SSL_THROW_RETURN;
            goto cleanup;
        }
        if (check_file(c->pool, J2S(value))) {
            tcn_Throw(e, "SSL_CONF command '%s' file '%s' does not exist or is empty", J2S(cmd), J2S(value));
            rc = SSL_THROW_RETURN;
            goto cleanup;
        }
    }
    else if (value_type == SSL_CONF_TYPE_DIR) {
        if (!J2S(value)) {
            tcn_Throw(e, "SSL_CONF command '%s' needs a non-empty directory argument", J2S(cmd));
            rc = SSL_THROW_RETURN;
            goto cleanup;
        }
        if (check_dir(c->pool, J2S(value))) {
            tcn_Throw(e, "SSL_CONF command '%s' directory '%s' does not exist", J2S(cmd), J2S(value));
            rc = SSL_THROW_RETURN;
            goto cleanup;
        }
    }

cleanup:
    TCN_FREE_CSTRING(cmd);
    TCN_FREE_CSTRING(value);
    return rc;
}

/* Assign an SSL_CTX to an SSL_CONF context */
TCN_IMPLEMENT_CALL(void, SSLConf, assign)(TCN_STDARGS, jlong cctx,
                                          jlong ctx)
{
    tcn_ssl_conf_ctxt_t *c = J2P(cctx, tcn_ssl_conf_ctxt_t *);
    tcn_ssl_ctxt_t *sc = J2P(ctx, tcn_ssl_ctxt_t *);
    UNREFERENCED_STDARGS;
    TCN_ASSERT(c != 0);
    TCN_ASSERT(c->cctx != 0);
    TCN_ASSERT(sc != 0);
    // sc->ctx == 0 is allowed!
    SSL_CONF_CTX_set_ssl_ctx(c->cctx, sc->ctx);
    sc->no_ocsp_check = c->no_ocsp_check;
    sc->ocsp_soft_fail = c->ocsp_soft_fail;
    sc->ocsp_timeout = c->ocsp_timeout;
    sc->ocsp_verify_flags = c->ocsp_verify_flags;
}

/* Apply a command to an SSL_CONF context */
TCN_IMPLEMENT_CALL(jint, SSLConf, apply)(TCN_STDARGS, jlong cctx,
                                         jstring cmd, jstring value)
{
    tcn_ssl_conf_ctxt_t *c = J2P(cctx, tcn_ssl_conf_ctxt_t *);
    int rc;
    unsigned long ec;
#ifndef HAVE_EXPORT_CIPHERS
    size_t len;
#endif
    char *buf = NULL;
    TCN_ALLOC_CSTRING(cmd);
    TCN_ALLOC_CSTRING(value);
    UNREFERENCED(o);
    TCN_ASSERT(c != 0);
    TCN_ASSERT(c->cctx != 0);
    if (!J2S(cmd)) {
        tcn_Throw(e, "Can not apply null SSL_CONF command");
        rc = SSL_THROW_RETURN;
        goto cleanup;
    }
#ifndef HAVE_EXPORT_CIPHERS
    if (!strcmp(J2S(cmd), "CipherString")) {
        /*
         *  Always disable NULL and export ciphers,
         *  no matter what was given in the config.
         */
        len = strlen(J2S(value)) + strlen(SSL_CIPHERS_ALWAYS_DISABLED) + 1;
        buf = malloc(len * sizeof(char));
        if (buf == NULL) {
            tcn_Throw(e, "Could not allocate memory to adjust cipher string");
            rc = SSL_THROW_RETURN;
            goto cleanup;
        }
        memcpy(buf, SSL_CIPHERS_ALWAYS_DISABLED, strlen(SSL_CIPHERS_ALWAYS_DISABLED));
        memcpy(buf + strlen(SSL_CIPHERS_ALWAYS_DISABLED), J2S(value), strlen(J2S(value)));
        buf[len - 1] = '\0';
    }
#endif
    if (!strcmp(J2S(cmd), "NO_OCSP_CHECK")) {
        /*
         * Skip as this is a Tomcat specific setting that will have been set
         * when check() was called.
         */
        rc = 1;
        goto cleanup;
    }
    if (!strcmp(J2S(cmd), "OCSP_SOFT_FAIL")) {
        /*
         * Skip as this is a Tomcat specific setting that will have been set
         * when check() was called.
         */
        rc = 1;
        goto cleanup;
    }
    if (!strcmp(J2S(cmd), "OCSP_TIMEOUT")) {
        /*
         * Skip as this is a Tomcat specific setting that will have been set
         * when check() was called.
         */
        rc = 1;
        goto cleanup;
    }
    if (!strcmp(J2S(cmd), "OCSP_VERIFY_FLAGS")) {
        /*
         * Skip as this is a Tomcat specific setting that will have been set
         * when check() was called.
         */
        rc = 1;
        goto cleanup;
    }
    SSL_ERR_clear();
    rc = SSL_CONF_cmd(c->cctx, J2S(cmd), buf != NULL ? buf : J2S(value));
    ec = SSL_ERR_get();
    if (rc <= 0 || ec != 0) {
        if (ec != 0) {
            char err[TCN_OPENSSL_ERROR_STRING_LENGTH];
            ERR_error_string_n(ec, err, TCN_OPENSSL_ERROR_STRING_LENGTH);
            tcn_Throw(e, "Could not apply SSL_CONF command '%s' with value '%s' (%s)", J2S(cmd), buf != NULL ? buf : J2S(value), err);
        } else {
            tcn_Throw(e, "Could not apply SSL_CONF command '%s' with value '%s'", J2S(cmd), buf != NULL ? buf : J2S(value));
        }
        rc = SSL_THROW_RETURN;
        goto cleanup;
    }

cleanup:
#ifndef HAVE_EXPORT_CIPHERS
    if (buf != NULL) {
        free(buf);
    }
#endif
    TCN_FREE_CSTRING(cmd);
    TCN_FREE_CSTRING(value);
    return rc;
}

/* Finish an SSL_CONF context */
TCN_IMPLEMENT_CALL(jint, SSLConf, finish)(TCN_STDARGS, jlong cctx)
{
    tcn_ssl_conf_ctxt_t *c = J2P(cctx, tcn_ssl_conf_ctxt_t *);
    int rc;
    unsigned long ec;

    UNREFERENCED_STDARGS;
    TCN_ASSERT(c != 0);
    TCN_ASSERT(c->cctx != 0);
    SSL_ERR_clear();
    rc = SSL_CONF_CTX_finish(c->cctx);
    ec = SSL_ERR_get();
    if (rc <= 0 || ec != 0) {
        if (ec != 0) {
            char err[TCN_OPENSSL_ERROR_STRING_LENGTH];
            ERR_error_string_n(ec, err, TCN_OPENSSL_ERROR_STRING_LENGTH);
            tcn_Throw(e, "Could not finish SSL_CONF commands (%s)", err);
        } else {
            tcn_Throw(e, "Could not finish SSL_CONF commands");
        }
        return SSL_THROW_RETURN;
    }
    return rc;
}


#else /* HAVE_SSL_CONF_CMD */
/* SSL_CONF is not supported.
 * Create empty stubs.
 */

TCN_IMPLEMENT_CALL(jlong, SSLConf, make)(TCN_STDARGS, jlong pool,
                                         jint flags)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(pool);
    UNREFERENCED(flags);
    return 0;
}

TCN_IMPLEMENT_CALL(void, SSLConf, free)(TCN_STDARGS, jlong cctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(cctx);
}

TCN_IMPLEMENT_CALL(jint, SSLConf, check)(TCN_STDARGS, jlong cctx,
                                         jstring cmd, jstring value)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(cctx);
    UNREFERENCED(cmd);
    UNREFERENCED(value);
    return APR_ENOTIMPL;
}

TCN_IMPLEMENT_CALL(void, SSLConf, assign)(TCN_STDARGS, jlong cctx,
                                          jlong ctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(cctx);
    UNREFERENCED(ctx);
}

TCN_IMPLEMENT_CALL(jint, SSLConf, apply)(TCN_STDARGS, jlong cctx,
                                         jstring cmd, jstring value)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(cctx);
    UNREFERENCED(cmd);
    UNREFERENCED(value);
    return APR_ENOTIMPL;
}

TCN_IMPLEMENT_CALL(jint, SSLConf, finish)(TCN_STDARGS, jlong cctx)
{
    UNREFERENCED_STDARGS;
    UNREFERENCED(cctx);
    return APR_ENOTIMPL;
}


#endif /* HAVE_SSL_CONF_CMD */
