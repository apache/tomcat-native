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

#define AJP13_WS_HEADER				0x1234
#define AJP_HEADER_LEN            (4)
#define AJP_HEADER_SZ_LEN         (2)

/*
 * Simple marshaling code.
 */
static void jk_msg_ajp_dump(struct jk_msg *_this, struct jk_logger *log,
                            char *err)
{
    int i=0;
    log->jkLog( log, JK_LOG_INFO,
                "%s pos=%d len=%d max=%d "
                "%x %x %x %x - %x %x %x %x -"
                "%x %x %x %x - %x %x %x %x\n",
                err, _this->pos, _this->len, _this->maxlen,  
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++]);
    
    i = _this->pos - 4;
    if(i < 0) {
        i=0;
    }
    
    log->jkLog( log, JK_LOG_INFO,
                "       %x %x %x %x - %x %x %x %x --- %x %x %x %x - %x %x %x %x\n", 
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++],
                _this->buf[i++],_this->buf[i++],_this->buf[i++],_this->buf[i++]);
}

 
static void jk_msg_ajp_reset(jk_msg_t *_this) 
{
    _this->len = 4;
    _this->pos = 4;
}

static void jk_msg_ajp_end(jk_msg_t *msg) 
{
    unsigned short len=msg->len - 4;
    
    msg->buf[0]=0x12;
    msg->buf[1]=0x34;

    msg->buf[2]=(unsigned char)((len >> 8) & 0xFF);
    msg->buf[3]=(unsigned char)(len & 0xFF);
}



static int jk_msg_ajp_appendLong(jk_msg_t *msg,
                                 unsigned long val)
{
    int len=msg->len;
    
    if(len + 4 > msg->maxlen) {
        return -1;
    }
    
    msg->buf[len]       = (unsigned char)((val >> 24) & 0xFF);
    msg->buf[len + 1]   = (unsigned char)((val >> 16) & 0xFF);
    msg->buf[len + 2]   = (unsigned char)((val >> 8) & 0xFF);
    msg->buf[len + 3]   = (unsigned char)(val & 0xFF);

    msg->len += 4;

    return 0;
}


static int jk_msg_ajp_appendInt(jk_msg_t *msg, 
                                unsigned short val) 
{
    int len=msg->len;
    if(len + 2 > msg->maxlen) {
        return -1;
    }

    msg->buf[len]       = (unsigned char)((val >> 8) & 0xFF);
    msg->buf[len + 1]   = (unsigned char)(val & 0xFF);

    msg->len += 2;

    return 0;
}

static int jk_msg_ajp_appendByte(jk_msg_t *msg, 
                                 unsigned char val)
{
    int len=msg->len;
    if(len + 1 > msg->maxlen) {
	    return -1;
    }

    msg->buf[len]= val;
    msg->len += 1;

    return 0;
}

static int jk_msg_ajp_appendString(jk_msg_t *msg, 
                                   const char *param) 
{
    int len;

    if(!param) {
        msg->appendInt( msg, 0xFFFF );
        return 0; 
    }

    len = strlen(param);
    if(msg->len + len + 2  > msg->maxlen) {
	    return -1;
    }

    /* ignore error - we checked once */
    msg->appendInt(msg, (unsigned short )len);

    /* We checked for space !!  */
    strncpy((char *)msg->buf + msg->len , param, len+1);    /* including \0 */
    jk_xlate_to_ascii((char *)msg->buf + msg->len, len+1);  /* convert from EBCDIC if needed */
    msg->len += len + 1;

    return 0;
}


