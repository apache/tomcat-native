/*
 * This is work is derived from material Copyright RSA Data Security, Inc.
 *
 * The RSA copyright statement and Licence for that original material is
 * included below. This is followed by the Apache copyright statement and
 * licence for the modifications made to that material.
 */

/* MD5C.C - RSA Data Security, Inc., MD5 message-digest algorithm
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
   rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD5 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.

   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD5 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.

   These notices must be retained in any copies of any part of this
   documentation and/or software.
 */

/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

/*
 * The ap_MD5Encode() routine uses much code obtained from the FreeBSD 3.0
 * MD5 crypt() function, which is licenced as follows:
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

/***************************************************************************
 * Description: MD5 encoding wrapper                                       *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

/*
 * JK MD5 Encoding function (jk_MD5Encode)
 *
 * Jk delegate MD5 encoding to ap_MD5Encode when used in Apache Web-Server.
 * When another web-server is used like NES/IIS, we should use corresponding calls.
 * NES/IIS specialists will add the necessary code but until that, I reused the code
 * from Apache HTTP server.
 * 
 * Nota: If you use an EBCDIC system without Apache, you'll have to use MD5 encoding
 * corresponding call or have a ebcdic2ascii() functions somewhere.
 * For example current AS/400 have MD5 encoding support APIs but olders not....
 */
 
#include "jk_global.h"
#include "jk_md5.h"

char * JK_METHOD jk_hextocstr(unsigned char *org, char * dst, int n)
{
    char * os = dst;
    unsigned char v;
    static unsigned char zitohex[] = "0123456789ABCDEF";

    while (--n >= 0) {
    v = *org++;
    *dst++ = zitohex[v >> 4];
    *dst++ = zitohex[v&0x0f];
    }
    *dst = 0;

    return (os);
}

#ifndef USE_APACHE_MD5 

/* Constants for MD5Transform routine.
 */

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static void MD5Transform(JK_UINT4 state[4], const unsigned char block[64]);
static void Encode(unsigned char *output, const JK_UINT4 *input, unsigned int len);
static void Decode(JK_UINT4 *output, const unsigned char *input, unsigned int len);
static void jk_MD5Init(JK_MD5_CTX *context);
static void jk_MD5Update(JK_MD5_CTX *context, const unsigned char *input, unsigned int inputLen);
/*static void jk_MD5Final(unsigned char digest[JK_MD5_DIGESTSIZE], JK_MD5_CTX *context);*/

