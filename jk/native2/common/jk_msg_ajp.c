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
 * Author:      Costin Manolache
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_msg.h"
#include "jk_logger.h"
#include "jk_endpoint.h"
#include "jk_channel.h"
#include "jk_requtil.h"

#define AJP13_WS_HEADER           0x1234
#define AJP_HEADER_LEN            (4)
#define AJP_HEADER_SZ_LEN         (2)

/*
 * Simple marshaling code.
 */
static void jk2_msg_ajp_dump(jk_env_t *env, struct jk_msg *_this,
                             char *err)
{
    int i=0;
    env->l->jkLog( env, env->l, JK_LOG_INFO,
                   "%s pos=%d len=%d max=%d \n",
                err, _this->pos, _this->len, _this->maxlen );
    
    env->l->jkLog( env, env->l, JK_LOG_INFO,
                "%2x %2x %2x %2x:%2x %2x %2x %2x:%2x %2x %2x %2x:%2x %2x %2x %2x \n", 
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++]);
    env->l->jkLog( env, env->l, JK_LOG_INFO,
                "%2x %2x %2x %2x:%2x %2x %2x %2x:%2x %2x %2x %2x:%2x %2x %2x %2x \n", 
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++]);
}

 
static void jk2_msg_ajp_reset(jk_env_t *env, jk_msg_t *_this) 
{
    _this->len = 4;
    _this->pos = 4;
}

static void jk2_msg_ajp_end(jk_env_t *env, jk_msg_t *msg) 
{
    unsigned short len=msg->len - 4;
    
    msg->buf[0]=0x12;
    msg->buf[1]=0x34;

    msg->buf[2]=(unsigned char)((len >> 8) & 0xFF);
    msg->buf[3]=(unsigned char)(len & 0xFF);
}



static int jk2_msg_ajp_appendLong(jk_env_t *env, jk_msg_t *msg,
                                  unsigned long val)
{
    int len=msg->len;
    
    if(len + 4 > msg->maxlen) {
        return JK_ERR;
    }
    
    msg->buf[len]       = (unsigned char)((val >> 24) & 0xFF);
    msg->buf[len + 1]   = (unsigned char)((val >> 16) & 0xFF);
    msg->buf[len + 2]   = (unsigned char)((val >> 8) & 0xFF);
    msg->buf[len + 3]   = (unsigned char)(val & 0xFF);

    msg->len += 4;

    return JK_OK;
}


static int jk2_msg_ajp_appendInt(jk_env_t *env, jk_msg_t *msg, 
                                 unsigned short val) 
{
    int len=msg->len;
    if(len + 2 > msg->maxlen) {
        return JK_ERR;
    }

    msg->buf[len]       = (unsigned char)((val >> 8) & 0xFF);
    msg->buf[len + 1]   = (unsigned char)(val & 0xFF);

    msg->len += 2;

    return JK_OK;
}

static int jk2_msg_ajp_appendByte(jk_env_t *env, jk_msg_t *msg, 
                                  unsigned char val)
{
    int len=msg->len;
    if(len + 1 > msg->maxlen) {
	    return JK_ERR;
    }

    msg->buf[len]= val;
    msg->len += 1;

    return JK_OK;
}

static int jk2_msg_ajp_appendString(jk_env_t *env, jk_msg_t *msg, 
                                    const char *param) 
{
    int len;

    if(!param) {
         msg->appendInt( env, msg, 0xFFFF );
        return JK_OK; 
    }

    len = strlen(param);
    if(msg->len + len + 2  > msg->maxlen) {
	    return JK_ERR;
    }

    /* ignore error - we checked once */
    msg->appendInt(env, msg, (unsigned short )len);

    /* We checked for space !!  */
    strncpy((char *)msg->buf + msg->len , param, len+1);    /* including \0 */
    jk_xlate_to_ascii((char *)msg->buf + msg->len, len+1);  /* convert from EBCDIC if needed */
    msg->len += len + 1;

    return JK_OK;
}


static int jk2_msg_ajp_appendBytes(jk_env_t *env, jk_msg_t  *msg,
                                   const unsigned char  *param,
                                   int len)
{
    if (! len) {
        return JK_OK;
    }

    if (msg->len + len > msg->maxlen) {
        return JK_ERR;
    }

    /* We checked for space !!  */
    memcpy((char *)msg->buf + msg->len, param, len); 
    msg->len += len;

    return JK_OK;
}

