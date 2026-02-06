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

/** SSL Utilities
 */

#include "tcn.h"

#include "apr_poll.h"
#include "ssl_private.h"

#ifdef WIN32
extern int WIN32_SSL_password_prompt(tcn_pass_cb_t *data);
#endif

#ifdef HAVE_OCSP
#include <openssl/bio.h>
#include <openssl/ocsp.h>
/* defines with the values as seen by the asn1parse -dump openssl command */
#define ASN1_SEQUENCE 0x30
#define ASN1_OID      0x06
#define ASN1_STRING   0x86
static int ssl_verify_OCSP(X509_STORE_CTX *ctx, int timeout, int verifyFlags);
static int ssl_ocsp_request(X509 *cert, X509 *issuer, X509_STORE_CTX *ctx, int timeout, int verifyFlags);
#endif

/*  _________________________________________________________________
**
**  Additional High-Level Functions for OpenSSL
**  _________________________________________________________________
*/

/* we initialize this index at startup time
 * and never write to it at request time,
 * so this static is thread safe.
 * also note that OpenSSL increments at static variable when
 * SSL_get_ex_new_index() is called, so we _must_ do this at startup.
 */
static int SSL_app_data2_idx = -1;
static int SSL_app_data3_idx = -1;
static int SSL_app_data4_idx = -1;

void SSL_init_app_data_idx(void)
{
    int i;

    if (SSL_app_data2_idx > -1) {
        return;
    }

    /* we _do_ need to call this twice */
    for (i = 0; i <= 1; i++) {
        SSL_app_data2_idx =
            SSL_get_ex_new_index(0,
                                 "Second Application Data for SSL",
                                 NULL, NULL, NULL);
    }

    if (SSL_app_data3_idx > -1) {
        return;
    }

    SSL_app_data3_idx =
            SSL_get_ex_new_index(0,
                                 "Third Application Data for SSL",
                                  NULL, NULL, NULL);

    if (SSL_app_data4_idx > -1) {
        return;
    }

    SSL_app_data4_idx =
            SSL_get_ex_new_index(0,
                                 "Fourth Application Data for SSL",
                                  NULL, NULL, NULL);

}

void *SSL_get_app_data2(SSL *ssl)
{
    return (void *)SSL_get_ex_data(ssl, SSL_app_data2_idx);
}

void SSL_set_app_data2(SSL *ssl, void *arg)
{
    SSL_set_ex_data(ssl, SSL_app_data2_idx, (char *)arg);
    return;
}


void *SSL_get_app_data3(const SSL *ssl)
{
    return SSL_get_ex_data(ssl, SSL_app_data3_idx);
}

void SSL_set_app_data3(SSL *ssl, void *arg)
{
    SSL_set_ex_data(ssl, SSL_app_data3_idx, arg);
}

void *SSL_get_app_data4(const SSL *ssl)
{
    return SSL_get_ex_data(ssl, SSL_app_data4_idx);
}

void SSL_set_app_data4(SSL *ssl, void *arg)
{
    SSL_set_ex_data(ssl, SSL_app_data4_idx, arg);
}

/* Simple echo password prompting */
int SSL_password_prompt(tcn_pass_cb_t *data)
{
    int rv = 0;
    data->password[0] = '\0';
#ifdef WIN32
    rv = WIN32_SSL_password_prompt(data);
#else
    EVP_read_pw_string(data->password, SSL_MAX_PASSWORD_LEN,
                       data->prompt, 0);
#endif
    rv = (int)strlen(data->password);
    if (rv > 0) {
        /* Remove LF char if present */
        char *r = strchr(data->password, '\n');
        if (r) {
            *r = '\0';
            rv--;
        }
#ifdef WIN32
        if ((r = strchr(data->password, '\r'))) {
            *r = '\0';
            rv--;
        }
#endif
    }
    return rv;
}

int SSL_password_callback(char *buf, int bufsiz, int verify,
                          void *cb)
{
    tcn_pass_cb_t *cb_data = (tcn_pass_cb_t *)cb;

    if (buf == NULL)
        return 0;
    *buf = '\0';
    if (cb_data == NULL)
        cb_data = &tcn_password_callback;
    if (!cb_data->prompt)
        cb_data->prompt = SSL_DEFAULT_PASS_PROMPT;
    if (cb_data->password[0]) {
        /* Return already obtained password */
        strncpy(buf, cb_data->password, bufsiz);
        buf[bufsiz - 1] = '\0';
        return (int)strlen(buf);
    }
    else {
        if (SSL_password_prompt(cb_data) > 0)
            strncpy(buf, cb_data->password, bufsiz);
    }
    buf[bufsiz - 1] = '\0';
    return (int)strlen(buf);
}

