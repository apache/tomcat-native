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

/***************************************************************************
 * Description: MD5 encoding wrapper                                       *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                           *
 ***************************************************************************/

/*
 * JK MD5 Encoding function (jk_MD5Encode)
 *
 * Use the APR_UTIL apr_md5
 *
 * Nota: If you use an EBCDIC system without Apache, you'll have to use MD5 encoding
 * corresponding call or have a ebcdic2ascii() functions somewhere.
 * For example current AS/400 have MD5 encoding support APIs but olders not....
 */
 
#include "jk_global.h"
#include "jk_env.h"
#include "jk_md5.h"
#include "apr_md5.h"

char * JK_METHOD jk2_hextocstr(unsigned char *org, char * dst, int n)
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

char * JK_METHOD jk2_md5(const unsigned char *org, const unsigned char *org2, char *dst)
{
    apr_md5_ctx_t ctx;
    char          buf[JK_MD5_DIGESTSIZE + 1];

    apr_md5_init(&ctx);
    apr_md5_update(&ctx, org, strlen(org));

    if (org2 != NULL)
        apr_md5_update(&ctx, org2, strlen(org2));

    apr_md5_final(buf, &ctx);

    return (jk2_hextocstr(buf, dst, JK_MD5_DIGESTSIZE));
}


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
        printf("%s => %s\n", argv[1], jk2_md5(argv[1], NULL, xxx));
}

#endif 
