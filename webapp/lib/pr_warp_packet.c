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
 * 4. The names  "The  Jakarta  Project",  "WebApp",  and  "Apache  Software *
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
2 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
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

/* @version $Id$ */
#include "pr_warp.h"

void p_reset(warp_packet *pack) {
    pack->type=TYPE_INVALID;
    pack->type=TYPE_INVALID;
    pack->size=0;
    pack->curr=0;
    pack->buff[0]='\0';
}

warp_packet *p_create(apr_pool_t *pool) {
    warp_packet *pack=NULL;

    if (pool==NULL) return(NULL);
    pack=(warp_packet *)apr_palloc(pool,sizeof(warp_packet));
    pack->pool=pool;
    p_reset(pack);
    return(pack);
}

wa_boolean p_read_ushort(warp_packet *pack, int *x) {
    int k=0;

    if ((pack->curr+2)>=pack->size) return(wa_false);
    k=(pack->buff[pack->curr++]&0x0ff)<<8;
    k=k|(pack->buff[pack->curr++]&0x0ff);
    *x=k;
    return(wa_true);
}

wa_boolean p_read_int(warp_packet *pack, int *x) {
    int k=0;

    if ((pack->curr+2)>=pack->size) return(wa_false);
    k=(pack->buff[pack->curr++]&0x0ff)<<24;
    k=k|((pack->buff[pack->curr++]&0x0ff)<<16);
    k=k|((pack->buff[pack->curr++]&0x0ff)<<8);
    k=k|(pack->buff[pack->curr++]&0x0ff);
    *x=k;
    return(wa_true);
}

wa_boolean p_read_string(warp_packet *pack, char **x) {
    int len=0;

    if (p_read_ushort(pack,&len)==wa_false) {
        *x=NULL;
        return(wa_false);
    }
    if ((pack->curr+len)>=pack->size) {
        *x=NULL;
        return(wa_false);
    }

    *x=(char *)apr_palloc(pack->pool,(len+1)*sizeof(char));
    if (*x==NULL) return(wa_false);

    apr_cpystrn(*x,&pack->buff[pack->curr],len);
    pack->curr+=len;
    return(wa_true);
}

wa_boolean p_write_ushort(warp_packet *pack, int x) {
    if (pack->size>65533) return(wa_false);
    pack->buff[pack->size++]=(x>>8)&0x0ff;
    pack->buff[pack->size++]=x&0x0ff;
    return(wa_true);
}

wa_boolean p_write_int(warp_packet *pack, int x) {
    if (pack->size>65531) return(wa_false);
    pack->buff[pack->size++]=(x>>24)&0x0ff;
    pack->buff[pack->size++]=(x>>16)&0x0ff;
    pack->buff[pack->size++]=(x>>8)&0x0ff;
    pack->buff[pack->size++]=x&0x0ff;
    return(wa_true);
}

wa_boolean p_write_string(warp_packet *pack, char *x) {
    int len=0;
    char *k=NULL;
    int q=0;

    if (x==NULL) return(p_write_ushort(pack,0));
    for (k=x; k[0]!='\0'; k++);
    len=k-x;
    if (p_write_ushort(pack,len)==wa_false) {
        pack->size-=2;
        return(wa_false);
    }
    if ((pack->size+len)>65535) {
        pack->size-=2;
        return(wa_false);
    }
    for (q=0;q<len;q++) pack->buff[pack->size++]=x[q];
    return(wa_true);
}