/*  _________________________________________________________________
**
**  Custom (EC)DH parameter support
**  _________________________________________________________________
*/
EVP_PKEY *SSL_dh_GetParamFromFile(const char *file)
{
    EVP_PKEY *evp = NULL;
    BIO *bio;

    if ((bio = BIO_new_file(file, "r")) == NULL)
        return NULL;
    evp = PEM_read_bio_Parameters_ex(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (evp && !EVP_PKEY_is_a(evp, "DH")) {
        EVP_PKEY_free(evp);
        return NULL;
    }
    return evp;
}

#ifdef HAVE_ECC
int SSL_ec_GetParamFromFile(const char *file)
{
    EVP_PKEY *evp = NULL;
    BIO *bio;
    char curve_name[80];

    if ((bio = BIO_new_file(file, "r")) == NULL)
        return NID_undef;
    evp = PEM_read_bio_Parameters_ex(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (evp && !EVP_PKEY_is_a(evp, "EC")) {
        EVP_PKEY_free(evp);
        return NID_undef;
    }

    OSSL_PARAM param[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, curve_name, sizeof(curve_name)),
        OSSL_PARAM_construct_end()
    };

    /* Query the curve name from the EVP_PKEY params object */
    if (EVP_PKEY_get_params(evp, param) <= 0) {
        EVP_PKEY_free(evp);
        return NID_undef; /* Failed to retrieve the curve name */
    }

    /* Convert the curve name to the NID */
    int nid = OBJ_sn2nid(curve_name);
    if (nid == NID_undef) {
        /* If the short name didn't resolve, try the long name */
        nid = OBJ_ln2nid(curve_name);
    }

    EVP_PKEY_free(evp);
    return nid; /* Returns the curve's NID, or NID_undef on failure */
}
#endif

/*
 * Read a file that optionally contains the server certificate in PEM
 * format, possibly followed by a sequence of CA certificates that
 * should be sent to the peer in the SSL Certificate message.
 */
int SSL_CTX_use_certificate_chain(SSL_CTX *ctx, const char *file,
                                  int skipfirst)
{
    BIO *bio;
    X509 *x509;
    unsigned long err;
    int n;

    if ((bio = BIO_new(BIO_s_file())) == NULL)
        return -1;
    if (BIO_read_filename(bio, file) <= 0) {
        BIO_free(bio);
        return -1;
    }
    /* optionally skip a leading server certificate */
    if (skipfirst) {
        if ((x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL)) == NULL) {
            BIO_free(bio);
            return -1;
        }
        X509_free(x509);
    }

    /* free a perhaps already configured extra chain */
    SSL_CTX_clear_extra_chain_certs(ctx);

    /* create new extra chain by loading the certs */
    n = 0;
    while ((x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL)) != NULL) {
        if (!SSL_CTX_add_extra_chain_cert(ctx, x509)) {
            X509_free(x509);
            BIO_free(bio);
            return -1;
        }
        n++;
    }
    /* Make sure that only the error is just an EOF */
    if ((err = ERR_peek_error()) > 0) {
        if (!(   ERR_GET_LIB(err) == ERR_LIB_PEM
              && ERR_GET_REASON(err) == PEM_R_NO_START_LINE)) {
            BIO_free(bio);
            return -1;
        }
        while (SSL_ERR_get() > 0) ;
    }
    BIO_free(bio);
    return n;
}

/*
 * This OpenSSL callback function is called when OpenSSL
 * does client authentication and verifies the certificate chain.
 */

