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

    if ((pack->curr+2)>pack->size) return(wa_false);
    k=(pack->buff[pack->curr++]&0x0ff)<<8;
    k=k|(pack->buff[pack->curr++]&0x0ff);
    *x=k;
    return(wa_true);
}

wa_boolean p_read_int(warp_packet *pack, int *x) {
    int k=0;

    if ((pack->curr+2)>pack->size) return(wa_false);
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
        wa_debug(WA_MARK,"Cannot read string length");
        return(wa_false);
    }
    if ((pack->curr+len)>pack->size) {
        *x=NULL;
        wa_debug(WA_MARK,"String too long (len=%d curr=%d size=%d)",
                 len,pack->curr,pack->size);
        return(wa_false);
    }

    *x=(char *)apr_palloc(pack->pool,(len+2)*sizeof(char));
    if (*x==NULL) return(wa_false);

    apr_cpystrn(*x,&pack->buff[pack->curr],len+1);
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
