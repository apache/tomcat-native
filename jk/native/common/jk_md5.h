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

/*
 * This is work is derived from material Copyright RSA Data Security, Inc.
 *
 * The RSA copyright statement and Licence for that original material is
 * included below. This is followed by the Apache copyright statement and
 * licence for the modifications made to that material.
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

#ifndef JK_APACHE_MD5_H
#define JK_APACHE_MD5_H

#ifdef __cplusplus
extern "C" {
#endif

/* MD5.H - header file for MD5.C */

#define JK_MD5_DIGESTSIZE 16

/* JK_UINT4 defines a four byte word */
typedef unsigned int JK_UINT4;

/* MD5 context. */
typedef struct {
    JK_UINT4 state[4];		/* state (ABCD) */
    JK_UINT4 count[2];		/* number of bits, modulo 2^64 (lsb first) */
    unsigned char buffer[64];	/* input buffer */
} JK_MD5_CTX;

/*
 * Define the Magic String prefix that identifies a password as being
 * hashed using our algorithm.
 */
#define JK_MD5PW_ID "$apr1$"
#define JK_MD5PW_IDLEN 6

char * JK_METHOD jk_hextocstr(unsigned char *org, char * dst, int n);
char * JK_METHOD jk_md5(const unsigned char *org, const unsigned char *org2, char *dst);


#ifdef __cplusplus
}
#endif

#endif	/* !JK_APACHE_MD5_H */
