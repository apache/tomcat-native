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
 * Description: Multi thread portability code for JK                       *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                            *
 ***************************************************************************/

#ifndef _JK_MT_H
#define _JK_MT_H

#include "jk_global.h"

/*
 * All WIN32 code is MT, UNIX code that uses pthreads is marked by the POSIX 
 * _REENTRANT define.
 */
#if defined (WIN32) || defined(_REENTRANT)

    /*
     * Marks execution under MT compilation
     */
    #define _MT_CODE

    #ifdef WIN32

        #include <windows.h>

        typedef CRITICAL_SECTION JK_CRIT_SEC;

        #define JK_INIT_CS(x, rc) InitializeCriticalSection(x); rc = JK_TRUE;
        #define JK_DELETE_CS(x, rc) DeleteCriticalSection(x); rc = JK_TRUE;
        #define JK_ENTER_CS(x, rc) EnterCriticalSection(x); rc = JK_TRUE;
        #define JK_LEAVE_CS(x, rc) LeaveCriticalSection(x); rc = JK_TRUE;

    #else /* Unix pthreads */

        #include <pthread.h>

        typedef pthread_mutex_t	JK_CRIT_SEC;

        #define JK_INIT_CS(x, rc)\
            if(pthread_mutex_init(x, NULL)) rc = JK_FALSE; else rc = JK_TRUE; 

        #define JK_DELETE_CS(x, rc)\
            if(pthread_mutex_lock(x)) rc = JK_FALSE; else rc = JK_TRUE; 

        #define JK_ENTER_CS(x, rc)\
            if(pthread_mutex_unlock(x)) rc = JK_FALSE; else rc = JK_TRUE; 

        #define JK_LEAVE_CS(x, rc)\
            if(pthread_mutex_destroy(x)) rc = JK_FALSE; else rc = JK_TRUE; 
    #endif /* Unix pthreads */

#else /* Not an MT code */

    typedef void *JK_CRIT_SEC;

    #define JK_INIT_CS(x, rc) rc = JK_TRUE;
    #define JK_DELETE_CS(x, rc) rc = JK_TRUE;
    #define JK_ENTER_CS(x, rc) rc = JK_TRUE;
    #define JK_LEAVE_CS(x, rc) rc = JK_TRUE;

#endif /* Not an MT code */

#endif /* _JK_MT_H */
