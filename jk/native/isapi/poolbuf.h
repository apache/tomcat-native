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
 * Description: ISAPI plugin for Tomcat                                    *
 * Author:      Andy Armstrong <andy@tagish.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef __poolbuf_h
#define __poolbuf_h

#include <stddef.h>
#include "jk_pool.h"

#define poolbuf__MINCHUNK 2048

typedef struct poolbuf__chunk
{
    size_t size;
    struct poolbuf__chunk *next;

} poolbuf__chunk;

typedef enum
{ WRITE, READ } poolbuf__state;

typedef struct
{
    jk_pool_t *p;
    poolbuf__chunk *head;
    poolbuf__chunk *current;

    /* current state */
    poolbuf__state state;

    /* total number of bytes available to read */
    size_t avail;

    /* offsets within the current chunk */
    unsigned int writePos;
    unsigned int readPos;

} poolbuf;

/* Initialise a poolbuf. */
void poolbuf_init(poolbuf * pb, jk_pool_t *p);
size_t poolbuf_write(poolbuf * pb, const void *buf, size_t size);
size_t poolbuf_read(poolbuf * pb, void *buf, size_t size);
size_t poolbuf_available(poolbuf * pb);
void poolbuf_destroy(poolbuf * pb);


#endif /* __poolbuf_h */
