/* Copyright 1999-2004 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AJP_H
#define AJP_H

#include "apr_version.h"
#include "apr.h"

#include "apr_hooks.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_md5.h"
#include "apr_network_io.h"
#include "apr_pools.h"
#include "apr_strings.h"
#include "apr_uri.h"
#include "apr_date.h"
#include "apr_fnmatch.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"

#if APR_HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if APR_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if APR_CHARSET_EBCDIC

#define USE_CHARSET_EBCDIC
#define ajp_xlate_to_ascii(b, l) ap_xlate_proto_to_ascii(b, l)
#define ajp_xlate_from_ascii(b, l) ap_xlate_proto_from_ascii(b, l)

#else                           /* APR_CHARSET_EBCDIC */

#define ajp_xlate_to_ascii(b, l) 
#define ajp_xlate_from_ascii(b, l) 

#endif

#ifdef AJP_USE_HTTPD_WRAP
#include "httpd_wrap.h"
#endif

struct ajp_msg
{
	char *		buf;
	apr_size_t	headerLen;
	apr_size_t  len;
	apr_size_t	pos;
	int     serverSide;
};

typedef struct ajp_msg ajp_msg_t;

#define AJP_HEADER_LEN            	4
#define AJP_HEADER_SZ_LEN         	2
#define AJP_MSG_BUFFER_SZ 		(8*1024)
#define AJP13_MAX_SEND_BODY_SZ      (AJP_DEF_BUFFER_SZ - 6)

/* Webserver ask container to take control (logon phase) */
#define CMD_AJP13_PING               (unsigned char)8

/* Webserver check if container is alive, since container should respond by cpong */
#define CMD_AJP13_CPING              (unsigned char)10

/* Container response to cping request */
#define CMD_AJP13_CPONG              (unsigned char)9


#endif /* AJP_H */

