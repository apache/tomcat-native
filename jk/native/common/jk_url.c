/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/***************************************************************************
 * Description: URL manipulation subroutines. (ported from mod_proxy).     *
 * Version:     $Revision: 531816 $                                        *
 ***************************************************************************/

#include "jk_global.h"
#include "jk_url.h"

#ifdef HAVE_APR
#define JK_ISXDIGIT(x) apr_isxdigit((x))
#define JK_ISDIGIT(x)  apr_isdigit((x))
#define JK_ISUPPER(x)  apr_isupper((x))
#define JK_ISALNUM(x)  apr_isalnum((x))
#else
#define JK_ISXDIGIT(x) isxdigit((int)(unsigned char)((x)))
#define JK_ISDIGIT(x)  isdigit((int)(unsigned char)((x)))
#define JK_ISUPPER(x)  isupper((int)(unsigned char)((x)))
#define JK_ISALNUM(x)  isalnum((int)(unsigned char)((x)))
#endif

static void jk_c2hex(int ch, char *x)
{
#if !CHARSET_EBCDIC
    int i;

    x[0] = '%';
    i = (ch & 0xF0) >> 4;
    if (i >= 10) {
        x[1] = ('A' - 10) + i;
    }
    else {
        x[1] = '0' + i;
    }

    i = ch & 0x0F;
    if (i >= 10) {
        x[2] = ('A' - 10) + i;
    }
    else {
        x[2] = '0' + i;
    }
#else /*CHARSET_EBCDIC*/
    static const char ntoa[] = { "0123456789ABCDEF" };
    char buf[1];

    ch &= 0xFF;

    buf[0] = ch;
    jk_xlate_to_ascii(buf, 1);

    x[0] = '%';
    x[1] = ntoa[(buf[0] >> 4) & 0x0F];
    x[2] = ntoa[buf[0] & 0x0F];
    x[3] = '\0';
#endif /*CHARSET_EBCDIC*/
}

/*
 * Convert a URL-encoded string to canonical form.
 * It encodes those which must be encoded, and does not touch
 * those which must not be touched.
 * String x must be '\0'-terminated.
 * String y must be pre-allocated with len maxlen
 * (including the terminating '\0').
 */
int jk_canonenc(const char *x, char *y, int maxlen)
{
    int i, j;
    int ch = x[0];
    char *allowed;  /* characters which should not be encoded */
    char *reserved; /* characters which much not be en/de-coded */

/*
 * N.B. in addition to :@&=, this allows ';' in an http path
 * and '?' in an ftp path -- this may be revised
 */
    allowed = "~$-_.+!*'(),;:@&=";
    reserved = "/";

    for (i = 0, j = 0; ch != '\0' && j < maxlen; i++, j++, ch=x[i]) {
/* always handle '/' first */
        if (strchr(reserved, ch)) {
            y[j] = ch;
            continue;
        }
/* recode it, if necessary */
        if (!JK_ISALNUM(ch) && !strchr(allowed, ch)) {
            if (j+2<maxlen) {
                jk_c2hex(ch, &y[j]);
                j += 2;
            }
            else {
                return JK_FALSE;
            }
        }
        else {
            y[j] = ch;
        }
    }
    if (j<maxlen) {
        y[j] = '\0';
        return JK_TRUE;
    }
    else {
        return JK_FALSE;
    }
}
