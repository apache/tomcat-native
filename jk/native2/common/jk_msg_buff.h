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

/***************************************************************************
 * Description: Data marshaling. XDR like                                  *
 * Author:      Costin <costin@costin.dnt.ro>                              *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_MSG_BUF_H
#define JK_MSG_BUF_H

#include "jk_pool.h"
#include "jk_logger.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DEF_BUFFER_SZ (8 * 1024)

/* XXX replace all return values with error codes */
#define ERR_BAD_PACKET -5

/*
RPC details:

  - one parameter  - use a structure for more. The method
    is encoded as part of the request
  - one or no result
  - 



 */

struct jk_msg_buf;
typedef struct jk_msg_buf jk_msg_buf_t;

/* -------------------- Setup routines -------------------- */

/** Allocate a buffer.
 */
jk_msg_buf_t *jk_b_new(jk_pool_t *p); 

/** Set up a buffer with an existing buffer
 */
int jk_b_set_buffer(jk_msg_buf_t *msg, 
                    char *data, 
                    int buffSize );

/*
 * Set up a buffer with a new buffer of buffSize
 */
int jk_b_set_buffer_size(jk_msg_buf_t *msg, 
                         int buffSize);

/*
 * Finalize the buffer before sending - set length fields, etc
 */
void jk_b_end(jk_msg_buf_t *msg,
			  int protoh);

/*
 * Recycle the buffer - z for a new invocation 
 */
void jk_b_reset(jk_msg_buf_t *msg );

/*
 * Return the buffer body 
 */ 
unsigned char *jk_b_get_buff(jk_msg_buf_t *msg);

/* 
 * Return the current reading position
 */
unsigned int jk_b_get_pos(jk_msg_buf_t *msg);

/*
 * Buffer size 
 */
int jk_b_get_size(jk_msg_buf_t *msg);

void jk_b_set_len(jk_msg_buf_t *msg, 
                  int len);

void jk_b_set_pos(jk_msg_buf_t *msg, 
                  int pos);

/*
 * Get the  message length for incomming buffers
 *   or the current length for outgoing
 */
unsigned int jk_b_get_len(jk_msg_buf_t *msg);

/*
 * Dump the buffer header
 *   @param err Message text
 */
void jk_b_dump(jk_msg_buf_t *msg, 
               char *err); 

/* -------------------- Real encoding -------------------- */

int jk_b_append_byte(jk_msg_buf_t *msg, 
                     unsigned char val);

int jk_b_append_bytes(jk_msg_buf_t *        msg, 
                      const unsigned char * param,
                      int                   len);

int jk_b_append_int(jk_msg_buf_t *msg, 
                    unsigned short val);

int jk_b_append_long(jk_msg_buf_t *msg,
                     unsigned long val);

int jk_b_append_string(jk_msg_buf_t *msg, 
                       const char *param);


/* -------------------- Decoding -------------------- */

/** Get a byte from the current position 
 */
unsigned char jk_b_get_byte(jk_msg_buf_t *msg);

/** Get an int from the current position
 */
unsigned short jk_b_get_int(jk_msg_buf_t *msg);

/** Get a long from the current position
 */
unsigned long jk_b_get_long(jk_msg_buf_t *msg);

/** Get a String from the current position
 */
unsigned char *jk_b_get_string(jk_msg_buf_t *msg);

/** Get Bytes from the current position
 */
int jk_b_get_bytes(jk_msg_buf_t *msg, 
                   unsigned char * buf, 
                   int len);

/** Get a byte from an arbitrary position
 */
unsigned char jk_b_pget_byte(jk_msg_buf_t *msg,
                             int pos);

/** Get an int from an arbitrary position 
 */
unsigned short jk_b_pget_int(jk_msg_buf_t *msg, 
                             int pos);

/** Get a long from an arbitrary position 
 */
unsigned long jk_b_pget_long(jk_msg_buf_t *msg, 
                             int pos);

/* --------------------- Help ------------------------ */
void jk_dump_buff(jk_logger_t *l, 
                  const char *file,
                  int line,
                  int level,
                  char * what,
                  jk_msg_buf_t * msg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_MSG_BUF_H */
