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
#ifndef _PR_WARP_H_
#define _PR_WARP_H_

#include <wa.h>

/* ************************************************************************* */
/* INSTANCE VARIABLES                                                        */
/* ************************************************************************* */
/* The list of all configured connections */
extern wa_chain *warp_connections;
/* The list of all deployed connections */
extern wa_chain *warp_applications;
/* The warp provider structure */
extern wa_provider wa_provider_warp;

/* ************************************************************************* */
/* STRUCTURES                                                                */
/* ************************************************************************* */

/* The WARP connection configuration structure */
typedef struct warp_config {
    apr_sockaddr_t *addr;
    apr_socket_t *sock;
    int serv;
} warp_config;

/* The WARP packet structure */
typedef struct warp_packet {
    apr_pool_t *pool;
    int type;
    int size;
    int curr;
    char buff[65536];
} warp_packet;

/* Structure for processin headers in WARP */
typedef struct warp_header {
    wa_connection *conn;
    warp_packet *pack;
    wa_boolean fail;
} warp_header;

/* ************************************************************************* */
/* DEFINITIONS                                                               */
/* ************************************************************************* */

/* WARP definitions */
#define VERS_MAJOR 0
#define VERS_MINOR 10
#include "pr_warp_defs.h"

/* ************************************************************************* */
/* PACKET FUNCTIONS FROM PR_WARP_PACKET.C                                    */
/* ************************************************************************* */
void p_reset(warp_packet *pack);
warp_packet *p_create(apr_pool_t *pool);
wa_boolean p_read_ushort(warp_packet *pack, int *x);
wa_boolean p_read_int(warp_packet *pack, int *x);
wa_boolean p_read_string(warp_packet *pack, char **x);
wa_boolean p_write_ushort(warp_packet *pack, int x);
wa_boolean p_write_int(warp_packet *pack, int x);
wa_boolean p_write_string(warp_packet *pack, char *x);

/* ************************************************************************* */
/* NETWORK FUNCTIONS FROM PR_WARP_NETWORK.C                                  */
/* ************************************************************************* */
wa_boolean n_recv(apr_socket_t *sock, warp_packet *pack);
wa_boolean n_send(apr_socket_t *sock, warp_packet *pack);
wa_boolean n_connect(wa_connection *conn);
void n_disconnect(wa_connection *conn);

/* ************************************************************************* */
/* CONFIGURATION FUNCTIONS FROM PR_WARP_CONFIG.C                             */
/* ************************************************************************* */
wa_boolean c_check(wa_connection *conn, warp_packet *pack);
wa_boolean c_configure(wa_connection *conn);

#endif /* ifndef _PR_WARP_H_ */
