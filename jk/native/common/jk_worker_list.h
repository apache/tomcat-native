/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2003 The Apache Software Foundation.          *
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
 * Description: Worker list                                                *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
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
        #ifdef HAVE_JNI
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
            #ifdef HAVE_JNI
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

