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

/**
 * Tools to encapsulate the VM
 *
 * @author:  Gal Shachor <shachor@il.ibm.com>
 * @author: Costin Manolache
 */
#ifndef JK_VM_H
#define JK_VM_H

#include "jk_pool.h"
#include "jk_env.h"
#include "jk_logger.h"
#include "jk_service.h"
#include "jk_map.h"

#define JK2_MAXOPTIONS  64
struct jk_vm {
    struct jk_bean *mbean;
    
    /* General name/value properties
     */
    struct jk_map *properties;
    
    struct jk_pool *pool;
    /** Should be JavaVM *, but we want this to be compilable
        without jni.h ( it'll be used to start out-of-process as
        well, same options and config
    */
    void *jvm;   

    /* Full path to the jni javai/jvm dll
     */
    char *jvm_dll_path;

    /*
     * All initialization options
     */
    char *options[JK2_MAXOPTIONS];

    /*
     * -Djava.class.path options
     */
    char *classpath[JK2_MAXOPTIONS];

    int nOptions;
    
    int nClasspath;
    /** Create the VM, attach - don't execute anything
     */
    int (*init)(struct jk_env *env, struct jk_vm *p );

    void *(*attach)(struct jk_env *env, struct jk_vm *p);

    void (*detach)(struct jk_env *env, struct jk_vm *p);

    void (*destroy)(struct jk_env *env, struct jk_vm *p);
};

typedef struct jk_vm jk_vm_t;

#endif