int SSL_callback_SSL_verify(int ok, X509_STORE_CTX *ctx)
{
   /* Get Apache context back through OpenSSL context */
    SSL *ssl = X509_STORE_CTX_get_ex_data(ctx,
                                          SSL_get_ex_data_X509_STORE_CTX_idx());
    tcn_ssl_conn_t *con = (tcn_ssl_conn_t *)SSL_get_app_data(ssl);
    /* Get verify ingredients */
    int errnum            = X509_STORE_CTX_get_error(ctx);
    int errdepth          = X509_STORE_CTX_get_error_depth(ctx);
    int verify            = con->ctx->verify_mode;
    int depth             = con->ctx->verify_depth;
    int ocsp_check_type   = con->ctx->no_ocsp_check;
    int ocsp_soft_fail    = con->ctx->ocsp_soft_fail;
    int ocsp_timeout      = con->ctx->ocsp_timeout;
    int ocsp_verify_flags = con->ctx->ocsp_verify_flags;

#if defined(SSL_OP_NO_TLSv1_3)
    con->pha_state = PHA_COMPLETE;
#endif

    if (verify == SSL_CVERIFY_UNSET || verify == SSL_CVERIFY_NONE) {
        return 1;
    }

    if ((SSL_VERIFY_ERROR_IS_OPTIONAL(errnum) || errnum == X509_V_OK) && (verify == SSL_CVERIFY_OPTIONAL_NO_CA)) {
        SSL_set_verify_result(ssl, X509_V_OK);
        // Skip OCSP checks since the CA is optional
        return 1;
    }

    /*
     * Expired certificates vs. "expired" CRLs: by default, OpenSSL
     * turns X509_V_ERR_CRL_HAS_EXPIRED into a "certificate_expired(45)"
     * SSL alert, but that's not really the message we should convey to the
     * peer (at the very least, it's confusing, and in many cases, it's also
     * inaccurate, as the certificate itself may very well not have expired
     * yet). We set the X509_STORE_CTX error to something which OpenSSL's
     * s3_both.c:ssl_verify_alarm_type() maps to SSL_AD_CERTIFICATE_UNKNOWN,
     * i.e. the peer will receive a "certificate_unknown(46)" alert.
     * We do not touch errnum, though, so that later on we will still log
     * the "real" error, as returned by OpenSSL.
     */
    if (!ok && errnum == X509_V_ERR_CRL_HAS_EXPIRED) {
        X509_STORE_CTX_set_error(ctx, -1);
    }

#ifdef HAVE_OCSP
    /* First perform OCSP validation if possible */
    if (ocsp_check_type == 0) {
       if (ok) {
            /* If there was an optional verification error, it's not
             * possible to perform OCSP validation since the issuer may be
             * missing/untrusted.  Fail in that case.
             */
            if (SSL_VERIFY_ERROR_IS_OPTIONAL(errnum)) {
                X509_STORE_CTX_set_error(ctx, X509_V_ERR_APPLICATION_VERIFICATION);
                errnum = X509_V_ERR_APPLICATION_VERIFICATION;
                ok = 0;
            }
            else {
                int ocsp_response = ssl_verify_OCSP(ctx, ocsp_timeout, ocsp_verify_flags);
                if (ocsp_response == OCSP_STATUS_REVOKED) {
                    ok = 0 ;
                    errnum = X509_STORE_CTX_get_error(ctx);
                }
                else if (ocsp_response == OCSP_STATUS_UNKNOWN) {
                    errnum = X509_STORE_CTX_get_error(ctx);
                    if (errnum != 0 && !(ocsp_soft_fail && errnum == X509_V_ERR_UNABLE_TO_GET_CRL))
                        ok = 0;
                }
            }
        }
    }
#endif
    /*
     * If we already know it's not ok, log the real reason
     */
    if (!ok) {
        /* TODO: Some logging
         * Certificate Verification: Error
         */
        if (con->peer) {
            X509_free(con->peer);
            con->peer = NULL;
        }
    }
    if (errdepth > depth) {
        /* TODO: Some logging
         * Certificate Verification: Certificate Chain too long
         */
        ok = 0;
    }
    return ok;
}

/*
 * This callback function is executed while OpenSSL processes the SSL
 * handshake and does SSL record layer stuff.  It's used to trap
 * client-initiated renegotiations, and for dumping everything to the
 * log.
 */
void SSL_callback_handshake(const SSL *ssl, int where, int rc)
{
    tcn_ssl_conn_t *con = (tcn_ssl_conn_t *)SSL_get_app_data(ssl);
#ifdef HAVE_TLSV1_3
    const SSL_SESSION *session = SSL_get_session(ssl);
#endif

    /* Retrieve the conn_rec and the associated SSLConnRec. */
    if (con == NULL) {
        return;
    }

#ifdef HAVE_TLSV1_3
    /* TLS 1.3 does not use renegotiation so do not update the renegotiation
     * state once we know we are using TLS 1.3. */
    if (session != NULL) {
        if (SSL_SESSION_get_protocol_version(session) == TLS1_3_VERSION) {
            return;
        }
    }
#endif

    /* If the reneg state is to reject renegotiations, check the SSL
     * state machine and move to ABORT if a Client Hello is being
     * read. */
    if ((where & SSL_CB_HANDSHAKE_START) &&
         con->reneg_state == RENEG_REJECT) {
        con->reneg_state = RENEG_ABORT;
    }
    /* If the first handshake is complete, change state to reject any
     * subsequent client-initated renegotiation. */
    else if ((where & SSL_CB_HANDSHAKE_DONE) && con->reneg_state == RENEG_INIT) {
        con->reneg_state = RENEG_REJECT;
    }
}

/* The code here is inspired by nghttp2
 *
 * See https://github.com/tatsuhiro-t/nghttp2/blob/ae0100a9abfcf3149b8d9e62aae216e946b517fb/src/shrpx_ssl.cc#L244 */