static int jk_msg_ajp_appendBytes(jk_msg_t         *msg,
                                  const unsigned char  *param,
                                  int                   len)
{
    if (! len) {
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

static unsigned long jk_msg_ajp_getLong(jk_msg_t *msg)
{
    unsigned long i;
    
    if(msg->pos + 3 > msg->len) {
        msg->l->jkLog( msg->l, JK_LOG_ERROR,
                       "Error: try to get data past end of the buffer\n");
        return -1;
    }
    i  = ((msg->buf[(msg->pos++)] & 0xFF)<<24);
    i |= ((msg->buf[(msg->pos++)] & 0xFF)<<16);
    i |= ((msg->buf[(msg->pos++)] & 0xFF)<<8);
    i |= ((msg->buf[(msg->pos++)] & 0xFF));
    return i;
}

static unsigned short jk_msg_ajp_getInt(jk_msg_t *msg) 
{
    int i;
    if(msg->pos + 1 > msg->len) {
        msg->l->jkLog( msg->l, JK_LOG_ERROR,
                       "Error: try to get data past end of the buffer\n");
        return -1;
    }
    i  = ((msg->buf[(msg->pos++)] & 0xFF)<<8);
    i += ((msg->buf[(msg->pos++)] & 0xFF));
    return i;
}

static unsigned short jk_msg_ajp_peekInt(jk_msg_t *msg) 
{
    int i;
    if(msg->pos + 1 > msg->len) {
        msg->l->jkLog( msg->l, JK_LOG_ERROR,
                       "Error: try to get data past end of the buffer\n");
        return -1;
    }
    i  = ((msg->buf[(msg->pos)] & 0xFF)<<8);
    i += ((msg->buf[(msg->pos+1)] & 0xFF));
    return i;
}

static unsigned char jk_msg_ajp_getByte(jk_msg_t *msg) 
{
    unsigned char rc;
    if(msg->pos > msg->len) {
        msg->l->jkLog( msg->l, JK_LOG_ERROR,
                       "Error: try to get data past end of the buffer\n");
        return -1;
    }
    rc = msg->buf[msg->pos++];
    
    return rc;
}

static unsigned char *jk_msg_ajp_getString(jk_msg_t *msg) 
{
    int size = jk_msg_ajp_getInt(msg);
    int start = msg->pos;

    if((size < 0 ) || (size + start > msg->maxlen)) { 
        msg->l->jkLog( msg->l, JK_LOG_ERROR,
                       "Error: try to get data past end of the buffer\n");
        return (unsigned char *)"ERROR"; /* XXX */
    }

    msg->pos += size;
    msg->pos++;  /* terminating NULL */
    
    return (unsigned char *)(msg->buf + start); 
}

static unsigned char *jk_msg_ajp_getBytes(jk_msg_t *msg, int *lenP) 
{
    int size = jk_msg_ajp_getInt(msg);
    int start = msg->pos;

    *lenP=size;
    
    if((size < 0 ) || (size + start > msg->maxlen)) { 
        msg->l->jkLog( msg->l, JK_LOG_ERROR,
                       "Error: try to get data past end of the buffer\n");
        return (unsigned char *)"ERROR"; /* XXX */
    }

    msg->pos += size;
    msg->pos++;  /* terminating NULL */
    
    return (unsigned char *)(msg->buf + start); 
}


/** Shame-less copy from somewhere.
    assert (src != dst)
 */
static void swap_16(unsigned char *src, unsigned char *dst) 
{
    *dst++ = *(src + 1 );
    *dst= *src;
}


/*
 * Send a message to endpoint, using corresponding PROTO HEADER
 */
static int jk_msg_ajp_send(jk_msg_t   *msg,
                            jk_endpoint_t *ae )
{
    int err;
    jk_channel_t *channel=ae->worker->channel;

    jk_msg_ajp_end(msg);

    /* jk_msg_ajp_dump(l, JK_LOG_DEBUG, "sending to ajp13", msg); */

    err=channel->send( channel, ae, 
                       msg->buf, msg->len );
    
    if( err!=JK_TRUE ) {
        return err;
    }

    return JK_TRUE;
}


/*
 * Receive a message from endpoint
 */
static int jk_msg_ajp_receive(jk_msg_t   *msg,
                              jk_endpoint_t *ae )
{
    unsigned char head[4];
    int           rc;
    int           msglen;
    jk_channel_t *channel=ae->worker->channel;
    jk_logger_t *l=msg->l;
    

    rc=channel->recv( channel, ae, head, 4 );

    if(rc < 0) {
        l->jkLog(l, JK_LOG_ERROR,
                 "msgAjp.receive(): Read head failed %d %d\n",
                 rc, errno);
        return JK_FALSE;
    }

    if( head[0] != 0x41 || head[1] != 0x42 ) {
        l->jkLog(l, JK_LOG_ERROR,
                 "msgAjp.receive(): Bad signature %x%x\n", head[0], head[1]);
        return JK_FALSE;
    }

    msglen  = ((head[2]&0xff)<<8);
    msglen += (head[3] & 0xFF);

    if(msglen > msg->maxlen ) {
        l->jkLog(l, JK_LOG_ERROR,
                 "msgAjp.receive(): Incoming message is too big %d\n", msglen);
        return JK_FALSE;
    }

    msg->len=msglen;
    msg->pos=0;

    rc=channel->recv( channel, ae, msg->buf, msglen);

    if(rc < 0) {
        l->jkLog(l, JK_LOG_ERROR,
                 "msgAjp.receive(): Error receiving message body %d %d\n",
                 rc, errno);
        return JK_FALSE;
    }

    l->jkLog(l, JK_LOG_INFO, "msgAjp.receive(): Received len=%d type=%d\n",
             msglen, (int)msg->buf[0]);
    
    return JK_TRUE;
}


/** 
 * Special method. Will read data from the server and add them as
 * bytes. It is equivalent with jk_requtil_readFully() in a buffer
 * and then jk_msg_appendBytes(), except that we use directly the
 * internal buffer.
 *
 * Returns -1 on error, else number of bytes read
 */
static int jk_msg_ajp_appendFromServer(jk_msg_t    *msg,
                                       jk_ws_service_t *r,
                                       jk_endpoint_t  *ae,
                                       int            len )
{
    unsigned char *read_buf = msg->buf;
    jk_logger_t *l=msg->l;

    jk_msg_ajp_reset(msg);

    read_buf += AJP_HEADER_LEN;    /* leave some space for the buffer headers */
    read_buf += AJP_HEADER_SZ_LEN; /* leave some space for the read length */

    /* Pick the max size since we don't know the content_length */
    if (r->is_chunked && len == 0) {
	len = AJP13_MAX_SEND_BODY_SZ;
    }

    if ((len = jk_requtil_readFully(r, read_buf, len)) < 0) {
        l->jkLog(l, JK_LOG_ERROR,
                 "msgAjp.appendFromServer() error reading from server\n");
        return -1;
    }

    if (!r->is_chunked) {
	r->left_bytes_to_send -= len;
    }

    if (len > 0) {
	/* Recipient recognizes empty packet as end of stream, not
	   an empty body packet */
        if(0 != jk_msg_ajp_appendInt(msg, (unsigned short)len)) {
            l->jkLog(l, JK_LOG_ERROR, 
                     "read_into_msg_buff: Error - jk_b_append_int failed\n");
            return -1;
	}
    }

    msg->len= msg->len + len;

    return len;
}


jk_msg_t *jk_msg_ajp_create(jk_pool_t *p, jk_logger_t *log, int buffSize) 
{
    jk_msg_t *msg = 
        (jk_msg_t *)p->calloc(p, sizeof(jk_msg_t));

    if( buffSize==0 )
        buffSize=DEF_BUFFER_SZ;
    if(!msg) {
        return NULL;
    }
    msg->pool = p;

    msg->buf= (unsigned char *)msg->pool->alloc(msg->pool, buffSize);
    
    if(msg->buf==NULL) {
        return NULL;
    }
    
    msg->l=log;
    msg->maxlen=buffSize;
    msg->len=0;

    msg->reset=jk_msg_ajp_reset;
    msg->end=jk_msg_ajp_end;
    msg->dump=jk_msg_ajp_dump;

    msg->appendByte=jk_msg_ajp_appendByte;
    msg->appendBytes=jk_msg_ajp_appendBytes;
    msg->appendInt=jk_msg_ajp_appendInt;
    msg->appendLong=jk_msg_ajp_appendLong;
    msg->appendString=jk_msg_ajp_appendString;

    msg->appendFromServer=jk_msg_ajp_appendFromServer;

    msg->getByte=jk_msg_ajp_getByte;
    msg->getInt=jk_msg_ajp_getInt;
    msg->peekInt=jk_msg_ajp_peekInt;
    msg->getLong=jk_msg_ajp_getLong;
    msg->getString=jk_msg_ajp_getString;
    msg->getBytes=jk_msg_ajp_getBytes;

    msg->send=jk_msg_ajp_send;
    msg->receive=jk_msg_ajp_receive;
                       
    return msg;
}

