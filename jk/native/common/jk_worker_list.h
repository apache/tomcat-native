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

struct worker_factory_record
{
    const char *name;
    worker_factory fac;
};
typedef struct worker_factory_record worker_factory_record_t;

static jk_map_t *worker_map;

static worker_factory_record_t worker_factories[] = {
    /*
     * AJPv12 worker, this is the stable worker.
     */
    {JK_AJP12_WORKER_NAME, ajp12_worker_factory},
    /*
     * AJPv13 worker, fast bi-directional worker.
     */
    {JK_AJP13_WORKER_NAME, ajp13_worker_factory},
    /*
     * AJPv14 worker, next generation fast bi-directional worker.
     */
    {JK_AJP14_WORKER_NAME, ajp14_worker_factory},
    /*
     * In process JNI based worker. Requires the server to be 
     * multithreaded and to use native threads.
     */
#ifdef HAVE_JNI
    {JK_JNI_WORKER_NAME, jni_worker_factory},
#endif
    /*
     * Load balancing worker. Performs round robin with sticky 
     * session load balancing.
     */
    {JK_LB_WORKER_NAME, lb_worker_factory},

    /*
     * Marks the end of the worker factory list.
     */
    {NULL, NULL}
};
#endif /* _JK_WORKER_LIST_H */
#endif /* _PLACE_WORKER_LIST_HERE */