int select_next_proto(SSL *ssl, const unsigned char **out, unsigned char *outlen,
        const unsigned char *in, unsigned int inlen, unsigned char *supported_protos,
        unsigned int supported_protos_len, int failure_behavior) {

    unsigned int i = 0;
    unsigned char target_proto_len;
    const unsigned char *p;
    const unsigned char *end;
    const unsigned char *proto;
    unsigned char proto_len = '\0';

    while (i < supported_protos_len) {
        target_proto_len = *supported_protos;
        ++supported_protos;

        p = in;
        end = in + inlen;

        while (p < end) {
            proto_len = *p;
            proto = ++p;

            if (proto + proto_len <= end && target_proto_len == proto_len &&
                    memcmp(supported_protos, proto, proto_len) == 0) {

                // We found a match, so set the output and return with OK!
                *out = proto;
                *outlen = proto_len;

                return SSL_TLSEXT_ERR_OK;
            }
            // Move on to the next protocol.
            p += proto_len;
        }

        // increment len and pointers.
        i += target_proto_len;
        supported_protos += target_proto_len;
    }

    if (supported_protos_len > 0 && inlen > 0 && failure_behavior == SSL_SELECTOR_FAILURE_CHOOSE_MY_LAST_PROTOCOL) {
         // There were no match but we just select our last protocol and hope the other peer support it.
         //
         // decrement the pointer again so the pointer points to the start of the protocol.
         p -= proto_len;
         *out = p;
         *outlen = proto_len;
         return SSL_TLSEXT_ERR_OK;
    }
    // TODO: OpenSSL currently not support to fail with fatal error. Once this changes we can also support it here.
    //       Issue https://github.com/openssl/openssl/issues/188 has been created for this.
    // Nothing matched so not select anything and just accept.
    return SSL_TLSEXT_ERR_NOACK;
}

int SSL_callback_alpn_select_proto(SSL* ssl, const unsigned char **out, unsigned char *outlen,
        const unsigned char *in, unsigned int inlen, void *arg) {
    tcn_ssl_ctxt_t *ssl_ctxt = arg;
    return select_next_proto(ssl, out, outlen, in, inlen, ssl_ctxt->alpn_proto_data, ssl_ctxt->alpn_proto_len, ssl_ctxt->alpn_selector_failure_behavior);
}
#ifdef HAVE_OCSP

/* Function that is used to do the OCSP verification */
static int ssl_verify_OCSP(X509_STORE_CTX *ctx, int timeout, int verifyFlags)
{
    X509 *cert, *issuer;
    int r = OCSP_STATUS_UNKNOWN;

    cert = X509_STORE_CTX_get_current_cert(ctx);

    if (!cert) {
        /* starting with OpenSSL 1.0, X509_STORE_CTX_get_current_cert()
         * may yield NULL. Return early, but leave the ctx error as is. */
        return OCSP_STATUS_UNKNOWN;
    }
    /* No need to check cert->valid, because ssl_verify_OCSP() only
     * is called if OpenSSL already successfully verified the certificate
     * (parameter "ok" in SSL_callback_SSL_verify() must be true).
     */
    else if (X509_check_issued(cert,cert) == X509_V_OK) {
        /* don't do OCSP checking for valid self-issued certs */
        X509_STORE_CTX_set_error(ctx, X509_V_OK);
        return OCSP_STATUS_UNKNOWN;
    }

    /* if we can't get the issuer, we cannot perform OCSP verification */
    issuer = X509_STORE_CTX_get0_current_issuer(ctx);
    if (issuer != NULL) {
        r = ssl_ocsp_request(cert, issuer, ctx, timeout, verifyFlags);
        switch (r) {
        case OCSP_STATUS_OK:
            X509_STORE_CTX_set_error(ctx, X509_V_OK);
            break;
        case OCSP_STATUS_REVOKED:
            /* we set the error if we know that it is revoked */
            X509_STORE_CTX_set_error(ctx, X509_V_ERR_CERT_REVOKED);
            break;
        case OCSP_STATUS_UNKNOWN:
            /* ssl_ocsp_request() sets the error correctly already. */
            break;
        }
    }
    return r;
}


/* Helps with error handling or realloc */
static void *apr_xrealloc(void *buf, size_t oldlen, size_t len, apr_pool_t *p)
{
    void *newp = apr_palloc(p, len);

    if(newp)
        memcpy(newp, buf, oldlen);
    return newp;
}

/* Parses an ASN.1 length.
 * On entry, asn1 points to the current tag.
 * Updates the pointer to the ASN.1 structure to point to the start of the data.
 * Returns 0 on success, 1 on failure.
 */
static int parse_asn1_length(unsigned char **asn1, int *len) {

    /* Length immediately follows tag so increment before reading first (and
     * possibly only) length byte.
     */
    (*asn1)++;

    if (**asn1 & 0x80) {
        // MSB set. Remaining bits are number of bytes used to store the length.
        int i, l;

        // How many bytes for this length?
        i = **asn1 & 0x7F;

        if (i == 0) {
            /* This is the indefinite form of length. Since certificates use DER
             * this should never happen and is therefore an error.
             */
            return 1;
        }
        if (i > 3) {
            /* Three bytes for length gives a maximum of 16MB which should be
             * far more than is required. (2 bytes is 64K which is probably more
             * than enough but play safe.)
             */
            return 1;
        }

        // Most significant byte is first
        l = 0;
        while (i > 0) {
            l <<= 8;
            (*asn1)++;
            l += **asn1;
            i--;
        }
        *len = l;
    } else {
        // Single byte length
        *len = **asn1;
    }

    (*asn1)++;

    return 0;
}

