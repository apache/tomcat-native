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

#include "poolbuf.h"

/* Macro to return the address of the first byte in a poolbuf__chunk on
 * the understanding that the buffer follows the structure in memory.
 */
#define poolbuf__buf(chnk) \
	((char *) ((poolbuf__chunk *) chnk + 1))

void poolbuf_init(poolbuf *pb, jk_pool_t *p)
{
	pb->p			= p;
	pb->head		=
	pb->current		= NULL;
	pb->readPos		=
	pb->writePos	= 0;
	pb->avail		= 0;
	pb->state		= WRITE;
}

/* Write bytes to the buffer returning the number of bytes successfully
 * written. Can't be called again once poolbuf_read() has been called.
 */
size_t poolbuf_write(poolbuf *pb, const void *buf, size_t size)
{
	const char *cbuf = (const char *) buf;
	size_t left = size;

	if (READ == pb->state)
		return 0;

	/* first work out what we can write into the current buffer */
	if (pb->current != NULL && pb->writePos < pb->current->size)
	{
		char *chbuf = poolbuf__buf(pb->current) + pb->writePos;
		size_t sz = pb->current->size - pb->writePos;
		if (sz > left) sz = left;
		memcpy(chbuf, cbuf, sz);
		pb->writePos += sz;
		pb->avail += sz;
		cbuf += sz;
		left -= sz;
	}

	/* something left that we couldn't fit in the last chunk */
	if (left > 0)
	{
		poolbuf__chunk *chnk;
		size_t sz = size;

		if (sz < poolbuf__MINCHUNK)
			sz = poolbuf__MINCHUNK;
		if (NULL == pb->p || NULL == (chnk = jk_pool_alloc(pb->p, sz + sizeof(poolbuf__chunk))))
			return size - left;

		chnk->next = NULL;
		chnk->size = sz;
		if (NULL == pb->head) pb->head = chnk;
		if (NULL != pb->current) pb->current->next = chnk;
		pb->current = chnk;
		memcpy(poolbuf__buf(chnk), cbuf, left);
		pb->avail += left;
		pb->writePos = left;
	}

	return size;
}

/* Read bytes from the buffer returning the number of bytes read (which
 * will be less than desired when the end of the buffer is reached). Once
 * poolbuf_read() has been called poolbuf_write() may not be called again.
 */
size_t poolbuf_read(poolbuf *pb, void *buf, size_t size)
{
	char *cbuf = (char *) buf;
	size_t nread = 0;

	if (WRITE == pb->state)
	{
		/* Move to read mode. Once we've done this subsequent
		 * writes are not allowed.
		 */
		pb->current = pb->head;
		pb->readPos	= 0;
		pb->state	= READ;
	}

	while (size > 0 && pb->avail > 0)
	{
		size_t sz = pb->current->size - pb->readPos;
		if (sz > pb->avail) sz = pb->avail;
		if (sz > size) sz = size;
		memcpy(cbuf, poolbuf__buf(pb->current) + pb->readPos, sz);
		pb->readPos += sz;
		if (pb->readPos == pb->current->size)
		{
			pb->current = pb->current->next;
			pb->readPos = 0;
		}
		pb->avail -= sz;
		nread += sz;
	}

	return nread;
}

/* Find out how many bytes are available for reading.
 */
size_t poolbuf_available(poolbuf *pb)
{
	return pb->avail;
}

/* Destroy the buffer. This doesn't actually free any memory
 * because the jk_pool functions don't support freeing individual
 * chunks, but it does recycle the buffer for subsequent use.
 */
void poolbuf_destroy(poolbuf *pb)
{
	pb->p			= NULL;
	pb->head		=
	pb->current		= NULL;
	pb->readPos		=
	pb->writePos	= 0;
	pb->avail		= 0;
	pb->state		= WRITE;
}
