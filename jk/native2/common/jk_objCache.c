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

/* See jk_objCache.h for docs */

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_channel.h"
#include "jk_msg_buff.h"
#include "jk_ajp14.h" 
#include "jk_logger.h"
#include "jk_service.h"
#include "jk_env.h"

#include "jk_objCache.h"

static int 
jk_objCache_put(jk_objCache_t *_this, void *obj)
{
    int rc;
    
/*     if( _this->debug > 0 )  */
/*         _this->l->jkLog(_this->l, JK_LOG_DEBUG, "Cache put\n"); */

    if(_this->ep_cache_sz <= 0 )
        return JK_FALSE;

    
    JK_ENTER_CS(&_this->cs, rc);
    if(rc) {
        int i;
        
        for(i = 0 ; i < _this->ep_cache_sz ; i++) {
            if(!_this->ep_cache[i]) {
                _this->ep_cache[i] = obj;
                break;
            }
        }
        JK_LEAVE_CS(&_this->cs, rc);
        if(i < _this->ep_cache_sz) {
            /*   l->jkLog(l, JK_LOG_DEBUG, "Return endpoint to pool\n"); */
            return JK_TRUE;
        }
    }

    /* Can't enter CS */ 
    return JK_FALSE; 
}

static void * 
jk_objCache_get(jk_objCache_t *_this )
{
    int rc;
    void *ae=NULL;

    if (_this->ep_cache_sz <= 0 )
        return NULL;

    
    JK_ENTER_CS(&_this->cs, rc);
    if (rc) {
        int i;
        
        for (i = 0 ; i < _this->ep_cache_sz ; i++) {
            if (_this->ep_cache[i]) {
                ae = _this->ep_cache[i];
                _this->ep_cache[i] = NULL;
                break;
            }
        }
        JK_LEAVE_CS(&_this->cs, rc);

        return ae;
    }
    return NULL;
}