/* parses the ocsp url and updates the ocsp_urls and nocsp_urls variables
   returns 0 on success, 1 on failure */
static int parse_ocsp_url(unsigned char *asn1, char ***ocsp_urls,
                          int *nocsp_urls, apr_pool_t *p)
{
    char **new_ocsp_urls, *ocsp_url;
    int len, err = 0, new_nocsp_urls;

    if (*asn1 == ASN1_STRING) {
        err = parse_asn1_length(&asn1, &len);

        if (!err) {
            new_nocsp_urls = *nocsp_urls+1;
            if ((new_ocsp_urls = apr_xrealloc(*ocsp_urls,*nocsp_urls, new_nocsp_urls, p)) == NULL)
                err = 1;
        }
        if (!err) {
            *ocsp_urls  = new_ocsp_urls;
            *nocsp_urls = new_nocsp_urls;
            *(*ocsp_urls + *nocsp_urls) = NULL;
            if ((ocsp_url = apr_palloc(p, len + 1)) == NULL) {
                err = 1;
            }
            else {
                memcpy(ocsp_url, asn1, len);
                ocsp_url[len] = '\0';
                *(*ocsp_urls + *nocsp_urls - 1) = ocsp_url;
            }
        }
    }
    return err;

}

/* parses the ANS1 OID and if it is an OCSP OID then calls the parse_ocsp_url function */
static int parse_ASN1_OID(unsigned char *asn1, char ***ocsp_urls, int *nocsp_urls, apr_pool_t *p)
{
    int len, err = 0 ;
    const unsigned char OCSP_OID[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01};

    err = parse_asn1_length(&asn1, &len);

    if (!err && len == 8 && memcmp(asn1, OCSP_OID, 8) == 0) {
        asn1+=len;
        err = parse_ocsp_url(asn1, ocsp_urls, nocsp_urls, p);
    }
    return err;
}


/* Parses an ASN1 Sequence. It is a recursive function, since if it finds a  sequence
   within the sequence it calls recursively itself. This function stops when it finds
   the end of the ASN1 sequence (marked by '\0'), so if there are other sequences within
   the same sequence the while loop parses the sequences */

/* This algo was developed with AIA in mind so it was tested only with this extension */
static int parse_ASN1_Sequence(unsigned char *asn1, char ***ocsp_urls,
                               int *nocsp_urls, apr_pool_t *p)
{
    int len = 0 , err = 0;

    while (!err && *asn1 != '\0') {
        switch(*asn1) {
            case ASN1_SEQUENCE:
                err = parse_asn1_length(&asn1, &len);
                if (!err) {
                    err = parse_ASN1_Sequence(asn1, ocsp_urls, nocsp_urls, p);
                }
            break;
            case ASN1_OID:
                err = parse_ASN1_OID(asn1,ocsp_urls,nocsp_urls, p);
                return err;
            break;
            default:
                err = 1; /* we shouldn't have any errors */
            break;
        }
        asn1+=len;
    }
    return err;
}

/* the main function that gets the ASN1 encoding string and returns
   a pointer to a NULL terminated "array" of char *, that contains
   the ocsp_urls */
static char **decode_OCSP_url(ASN1_OCTET_STRING *os, apr_pool_t *p)
{
    char **response = NULL;
    unsigned char *ocsp_urls;
    int len, numofresponses = 0 ;

    len = ASN1_STRING_length(os);

    ocsp_urls = apr_palloc(p,  len + 1);
    memcpy(ocsp_urls,os->data, len);
    ocsp_urls[len] = '\0';

    if ((response = apr_pcalloc(p, sizeof(char *))) == NULL) {
        return NULL;
    }
    if (parse_ASN1_Sequence(ocsp_urls, &response, &numofresponses, p) ||
            numofresponses ==0) {
        response = NULL;
    }
    return response;
}


/* stolen from openssl ocsp command */
static int add_ocsp_cert(OCSP_REQUEST *req, X509 *cert, X509 *issuer)
{
    OCSP_CERTID *id;

    if (!issuer)
        return 0;
    id = OCSP_cert_to_id(NULL, cert, issuer);
    if (!id)
        return 0;
    if (!OCSP_request_add0_id(req, id)) {
        OCSP_CERTID_free(id);
        return 0;
    } else {
        /* id will be freed by OCSP_REQUEST_free() */
        return 1;
    }
}


