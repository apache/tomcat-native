/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */

/***************************************************************************
 * Description: Global definitions and include files that should exist     *
 *              anywhere                                                   *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                               *
 ***************************************************************************/

#ifndef JK_GLOBAL_H
#define JK_GLOBAL_H

#include "jk_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
    #include <windows.h>
    #include <winsock.h>
    #include <sys/timeb.h>
#else
    #include <unistd.h>
    #include <netdb.h>

    #include <netinet/in.h>
    #include <sys/socket.h>
    #ifndef NETWARE
        #include <netinet/tcp.h>
        #include <arpa/inet.h>
        #include <sys/un.h>
        #ifndef _OSD_POSIX
            #include <sys/socketvar.h>
        #endif
        #ifndef HPUX11
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
/* define snprint to match windows version */
#define snprintf _snprintf
/* define vsnprint to match windows version */
#define vsnprintf _vsnprintf
#endif

#ifdef NETWARE
#define USE_SPRINTF
#define USE_VSPRINTF
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_GLOBAL_H */