static unsigned long jk2_msg_ajp_getLong(jk_env_t *env, jk_msg_t *msg)
{
    unsigned long i;
    
    if(msg->pos + 3 > msg->len) {
        env->l->jkLog( env, env->l, JK_LOG_ERROR,
                       "msg_ajp.getLong(): BufferOverflowException %d %d\n",
                       msg->pos, msg->len);
        msg->dump(env, msg, "BUFFER OVERFLOW");
        return -1;
    }
    i  = ((msg->buf[(msg->pos++)] & 0xFF)<<24);
    i |= ((msg->buf[(msg->pos++)] & 0xFF)<<16);
    i |= ((msg->buf[(msg->pos++)] & 0xFF)<<8);
    i |= ((msg->buf[(msg->pos++)] & 0xFF));
    return i;
}

static unsigned short jk2_msg_ajp_getInt(jk_env_t *env, jk_msg_t *msg) 
{
    int i;
    if(msg->pos + 1 > msg->len) {
        env->l->jkLog( env, env->l, JK_LOG_ERROR,
                       "msg_ajp.geInt(): BufferOverflowException %d %d\n",
                       msg->pos, msg->len);
        msg->dump(env, msg, "BUFFER OVERFLOW");
        return -1;
    }
    i  = ((msg->buf[(msg->pos++)] & 0xFF)<<8);
    i += ((msg->buf[(msg->pos++)] & 0xFF));
    return i;
}

static unsigned short jk2_msg_ajp_peekInt(jk_env_t *env, jk_msg_t *msg) 
{
    int i;
    if(msg->pos + 1 > msg->len) {
        env->l->jkLog( env, env->l, JK_LOG_ERROR,
                       "msg_ajp.peekInt(): BufferOverflowException %d %d\n",
                       msg->pos, msg->len);
        msg->dump(env, msg, "BUFFER OVERFLOW");
        return -1;
    }
    i  = ((msg->buf[(msg->pos)] & 0xFF)<<8);
    i += ((msg->buf[(msg->pos+1)] & 0xFF));
    return i;
}

static unsigned char jk2_msg_ajp_getByte(jk_env_t *env, jk_msg_t *msg) 
{
    unsigned char rc;
    if(msg->pos > msg->len) {
        env->l->jkLog( env, env->l, JK_LOG_ERROR,
                       "msg_ajp.getByte(): BufferOverflowException %d %d\n",
                       msg->pos, msg->len);
        msg->dump(env, msg, "BUFFER OVERFLOW");        
        return -1;
    }
    rc = msg->buf[msg->pos++];
    
    return rc;
}

static unsigned char *jk2_msg_ajp_getString(jk_env_t *env, jk_msg_t *msg) 
{
    int size = jk2_msg_ajp_getInt(env, msg);
    int start = msg->pos;

    if((size < 0 ) || (size + start > msg->maxlen)) { 
        env->l->jkLog( env, env->l, JK_LOG_ERROR,
                       "msg_ajp.getString(): BufferOverflowException %d %d\n",
                       msg->pos, msg->len);
        msg->dump(env, msg, "BUFFER OVERFLOW");
        return (unsigned char *)"ERROR"; /* XXX */
    }

    msg->pos += size;
    msg->pos++;  /* terminating NULL */
    
    return (unsigned char *)(msg->buf + start); 
}

static unsigned char *jk2_msg_ajp_getBytes(jk_env_t *env, jk_msg_t *msg, int *lenP) 
{
    int size = jk2_msg_ajp_getInt(env, msg);
    int start = msg->pos;

    *lenP=size;
    
    if((size < 0 ) || (size + start > msg->maxlen)) { 
        env->l->jkLog( env, env->l, JK_LOG_ERROR,
                       "msg_ajp.getBytes(): BufferOverflowException %d %d\n",
                       msg->pos, msg->len);
        msg->dump(env, msg, "BUFFER OVERFLOW");
        return (unsigned char *)"ERROR"; /* XXX */
    }

    msg->pos += size;
    msg->pos++;  /* terminating NULL */
    
    return (unsigned char *)(msg->buf + start); 
}


