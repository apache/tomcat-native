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
 * +-------------------------+-------------------------+
 * | LOGIN SEED CMD (1 byte) | MD5 of entropy (String) |
 * +-------------------------+-------------------------+
 *
 * +--------------------+------------------------+------------------------------+
 * | LOGOK CMD (1 byte) | NEGOCIED DATA (32bits) | SERVLET ENGINE INFO(CString) |
 * +--------------------+------------------------+------------------------------+
 *
 *
 * +---------------------+-----------------------+
 * | LOGNOK CMD (1 byte) | FAILURE CODE (32bits) |
 * +---------------------+-----------------------+
 */
 
/*
 * Third Login Phase (web server -> servlet engine), md5 of seed + secret is sent
 */
#define AJP14_LOGCOMP_CMD	            (apr_byte_t)0x12

/* web-server want context info after login */
#define AJP14_CONTEXT_INFO_NEG          0x80000000

/* web-server want context updates */
#define AJP14_CONTEXT_UPDATE_NEG        0x40000000

/* communication could use AJP14 */
#define AJP14_PROTO_SUPPORT_AJP14_NEG   0x00010000

#define AJP14_ENTROPY_SEED_LEN		    32      /* we're using MD5 => 32 chars */
#define AJP14_COMPUTED_KEY_LEN		    32      /* we're using MD5 also */


#define AJP14_MD5_DIGESTSIZE            16
