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
#include "jk_msg.h"
#include "jk_env.h"
#include "jk_requtil.h"
#include "jk_env.h"
#include "jk_handler.h"
#include "jk_endpoint.h"

#include "jk_registry.h"

/*
 * Write a body chunk from the servlet container to the web server
 */
#define JK_AJP13_SEND_BODY_CHUNK    (unsigned char)3

/*
 * Send response headers from the servlet container to the web server.
 */
#define JK_AJP13_SEND_HEADERS       (unsigned char)4

/*
 * Marks the end of response.
 */
#define JK_AJP13_GET_BODY_CHUNK     (unsigned char)6

/*
 * Marks the end of response.
 */
#define JK_AJP13_END_RESPONSE       (unsigned char)5


/** SEND_HEADERS handler
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
static int jk_handler_startResponse(jk_env_t *env, jk_msg_t   *msg,
                                    jk_ws_service_t  *s, jk_endpoint_t *ae )
{
    int err=JK_FALSE;
    int i;
    jk_pool_t * pool = s->pool;
    int headerCount;

    s->status = msg->getInt(env, msg);
    s->msg = (char *)msg->getString(env, msg);
    if (s->msg) {
        jk_xlate_from_ascii(s->msg, strlen(s->msg));
        /* Do we want this ? Probably not needed, but safer ! */
        s->msg = ae->cPool->pstrdup( env, ae->cPool, s->msg );
    }
    headerCount = msg->getInt(env, msg);

    /* XXX assert msg->headers_out is set - the server adapter should know what
       kind of map to use ! */

    for(i = 0 ; i < headerCount ; i++) {
        char *nameS;
        char *valueS;
        unsigned short name = msg->peekInt(env, msg) ;

        if ((name & 0XFF00) == 0XA000) {
            msg->getInt(env, msg);
            name = name & 0X00FF;
            if (name <= SC_RES_HEADERS_NUM) {
                nameS = (char *)jk_requtil_getHeaderById(env, name);
            } else {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "handler.response() Invalid header id (%d)\n",
                              name);
                return JK_HANDLER_FATAL;
            }
        } else {
            nameS = (char *)msg->getString(env, msg);
            if (nameS==NULL) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "handler.response() Null header name \n");
                return JK_HANDLER_FATAL;
            }
            jk_xlate_from_ascii(nameS, strlen(nameS));
        }
        
        valueS = (char *)msg->getString(env, msg);
        if (valueS==NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                          "Error ajp_unmarshal_response - Null header value\n");
            return JK_HANDLER_FATAL;
        }
        
        jk_xlate_from_ascii(valueS, strlen(valueS));
        
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "handler.response() Header[%d] [%s] = [%s]\n", 
                      i, nameS, valueS);

        /* Do we want this ? Preserve the headers, maybe someone will
         need them. Alternative is to use a different buffer every time,
         which may be more efficient. */
        /* We probably don't need that, we'll send them in the next call */
        /*Apache does it - will change if we add jk_map_apache
          nameS=ae->cPool->pstrdup( ae->cPool, nameS ); */
        /*         valueS=ae->cPool->pstrdup( ae->cPool, valueS ); */
        
        s->headers_out->add( env, s->headers_out, nameS, valueS );
    }
    
    env->l->jkLog(env, env->l, JK_LOG_INFO,
                  "handler.response(): status=%d headers=%d\n",
                  s->status, headerCount);

    err=s->head(env, s);
    if( err!=JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "handler.response() Error sending response");
        return JK_HANDLER_ERROR;
    }
    
    return JK_HANDLER_OK;
}

/** SEND_BODY_CHUNK handler
 */
static int jk_handler_sendChunk(jk_env_t *env, jk_msg_t   *msg,
                                jk_ws_service_t  *r, jk_endpoint_t *ae)
{
    int err;
    int len;
    char *buf;

    buf=msg->getBytes( env, msg, &len );

    err=r->write(env, r, buf, len);
    if( err!= JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "Error ajp_process_callback - write failed\n");
        return JK_HANDLER_ERROR;
    }

    return JK_HANDLER_OK;
}

static int jk_handler_endResponse(jk_env_t *env, jk_msg_t   *msg,
                                  jk_ws_service_t  *r,jk_endpoint_t *ae)
{
    ae->reuse = (int)msg->getByte(env, msg);
            
    if((ae->reuse & 0X01) != ae->reuse) {
        /*
         * Strange protocol error.
         */
        ae->reuse = JK_FALSE;
    }
    return JK_HANDLER_LAST;
}

/** SEND_BODY_CHUNK handler
 */
static int jk_handler_getChunk(jk_env_t *env, jk_msg_t   *msg,
                               jk_ws_service_t  *r, jk_endpoint_t *ae)
{
    int len = msg->getInt(env, msg);
    
    if(len > AJP13_MAX_SEND_BODY_SZ) {
        len = AJP13_MAX_SEND_BODY_SZ;
    }
    if(len > r->left_bytes_to_send) {
        len = r->left_bytes_to_send;
    }
    if(len < 0) {
        len = 0;
    }

    len=msg->appendFromServer( env, msg, r, ae, len );
    /* the right place to add file storage for upload */
    if (len >= 0) {
        r->content_read += len;
        return JK_HANDLER_RESPONSE;
    }                  
            
    env->l->jkLog(env, env->l, JK_LOG_ERROR,
                  "handler_request.getChunk() - read failed len=%d\n",len);
    return JK_HANDLER_FATAL;	    
}

int JK_METHOD jk_handler_response_factory( jk_env_t *env, jk_pool_t *pool,
                                           void **result,
                                           const char *type, const char *name)
{
    jk_map_t *map;
    jk_handler_t *h;
    
    jk_map_default_create( env, &map, pool );
    *result=map;
    
    h=(jk_handler_t *)pool->calloc( env, pool, sizeof( jk_handler_t));
    h->name="sendHeaders";
    h->messageId=JK_AJP13_SEND_HEADERS;
    h->callback=jk_handler_startResponse;
    map->put( env, map, h->name, h, NULL );

    h=(jk_handler_t *)pool->calloc( env, pool, sizeof( jk_handler_t));
    h->name="sendChunk";
    h->messageId=JK_AJP13_SEND_BODY_CHUNK;
    h->callback=jk_handler_sendChunk;
    map->put( env, map, h->name, h, NULL );
    
    h=(jk_handler_t *)pool->calloc( env, pool, sizeof( jk_handler_t));
    h->name="endResponse";
    h->messageId=JK_AJP13_END_RESPONSE;
    h->callback=jk_handler_endResponse;
    map->put( env, map, h->name, h, NULL );

    h=(jk_handler_t *)pool->calloc( env, pool, sizeof( jk_handler_t));
    h->name="getChunk";
    h->messageId=JK_AJP13_GET_BODY_CHUNK;
    h->callback=jk_handler_getChunk;
    map->put( env, map, h->name, h, NULL );

    return JK_TRUE;
}
