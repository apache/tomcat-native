/* Copyright 1999-2004 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ajp.h"

apr_status_t ajp_msg_check_header(ajp_env_t *env, ajp_msg_t * msg)
{
    apr_byte_t *head = msg->buf;
    apr_size_t msglen;

    if (!((head[0] == 0x41 && head[1] == 0x42) ||
          (head[0] == 0x12 && head[1] == 0x34))) {
        
        fprintf(stderr, 
                "ajp_check_msg_header() got bad signature %x%x\n",
                head[0], head[1]);

        return -1;
    }

    msglen  = ((head[2] & 0xff) << 8);
    msglen += (head[3] & 0xFF);

    if (msglen > AJP_MSG_BUFFER_SZ) {
        fprintf(stderr, 
                "ajp_check_msg_header() incoming message is too big %d, max is %d\n",
                 msglen, AJP_MSG_BUFFER_SZ);
        return -2;
    }

    msg->len = msglen + AJP_HEADER_LEN;
    msg->pos = AJP_HEADER_LEN;

    return 0;
}

void ajp_msg_reset(ajp_env_t *env, ajp_msg_t * msg)
{
    msg->len = AJP_HEADER_LEN;
    msg->pos = AJP_HEADER_LEN;
}

void ajp_msg_end(ajp_env_t *env, ajp_msg_t * msg)
{
    apr_size_t len = msg->len - AJP_HEADER_LEN;

    if (msg->serverSide) {
        msg->buf[0] = 0x41;
        msg->buf[1] = 0x42;
    }
    else {
        msg->buf[0] = 0x12;
        msg->buf[1] = 0x34;
    }

    msg->buf[2] = (apr_byte_t)((len >> 8) & 0xFF);
    msg->buf[3] = (apr_byte_t)(len & 0xFF);
}

apr_status_t ajp_msg_append_uint32(ajp_env_t *env, ajp_msg_t *msg,
                                   const apr_uint32_t val)
{
    apr_size_t len = msg->len;

    if (len + AJP_HEADER_LEN > AJP_MSG_BUFFER_SZ) {
        return -1;
    }

    msg->buf[len]     = (apr_byte_t)((val >> 24) & 0xFF);
    msg->buf[len + 1] = (apr_byte_t)((val >> 16) & 0xFF);
    msg->buf[len + 2] = (apr_byte_t)((val >> 8) & 0xFF);
    msg->buf[len + 3] = (apr_byte_t)(val & 0xFF);

    msg->len += 4;

    return 0;
}

apr_status_t ajp_msg_append_uint16(ajp_env_t *env, ajp_msg_t *msg,
                                   const apr_uint16_t val)
{
    apr_size_t len = msg->len;

    if (len + 2 > AJP_MSG_BUFFER_SZ) {
        return -1;
    }

    msg->buf[len]     = (apr_byte_t)((val >> 8) & 0xFF);
    msg->buf[len + 1] = (apr_byte_t)(val & 0xFF);

    msg->len += 2;

    return 0;
}

apr_status_t ajp_msg_append_byte(ajp_env_t *env, ajp_msg_t *msg,
                                 const apr_byte_t val)
{
    apr_size_t len = msg->len;

    if ((len + 1) > AJP_MSG_BUFFER_SZ) {
        return -1;
    }

    msg->buf[len] = val;
    msg->len += 1;

    return 0;
}

apr_status_t ajp_msg_append_cvt_string(ajp_env_t *env, ajp_msg_t *msg,
                                       const char *param, int convert)
{
    apr_size_t len;

    if (param == NULL) {
        return(ajp_msg_append_int(env, msg, 0xFFFF));
    }

    len = strlen(param);
    if (msg->len + len + 2 > AJP_MSG_BUFFER_SZ) {
        return -1;
    }

    /* ignore error - we checked once */
    ajp_msg_append_int(env, msg, (unsigned short)len);

    /* We checked for space !!  */
    strncpy((char *)msg->buf + msg->len, param, len + 1);          /* including \0 */

    if (convert)
        ajp_xlate_to_ascii((char *)msg->buf + msg->len, len + 1);  /* convert from EBCDIC if needed */

    msg->len += len + 1;

    return 0;
}

apr_status_t ajp_msg_append_string(ajp_env_t *env, ajp_msg_t *msg,
                                   const char *param)
{
    return ajp_msg_append_cvt_string(env, msg, param, 1);
}


apr_status_t ajp_msg_append_bytes(ajp_env_t *env, ajp_msg_t *msg,
                                  const apr_byte_t *param, const apr_size_t len)
{
    if (!len) {
        return 0;
    }

    if ((msg->len + len) > AJP_MSG_BUFFER_SZ) {
        return -1;
    }

    /* We checked for space !!  */
    memcpy((char *)msg->buf + msg->len, param, len);
    msg->len += len;

    return 0;
}

