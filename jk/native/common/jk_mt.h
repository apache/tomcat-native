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
 * Description: Multi thread portability code for JK                       *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
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
            if(pthread_mutex_destroy(x)) rc = JK_FALSE; else rc = JK_TRUE; 

        #define JK_ENTER_CS(x, rc)\
            if(pthread_mutex_lock(x)) rc = JK_FALSE; else rc = JK_TRUE; 

        #define JK_LEAVE_CS(x, rc)\
            if(pthread_mutex_unlock(x)) rc = JK_FALSE; else rc = JK_TRUE; 
    #endif /* Unix pthreads */

#else /* Not an MT code */

    typedef void *JK_CRIT_SEC;

    #define JK_INIT_CS(x, rc) rc = JK_TRUE;
    #define JK_DELETE_CS(x, rc) rc = JK_TRUE;
    #define JK_ENTER_CS(x, rc) rc = JK_TRUE;
    #define JK_LEAVE_CS(x, rc) rc = JK_TRUE;

#endif /* Not an MT code */

#endif /* _JK_MT_H */
