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

/** Common attributes for all channels
 */
int JK_METHOD jk2_channel_setAttribute(jk_env_t *env,
                                       jk_bean_t *mbean, 
                                       char *name, void *valueP)
{
    jk_channel_t *ch=(jk_channel_t *)mbean->object;
    char *value=valueP;

    if( strcmp( "debug", name ) == 0 ) {
        ch->mbean->debug=atoi( value );
    } else if( strcmp( "disabled", name ) == 0 ) {
        ch->mbean->disabled=atoi( value );
        if( ch->worker!=NULL)
        ch->worker->mbean->disabled=ch->mbean->disabled;
    } else {
	if( ch->worker!=NULL ) {
            return ch->worker->mbean->setAttribute( env, ch->worker->mbean, name, valueP );
        }
        return JK_ERR;
    }
    return JK_OK;
}



/** Called by java ( local or remote ). 
 */
int JK_METHOD jk2_channel_invoke(jk_env_t *env, jk_bean_t *bean, jk_endpoint_t *ep, int code,
                                 jk_msg_t *msg, int raw)
{
    jk_channel_t *ch=(jk_channel_t *)bean->object;
    int rc=JK_OK;

    if( ch->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "ch.%d() \n", code);
    
    switch( code ) {
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
        if( rc==JK_OK )
            return JK_INVOKE_WITH_RESPONSE;
        return rc;
    }
    case CH_WRITE: {
        if( ch->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO, "ch.send()\n");
        if( ch->serverSide )
            msg->serverSide=JK_TRUE;
        if( ch->send != NULL )
            rc=ch->send(env, ch, ep, msg);
        return rc;
    }
    }/* switch */
    return JK_ERR;
}