/* Creates the APR socket and connect to the hostname. Returns the
   socket or NULL if there is an error.
*/
static apr_socket_t *make_socket(char *hostname, int port, apr_pool_t *mp)
{
    apr_sockaddr_t *sa_in;
    apr_status_t status;
    apr_socket_t *sock = NULL;


    status = apr_sockaddr_info_get(&sa_in, hostname, APR_INET, port, 0, mp);

    if (status == APR_SUCCESS)
        status = apr_socket_create(&sock, sa_in->family, SOCK_STREAM, APR_PROTO_TCP, mp);
    if (status == APR_SUCCESS)
        status = apr_socket_connect(sock, sa_in);

    if (status == APR_SUCCESS)
        return sock;
    return NULL;
}


/* Creates the request in a memory BIO in order to send it to the OCSP server.
   Most parts of this function are taken from mod_ssl support for OCSP (with some
   minor modifications
*/
static BIO *serialize_request(OCSP_REQUEST *req, char *host, int port, char *path)
{
    BIO *bio;
    int len;

    len = i2d_OCSP_REQUEST(req, NULL);

    bio = BIO_new(BIO_s_mem());

    BIO_printf(bio, "POST %s HTTP/1.0\r\n"
      "Host: %s:%d\r\n"
      "Content-Type: application/ocsp-request\r\n"
      "Content-Length: %d\r\n"
      "\r\n",
      path, host, port, len);

    if (i2d_OCSP_REQUEST_bio(bio, req) != 1) {
        BIO_free(bio);
        return NULL;
    }

    return bio;
}


/* Send the OCSP request to the OCSP server. Taken from mod_ssl OCSP support */
static int ocsp_send_req(apr_socket_t *sock, BIO *req)
{
    int len;
    char buf[TCN_BUFFER_SZ];
    apr_status_t rv;

    while ((len = BIO_read(req, buf, sizeof buf)) > 0) {
        char *wbuf = buf;
        apr_size_t remain = len;

        do {
            apr_size_t wlen = remain;
            rv = apr_socket_send(sock, wbuf, &wlen);
            wbuf += remain;
            remain -= wlen;
        } while (rv == APR_SUCCESS && remain > 0);

        if (rv != APR_SUCCESS) {
            return 0;
        }
    }

    return 1;
}



/* Parses the buffer from the response and extracts the OCSP response.
   Taken from openssl library */
static OCSP_RESPONSE *parse_ocsp_resp(char *buf, int len)
{
    BIO *mem = NULL;
    char tmpbuf[1024];
    OCSP_RESPONSE *resp = NULL;
    char *p, *q, *r;
    int retcode;

    mem = BIO_new(BIO_s_mem());
    if(mem == NULL)
        return NULL;

    BIO_write(mem, buf, len);  /* write the buffer to the bio */
    if (BIO_gets(mem, tmpbuf, 512) <= 0) {
        goto err;
    }
    /* Parse the HTTP response. This will look like this:
     * "HTTP/1.0 200 OK". We need to obtain the numeric code and
     * (optional) informational message.
     */

    /* Skip to first white space (passed protocol info) */
    for (p = tmpbuf; *p && !apr_isspace(*p); p++)
        continue;
    if (!*p) {
        goto err;
    }
    /* Skip past white space to start of response code */
    while (apr_isspace(*p))
        p++;
    if (!*p) {
        goto err;
    }
    /* Find end of response code: first whitespace after start of code */
    for (q = p; *q && !apr_isspace(*q); q++)
        continue;
    if (!*q) {
        goto err;
    }
    /* Set end of response code and start of message */
    *q++ = 0;
    /* Attempt to parse numeric code */
    retcode = strtoul(p, &r, 10);
    if (*r)
        goto err;
    /* Skip over any leading white space in message */
    while (apr_isspace(*q))
        q++;
    if (*q) {
        /* Finally zap any trailing white space in message (include CRLF) */
        /* We know q has a non white space character so this is OK */
        for(r = q + strlen(q) - 1; apr_isspace(*r); r--) *r = 0;
    }
    if (retcode != 200) {
        goto err;
    }
    /* Find blank line marking beginning of content */
    while (BIO_gets(mem, tmpbuf, 512) > 0) {
        for (p = tmpbuf; apr_isspace(*p); p++)
            continue;
        if (!*p)
            break;
    }
    if (*p) {
        goto err;
    }
    if (!(resp = d2i_OCSP_RESPONSE_bio(mem, NULL))) {
        goto err;
    }
err:
    /* XXX No error logging? */
    BIO_free(mem);
    return resp;
}


/* Reads the response from the APR socket to a buffer, and parses the buffer to
   return the OCSP response  */
