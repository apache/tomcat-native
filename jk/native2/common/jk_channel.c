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
    jk_channel_t *ch = (jk_channel_t *)mbean->object;
    char *value = valueP;

    if (strcmp("debug", name) == 0) {
        ch->mbean->debug = atoi(value);
    }
    else if (strcmp("disabled", name) == 0) {
        ch->mbean->disabled = atoi(value);
        if (ch->worker != NULL)
            ch->worker->mbean->disabled = ch->mbean->disabled;
    }
    else {
        if (ch->worker != NULL) {
            return ch->worker->mbean->setAttribute(env, ch->worker->mbean,
                                                   name, valueP);
        }
        return JK_ERR;
    }
    return JK_OK;
}



/** Called by java ( local or remote ). 
 */
int JK_METHOD jk2_channel_invoke(jk_env_t *env, jk_bean_t *bean,
                                 jk_endpoint_t *ep, int code, jk_msg_t *msg,
                                 int raw)
{
    jk_channel_t *ch = (jk_channel_t *)bean->object;
    int rc = JK_OK;

    if (ch->mbean->debug > 0)
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "ch.%d() \n", code);

    switch (code) {
    case CH_OPEN:{
            if (ch->mbean->debug > 0)
                env->l->jkLog(env, env->l, JK_LOG_DEBUG, "ch.open()\n");
            if (ch->open != NULL)
                rc = ch->open(env, ch, ep);
            return rc;
        }
    case CH_CLOSE:{
            if (ch->mbean->debug > 0)
                env->l->jkLog(env, env->l, JK_LOG_DEBUG, "ch.close()\n");
            if (ch->close != NULL)
                rc = ch->close(env, ch, ep);
            return rc;
        }
    case CH_READ:{
            if (ch->mbean->debug > 0)
                env->l->jkLog(env, env->l, JK_LOG_DEBUG, "ch.recv()\n");
            if (ch->recv != NULL)
                rc = ch->recv(env, ch, ep, msg);
            if (rc == JK_OK)
                return JK_INVOKE_WITH_RESPONSE;
            return rc;
        }
    case CH_WRITE:{
            if (ch->mbean->debug > 0)
                env->l->jkLog(env, env->l, JK_LOG_DEBUG, "ch.send()\n");
            if (ch->serverSide)
                msg->serverSide = JK_TRUE;
            if (ch->send != NULL)
                rc = ch->send(env, ch, ep, msg);
            return rc;
        }
    case CH_HASINPUT:{
            if (ch->mbean->debug > 0)
                env->l->jkLog(env, env->l, JK_LOG_DEBUG, "ch.hasinput()\n");
            if (ch->serverSide)
                msg->serverSide = JK_TRUE;
            if (ch->hasinput != NULL)
                rc = ch->hasinput(env, ch, ep, 1000);   /* Well we should handle timeout better isn't it ? */
            return rc;
        }
    }                           /* switch */
    return JK_ERR;
}
