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
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_connect.h"
#include "jk_util.h"
#include "jk_sockbuf.h"
#include "jk_msg_buff.h"
#include "jk_logger.h"

struct jk_msg_buf_t
{
    jk_pool_t *pool;

    unsigned char *buf;
    int pos;
    int len;
    int maxlen;
};

static char *jk_HEX = "0123456789ABCDEFX";

/*
 * Simple marshaling code.
 */

void jk_b_reset(jk_msg_buf_t *msg)
{
    msg->len = 4;
    msg->pos = 4;
}

int jk_b_append_long(jk_msg_buf_t *msg, unsigned long val)
{
    if (msg->len + 4 > msg->maxlen) {
        return -1;
    }

    msg->buf[msg->len++] = (unsigned char)((val >> 24) & 0xFF);
    msg->buf[msg->len++] = (unsigned char)((val >> 16) & 0xFF);
    msg->buf[msg->len++] = (unsigned char)((val >> 8) & 0xFF);
    msg->buf[msg->len++] = (unsigned char)((val) & 0xFF);

    return 0;
}


int jk_b_append_int(jk_msg_buf_t *msg, unsigned short val)
{
    if (msg->len + 2 > msg->maxlen) {
        return -1;
    }

    msg->buf[msg->len++] = (unsigned char)((val >> 8) & 0xFF);
    msg->buf[msg->len++] = (unsigned char)((val) & 0xFF);

    return 0;
}


int jk_b_append_byte(jk_msg_buf_t *msg, unsigned char val)
{
    if (msg->len + 1 > msg->maxlen) {
        return -1;
    }

    msg->buf[msg->len++] = val;

    return 0;
}


void jk_b_end(jk_msg_buf_t *msg, int protoh)
{
    /* 
     * Ugly way to set the size in the right position 
     */
    int hlen = msg->len - 4;

    msg->buf[0] = (unsigned char)((protoh >> 8) & 0xFF);
    msg->buf[1] = (unsigned char)((protoh) & 0xFF);
    msg->buf[2] = (unsigned char)((hlen >> 8) & 0xFF);
    msg->buf[3] = (unsigned char)((hlen) & 0xFF);

}


jk_msg_buf_t *jk_b_new(jk_pool_t *p)
{
    jk_msg_buf_t *msg =
        (jk_msg_buf_t *)jk_pool_alloc(p, sizeof(jk_msg_buf_t));

    if (!msg) {
        return NULL;
    }

    msg->pool = p;

    return msg;
}

int jk_b_set_buffer(jk_msg_buf_t *msg, unsigned char *data, int buffSize)
{
    if (!msg) {
        return -1;
    }

    msg->len = 0;
    msg->buf = data;
    msg->maxlen = buffSize;

    return 0;
}


int jk_b_set_buffer_size(jk_msg_buf_t *msg, int buffSize)
{
    unsigned char *data = (unsigned char *)jk_pool_alloc(msg->pool, buffSize);

    if (!data) {
        return -1;
    }

    jk_b_set_buffer(msg, data, buffSize);
    return 0;
}

unsigned char *jk_b_get_buff(jk_msg_buf_t *msg)
{
    return msg->buf;
}

unsigned int jk_b_get_pos(jk_msg_buf_t *msg)
{
    return msg->pos;
}

void jk_b_set_pos(jk_msg_buf_t *msg, int pos)
{
    msg->pos = pos;
}

unsigned int jk_b_get_len(jk_msg_buf_t *msg)
{
    return msg->len;
}

void jk_b_set_len(jk_msg_buf_t *msg, int len)
{
    msg->len = len;
}

int jk_b_get_size(jk_msg_buf_t *msg)
{
    return msg->maxlen;
}

#ifdef AS400
int jk_b_append_asciistring(jk_msg_buf_t *msg, const char *param)
{
    int len;

    if (!param) {
        jk_b_append_int(msg, 0xFFFF);
        return 0;
    }

    len = strlen(param);
    if (msg->len + len + 2 > msg->maxlen) {
        return -1;
    }

    /* ignore error - we checked once */
    jk_b_append_int(msg, (unsigned short)len);

    /* We checked for space !!  */
    strncpy((char *)msg->buf + msg->len, param, len + 1);       /* including \0 */
    msg->len += len + 1;

    return 0;
}
#endif

int jk_b_append_string(jk_msg_buf_t *msg, const char *param)
{
    unsigned short len;

    if (!param) {
        jk_b_append_int(msg, 0xFFFF);
        return 0;
    }

    len = (unsigned short)strlen(param);
    if (msg->len + len + 2 > msg->maxlen) {
        return -1;
    }

    /* ignore error - we checked once */
    jk_b_append_int(msg, len);

    /* We checked for space !!  */
    strncpy((char *)msg->buf + msg->len, param, len + 1);       /* including \0 */
#if defined(AS400) || defined(_OSD_POSIX)
    /* convert from EBCDIC if needed */
    jk_xlate_to_ascii((char *)msg->buf + msg->len, len + 1);
#endif
    msg->len += len + 1;

    return 0;
}


