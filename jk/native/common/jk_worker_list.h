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
 * Description: Worker list                                                *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

/*
 * This file includes a list of all the possible workers in the jk library
 * plus their factories. 
 *
 * If you want to add a worker just place it in the worker_factories array
 * with its unique name and factory.
 *
 * If you want to remove a worker, hjust comment out its line in the 
 * worker_factories array as well as its header file. For example, look
 * at what we have done to the ajp23 worker.
 *
 * Note: This file should be included only in the jk_worker controller.
 * Currently the jk_worker controller is located in jk_worker.c
 */
#ifdef _PLACE_WORKER_LIST_HERE
    #ifndef _JK_WORKER_LIST_H
    #define _JK_WORKER_LIST_H

        #include "jk_ajp12_worker.h"
        #include "jk_ajp13_worker.h"
        #include "jk_ajp14_worker.h"
        #ifndef HPUX11GCC
            #include "jk_jni_worker.h"
        #endif
        #include "jk_lb_worker.h"

        struct worker_factory_record {
            const char *name;
            worker_factory fac;
        };
        typedef struct worker_factory_record worker_factory_record_t;

        static jk_map_t *worker_map;

        static worker_factory_record_t worker_factories[] = {
            /*
             * AJPv12 worker, this is the stable worker.
             */
            { JK_AJP12_WORKER_NAME, ajp12_worker_factory},
            /*
             * AJPv13 worker, fast bi-directional worker.
             */
            { JK_AJP13_WORKER_NAME, ajp13_worker_factory},
            /*
             * AJPv14 worker, next generation fast bi-directional worker.
             */
            { JK_AJP14_WORKER_NAME, ajp14_worker_factory},
            /*
             * In process JNI based worker. Requires the server to be 
             * multithreaded and to use native threads.
             */
            #ifndef HPUX11GCC
                { JK_JNI_WORKER_NAME, jni_worker_factory},
            #endif
            /*
             * Load balancing worker. Performs round robin with sticky 
             * session load balancing.
             */
            { JK_LB_WORKER_NAME, lb_worker_factory},

            /*
             * Marks the end of the worker factory list.
             */
            { NULL, NULL}
    };
    #endif /* _JK_WORKER_LIST_H */
#endif /* _PLACE_WORKER_LIST_HERE */

