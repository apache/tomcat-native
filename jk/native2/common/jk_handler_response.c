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

#include "jk_global.h"
#include "jk_service.h"
#include "jk_msg_buff.h"
#include "jk_env.h"
#include "jk_requtil.h"


/*
AJPV13_RESPONSE/AJPV14_RESPONSE:=
    response_prefix (2)
    status          (short)
    status_msg      (short)
    num_headers     (short)
    num_headers*(res_header_name header_value)
    *body_chunk
    terminator      boolean <! -- recycle connection or not  -->

req_header_name := 
    sc_req_header_name | (string)

res_header_name := 
    sc_res_header_name | (string)

header_value :=
    (string)

body_chunk :=
    length  (short)
    body    length*(var binary)

 */
int ajp_unmarshal_response(jk_msg_buf_t   *msg,
                           jk_ws_service_t  *s,
                           jk_endpoint_t *ae,
                           jk_logger_t    *l)
{
    jk_pool_t * p = &ae->pool;

    s->status = jk_b_get_int(msg);

    if (!s->status) {
        l->jkLog(l, JK_LOG_ERROR, "Error ajp_unmarshal_response - Null status\n");
        return JK_FALSE;
    }

    s->msg = (char *)jk_b_get_string(msg);
    if (s->msg) {
        jk_xlate_from_ascii(s->msg, strlen(s->msg));
    }

    l->jkLog(l, JK_LOG_DEBUG, "ajp_unmarshal_response: status = %d\n", s->status);

    s->out_headers = jk_b_get_int(msg);
    s->out_header_names = s->out_header_values = NULL;

    l->jkLog(l, JK_LOG_DEBUG, "ajp_unmarshal_response: Number of headers is = %d\n", s->out_headers);

    if (s->out_headers) {
        s->out_header_names = p->alloc(p, sizeof(char *) * s->out_headers);
        s->out_header_values = p->alloc(p, sizeof(char *) * s->out_headers);

        if (s->out_header_names && s->out_header_values) {
            unsigned i;
            for(i = 0 ; i < s->out_headers ; i++) {
                unsigned short name = jk_b_pget_int(msg, jk_b_get_pos(msg)) ;
                
                if ((name & 0XFF00) == 0XA000) {
                    jk_b_get_int(msg);
                    name = name & 0X00FF;
                    if (name <= SC_RES_HEADERS_NUM) {
                        s->out_header_names[i] = (char *)jk_requtil_getHeaderById(name);
                    } else {
                        l->jkLog(l, JK_LOG_ERROR, "Error ajp_unmarshal_response - No such sc (%d)\n", name);
                        return JK_FALSE;
                    }
                } else {
                    s->out_header_names[i] = (char *)jk_b_get_string(msg);
                    if (!s->out_header_names[i]) {
                        l->jkLog(l, JK_LOG_ERROR, "Error ajp_unmarshal_response - Null header name\n");
                        return JK_FALSE;
                    }
                    jk_xlate_from_ascii(s->out_header_names[i],
                                 strlen(s->out_header_names[i]));

                }

                s->out_header_values[i] = (char *)jk_b_get_string(msg);
                if (!s->out_header_values[i]) {
                    l->jkLog(l, JK_LOG_ERROR, "Error ajp_unmarshal_response - Null header value\n");
                    return JK_FALSE;
                }

                jk_xlate_from_ascii(s->out_header_values[i],
                             strlen(s->out_header_values[i]));

                l->jkLog(l, JK_LOG_DEBUG, "ajp_unmarshal_response: Header[%d] [%s] = [%s]\n", 
                       i,
                       s->out_header_names[i],
                       s->out_header_values[i]);
            }
        }
    }

    return JK_TRUE;
}