/** Shame-less copy from somewhere.
    assert (src != dst)
 */
static void jk2_swap_16(unsigned char *src, unsigned char *dst) 
{
    *dst++ = *(src + 1 );
    *dst= *src;
}

/** Process the request header. At least the header must be
    available - the channel may get more data it it can, to
    avoid multiple system calls.
*/
static int jk2_msg_ajp_checkHeader(jk_env_t *env, jk_msg_t *msg,
                                   jk_endpoint_t *ae )
{
    char *head=msg->buf;
    int msglen;

    if( head[0] != 0x41 || head[1] != 0x42 ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "msgAjp.receive(): Bad signature %x%x\n",
                      head[0], head[1]);
        return -1;
    }

    msglen  = ((head[2]&0xff)<<8);
    msglen += (head[3] & 0xFF);

    if(msglen > msg->maxlen ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "msgAjp.receive(): Incoming message is too big %d\n",
                      msglen);
        return -2;
    }

    msg->len=msglen+4;
    msg->pos=4;

    return msglen;
}



/** 
 * Special method. Will read data from the server and add them as
 * bytes. It is equivalent with jk2_requtil_readFully() in a buffer
 * and then jk_msg_appendBytes(), except that we use directly the
 * internal buffer.
 *
 * Returns -1 on error, else number of bytes read
 */
static int jk2_msg_ajp_appendFromServer(jk_env_t *env, jk_msg_t    *msg,
                                       jk_ws_service_t *r,
                                       jk_endpoint_t  *ae,
                                       int            len )
{
    unsigned char *read_buf = msg->buf;

    jk2_msg_ajp_reset(env, msg);

    read_buf += AJP_HEADER_LEN;    /* leave some space for the buffer headers */
    read_buf += AJP_HEADER_SZ_LEN; /* leave some space for the read length */

    /* Pick the max size since we don't know the content_length */
    if (r->is_chunked && len == 0) {
	len = AJP13_MAX_SEND_BODY_SZ;
    }

    if ((len = jk2_requtil_readFully(env, r, read_buf, len)) < 0) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "msgAjp.appendFromServer() error reading from server\n");
        return -1;
    }

    if (!r->is_chunked) {
	r->left_bytes_to_send -= len;
    }

    if (len > 0) {
	/* Recipient recognizes empty packet as end of stream, not
	   an empty body packet */
        if(0 != jk2_msg_ajp_appendInt(env, msg, (unsigned short)len)) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                          "msg.appendFromServer(): appendInt failed\n");
            return -1;
	}
    }

    msg->len= msg->len + len;

    return len;
}


jk_msg_t *jk2_msg_ajp_create(jk_env_t *env, jk_pool_t *pool, int buffSize) 
{
    jk_msg_t *msg = 
        (jk_msg_t *)pool->calloc(env, pool, sizeof(jk_msg_t));

    if( buffSize==0 )
        buffSize=DEF_BUFFER_SZ;
    if(!msg) {
        return NULL;
    }
    msg->pool = pool;

    msg->buf= (unsigned char *)msg->pool->alloc(env, msg->pool, buffSize);
    
    if(msg->buf==NULL) {
        return NULL;
    }
    
    msg->maxlen=buffSize;
    msg->len=0;

    msg->headerLength=4;

    msg->reset=jk2_msg_ajp_reset;
    msg->end=jk2_msg_ajp_end;
    msg->dump=jk2_msg_ajp_dump;

    msg->appendByte=jk2_msg_ajp_appendByte;
    msg->appendBytes=jk2_msg_ajp_appendBytes;
    msg->appendInt=jk2_msg_ajp_appendInt;
    msg->appendLong=jk2_msg_ajp_appendLong;
    msg->appendString=jk2_msg_ajp_appendString;

    msg->appendFromServer=jk2_msg_ajp_appendFromServer;

    msg->getByte=jk2_msg_ajp_getByte;
    msg->getInt=jk2_msg_ajp_getInt;
    msg->peekInt=jk2_msg_ajp_peekInt;
    msg->getLong=jk2_msg_ajp_getLong;
    msg->getString=jk2_msg_ajp_getString;
    msg->getBytes=jk2_msg_ajp_getBytes;

    msg->checkHeader=jk2_msg_ajp_checkHeader;
                       
    return msg;
}

