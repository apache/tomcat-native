/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/* Portions of this software are based upon public domain software
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

char *JK_METHOD jk2_hextocstr(unsigned char *org, char *dst, int n)
{
    char *os = dst;
    unsigned char v;
    static unsigned char zitohex[] = "0123456789ABCDEF";

    while (--n >= 0) {
        v = *org++;
        *dst++ = zitohex[v >> 4];
        *dst++ = zitohex[v & 0x0f];
    }
    *dst = 0;

    return (os);
}

char *JK_METHOD jk2_md5(const unsigned char *org, const unsigned char *org2,
                        char *dst)
{
    apr_md5_ctx_t ctx;
    char buf[JK_MD5_DIGESTSIZE + 1];

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

main(int argc, char **argv)
{
    char xxx[(2 * JK_MD5_DIGESTSIZE) + 1];

    if (argc > 1)
        printf("%s => %s\n", argv[1], jk2_md5(argv[1], NULL, xxx));
}

#endif
