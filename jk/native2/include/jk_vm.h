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

struct jk_vm {
    
    struct jk_pool *pool;
    /** Should be JavaVM *, but we want this to be compilable
        without jni.h ( it'll be used to start out-of-process as
        well, same options and config
    */
    void *jvm;   

    char *tomcat_classpath;

    /* Full path to the jni javai/jvm dll
     */
    char *jvm_dll_path;

    /*
     * Initial Java heap size
     */
    unsigned tomcat_ms;

    /*
     * Max Java heap size
     */
    unsigned tomcat_mx;

    /*
     * Java system properties
     */
    char **sysprops;

    /*
     * Java 2 initialization options (-X... , -verbose etc.)
     */
    char **java2opts;

    /*
     * Java 2 lax/strict option checking (bool)
     */    
    int java2lax;

    /** Process the properties and set internal structures
     */
    int (*init)(struct jk_env *env, struct jk_vm *p,
                struct jk_map *props, char *prefix);


    /** Load the java libs, prepare for openning.
     */
    int (*load)(struct jk_env *env, struct jk_vm *p );

    /** Create the VM. 
     */
    int (*open)(struct jk_env *env, struct jk_vm *p );

    void *(*attach)(struct jk_env *env, struct jk_vm *p);

    void (*detach)(struct jk_env *env, struct jk_vm *p);
};

typedef struct jk_vm jk_vm_t;

#endif
