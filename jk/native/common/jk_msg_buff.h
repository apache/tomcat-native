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
 * Description: Data marshaling. XDR like                                  *
 * Author:      Costin <costin@costin.dnt.ro>                              *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_MSG_BUF_H
#define JK_MSG_BUF_H


#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

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

typedef struct jk_msg_buf_t jk_msg_buf_t;

/* -------------------- Setup routines -------------------- */

/** Allocate a buffer.
 */
jk_msg_buf_t *jk_b_new(jk_pool_t *p);

/** Set up a buffer with an existing buffer
 */
int jk_b_set_buffer(jk_msg_buf_t *msg, unsigned char *data, int buffSize);

/*
 * Set up a buffer with a new buffer of buffSize
 */
int jk_b_set_buffer_size(jk_msg_buf_t *msg, int buffSize);

/*
 * Finalize the buffer before sending - set length fields, etc
 */
void jk_b_end(jk_msg_buf_t *msg, int protoh);

/*
 * Recycle the buffer - z for a new invocation 
 */
void jk_b_reset(jk_msg_buf_t *msg);

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

void jk_b_set_len(jk_msg_buf_t *msg, int len);

void jk_b_set_pos(jk_msg_buf_t *msg, int pos);

/*
 * Get the  message length for incomming buffers
 *   or the current length for outgoing
 */
unsigned int jk_b_get_len(jk_msg_buf_t *msg);


/* -------------------- Real encoding -------------------- */

int jk_b_append_byte(jk_msg_buf_t *msg, unsigned char val);

int jk_b_append_bytes(jk_msg_buf_t *msg,
                      const unsigned char *param, int len);

int jk_b_append_int(jk_msg_buf_t *msg, unsigned short val);

int jk_b_append_long(jk_msg_buf_t *msg, unsigned long val);

int jk_b_append_string(jk_msg_buf_t *msg, const char *param);

#ifdef AS400
int jk_b_append_asciistring(jk_msg_buf_t *msg, const char *param);
#endif

int jk_b_append_bytes(jk_msg_buf_t *msg,
                      const unsigned char *param, int len);

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
int jk_b_get_bytes(jk_msg_buf_t *msg, unsigned char *buf, int len);

/** Get a byte from an arbitrary position
 */
unsigned char jk_b_pget_byte(jk_msg_buf_t *msg, int pos);

/** Get an int from an arbitrary position 
 */
unsigned short jk_b_pget_int(jk_msg_buf_t *msg, int pos);

/** Get a long from an arbitrary position 
 */
unsigned long jk_b_pget_long(jk_msg_buf_t *msg, int pos);

/* --------------------- Help ------------------------ */
void jk_dump_buff(jk_logger_t *l,
                  const char *file,
                  int line, const char *funcname,
                  int level, char *what, jk_msg_buf_t *msg);

/** Copy a msg buf into another one
  */
int jk_b_copy(jk_msg_buf_t *smsg, jk_msg_buf_t *dmsg);

#ifdef __cplusplus
}
#endif    /* __cplusplus */
#endif    /* JK_MSG_BUF_H */
