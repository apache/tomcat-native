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
 * Author:      Henri Gomez <hgomez@slib.fr>                               *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_msg_buff.h"
#include "jk_logger.h"

struct jk_msg_buf {
    jk_pool_t *pool;

    unsigned char *buf;
    int pos; 
    int len;
    int maxlen;
};


/*
 * Simple marshaling code.
 */

/* XXX what's above this line can go to .h XXX */
void jk_b_dump(jk_msg_buf_t *msg, 
               char *err) 
{
    int i=0;
	printf("%s %d/%d/%d %x %x %x %x - %x %x %x %x - %x %x %x %x - %x %x %x %x\n", err, msg->pos, msg->len, msg->maxlen,  
	       msg->buf[i++],msg->buf[i++],msg->buf[i++],msg->buf[i++],
	       msg->buf[i++],msg->buf[i++],msg->buf[i++],msg->buf[i++],
	       msg->buf[i++],msg->buf[i++],msg->buf[i++],msg->buf[i++],
	       msg->buf[i++],msg->buf[i++],msg->buf[i++],msg->buf[i++]);

	i = msg->pos - 4;
    if(i < 0) {
        i=0;
    }
	
    printf("        %x %x %x %x - %x %x %x %x --- %x %x %x %x - %x %x %x %x\n", 
	       msg->buf[i++],msg->buf[i++],msg->buf[i++],msg->buf[i++],
	       msg->buf[i++],msg->buf[i++],msg->buf[i++],msg->buf[i++],
	       msg->buf[i++],msg->buf[i++],msg->buf[i++],msg->buf[i++],
	       msg->buf[i++],msg->buf[i++],msg->buf[i++],msg->buf[i++]);

}

void jk_b_reset(jk_msg_buf_t *msg) 
{
    msg->len = 4;
    msg->pos = 4;
}


static void jk_b_set_long(jk_msg_buf_t *msg,
                  int pos,
                  unsigned long val)
{   
    msg->buf[pos]       = (unsigned char)((val >> 24) & 0xFF);
    msg->buf[pos + 1]   = (unsigned char)((val >> 16) & 0xFF);
    msg->buf[pos + 2]   = (unsigned char)((val >> 8) & 0xFF);
    msg->buf[pos + 3]   = (unsigned char)(val & 0xFF);
}


int jk_b_append_long(jk_msg_buf_t *msg,
                    unsigned long val)
{
    if(msg->len + 4 > msg->maxlen) {
        return -1;
    }

    jk_b_set_long(msg, msg->len, val);

    msg->len += 4;

    return 0;
}


static void jk_b_set_int(jk_msg_buf_t *msg, 
                  int pos, 
                  unsigned short val) 
{
    msg->buf[pos]       = (unsigned char)((val >> 8) & 0xFF);
    msg->buf[pos + 1]   = (unsigned char)(val & 0xFF);
}


int jk_b_append_int(jk_msg_buf_t *msg, 
                    unsigned short val) 
{
    if(msg->len + 2 > msg->maxlen) {
	    return -1;
    }

    jk_b_set_int(msg, msg->len, val);

    msg->len += 2;

    return 0;
}


static void jk_b_set_byte(jk_msg_buf_t *msg, 
                   int pos, 
                   unsigned char val) 
{
    msg->buf[pos]= val;
}

int jk_b_append_byte(jk_msg_buf_t *msg, 
                     unsigned char val)
{
    if(msg->len + 1 > msg->maxlen) {
	    return -1;
    }

    jk_b_set_byte(msg, msg->len, val);

    msg->len += 1;

    return 0;
}


void jk_b_end(jk_msg_buf_t *msg, int protoh) 
{
    /* 
     * Ugly way to set the size in the right position 
     */
    jk_b_set_int(msg, 2, (unsigned short )(msg->len - 4)); /* see protocol */
    jk_b_set_int(msg, 0, (unsigned short) protoh);
}


jk_msg_buf_t *jk_b_new(jk_pool_t *p) 
{
    jk_msg_buf_t *msg = 
            (jk_msg_buf_t *)jk_pool_alloc(p, sizeof(jk_msg_buf_t));

    if(!msg) {
        return NULL;
    }

    msg->pool = p;
    
    return msg;
}

int jk_b_set_buffer(jk_msg_buf_t *msg, 
                    char *data, 
                    int buffSize) 
{
    if(!msg) {
        return -1;
    }

    msg->len = 0;
    msg->buf = (unsigned char *)data;
    msg->maxlen = buffSize;
    
    return 0;
}


