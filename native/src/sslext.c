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

/** SSL extensions - used for specialized taks - distributed session reuse, 
 *  NPN, etc. TODO: also add SNI.
 *
 * @author Costin Manolache
 */

#include "tcn.h"

#include "apr_file_io.h"
#include "apr_thread_mutex.h"
#include "apr_poll.h"

#ifdef HAVE_OPENSSL
#include "ssl_private.h"

TCN_IMPLEMENT_CALL(jint, SSLExt, setSessionData)(TCN_STDARGS, jlong tcsock, jbyteArray buf, jint len)
{
	tcn_socket_t *s = J2P(tcsock, tcn_socket_t *);
	tcn_ssl_conn_t *tcssl = (tcn_ssl_conn_t *)s->opaque;
	jbyte bytes[TCN_BUFFER_SZ];
	const jbyte *bytesp = &bytes[0];

	if (len > TCN_BUFFER_SZ) {
		return -1;
	}
	(*e)->GetByteArrayRegion(e, buf, 0, len, bytes);
	SSL_SESSION* ssl_session = d2i_SSL_SESSION(NULL, (const unsigned char **)&bytesp, len);

	SSL_set_session(tcssl->ssl, ssl_session);
	return 0;
}

TCN_IMPLEMENT_CALL(jbyteArray, SSLExt, getSessionData)(TCN_STDARGS, jlong tcsock)
{
	tcn_socket_t *s = J2P(tcsock, tcn_socket_t *);
	tcn_ssl_conn_t *tcssl = (tcn_ssl_conn_t *)s->opaque;
	SSL_SESSION *sess = SSL_get_session(tcssl->ssl);

	int size = i2d_SSL_SESSION(sess, NULL);
	if (size == 0 || size > TCN_BUFFER_SZ) {
		return NULL;
	}

	jbyteArray javaBytes = (*e)->NewByteArray(e, size);
	if (javaBytes != NULL) {
		jbyte bytes[TCN_BUFFER_SZ];
		unsigned char *bytesp = (unsigned char *)&bytes[0];

		i2d_SSL_SESSION(sess, &bytesp);
		(*e)->SetByteArrayRegion(e, javaBytes, 0, size, bytes);
	}

	return javaBytes;
}

TCN_IMPLEMENT_CALL(jint, SSLExt, getTicket)(TCN_STDARGS, jlong tcsock, jbyteArray buf)
{
	tcn_socket_t *s = J2P(tcsock, tcn_socket_t *);
	tcn_ssl_conn_t *tcssl = (tcn_ssl_conn_t *)s->opaque;
	int bufLen = (*e)->GetArrayLength(e, buf);

	SSL_SESSION *x = SSL_get_session(tcssl->ssl);

	if (!x->tlsext_tick || x->tlsext_ticklen > bufLen) {
		return 0;
	}
	(*e)->SetByteArrayRegion(e, buf, 0, x->tlsext_ticklen, (jbyte *) &x->tlsext_tick[0]);

	return x->tlsext_ticklen;
}

TCN_IMPLEMENT_CALL(jint, SSLExt, setTicket)(TCN_STDARGS, jlong tcsock, jbyteArray buf,
		jint len)
{
	tcn_socket_t *s = J2P(tcsock, tcn_socket_t *);
	tcn_ssl_conn_t *tcssl = (tcn_ssl_conn_t *)s->opaque;

	char * requestedTicket = apr_pcalloc(tcssl->pool, len);
	(*e)->GetByteArrayRegion(e, buf, 0, len, (jbyte *) requestedTicket);
	SSL_set_session_ticket_ext(tcssl->ssl, requestedTicket, len);
	return 0;
}

TCN_IMPLEMENT_CALL(jint, SSLExt, setTicketKeys)(TCN_STDARGS, jlong tc_ssl_ctx, jbyteArray buf, jint len)
{
	tcn_ssl_ctxt_t *sslctx = J2P(tc_ssl_ctx, tcn_ssl_ctxt_t *);
	unsigned char keys[48];

	(*e)->GetByteArrayRegion(e, buf, 0, 48, (jbyte *) keys);

	SSL_CTX_set_tlsext_ticket_keys(sslctx->ctx, keys, sizeof(keys));
	return 0;
}

