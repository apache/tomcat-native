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

/***************************************************************************
 * Description: Experimental bi-directionl protocol handler.               *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                           *
 ***************************************************************************/


#include "jk_global.h"
#include "jk_util.h"
#include "jk_ajp_common.h"
#include "jk_ajp13.h"

int ajp13_marshal_shutdown_into_msgb(jk_msg_buf_t *msg,
                                     jk_pool_t *p, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);
    /* To be on the safe side */
    jk_b_reset(msg);

    /*
     * Just a single byte with s/d command.
     */
    if (jk_b_append_byte(msg, JK_AJP13_SHUTDOWN)) {
        jk_log(l, JK_LOG_ERROR,
               "failed appending shutdown message\n");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}