apr_uint32_t ajp_msg_get_uint32(ajp_env_t *env, ajp_msg_t *msg)
{
    apr_uint32_t i;

    if ((msg->pos + 3) > msg->len) {
        fprintf(stderr, 
                "ajp_msg_get_long(): BufferOverflowException %d %d\n",
                msg->pos, msg->len);

        return -1;
    }

    i  = ((msg->buf[(msg->pos++)] & 0xFF) << 24);
    i |= ((msg->buf[(msg->pos++)] & 0xFF) << 16);
    i |= ((msg->buf[(msg->pos++)] & 0xFF) << 8);
    i |= ((msg->buf[(msg->pos++)] & 0xFF));
    return i;
}

apr_uint16_t ajp_msg_get_uint16(ajp_env_t *env, ajp_msg_t *msg)
{
    apr_uint16_t i;
    
    if (msg->pos + 1 > msg->len) {
        fprintf(stderr, 
                "ajp_msg_get_int(): BufferOverflowException %d %d\n",
                msg->pos, msg->len);

        return -1;
    }

    i  = ((msg->buf[(msg->pos++)] & 0xFF) << 8);
    i += ((msg->buf[(msg->pos++)] & 0xFF));

    return i;
}

apr_uint16_t ajp_msg_peek_int(ajp_env_t *env, ajp_msg_t *msg)
{
    apr_uint16_t i;

    if (msg->pos + 1 > msg->len) {
        fprintf(stderr, 
                "ajp_msg_peek_int(): BufferOverflowException %d %d\n",
                msg->pos, msg->len);

        return -1;
    }
    i = ((msg->buf[(msg->pos)] & 0xFF) << 8);
    i += ((msg->buf[(msg->pos + 1)] & 0xFF));
    return i;
}

apr_byte_t ajp_msg_get_byte(ajp_env_t *env, ajp_msg_t *msg)
{
    apr_byte_t rc;

    if (msg->pos > msg->len) {
        fprintf(stderr, 
                "ajp_msg_get_byte(): BufferOverflowException %d %d\n",
                msg->pos, msg->len);

        return -1;
    }
    rc = msg->buf[msg->pos++];

    return rc;
}

char * ajp_msg_get_string(ajp_env_t *env, ajp_msg_t *msg)
{
    apr_size_t size  = (apr_size_t)ajp_msg_get_uint16(env, msg);
    apr_size_t start = msg->pos;

    if ((size < 0) || (size + start > AJP_MSG_BUFFER_SZ)) {
        fprintf(stderr, 
                "ajp_msg_get_string(): BufferOverflowException %d %d\n",
                msg->pos, msg->len);

        return (unsigned char *)"ERROR";        /* XXX */
    }

    msg->pos += size;
    msg->pos++;                 /* terminating NULL */

    return (unsigned char *)(msg->buf + start);
}

apr_byte_t *ajp_msg_get_bytes(ajp_env_t *env, ajp_msg_t *msg, apr_size_t *lenP)
{
    apr_size_t size = (apr_size_t)ajp_msg_get_uint16(env, msg);
    apr_size_t start = msg->pos;

    *lenP = size;

    if ((size < 0) || (size + start > AJP_MSG_BUFFER_SZ)) {
        fprintf(stderr, 
                "ajp_msg_get_bytes(): BufferOverflowException %d %d\n",
                msg->pos, msg->len);

        return (unsigned char *)"ERROR";        /* XXX */
    }

    msg->pos += size;
    msg->pos++;                 /* terminating NULL */

    return (apr_byte_t *)(msg->buf + start);
}

ajp_msg_t *ajp_msg_create(ajp_env_t *env)
{
    ajp_msg_t *msg = (ajp_msg_t *)apr_pcalloc(env->pool, sizeof(ajp_msg_t));

    if (!msg)
        return NULL;

    msg->serverSide = 0;

    msg->buf = (apr_byte_t *)apr_palloc(env->pool, AJP_MSG_BUFFER_SZ);

    if (msg->buf == NULL) {
        return NULL;
    }

    msg->len = 0;
    msg->headerLen = AJP_HEADER_LEN;

    return msg;
}

apr_status_t ajp_msg_copy(ajp_env_t *env, ajp_msg_t *msg, ajp_msg_t *dmsg)
{
    if (dmsg == NULL)
        return -1;

    if (msg->len > AJP_MSG_BUFFER_SZ) {
        fprintf(stderr, 
                "ajp_msg_copy(): destination buffer too small %d/%d\n",
                msg->len, AJP_MSG_BUFFER_SZ);
        return -2;
    }

    memcpy(dmsg->buf, msg->buf, msg->len);
    dmsg->len = msg->len;
    dmsg->pos = msg->pos;

    return dmsg->len;
}