// Debug code - copied from openssl app

long bio_dump_callback(BIO *bio, int cmd, const char *argp,
		int argi, long argl, long ret)
{
	BIO *out;

	out=(BIO *)BIO_get_callback_arg(bio);
	if (out == NULL) return(ret);

	if (cmd == (BIO_CB_READ|BIO_CB_RETURN))
	{
		BIO_printf(out,"SSL read from %p [%p] (%lu bytes => %ld (0x%lX))\n",
				(void *)bio,argp,(unsigned long)argi,ret,ret);
		BIO_dump(out,argp,(int)ret);
		return(ret);
	}
	else if (cmd == (BIO_CB_WRITE|BIO_CB_RETURN))
	{
		BIO_printf(out,"SSL write to %p [%p] (%lu bytes => %ld (0x%lX))\n",
				(void *)bio,argp,(unsigned long)argi,ret,ret);
		BIO_dump(out,argp,(int)ret);
	}
	return(ret);
}

void msg_cb(int write_p, int version, int content_type,
		const void *buf, size_t len, SSL *ssl, void *arg)
{
	BIO *bio = arg;
	const char *str_write_p, *str_version, *str_content_type = "", *str_details1 = "", *str_details2= "";

	str_write_p = write_p ? ">>>" : "<<<";

	switch (version)
	{
		case SSL2_VERSION:
		str_version = "SSL 2.0";
		break;
		case SSL3_VERSION:
		str_version = "SSL 3.0 ";
		break;
		case TLS1_VERSION:
		str_version = "TLS 1.0 ";
		break;
		case DTLS1_VERSION:
		str_version = "DTLS 1.0 ";
		break;
		case DTLS1_BAD_VER:
		str_version = "DTLS 1.0 (bad) ";
		break;
		default:
		str_version = "???";
	}

	if (version == SSL2_VERSION)
	{
		str_details1 = "???";

		if (len > 0)
		{
			switch (((const unsigned char*)buf)[0])
			{
				case 0:
				str_details1 = ", ERROR:";
				str_details2 = " ???";
				if (len >= 3)
				{
					unsigned err = (((const unsigned char*)buf)[1]<<8) + ((const unsigned char*)buf)[2];

					switch (err)
					{
						case 0x0001:
						str_details2 = " NO-CIPHER-ERROR";
						break;
						case 0x0002:
						str_details2 = " NO-CERTIFICATE-ERROR";
						break;
						case 0x0004:
						str_details2 = " BAD-CERTIFICATE-ERROR";
						break;
						case 0x0006:
						str_details2 = " UNSUPPORTED-CERTIFICATE-TYPE-ERROR";
						break;
					}
				}

				break;
				case 1:
				str_details1 = ", CLIENT-HELLO";
				break;
				case 2:
				str_details1 = ", CLIENT-MASTER-KEY";
				break;
				case 3:
				str_details1 = ", CLIENT-FINISHED";
				break;
				case 4:
				str_details1 = ", SERVER-HELLO";
				break;
				case 5:
				str_details1 = ", SERVER-VERIFY";
				break;
				case 6:
				str_details1 = ", SERVER-FINISHED";
				break;
				case 7:
				str_details1 = ", REQUEST-CERTIFICATE";
				break;
				case 8:
				str_details1 = ", CLIENT-CERTIFICATE";
				break;
			}
		}
	}

	if (version == SSL3_VERSION ||
			version == TLS1_VERSION ||
			version == DTLS1_VERSION ||
			version == DTLS1_BAD_VER)
	{
		switch (content_type)
		{
			case 20:
			str_content_type = "ChangeCipherSpec";
			break;
			case 21:
			str_content_type = "Alert";
			break;
			case 22:
			str_content_type = "Handshake";
			break;
		}

		if (content_type == 21) /* Alert */
		{
			str_details1 = ", ???";

			if (len == 2)
			{
				switch (((const unsigned char*)buf)[0])
				{
					case 1:
					str_details1 = ", warning";
					break;
					case 2:
					str_details1 = ", fatal";
					break;
				}

				str_details2 = " ???";
				switch (((const unsigned char*)buf)[1])
				{
					case 0:
					str_details2 = " close_notify";
					break;
					case 10:
					str_details2 = " unexpected_message";
					break;
					case 20:
					str_details2 = " bad_record_mac";
					break;
					case 21:
					str_details2 = " decryption_failed";
					break;
					case 22:
					str_details2 = " record_overflow";
					break;
					case 30:
					str_details2 = " decompression_failure";
					break;
					case 40:
					str_details2 = " handshake_failure";
					break;
					case 42:
					str_details2 = " bad_certificate";
					break;
					case 43:
					str_details2 = " unsupported_certificate";
					break;
					case 44:
					str_details2 = " certificate_revoked";
					break;
					case 45:
					str_details2 = " certificate_expired";
					break;
					case 46:
					str_details2 = " certificate_unknown";
					break;
					case 47:
					str_details2 = " illegal_parameter";
					break;
					case 48:
					str_details2 = " unknown_ca";
					break;
					case 49:
					str_details2 = " access_denied";
					break;
					case 50:
					str_details2 = " decode_error";
					break;
					case 51:
					str_details2 = " decrypt_error";
					break;
					case 60:
					str_details2 = " export_restriction";
					break;
					case 70:
					str_details2 = " protocol_version";
					break;
					case 71:
					str_details2 = " insufficient_security";
					break;
					case 80:
					str_details2 = " internal_error";
					break;
					case 90:
					str_details2 = " user_canceled";
					break;
					case 100:
					str_details2 = " no_renegotiation";
					break;
					case 110:
					str_details2 = " unsupported_extension";
					break;
					case 111:
					str_details2 = " certificate_unobtainable";
					break;
					case 112:
					str_details2 = " unrecognized_name";
					break;
					case 113:
					str_details2 = " bad_certificate_status_response";
					break;
					case 114:
					str_details2 = " bad_certificate_hash_value";
					break;
				}
			}
		}

		if (content_type == 22) /* Handshake */
		{
			str_details1 = "???";

			if (len > 0)
			{
				switch (((const unsigned char*)buf)[0])
				{
					case 0:
					str_details1 = ", HelloRequest";
					break;
					case 1:
					str_details1 = ", ClientHello";
					break;
					case 2:
					str_details1 = ", ServerHello";
					break;
					case 3:
					str_details1 = ", HelloVerifyRequest";
					break;
					case 11:
					str_details1 = ", Certificate";
					break;
					case 12:
					str_details1 = ", ServerKeyExchange";
					break;
					case 13:
					str_details1 = ", CertificateRequest";
					break;
					case 14:
					str_details1 = ", ServerHelloDone";
					break;
					case 15:
					str_details1 = ", CertificateVerify";
					break;
					case 16:
					str_details1 = ", ClientKeyExchange";
					break;
					case 20:
					str_details1 = ", Finished";
					break;
				}
			}
		}
	}

	BIO_printf(bio, "%s %s%s [length %04lx]%s%s\n", str_write_p, str_version, str_content_type, (unsigned long)len, str_details1, str_details2);

	if (len > 0)
	{
		size_t num, i;

		BIO_printf(bio, "   ");
		num = len;
#if 0
		if (num > 16)
		num = 16;
#endif
		for (i = 0; i < num; i++)
		{
			if (i % 16 == 0 && i > 0)
			BIO_printf(bio, "\n   ");
			BIO_printf(bio, " %02x", ((const unsigned char*)buf)[i]);
		}
		if (i < len)
		BIO_printf(bio, " ...");
		BIO_printf(bio, "\n");
	}
	(void)BIO_flush(bio);
}

