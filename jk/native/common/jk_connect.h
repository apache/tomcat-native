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
 * Description: Socket connections header file                             *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                               *
 ***************************************************************************/

#ifndef JK_CONNECT_H
#define JK_CONNECT_H

#include "jk_logger.h"
#include "jk_global.h"

#ifndef WIN32
    #define closesocket         close
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int jk_resolve(char *host,
               int port,
               struct sockaddr_in *rc);

int jk_open_socket(struct sockaddr_in *addr, 
                   int ndelay,
                   int keepalive,
                   jk_logger_t *l);

int jk_close_socket(int s);

int jk_tcp_socket_sendfull(int sd, 
                           const unsigned char *b,
                           int len);

int jk_tcp_socket_recvfull(int sd, 
                           unsigned char *b, 
                           int len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_CONNECT_H */