int jk_b_append_bytes(jk_msg_buf_t *msg, const unsigned char *param, int len)
{
    if (!len) {
        return 0;
    }

    if (msg->len + len > msg->maxlen) {
        return -1;
    }

    /* We checked for space !!  */
    memcpy((char *)msg->buf + msg->len, param, len);
    msg->len += len;

    return 0;
}

unsigned long jk_b_get_long(jk_msg_buf_t *msg)
{
    unsigned long i;
    if (msg->pos + 3 > msg->len) {
        return 0xFFFFFFFF;
    }
    i = ((msg->buf[(msg->pos++)] & 0xFF) << 24);
    i |= ((msg->buf[(msg->pos++)] & 0xFF) << 16);
    i |= ((msg->buf[(msg->pos++)] & 0xFF) << 8);
    i |= ((msg->buf[(msg->pos++)] & 0xFF));
    return i;
}

unsigned long jk_b_pget_long(jk_msg_buf_t *msg, int pos)
{
    unsigned long i;
    i = ((msg->buf[(pos++)] & 0xFF) << 24);
    i |= ((msg->buf[(pos++)] & 0xFF) << 16);
    i |= ((msg->buf[(pos++)] & 0xFF) << 8);
    i |= ((msg->buf[(pos)] & 0xFF));
    return i;
}


unsigned short jk_b_get_int(jk_msg_buf_t *msg)
{
    unsigned short i;
    if (msg->pos + 1 > msg->len) {
        return 0xFFFF;
    }
    i = ((msg->buf[(msg->pos++)] & 0xFF) << 8);
    i += ((msg->buf[(msg->pos++)] & 0xFF));
    return i;
}

unsigned short jk_b_pget_int(jk_msg_buf_t *msg, int pos)
{
    unsigned short i;
    i = ((msg->buf[pos++] & 0xFF) << 8);
    i += ((msg->buf[pos] & 0xFF));
    return i;
}

unsigned char jk_b_get_byte(jk_msg_buf_t *msg)
{
    unsigned char rc;
    if (msg->pos > msg->len) {
        return 0xFF;
    }
    rc = msg->buf[msg->pos++];

    return rc;
}

unsigned char jk_b_pget_byte(jk_msg_buf_t *msg, int pos)
{
    return msg->buf[pos];
}


unsigned char *jk_b_get_string(jk_msg_buf_t *msg)
{
    unsigned short size = jk_b_get_int(msg);
    int start = msg->pos;

    if ((size == 0xFFFF) || (size + start > msg->maxlen)) {
        /* TODO: return NULL and deal with that in the code */
        return (unsigned char *)"ERROR";
    }

    msg->pos += size;
    msg->pos++;                 /* terminating NULL */

    return (unsigned char *)(msg->buf + start);
}

int jk_b_get_bytes(jk_msg_buf_t *msg, unsigned char *buf, int len)
{
    int start = msg->pos;

    if ((len < 0) || (len + start > msg->maxlen)) {
        return (-1);
    }

    memcpy(buf, msg->buf + start, len);
    msg->pos += len;
    return (len);
}



/** Helpie dump function 
 */
void jk_dump_buff(jk_logger_t *l,
                  const char *file,
                  int line, const char *funcname,
                  int level, char *what, jk_msg_buf_t *msg)
{
    int i = 0;
    char lb[80];
    char *current;
    int j;
    int len = msg->len;
    
    if (l == NULL)
        return;
    if (level == JK_LOG_DEBUG_LEVEL) {
        len = 0;
        if (l->level == JK_LOG_TRACE_LEVEL)
            len = msg->len;
    }
    
    if (len > 1024)
        len = 1024;

    jk_log(l, file, line, funcname, level,
           "%s pos=%d len=%d max=%d \n",
           what, msg->pos, msg->len, msg->maxlen);

    for (i = 0; i < len; i += 16) {
        current = &lb[0];

        for (j = 0; j < 16; j++) {
            unsigned char x = (msg->buf[i + j]);

            *current++ = jk_HEX[x >> 4];
            *current++ = jk_HEX[x & 0x0f];
            *current++ = ' ';
        }
        *current++ = ' ';
        *current++ = '-';
        *current++ = ' ';
        for (j = 0; j < 16; j++) {
            unsigned char x = msg->buf[i + j];

            if (x > 0x20 && x < 0x7F) {
                *current++ = x;
            }
            else {
                *current++ = '.';
            }
        }
        *current++ = '\n';
        *current++ = '\0';
            jk_log(l, file, line, funcname, level,
                   "%.4x    %s", i, lb);
    }
}


int jk_b_copy(jk_msg_buf_t *smsg, jk_msg_buf_t *dmsg)
{
    if (smsg == NULL || dmsg == NULL)
        return (-1);

    if (dmsg->maxlen < smsg->len)
        return (-2);

    memcpy(dmsg->buf, smsg->buf, smsg->len);
    dmsg->len = smsg->len;

    return (smsg->len);
}