TCN_IMPLEMENT_CALL(jint, SSLExt, debug)(TCN_STDARGS, jlong tcsock)
{
	tcn_socket_t *s = J2P(tcsock, tcn_socket_t *);
	tcn_ssl_conn_t *tcssl = (tcn_ssl_conn_t *)s->opaque;

	BIO_set_callback(SSL_get_rbio(tcssl->ssl), bio_dump_callback);
	BIO_set_callback(SSL_get_wbio(tcssl->ssl), bio_dump_callback);

	BIO_set_callback_arg(SSL_get_rbio(tcssl->ssl), (char *) tcssl->ctx->bio_os);
	BIO_set_callback_arg(SSL_get_wbio(tcssl->ssl), (char *) tcssl->ctx->bio_os);

	SSL_set_msg_callback(tcssl->ssl, msg_cb);
	SSL_set_msg_callback_arg(tcssl->ssl, (char *) tcssl->ctx->bio_os);
	return 0;
}

TCN_IMPLEMENT_CALL( jint, SSLExt, sslSetMode)(TCN_STDARGS, jlong tcsock, jint jmode)
{
	tcn_socket_t *s = J2P(tcsock, tcn_socket_t *);
	tcn_ssl_conn_t *tcssl = (tcn_ssl_conn_t *)s->opaque;
	int mode = SSL_get_mode(tcssl->ssl);

	mode |= jmode;
	SSL_set_mode(tcssl->ssl, mode);

	return mode;
}


