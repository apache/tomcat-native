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

/* Structure to maintain a pool of sockets to be used for a given
   WARP connection */
typedef struct warp_socket_pool {
    wa_chain * available_socket_list;
#if APR_HAS_THREADS 
    apr_thread_mutex_t * pool_mutex;
#endif
    int available_socket_list_size;
    wa_chain * available_elem_blocks;
} warp_socket_pool;


/* The WARP connection configuration structure */
typedef struct warp_config {
    warp_socket_pool * socket_pool;
    apr_sockaddr_t *addr;

#if APR_HAS_THREADS 
    apr_atomic_t open_socket_count;
    apr_atomic_t serv;
#else
    unsigned int open_socket_count;
    unsigned int serv;
#endif

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
    apr_socket_t *sock;
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
apr_socket_t *n_connect(wa_connection *conn);
void n_disconnect(wa_connection *conn, apr_socket_t * sock);

/* ************************************************************************* */
/* CONFIGURATION FUNCTIONS FROM PR_WARP_CONFIG.C                             */
/* ************************************************************************* */
wa_boolean c_check(wa_connection *conn, warp_packet *pack, apr_socket_t * sock);
wa_boolean c_configure(wa_connection *conn, apr_socket_t *sock);


/* ************************************************************************* */
/* SOCKET POOL FUNCTIONS FROM PR_WARP_SOCKETPOOL.C                           */
/* ************************************************************************* */

warp_socket_pool * warp_sockpool_create();
void warp_sockpool_destroy(warp_socket_pool * pool);
apr_socket_t * warp_sockpool_acquire(warp_socket_pool * pool);
void warp_sockpool_release(warp_socket_pool * pool, 
                           wa_connection *conn, 
                           apr_socket_t * sock);

#endif /* ifndef _PR_WARP_H_ */
