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

/***************************************************************************
 * Description: Global definitions and include files that should exist     *
 *              anywhere                                                   *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                               *
 ***************************************************************************/

#ifndef JK_GLOBAL_H
#define JK_GLOBAL_H

#ifndef WIN32
#include "portable.h"
#else
#define HAVE_VSNPRINTF
#define HAVE_SNPRINTF
#endif

#include "jk_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#ifdef _OSD_POSIX
#include "ap_config.h"
#endif

#ifdef AS400
#include "ap_config.h"
#include "apr_strings.h"
#include "apr_lib.h"
extern char *strdup (const char *str);
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
    #include <windows.h>
    #include <winsock.h>
    #include <sys/timeb.h>
#else
    #include <unistd.h>
    #if defined(NETWARE) && defined(__NOVELL_LIBC__)
        #include "novsock2.h"
        #define __sys_socket_h__
        #define __netdb_h__
        #define __netinet_in_h__
    #endif
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #ifndef NETWARE
        #include <netinet/tcp.h>
        #include <arpa/inet.h>
        #include <sys/un.h>
        #if !defined(_OSD_POSIX) && !defined(AS400) && !defined(CYGWIN)
            #include <sys/socketvar.h>
        #endif
        #if !defined(HPUX11) && !defined(AS400)
            #include <sys/select.h>
        #endif
    #endif
        
    #include <sys/time.h>
    #include <sys/ioctl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define JK_WORKER_FILE_TAG      ("worker_file")
#define JK_MOUNT_FILE_TAG       ("worker_mount_file")
#define JK_LOG_LEVEL_TAG        ("log_level")
#define JK_LOG_FILE_TAG         ("log_file")
#define JK_WORKER_NAME_TAG      ("worker")

#define JK_WORKER_FILE_DEF  ("workers.properties")
#define JK_LOG_LEVEL_DEF    ("emerg")

#define JK_TRUE  (1)
#define JK_FALSE (0)

#define JK_LF (10)
#define JK_CR (13)

#define JK_SESSION_IDENTIFIER "JSESSIONID"
#define JK_PATH_SESSION_IDENTIFIER ";jsessionid"

#if defined(WIN32) || defined(NETWARE)
    #define JK_METHOD __stdcall
    #define C_LEVEL_TRY_START       __try {
    #define C_LEVEL_TRY_END         }
    #define C_LEVEL_FINALLY_START   __finally {
    #define C_LEVEL_FINALLY_END     }
    #define PATH_SEPERATOR          (';')
    #define FILE_SEPERATOR          ('\\')
    #define PATH_ENV_VARIABLE       ("PATH")

    /* incompatible names... */
    #ifndef strcasecmp 
        #define strcasecmp stricmp
    #endif
#else
    #define JK_METHOD
    #define C_LEVEL_TRY_START       
    #define C_LEVEL_TRY_END         
    #define C_LEVEL_FINALLY_START   
    #define C_LEVEL_FINALLY_END     
    #define PATH_SEPERATOR          (':')
    #define FILE_SEPERATOR          ('/')
    #define PATH_ENV_VARIABLE       ("LD_LIBRARY_PATH")
#endif

/*
 * RECO STATUS
 */

#define RECO_NONE	0x00
#define RECO_INITED	0x01
#define RECO_FILLED	0x02

/*
 * JK options
 */

#define JK_OPT_FWDURIMASK           0x0003

#define JK_OPT_FWDURICOMPAT         0x0001
#define JK_OPT_FWDURICOMPATUNPARSED 0x0002
#define JK_OPT_FWDURIESCAPED        0x0003

#define JK_OPT_FWDURIDEFAULT        JK_OPT_FWDURICOMPAT

#define JK_OPT_FWDKEYSIZE           0x0004

#define JK_OPT_FWDDIRS              0x0008

/* Check for EBCDIC systems */

/* Check for Apache 2.0 running on an EBCDIC system */
#if APR_CHARSET_EBCDIC 

#include <util_ebcdic.h>

#define USE_CHARSET_EBCDIC
#define jk_xlate_to_ascii(b, l) ap_xlate_proto_to_ascii(b, l)
#define jk_xlate_from_ascii(b, l) ap_xlate_proto_from_ascii(b, l)

#else   /* APR_CHARSET_EBCDIC */

/* Check for Apache 1.3 running on an EBCDIC system */
#ifdef CHARSET_EBCDIC

#define USE_CHARSET_EBCDIC
#define jk_xlate_to_ascii(b, l) ebcdic2ascii(b, b, l)
#define jk_xlate_from_ascii(b, l) ascii2ebcdic(b, b, l)

#else /* CHARSET_EBCDIC */

/* We're in on an ASCII system */

#define jk_xlate_to_ascii(b, l)             /* NOOP */
#define jk_xlate_from_ascii(b, l)           /* NOOP */

#endif /* CHARSET_EBCDIC */

#endif /* APR_CHARSET_EBCDIC */

#ifdef WIN32
/* For WIN32, emulate gettimeofday() using _ftime() */
#define gettimeofday(tv,tz) { struct _timeb tb; _ftime(&tb); (tv)->tv_sec = tb.time; (tv)->tv_usec = tb.millitm * 1000; }
#ifdef HAVE_APR
#define snprintf apr_snprintf
#define vsnprintf apr_vsnprintf
#else
/* define snprint to match windows version */
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif
#endif

/* Use apr snprintf() and vsnprintf() when needed */
#if defined(HAVE_APR)
#if !defined(HAVE_SNPRINTF)
#define snprintf apr_snprintf
#endif
#if !defined(HAVE_VSNPRINTF)
#define vsnprintf apr_vsnprintf
#endif
#endif

/* XXXX There is a snprintf() and vsnprintf() in jk_util.c */
/* if those work remove the #define. */
#if defined(NETWARE) || defined(AS400)
#define USE_SPRINTF
#define USE_VSPRINTF
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_GLOBAL_H */