static unsigned char PADDING[64] =
{
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
   Rotation is separate from addition to prevent recomputation.
 */
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (JK_UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (JK_UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (JK_UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (JK_UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

/* MD5 initialization. Begins an MD5 operation, writing a new context.
 */
static void jk_MD5Init(JK_MD5_CTX *context)
{
    context->count[0] = context->count[1] = 0;
    /* Load magic initialization constants. */
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}

/* MD5 block update operation. Continues an MD5 message-digest
   operation, processing another message block, and updating the
   context.
 */
static void jk_MD5Update(JK_MD5_CTX *context, const unsigned char *input,
			      unsigned int inputLen)
{
    unsigned int i, idx, partLen;

    /* Compute number of bytes mod 64 */
    idx = (unsigned int) ((context->count[0] >> 3) & 0x3F);

    /* Update number of bits */
    if ((context->count[0] += ((JK_UINT4) inputLen << 3))
	< ((JK_UINT4) inputLen << 3)) {
	context->count[1]++;
    }
    context->count[1] += (JK_UINT4) inputLen >> 29;

    partLen = 64 - idx;

    /* Transform as many times as possible. */
#ifndef CHARSET_EBCDIC
    if (inputLen >= partLen) {
	memcpy(&context->buffer[idx], input, partLen);
	MD5Transform(context->state, context->buffer);

	for (i = partLen; i + 63 < inputLen; i += 64) {
	    MD5Transform(context->state, &input[i]);
	}

	idx = 0;
    }
    else {
	i = 0;
    }

    /* Buffer remaining input */
    memcpy(&context->buffer[idx], &input[i], inputLen - i);
#else /*CHARSET_EBCDIC*/
    if (inputLen >= partLen) {
	ebcdic2ascii(&context->buffer[idx], input, partLen);
	MD5Transform(context->state, context->buffer);

	for (i = partLen; i + 63 < inputLen; i += 64) {
	    unsigned char inp_tmp[64];
	    ebcdic2ascii(inp_tmp, &input[i], 64);
	    MD5Transform(context->state, inp_tmp);
	}

	idx = 0;
    }
    else {
	i = 0;
    }

    /* Buffer remaining input */
    ebcdic2ascii(&context->buffer[idx], &input[i], inputLen - i);
#endif /*CHARSET_EBCDIC*/
}

/* MD5 finalization. Ends an MD5 message-digest operation, writing the
   the message digest and zeroizing the context.
 */
void JK_METHOD jk_MD5Final(unsigned char digest[16], JK_MD5_CTX *context)
{
    unsigned char bits[8];
    unsigned int idx, padLen;


    /* Save number of bits */
    Encode(bits, context->count, 8);

#ifdef CHARSET_EBCDIC
    /* XXX: @@@: In order to make this no more complex than necessary,
     * this kludge converts the bits[] array using the ascii-to-ebcdic
     * table, because the following jk_MD5Update() re-translates
     * its input (ebcdic-to-ascii).
     * Otherwise, we would have to pass a "conversion" flag to jk_MD5Update()
     */
    ascii2ebcdic(bits,bits,8);

    /* Since everything is converted to ascii within jk_MD5Update(), 
     * the initial 0x80 (PADDING[0]) must be stored as 0x20 
     */
    ascii2ebcdic(PADDING,PADDING,1);
#endif /*CHARSET_EBCDIC*/

    /* Pad out to 56 mod 64. */
    idx = (unsigned int) ((context->count[0] >> 3) & 0x3f);
    padLen = (idx < 56) ? (56 - idx) : (120 - idx);
    jk_MD5Update(context, (const unsigned char *)PADDING, padLen);

    /* Append length (before padding) */
    jk_MD5Update(context, (const unsigned char *)bits, 8);

    /* Store state in digest */
    Encode(digest, context->state, 16);

    /* Zeroize sensitive information. */
    memset(context, 0, sizeof(*context));
}

/* MD5 basic transformation. Transforms state based on block. */
static void MD5Transform(JK_UINT4 state[4], const unsigned char block[64])
{
    JK_UINT4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    Decode(x, block, 64);

    /* Round 1 */
    FF(a, b, c, d, x[0], S11, 0xd76aa478);	/* 1 */
    FF(d, a, b, c, x[1], S12, 0xe8c7b756);	/* 2 */
    FF(c, d, a, b, x[2], S13, 0x242070db);	/* 3 */
    FF(b, c, d, a, x[3], S14, 0xc1bdceee);	/* 4 */
    FF(a, b, c, d, x[4], S11, 0xf57c0faf);	/* 5 */
    FF(d, a, b, c, x[5], S12, 0x4787c62a);	/* 6 */
    FF(c, d, a, b, x[6], S13, 0xa8304613);	/* 7 */
    FF(b, c, d, a, x[7], S14, 0xfd469501);	/* 8 */
    FF(a, b, c, d, x[8], S11, 0x698098d8);	/* 9 */
    FF(d, a, b, c, x[9], S12, 0x8b44f7af);	/* 10 */
    FF(c, d, a, b, x[10], S13, 0xffff5bb1);	/* 11 */
    FF(b, c, d, a, x[11], S14, 0x895cd7be);	/* 12 */
    FF(a, b, c, d, x[12], S11, 0x6b901122);	/* 13 */
    FF(d, a, b, c, x[13], S12, 0xfd987193);	/* 14 */
    FF(c, d, a, b, x[14], S13, 0xa679438e);	/* 15 */
    FF(b, c, d, a, x[15], S14, 0x49b40821);	/* 16 */

    /* Round 2 */
    GG(a, b, c, d, x[1], S21, 0xf61e2562);	/* 17 */
    GG(d, a, b, c, x[6], S22, 0xc040b340);	/* 18 */
    GG(c, d, a, b, x[11], S23, 0x265e5a51);	/* 19 */
    GG(b, c, d, a, x[0], S24, 0xe9b6c7aa);	/* 20 */
    GG(a, b, c, d, x[5], S21, 0xd62f105d);	/* 21 */
    GG(d, a, b, c, x[10], S22, 0x2441453);	/* 22 */
    GG(c, d, a, b, x[15], S23, 0xd8a1e681);	/* 23 */
    GG(b, c, d, a, x[4], S24, 0xe7d3fbc8);	/* 24 */
    GG(a, b, c, d, x[9], S21, 0x21e1cde6);	/* 25 */
    GG(d, a, b, c, x[14], S22, 0xc33707d6);	/* 26 */
    GG(c, d, a, b, x[3], S23, 0xf4d50d87);	/* 27 */
    GG(b, c, d, a, x[8], S24, 0x455a14ed);	/* 28 */
    GG(a, b, c, d, x[13], S21, 0xa9e3e905);	/* 29 */
    GG(d, a, b, c, x[2], S22, 0xfcefa3f8);	/* 30 */
    GG(c, d, a, b, x[7], S23, 0x676f02d9);	/* 31 */
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a);	/* 32 */

    /* Round 3 */
    HH(a, b, c, d, x[5], S31, 0xfffa3942);	/* 33 */
    HH(d, a, b, c, x[8], S32, 0x8771f681);	/* 34 */
    HH(c, d, a, b, x[11], S33, 0x6d9d6122);	/* 35 */
    HH(b, c, d, a, x[14], S34, 0xfde5380c);	/* 36 */
    HH(a, b, c, d, x[1], S31, 0xa4beea44);	/* 37 */
    HH(d, a, b, c, x[4], S32, 0x4bdecfa9);	/* 38 */
    HH(c, d, a, b, x[7], S33, 0xf6bb4b60);	/* 39 */
    HH(b, c, d, a, x[10], S34, 0xbebfbc70);	/* 40 */
    HH(a, b, c, d, x[13], S31, 0x289b7ec6);	/* 41 */
    HH(d, a, b, c, x[0], S32, 0xeaa127fa);	/* 42 */
    HH(c, d, a, b, x[3], S33, 0xd4ef3085);	/* 43 */
    HH(b, c, d, a, x[6], S34, 0x4881d05);	/* 44 */
    HH(a, b, c, d, x[9], S31, 0xd9d4d039);	/* 45 */
    HH(d, a, b, c, x[12], S32, 0xe6db99e5);	/* 46 */
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8);	/* 47 */
    HH(b, c, d, a, x[2], S34, 0xc4ac5665);	/* 48 */

    /* Round 4 */
    II(a, b, c, d, x[0], S41, 0xf4292244);	/* 49 */
    II(d, a, b, c, x[7], S42, 0x432aff97);	/* 50 */
    II(c, d, a, b, x[14], S43, 0xab9423a7);	/* 51 */
    II(b, c, d, a, x[5], S44, 0xfc93a039);	/* 52 */
    II(a, b, c, d, x[12], S41, 0x655b59c3);	/* 53 */
    II(d, a, b, c, x[3], S42, 0x8f0ccc92);	/* 54 */
    II(c, d, a, b, x[10], S43, 0xffeff47d);	/* 55 */
    II(b, c, d, a, x[1], S44, 0x85845dd1);	/* 56 */
    II(a, b, c, d, x[8], S41, 0x6fa87e4f);	/* 57 */
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0);	/* 58 */
    II(c, d, a, b, x[6], S43, 0xa3014314);	/* 59 */
    II(b, c, d, a, x[13], S44, 0x4e0811a1);	/* 60 */
    II(a, b, c, d, x[4], S41, 0xf7537e82);	/* 61 */
    II(d, a, b, c, x[11], S42, 0xbd3af235);	/* 62 */
    II(c, d, a, b, x[2], S43, 0x2ad7d2bb);	/* 63 */
    II(b, c, d, a, x[9], S44, 0xeb86d391);	/* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    /* Zeroize sensitive information. */
    memset(x, 0, sizeof(x));
}

/* Encodes input (JK_UINT4) into output (unsigned char). Assumes len is
   a multiple of 4.
 */
static void Encode(unsigned char *output, const JK_UINT4 *input, unsigned int len)
{
    unsigned int i, j;
    JK_UINT4 k;

    for (i = 0, j = 0; j < len; i++, j += 4) {
	k = input[i];
	output[j] = (unsigned char) (k & 0xff);
	output[j + 1] = (unsigned char) ((k >> 8) & 0xff);
	output[j + 2] = (unsigned char) ((k >> 16) & 0xff);
	output[j + 3] = (unsigned char) ((k >> 24) & 0xff);
    }
}

/* Decodes input (unsigned char) into output (JK_UINT4). Assumes len is
 * a multiple of 4.
 */
static void Decode(JK_UINT4 *output, const unsigned char *input, unsigned int len)
{
    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4)
	output[i] = ((JK_UINT4) input[j]) | (((JK_UINT4) input[j + 1]) << 8) |
	    (((JK_UINT4) input[j + 2]) << 16) | (((JK_UINT4) input[j + 3]) << 24);
}

/*
 * The following MD5 password encryption code was largely borrowed from
 * the FreeBSD 3.0 /usr/src/lib/libcrypt/crypt.c file, which is
 * licenced as stated at the top of this file.
 */
static void jk_to64(char *s, unsigned long v, int n)
{
    static unsigned char itoa64[] =         /* 0 ... 63 => ASCII - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    while (--n >= 0) {
	*s++ = itoa64[v&0x3f];
	v >>= 6;
    }
}

char * JK_METHOD jk_md5(const unsigned char *org, const unsigned char *org2, char *dst)
{
	JK_MD5_CTX  ctx;
	char		buf[JK_MD5_DIGESTSIZE + 1];

    jk_MD5Init(&ctx);
    jk_MD5Update(&ctx, org, strlen((const char *)org));

	if (org2 != NULL)
		jk_MD5Update(&ctx, org2, strlen((const char *)org2));

    jk_MD5Final((unsigned char *)buf, &ctx);
	return (jk_hextocstr((unsigned char *)buf, dst, JK_MD5_DIGESTSIZE));
}

#else /* USE_APACHE_MD5 */

#include "httpd.h"
#include "http_config.h"

#ifdef STANDARD20_MODULE_STUFF

#include "apr_md5.h"
#define  AP_MD5_CTX 	apr_md5_ctx_t
#define  ap_MD5Init 	apr_md5_init
#define  ap_MD5Update 	apr_md5_update
#define  ap_MD5Final 	apr_md5_final

#else /* STANDARD20_MODULE_STUFF */

#include "ap_md5.h"

#endif /* STANDARD20_MODULE_STUFF */

char * JK_METHOD jk_md5(const unsigned char *org, const unsigned char *org2, char *dst)
{
	AP_MD5_CTX ctx;
	char	   buf[JK_MD5_DIGESTSIZE + 1];

    ap_MD5Init(&ctx);
    ap_MD5Update(&ctx, org, strlen(org));

	if (org2 != NULL)
		ap_MD5Update(&ctx, org2, strlen(org2));

    ap_MD5Final(buf, &ctx);
	return (jk_hextocstr(buf, dst, JK_MD5_DIGESTSIZE));
}

#endif /* USE_APACHE_MD5 */

/* Test values:
 * ""                  D4 1D 8C D9 8F 00 B2 04  E9 80 09 98 EC F8 42 7E
 * "a"                 0C C1 75 B9 C0 F1 B6 A8  31 C3 99 E2 69 77 26 61
 * "abc                90 01 50 98 3C D2 4F B0  D6 96 3F 7D 28 E1 7F 72
 * "message digest"    F9 6B 69 7D 7C B7 93 8D  52 5A 2F 31 AA F1 61 D0
 *
 */

#ifdef TEST_JKMD5

main(int argc, char ** argv)
{
	char xxx[(2 * JK_MD5_DIGESTSIZE) + 1];

	if (argc > 1)
		printf("%s => %s\n", argv[1], jk_md5(argv[1], NULL, xxx));
}

#endif 
