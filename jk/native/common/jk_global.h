/*
 * Copyright (c) 1997-1999 The Java Apache Project.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * 4. The names "Apache JServ", "Apache JServ Servlet Engine" and 
 *    "Java Apache Project" must not be used to endorse or promote products 
 *    derived from this software without prior written permission.
 *
 * 5. Products derived from this software may not be called "Apache JServ"
 *    nor may "Apache" nor "Apache JServ" appear in their names without 
 *    prior written permission of the Java Apache Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *    
 * THIS SOFTWARE IS PROVIDED BY THE JAVA APACHE PROJECT "AS IS" AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE JAVA APACHE PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Java Apache Group. For more information
 * on the Java Apache Project and the Apache JServ Servlet Engine project,
 * please see <http://java.apache.org/>.
 *
 */

/***************************************************************************
 * Description: Global definitions and include files that should exist     *
 *              anywhere                                                   *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                               *
 ***************************************************************************/

#ifndef JK_GLOBAL_H
#define JK_GLOBAL_H

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
#else
    #include <unistd.h>
    #include <netdb.h>

    #include <netinet/in.h>
    #include <sys/socket.h>
    #ifndef NETWARE
        #include <netinet/tcp.h>
        #include <arpa/inet.h>
        #include <sys/un.h>
        #include <sys/socketvar.h>
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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_GLOBAL_H */