#else

/* OpenSSL is not supported.
 * Create empty stubs.
 */
TCN_IMPLEMENT_CALL( jint, SSLExt, debug)(TCN_STDARGS, jlong tcsock)
{
	return (jint)-APR_ENOTIMPL;
}

TCN_IMPLEMENT_CALL( jint, SSLExt, setSessionData)(TCN_STDARGS, jlong tcsock, jbyteArray buf, jint len)
{
	return (jint)-APR_ENOTIMPL;
}

TCN_IMPLEMENT_CALL( jbyteArray, SSLExt, getSessionData)(TCN_STDARGS, jlong tcsock)
{
	return (jint)-APR_ENOTIMPL;
}

TCN_IMPLEMENT_CALL( jint, SSLExt, getTicket)(TCN_STDARGS, jlong tcsock, jbyteArray buf)
{
	return (jint)-APR_ENOTIMPL;
}

TCN_IMPLEMENT_CALL( jint, SSLExt, setTicket)(TCN_STDARGS, jlong tcsock, jbyteArray buf, jint len)
{
	return (jint)-APR_ENOTIMPL;
}

TCN_IMPLEMENT_CALL( jint, SSLExt, setTicketKeys)(TCN_STDARGS, jlong tc_ssl_ctx, jbyteArray buf, jint len)
{
	return (jint)-APR_ENOTIMPL;
}

TCN_IMPLEMENT_CALL( jint, SSLExt, sslSetMode)(TCN_STDARGS, jlong tc_ssl_ctx, jint mode)
{
	return (jint)-APR_ENOTIMPL;
}

#endif

// if ssl is not available - this will not be defined
#ifdef SSL_set_tlsext_host_name
TCN_IMPLEMENT_CALL(jint, SSLExt, setSNI)(TCN_STDARGS, jlong tcsock, jbyteArray buf, jint len)
{
	tcn_socket_t *s = J2P(tcsock, tcn_socket_t *);
	tcn_ssl_conn_t *tcssl = (tcn_ssl_conn_t *)s->opaque;
	unsigned char bytes[TCN_BUFFER_SZ];
	const unsigned char *bytesp = &bytes[0];

	if (len > TCN_BUFFER_SZ) {
		return -1;
	}
	(*e)->GetByteArrayRegion(e, buf, 0, len, bytes);
	SSL_set_tlsext_host_name(tcssl->ssl, &bytesp);
	return 0;
}
#else
TCN_IMPLEMENT_CALL(jint, SSLExt, setSNI)(TCN_STDARGS, jlong tcsock, jbyteArray buf, jint len)
{
	return (jint)-APR_ENOTIMPL;
}
#endif