int jk_b_set_buffer_size(jk_msg_buf_t *msg, 
                         int buffSize) 
{
    unsigned char *data = (unsigned char *)jk_pool_alloc(msg->pool, buffSize);
    
    if(!data) {
	    return -1;
    }

    jk_b_set_buffer(msg, (char *)data, buffSize);
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

void jk_b_set_pos(jk_msg_buf_t *msg,
                          int pos) 
{
    msg->pos = pos;
}

unsigned int jk_b_get_len(jk_msg_buf_t *msg) 
{
    return msg->len;
}

void jk_b_set_len(jk_msg_buf_t *msg, 
                  int len) 
{
    msg->len=len;
}

int jk_b_get_size(jk_msg_buf_t *msg) 
{
    return msg->maxlen;
}

int jk_b_append_string(jk_msg_buf_t *msg, 
                       const char *param) 
{
    int len;

    if(!param) {
	    jk_b_append_int( msg, 0xFFFF );
	    return 0; 
    }

    len = strlen(param);
    if(msg->len + len + 2  > msg->maxlen) {
	    return -1;
    }

    /* ignore error - we checked once */
    jk_b_append_int(msg, (unsigned short )len);

    /* We checked for space !!  */
    strncpy((char *)msg->buf + msg->len , param, len+1);    /* including \0 */
    jk_xlate_to_ascii((char *)msg->buf + msg->len, len+1);  /* convert from EBCDIC if needed */
    msg->len += len + 1;

    return 0;
}


int jk_b_append_bytes(jk_msg_buf_t         *msg,
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

unsigned long jk_b_get_long(jk_msg_buf_t *msg)
{
    unsigned long i;
    if(msg->pos + 3 > msg->len) {
        printf( "Read after end \n");
        return -1;
    }
    i  = ((msg->buf[(msg->pos++)] & 0xFF)<<24);
    i |= ((msg->buf[(msg->pos++)] & 0xFF)<<16);
    i |= ((msg->buf[(msg->pos++)] & 0xFF)<<8);
    i |= ((msg->buf[(msg->pos++)] & 0xFF));
    return i;
}

unsigned long jk_b_pget_long(jk_msg_buf_t *msg,
                             int pos)
{
    unsigned long i;
    i  = ((msg->buf[(pos++)] & 0xFF)<<24);
    i |= ((msg->buf[(pos++)] & 0xFF)<<16);
    i |= ((msg->buf[(pos++)] & 0xFF)<<8);
    i |= ((msg->buf[(pos  )] & 0xFF));
    return i;
}


unsigned short jk_b_get_int(jk_msg_buf_t *msg) 
{
    int i;
    if(msg->pos + 1 > msg->len) {
	    printf( "Read after end \n");
	    return -1;
    }
    i  = ((msg->buf[(msg->pos++)] & 0xFF)<<8);
    i += ((msg->buf[(msg->pos++)] & 0xFF));
    return i;
}

unsigned short jk_b_pget_int(jk_msg_buf_t *msg, 
                             int pos) 
{
    int i;
	i  = ((msg->buf[pos++] & 0xFF)<<8);
    i += ((msg->buf[pos]   & 0xFF));
    return i;
}

unsigned char jk_b_get_byte(jk_msg_buf_t *msg) 
{
    unsigned char rc;
    if(msg->pos > msg->len) {
	    printf("Read after end \n");
	    return -1;
    }
    rc = msg->buf[msg->pos++];
    
    return rc;
}

unsigned char jk_b_pget_byte(jk_msg_buf_t *msg, 
                             int pos) 
{
    return msg->buf[pos];
}


unsigned char *jk_b_get_string(jk_msg_buf_t *msg) 
{
    int size = jk_b_get_int(msg);
    int start = msg->pos;

    if((size < 0 ) || (size + start > msg->maxlen)) { 
	    jk_b_dump(msg, "After get int"); 
	    printf("ERROR\n" );
	    return (unsigned char *)"ERROR"; /* XXX */
    }

    msg->pos += size;
    msg->pos++;  /* terminating NULL */
    
    return (unsigned char *)(msg->buf + start); 
}

int jk_b_get_bytes(jk_msg_buf_t *msg, unsigned char * buf, int len)
{
    int start = msg->pos;

    if((len < 0 ) || (len + start > msg->maxlen)) {
        jk_b_dump(msg, "After get bytes");
        printf("ERROR\n" ); 
        return (-1);
    }
    
	memcpy(buf, msg->buf + start, len);
    msg->pos += len;
	return (len);
}


/** Shame-less copy from somewhere.
    assert (src != dst)
 */
static void swap_16(unsigned char *src, unsigned char *dst) 
{
    *dst++ = *(src + 1 );
    *dst= *src;
}

/** Helpie dump function 
 */
void jk_dump_buff(jk_logger_t *l,
                      const char *file,
                      int line,
                      int level,
                      char * what,
                      jk_msg_buf_t * msg)
{
#ifdef USE_ALSO_BODY
        l->jkLog(l, file, line, level, "%s #%d %.*s\n",
                  what,
                  jk_b_get_len(msg),
                  jk_b_get_len(msg),
                  jk_b_get_buff(msg));
#else
l->jkLog(l, file, line, level, "%s #%d\n", what, jk_b_get_len(msg));
#endif
}

