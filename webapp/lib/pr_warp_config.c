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

/* @version $Id$ */
#include "pr_warp.h"

wa_boolean n_check(wa_connection *conn, warp_packet *pack) {
    warp_config *conf=(warp_config *)conn->conf;
    int maj=-1;
    int min=-1;
    int sid=-1;

    if (n_recv(conf->sock,pack)!=wa_true) {
        wa_log(WA_MARK,"Cannot receive handshake WARP packet");
        return(wa_false);
    }

    if (pack->type!=TYPE_CONF_WELCOME) {
        wa_log(WA_MARK,"Invalid WARP packet %d (WELCOME)",pack->type);
        return(wa_false);
    }

    if (p_read_ushort(pack,&maj)!=wa_true) {
        wa_log(WA_MARK,"Cannot read major version");
        return(wa_false);
    }

    if (p_read_ushort(pack,&min)!=wa_true) {
        wa_log(WA_MARK,"Cannot read minor version");
        return(wa_false);
    }

    if ((maj!=VERS_MAJOR)||(min!=VERS_MINOR)) {
        wa_log(WA_MARK,"Invalid WARP protocol version %d.%d",maj,min);
        return(wa_false);
    }

    if (p_read_int(pack,&sid)!=wa_true) {
        wa_log(WA_MARK,"Cannot read server id");
        return(wa_false);
    }

    conf->serv=sid;
    wa_debug(WA_MARK,"Connection \"%s\" checked WARP/%d.%d (SERVER ID=%d)",
        conn->name,maj,min,conf->serv);
    return(wa_true);
}

wa_boolean n_configure(wa_connection *conn) {
    warp_config *conf=(warp_config *)conn->conf;
    wa_chain *elem=warp_applications;
    apr_pool_t *pool=NULL;
    wa_boolean ret=wa_false;
    warp_packet *pack=NULL;

    if (apr_pool_create(&pool,wa_pool)!=APR_SUCCESS) {
        wa_log(WA_MARK,"Cannot create WARP temporary configuration pool");
        n_disconnect(conn);
        return(wa_false);
    }

    if ((pack=p_create(wa_pool))==NULL) {
        wa_log(WA_MARK,"Cannot create WARP configuration packet");
        apr_pool_destroy(pool);
        return(wa_false);
    }

    if ((ret=n_check(conn,pack))==wa_false) n_disconnect(conn);

    while (elem!=NULL) {
        wa_application *appl=(wa_application *)elem->curr;
        wa_debug(WA_MARK,"Deploying \"%s\" via \"%s\" in \"http://%s:%d%s\"",
                appl->name,conn->name,appl->host->name,appl->host->port,
                appl->rpth);
        p_reset(pack);
        pack->type=TYPE_CONF_DEPLOY;
        p_write_string(pack,appl->name);
        p_write_string(pack,appl->host->name);
        p_write_ushort(pack,appl->host->port);
        p_write_string(pack,appl->rpth);
        n_send(conf->sock,pack);

        elem=elem->next;
    }

    apr_pool_destroy(pool);
    return(ret);
}