#define BUFFER_SIZE 512
#define OCSP_MAX_RESPONSE_SIZE 65536
static OCSP_RESPONSE *ocsp_get_resp(apr_pool_t *mp, apr_socket_t *sock)
{
    int buflen;
    apr_size_t totalread = 0;
    apr_size_t readlen;
    char *buf, tmpbuf[BUFFER_SIZE];
    apr_status_t rv = APR_SUCCESS;
    apr_pool_t *p;
    OCSP_RESPONSE *resp;

    apr_pool_create(&p, mp);
    buflen = BUFFER_SIZE;
    buf = apr_palloc(p, buflen);
    if (buf == NULL) {
        apr_pool_destroy(p);
        return NULL;
    }

    while (rv == APR_SUCCESS ) {
        readlen = sizeof(tmpbuf);
        rv = apr_socket_recv(sock, tmpbuf, &readlen);
        if (rv == APR_SUCCESS) { /* if we have read something .. we can put it in the buffer*/
            if ((totalread + readlen) > OCSP_MAX_RESPONSE_SIZE) {
                apr_pool_destroy(p);
                return NULL;
            } else if ((totalread + readlen) >= buflen) {
                buf = apr_xrealloc(buf, buflen, buflen * 2, p);
                if (buf == NULL) {
                    apr_pool_destroy(p);
                    return NULL;
                }
                buflen *= 2; /* if needed we enlarge the buffer */
            }
            memcpy(buf + totalread, tmpbuf, readlen); /* the copy to the buffer */
            totalread += readlen; /* update the total bytes read */
        }
        else {
            if (rv == APR_EOF && readlen == 0)
                ; /* EOF, normal situation */
            else if (readlen == 0) {
                /* Not success, and readlen == 0 .. some error */
                apr_pool_destroy(p);
                return NULL;
            }
        }
    }

    resp = parse_ocsp_resp(buf, buflen);
    apr_pool_destroy(p);
    return resp;
}

/* Creates an OCSP request */
static OCSP_REQUEST *get_ocsp_request(X509 *cert, X509 *issuer)
{
    OCSP_REQUEST *ocsp_req = NULL;

    ocsp_req = OCSP_REQUEST_new();
    if (ocsp_req == NULL)
        return NULL;

    // Populate the request
    if (add_ocsp_cert(ocsp_req, cert, issuer) == 0) {
        OCSP_REQUEST_free(ocsp_req);
        return NULL;
    }

    // Add a nonce to protect against replay attacks
    OCSP_request_add1_nonce(ocsp_req, NULL, -1);

    return ocsp_req;
}

/* Submits an OCSP request and returns the OCSP_RESPONSE */
static OCSP_RESPONSE *get_ocsp_response(apr_pool_t *p, char *url, OCSP_REQUEST *ocsp_req, int timeout)
{
    OCSP_RESPONSE *ocsp_resp = NULL;
    BIO *bio_req;
    char *hostname, *path, *c_port;
    int port, use_ssl;
    int ok = 0;
    apr_socket_t *apr_sock = NULL;
    apr_pool_t *mp;
#ifdef LIBRESSL_VERSION_NUMBER
    if (OCSP_parse_url(url, &hostname, &c_port, &path, &use_ssl) == 0)
#else
    if (OSSL_HTTP_parse_url(url, &use_ssl, NULL, &hostname, &c_port, NULL, &path, NULL, NULL) == 0)
#endif
        goto end;

    if (sscanf(c_port, "%d", &port) != 1)
        goto end;

    /* create the BIO with the request to send */
    bio_req = serialize_request(ocsp_req, hostname, port, path);
    if (bio_req == NULL) {
        goto end;
    }

    apr_pool_create(&mp, p);
    apr_sock = make_socket(hostname, port, mp);
    if (apr_sock == NULL) {
        goto free_bio;
    }

    apr_socket_timeout_set(apr_sock, timeout);

    ok = ocsp_send_req(apr_sock, bio_req);
    if (ok) {
        ocsp_resp = ocsp_get_resp(mp, apr_sock);
    }
    apr_socket_close(apr_sock);

free_bio:
    BIO_free(bio_req);
    apr_pool_destroy(mp);

end:
    OPENSSL_free(hostname);
    OPENSSL_free(c_port);
    OPENSSL_free(path);

    return ocsp_resp;
}

