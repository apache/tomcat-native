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

/**
 * Common methods to all channels.
 * 
 * @author: Costin Manolache
 */

#include "jk_map.h"
#include "jk_env.h"
#include "jk_channel.h"
#include "jk_global.h"

#include <string.h>
#include "jk_registry.h"

#define CH_SET_ATTRIBUTE 0
#define CH_GET_ATTRIBUTE 1
#define CH_INIT 2
#define CH_DESTROY 3


#define CH_OPEN 4
#define CH_CLOSE 5
#define CH_READ 6
#define CH_WRITE 7

    
/** Called by java ( local or remote ). 
 */
int JK_METHOD jk2_channel_dispatch(jk_env_t *env, void *target, jk_endpoint_t *ep, jk_msg_t *msg)
{
    jk_bean_t *bean=(jk_bean_t *)target;
    jk_channel_t *ch=(jk_channel_t *)bean->object;
    int rc=JK_OK;

    int code=msg->getByte(env, msg );
    
    if( ch->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "ch.%d() \n", code);
    
    switch( code ) {
    case CH_SET_ATTRIBUTE: {
        char *name=msg->getString( env, msg );
        char *value=msg->getString( env, msg );
        if( ch->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                          "ch.setAttribute() %s %s %p\n", name, value, bean->setAttribute);
        if( bean->setAttribute != NULL)
            bean->setAttribute(env, bean, name, value );
        return JK_OK;
    }
    case CH_INIT: { 
        if( ch->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO, "ch.init()\n");
        if( ch->mbean->init != NULL )
            rc=ch->mbean->init(env, ch->mbean);
        return rc;
    }
    case CH_DESTROY: {
        if( ch->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO, "ch.init()\n");
        if( ch->mbean->destroy != NULL )
            rc=ch->mbean->destroy(env, ch->mbean);
        return rc;
        return rc;
    }
    case CH_OPEN: {
        if( ch->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO, "ch.open()\n");
        if( ch->open != NULL )
            rc=ch->open(env, ch, ep);
        return rc;
    }
    case CH_CLOSE: {
        if( ch->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO, "ch.close()\n");
        if( ch->close != NULL )
            rc=ch->close(env, ch, ep);
        return rc;
    }
    case CH_READ: {
        if( ch->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO, "ch.recv()\n");
        if( ch->recv != NULL )
            rc=ch->recv(env, ch, ep, msg);
        return rc;
    }
    case CH_WRITE: {
        if( ch->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO, "ch.send()\n");
        if( ch->send != NULL )
            rc=ch->send(env, ch, ep, msg);
        return rc;
    }
    }/* switch */
    return JK_ERR;
}

int JK_METHOD jk2_channel_setWorkerEnv( jk_env_t *env, jk_channel_t *ch, jk_workerEnv_t *wEnv ) {
    wEnv->registerHandler( env, wEnv, "channel",
                           "channelDispatch", JK_HANDLE_CH_DISPATCH,
                           jk2_channel_dispatch, NULL );
    return JK_OK;
}

