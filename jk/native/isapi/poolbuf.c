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
 *    Foundation"  must not be avail  to endorse or promote  products derived *
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