#ifdef  OPENSSL_NPN_NEGOTIATED 
// See ssl_client_socket_openssl.cc
//     tools/flip_server/spdy_ssl.cc

/** Callback in client mode.
 */
static int cb_request_npn(SSL *ssl, unsigned char **out, unsigned char *outlen,
		const unsigned char *in, unsigned int inlen, void *arg)
{
	tcn_ssl_ctxt_t *tcsslctx = (tcn_ssl_ctxt_t *)arg;
	/* TODO: verify in contains the protocol... */

	/* This callback only returns the protocol string, rather than a length
	 prefixed set. We assume that NEXT_PROTO_STRING is a one element list and
	 remove the first byte to chop off the length prefix. */
	if (tcsslctx->npn != 0) {
		int status = SSL_select_next_proto(out, outlen, in, inlen,
				tcsslctx->npn,
				strlen(tcsslctx->npn));
		switch (status) {
			case OPENSSL_NPN_UNSUPPORTED:
			break;
			case OPENSSL_NPN_NEGOTIATED://1
			break;
			case OPENSSL_NPN_NO_OVERLAP://2
			break;
		}
	}
	return SSL_TLSEXT_ERR_OK;
}

static int cb_server_npn(SSL *ssl, const unsigned char **out, unsigned int *outlen,
		void *arg)
{
	tcn_ssl_ctxt_t *tcsslctx = (tcn_ssl_ctxt_t *)arg;
	if (tcsslctx->npn != 0) {
		*out = (unsigned char*) tcsslctx->npn;
		*outlen = strlen(tcsslctx->npn);
	}
	return SSL_TLSEXT_ERR_OK;
}

TCN_IMPLEMENT_CALL(jint, SSLExt, setNPN)(TCN_STDARGS, jlong tc_ssl_ctx,
		jbyteArray buf, jint len)
{
	tcn_ssl_ctxt_t *sslctx = J2P(tc_ssl_ctx, tcn_ssl_ctxt_t *);

	sslctx->npn = apr_pcalloc(sslctx->pool, len);
	(*e)->GetByteArrayRegion(e, buf, 0, len, &sslctx->npn[0]);

	if (sslctx->mode == SSL_MODE_SERVER) {
		SSL_CTX_set_next_protos_advertised_cb(sslctx->ctx, cb_server_npn, sslctx);
	} else {
		SSL_CTX_set_next_proto_select_cb(sslctx->ctx, cb_request_npn, sslctx);
	}
	return 0;
}

/** Only valid after handshake 
 */
TCN_IMPLEMENT_CALL(jint, SSLExt, getNPN)(TCN_STDARGS, jlong tcsock, jbyteArray buf)
{
	const unsigned char *npn;
	unsigned npn_len;
	tcn_socket_t *s = J2P(tcsock, tcn_socket_t *);
	tcn_ssl_conn_t *tcssl = (tcn_ssl_conn_t *)s->opaque;
	int bufLen = (*e)->GetArrayLength(e, buf);

	SSL_get0_next_proto_negotiated(tcssl->ssl, &npn, &npn_len);

	if (npn_len == 0 || bufLen < npn_len) {
		return 0;
	}
	int len = npn_len;
	(*e)->SetByteArrayRegion(e, buf, 0, len, npn);

	return len;
}

#else

TCN_IMPLEMENT_CALL(jint, SSLExt, setNPN)(TCN_STDARGS, jlong tc_ssl_ctx,
		jbyteArray buf, jint len)
{
	return (jint)-APR_ENOTIMPL;
}

TCN_IMPLEMENT_CALL(jint, SSLExt, getNPN)(TCN_STDARGS, jlong tcsock, jbyteArray buf)
{
	return (jint)-APR_ENOTIMPL;
}

#endif
