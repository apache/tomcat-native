/*
 * Copyright (c) 1997-1999 The Java Apache Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * 4. The names "Apache JServ", "Apache JServ Servlet Engine" and 
 *    "Java Apache Project" must not be used to endorse or promote products 
 *    derived from this software without prior written permission.
 *
 * 5. Products derived from this software may not be called "Apache JServ"
 *    nor may "Apache" nor "Apache JServ" appear in their names without 
 *    prior written permission of the Java Apache Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *    
 * THIS SOFTWARE IS PROVIDED BY THE JAVA APACHE PROJECT "AS IS" AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE JAVA APACHE PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Java Apache Group. For more information
 * on the Java Apache Project and the Apache JServ Servlet Engine project,
 * please see <http://java.apache.org/>.
 *
 */

/***************************************************************************
 * Description: ajpv1.3 worker header file                                 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef JK_AJP13_WORKER_H
#define JK_AJP13_WORKER_H

#include "jk_pool.h"
#include "jk_connect.h"
#include "jk_util.h"
#include "jk_msg_buff.h"
#include "jk_ajp13.h"
#include "jk_logger.h"
#include "jk_service.h"
#include "jk_mt.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define JK_AJP13_WORKER_NAME ("ajp13")

struct ajp13_operation;
typedef struct ajp13_operation ajp13_operation_t;

struct ajp13_endpoint;
typedef struct ajp13_endpoint ajp13_endpoint_t;

struct ajp13_worker;
typedef struct ajp13_worker ajp13_worker_t;

struct ajp13_worker {
    struct sockaddr_in worker_inet_addr; /* Contains host and port */
    unsigned connect_retry_attempts;
    char *name;

    /* 
     * Open connections cache...
     * 
     * 1. Critical section object to protect the cache.
     * 2. Cache size. 
     * 3. An array of "open" endpoints.
     */
    JK_CRIT_SEC cs;
    unsigned ep_cache_sz;
    ajp13_endpoint_t **ep_cache;

    jk_worker_t worker;
};

/* 
 * endpoint, the remote which will does the work
 */
struct ajp13_endpoint {
    ajp13_worker_t *worker;

    jk_pool_t pool;
    jk_pool_atom_t buf[BIG_POOL_SIZE];

    int sd;
    int reuse;
    jk_endpoint_t endpoint;

    unsigned left_bytes_to_send;
};

/*
 * little struct to avoid multiples ptr passing
 * this struct is ready to hold upload file fd
 * to add upload persistant storage
 */
struct ajp13_operation {
    jk_msg_buf_t    *request;   /* original request storage */
    jk_msg_buf_t    *reply;     /* reply storage (chuncked by ajp13 */
    int     uploadfd;   /* future persistant storage id */
    int     recoverable;    /* if exchange could be conducted on another TC */
};


int JK_METHOD ajp13_worker_factory(jk_worker_t **w,
                                   const char *name,
                                   jk_logger_t *l);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_AJP13_WORKER_H */
