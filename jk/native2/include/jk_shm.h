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

#ifndef JK_SHM_H
#define JK_SHM_H

#include "jk_global.h"
#include "jk_env.h"
#include "jk_logger.h"
#include "jk_pool.h"
#include "jk_msg.h"
#include "jk_service.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

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
    struct jk_shm_slot
    {
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


    struct jk_shm_head
    {

        int structSize;

        int slotSize;

        int slotMaxCount;

        /* Last used position. Increase ( at least atomically, eventually with mutexes )
           each time a slot is created */
        int lastSlot;

        /* XXX Need a more generic mechanism */
        int lbVer;

        /* Array of used slots set to nonzero if used */
        char slots[1];

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
    struct jk_shm
    {
        struct jk_bean *mbean;

        struct jk_pool *pool;

        char *fname;

    /** Initialize the shared memory area. It'll map the shared memory 
     *  segment if it exists, or create and init it if not.
     */
        int (JK_METHOD * init) (struct jk_env * env, struct jk_shm * shm);

    /** Reset the shm area
     */
        int (JK_METHOD * reset) (struct jk_env * env, struct jk_shm * shm);


    /** Detach from the shared memory segment
     */
        int (JK_METHOD * destroy) (struct jk_env * env, struct jk_shm * shm);

    /** Get a shm slot. Each slot has different rules for synchronization, based on type. 
     */
        struct jk_shm_slot *(JK_METHOD * getSlot) (struct jk_env * env,
                                                   struct jk_shm * shm,
                                                   int pos);

    /** Create a slot. This typically involves inter-process synchronization.
     */
        struct jk_shm_slot *(JK_METHOD * createSlot) (struct jk_env * env,
                                                      struct jk_shm * shm,
                                                      char *name, int size);

        int size;

        int slotSize;

        int slotMaxCount;

        struct jk_shm_head *head;

        /* Memory image (the data after head) */
        void *image;

        /* Is the shmem attached or created */
        int attached;

        /* Use the main memory instead */
        int inmem;
        /* Private data (apr_shm_t) */
        void *privateData;
    };

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif
