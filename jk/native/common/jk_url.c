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
 * Description: URL manupilation subroutines. (ported from mod_proxy).     *
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

/* already called in the knowledge that the characters are hex digits */
static  int jk_hex2c(const char *x)
{
    int i, ch;

#if !CHARSET_EBCDIC
    ch = x[0];
    if (JK_ISDIGIT(ch)) {
        i = ch - '0';
    }
    else if (JK_ISUPPER(ch)) {
        i = ch - ('A' - 10);
    }
    else {
        i = ch - ('a' - 10);
    }
    i <<= 4;

    ch = x[1];
    if (JK_ISDIGIT(ch)) {
        i += ch - '0';
    }
    else if (JK_ISUPPER(ch)) {
        i += ch - ('A' - 10);
    }
    else {
        i += ch - ('a' - 10);
    }
    return i;
#else /*CHARSET_EBCDIC*/
    /*
     * we assume that the hex value refers to an ASCII character
     * so convert to EBCDIC so that it makes sense locally;
     *
     * example:
     *
     * client specifies %20 in URL to refer to a space char;
     * at this point we're called with EBCDIC "20"; after turning
     * EBCDIC "20" into binary 0x20, we then need to assume that 0x20
     * represents an ASCII char and convert 0x20 to EBCDIC, yielding
     * 0x40
     */
    char buf[1];

    if (1 == sscanf(x, "%2x", &i)) {
        buf[0] = i & 0xFF;
        jk_xlate_from_ascii(buf, 1);
        return buf[0];
    }
    else {
        return 0;
    }
#endif /*CHARSET_EBCDIC*/
}

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
 * canonicalise a URL-encoded string
 */

/*
 * Convert a URL-encoded string to canonical form.
 * It decodes characters which need not be encoded,
 * and encodes those which must be encoded, and does not touch
 * those which must not be touched.
 */
char * jk_canonenc(char *y, const char *x, int len,
                                       enum enctype t, int forcedec,
                                       int proxyreq)
{
    int i, j, ch;
    char *allowed;  /* characters which should not be encoded */
    char *reserved; /* characters which much not be en/de-coded */

/*
 * N.B. in addition to :@&=, this allows ';' in an http path
 * and '?' in an ftp path -- this may be revised
 *
 * Also, it makes a '+' character in a search string reserved, as
 * it may be form-encoded. (Although RFC 1738 doesn't allow this -
 * it only permits ; / ? : @ = & as reserved chars.)
 */
    if (t == enc_path) {
        allowed = "~$-_.+!*'(),;:@&=";
    }
    else if (t == enc_search) {
        allowed = "$-_.!*'(),;:@&=";
    }
    else if (t == enc_user) {
        allowed = "$-_.+!*'(),;@&=";
    }
    else if (t == enc_fpath) {
        allowed = "$-_.+!*'(),?:@&=";
    }
    else {            /* if (t == enc_parm) */
        allowed = "$-_.+!*'(),?/:@&=";
    }

    if (t == enc_path) {
        reserved = "/";
    }
    else if (t == enc_search) {
        reserved = "+";
    }
    else {
        reserved = "";
    }

    /* y = apr_palloc(p, 3 * len + 1); */

    for (i = 0, j = 0; i < len; i++, j++) {
/* always handle '/' first */
        ch = x[i];
        if (strchr(reserved, ch)) {
            y[j] = ch;
            continue;
        }
/*
 * decode it if not already done. do not decode reverse proxied URLs
 * unless specifically forced
 */
        if ((forcedec || (proxyreq && proxyreq != JK_PROXYREQ_REVERSE)) && ch == '%') {
            if (!JK_ISXDIGIT(x[i + 1]) || !JK_ISXDIGIT(x[i + 2])) {
                return NULL;
            }
            ch = jk_hex2c(&x[i + 1]);
            i += 2;
            if (ch != 0 && strchr(reserved, ch)) {  /* keep it encoded */
                jk_c2hex(ch, &y[j]);
                j += 2;
                continue;
            }
        }
/* recode it, if necessary */
        if (!JK_ISALNUM(ch) && !strchr(allowed, ch)) {
            jk_c2hex(ch, &y[j]);
            j += 2;
        }
        else {
            y[j] = ch;
        }
    }
    y[j] = '\0';
    return y;
}
