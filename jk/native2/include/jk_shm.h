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

#ifndef JK_SHM_H
#define JK_SHM_H

#include "jk_global.h"
#include "jk_env.h"
#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_msg.h"
#include "jk_service.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct jk_env;
struct jk_shm;
    
typedef struct jk_shm jk_shm_t;
typedef struct jk_shm_slot jk_shm_slot_t;
typedef struct jk_shm_head jk_shm_head_t;


/* That's the default - should be enough for most cases.
   8k is what we use as the default packet size in ajp13, and 256
   slots should be enough for now ( == 256 workers )
   XXX implement the setters to override this
*/
#define DEFAULT_SLOT_SIZE 1024 * 8
#define DEFAULT_SLOT_COUNT 256


    
/** Each shared memory slot has at least the following components.
 */
struct jk_shm_slot {
    /** Version of the segment. Whoever writes it must change the
        version after writing. Various components will check the version
        and refresh if needed
    */
    int ver;

    /** Size of the segment */
    int size;

    int structSize;
    int structCnt;
    
    /** Full name of the segment. type:localName convention.
     */
    char name[64];

    char data[1];
};

    
struct jk_shm_head {
    struct jk_shm_slot slot;
    
    int slotSize;
    
    int slotMaxCount;
    
    /* Last used position. Increase ( at least atomically, eventually with mutexes )
       each time a slot is created */
    int lastSlot;

    /* XXX Need a more generic mechanism */
    int lbVer;
};


/**
 *  Shared memory support. This is similar with the scoreboard or jserv's worker shm, but
 *  organized a bit more generic to support use of shm as a channel and to support config
 *  changes.
 * 
 *  The structure is organized as an array of 'slots'. Each slot has a name and data. Slots are long lived -
 *  they are never destroyed.
 *  Each slot has it's own rules for content and synchronization - but typically they are 'owned'
 *  by a process or thread and use various conventions to avoid the need for sync.
  *
 * @author Costin Manolache
 */
struct jk_shm {
    struct jk_bean *mbean;

    struct jk_pool *pool;

    char *fname;

    /** Initialize the shared memory area. It'll map the shared memory 
     *  segment if it exists, or create and init it if not.
     */
    int (JK_METHOD *init)(struct jk_env *env, struct jk_shm *shm);

    /** Reset the shm area
     */
    int (JK_METHOD *reset)(struct jk_env *env, struct jk_shm *shm);

    
    /** Detach from the shared memory segment
     */
    int (JK_METHOD *destroy)(struct jk_env *env, struct jk_shm *shm);

    /** Get a shm slot. Each slot has different rules for synchronization, based on type. 
     */
    struct jk_shm_slot *(JK_METHOD *getSlot)(struct jk_env *env, struct jk_shm *shm, int pos);

    /** Create a slot. This typically involves inter-process synchronization.
     */
    struct jk_shm_slot *(JK_METHOD *createSlot)(struct jk_env *env, struct jk_shm *shm, char *name, int size);

    int size;

    int slotSize;
    
    int slotMaxCount;
    
    void *image;

    struct jk_shm_head *head;

    /* Private data */
    void *privateData;
};
    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif 