/* Process the OCSP_RESPONSE and returns the corresponding
   answer according to the status.
*/
static int process_ocsp_response(OCSP_REQUEST *ocsp_req, OCSP_RESPONSE *ocsp_resp, X509 *cert, X509 *issuer,
        X509_STORE_CTX *ctx, int verifyFlags)
{
    int r, o = V_OCSP_CERTSTATUS_UNKNOWN, i;
    OCSP_BASICRESP *bs;
    OCSP_SINGLERESP *ss;
    OCSP_CERTID *certid;
    ASN1_GENERALIZEDTIME *thisupd;
    ASN1_GENERALIZEDTIME *nextupd;
    const STACK_OF(X509) *certStack;

    r = OCSP_response_status(ocsp_resp);

    if (r != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
        return OCSP_STATUS_UNKNOWN;
    }

    bs = OCSP_response_get1_basic(ocsp_resp);
    if (OCSP_check_nonce(ocsp_req, bs) == 0) {
        X509_STORE_CTX_set_error(ctx, X509_V_ERR_OCSP_RESP_INVALID);
        o = OCSP_STATUS_UNKNOWN;
        goto clean_bs;
    }

    certStack = OCSP_resp_get0_certs(bs);
    // Cast to non-const pointer is OK here since OCSP_basic_verify does not modify the provided certs
    if (OCSP_basic_verify(bs, (STACK_OF(X509) *)certStack, X509_STORE_CTX_get0_store(ctx), verifyFlags) <= 0) {
        X509_STORE_CTX_set_error(ctx, X509_V_ERR_OCSP_SIGNATURE_FAILURE);
        o = OCSP_STATUS_UNKNOWN;
        goto clean_bs;
    }

    certid = OCSP_cert_to_id(NULL, cert, issuer);
    if (certid == NULL) {
        X509_STORE_CTX_set_error(ctx, X509_V_ERR_OCSP_RESP_INVALID);
        o = OCSP_STATUS_UNKNOWN;
        goto clean_bs;
    }

    ss = OCSP_resp_get0(bs, OCSP_resp_find(bs, certid, -1)); /* find by serial number and get the matching response */
    i = OCSP_single_get0_status(ss, NULL, NULL, &thisupd, &nextupd);
    if (OCSP_check_validity(thisupd, nextupd, OCSP_MAX_SKEW, -1) <= 0) {
        X509_STORE_CTX_set_error(ctx, X509_V_ERR_OCSP_NOT_YET_VALID);
        o = OCSP_STATUS_UNKNOWN;
        goto clean_certid;
    }
    if (OCSP_check_validity(thisupd, nextupd, OCSP_MAX_SKEW, OCSP_MAX_SKEW) <= 0) {
        X509_STORE_CTX_set_error(ctx, X509_V_ERR_OCSP_HAS_EXPIRED);
        o = OCSP_STATUS_UNKNOWN;
        goto clean_certid;
    }

    if (i == V_OCSP_CERTSTATUS_GOOD)
        o =  OCSP_STATUS_OK;
    else if (i == V_OCSP_CERTSTATUS_REVOKED)
        o = OCSP_STATUS_REVOKED;
    else if (i == V_OCSP_CERTSTATUS_UNKNOWN)
        o = OCSP_STATUS_UNKNOWN;

clean_certid:
    OCSP_CERTID_free(certid);
clean_bs:
    OCSP_BASICRESP_free(bs);
    return o;
}

static int ssl_ocsp_request(X509 *cert, X509 *issuer, X509_STORE_CTX *ctx, int timeout, int verifyFlags)
{
    char **ocsp_urls = NULL;
    int nid;
    int rv = OCSP_STATUS_UNKNOWN;
    X509_EXTENSION *ext;
    ASN1_OCTET_STRING *os;
    apr_pool_t *p;

    apr_pool_create(&p, NULL);

    /* Get the proper extension */
    nid = X509_get_ext_by_NID(cert,NID_info_access,-1);
    if (nid >= 0 ) {
        ext = X509_get_ext(cert,nid);
        os = X509_EXTENSION_get_data(ext);

        ocsp_urls = decode_OCSP_url(os, p);
    }

    /* if we find the extensions and we can parse it check
       the ocsp status. Otherwise, return OCSP_STATUS_UNKNOWN */
    if (ocsp_urls != NULL) {
        OCSP_REQUEST *req;
        OCSP_RESPONSE *resp = NULL;
        /* for the time being just check for the fist response .. a better
           approach is to iterate for all the possible ocsp urls */
        req = get_ocsp_request(cert, issuer);
        if (req != NULL) {
            resp = get_ocsp_response(p, ocsp_urls[0], req, timeout);
            if (resp != NULL) {
                rv = process_ocsp_response(req, resp, cert, issuer, ctx, verifyFlags);
            } else {
                /* Unable to send request / receive response. */
                X509_STORE_CTX_set_error(ctx, X509_V_ERR_UNABLE_TO_GET_CRL);
            }
        } else {
            /* correct error code for application errors? */
            X509_STORE_CTX_set_error(ctx, X509_V_ERR_APPLICATION_VERIFICATION);
        }

        if (req != NULL) {
            OCSP_REQUEST_free(req);
        }

        if (resp != NULL) {
            OCSP_RESPONSE_free(resp);
        }
    }
    apr_pool_destroy(p);
    return rv;
}

#endif /* HAVE_OCSP */
