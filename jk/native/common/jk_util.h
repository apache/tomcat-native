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
 * Description: Various utility functions                                  *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                               *
 ***************************************************************************/
#ifndef _JK_UTIL_H
#define _JK_UTIL_H

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_logger.h"
#include "jk_service.h"

int jk_parse_log_level(const char *level);

int jk_open_file_logger(jk_logger_t **l,
                        const char *file,
                        int level);

int jk_close_file_logger(jk_logger_t **l);

int jk_log(jk_logger_t *l,
           const char *file,
           int line,
           int level,
           const char *fmt, ...);

/* [V] Two general purpose functions. Should ease the function bloat. */
int jk_get_worker_str_prop(jk_map_t *m,
			   const char *wname,
			   const char *pname,
			   char **prop);

int jk_get_worker_int_prop(jk_map_t *m,
			   const char *wname,
			   const char *pname,
			   int *prop);

char *jk_get_worker_host(jk_map_t *m,
                         const char *wname,
                         const char *def);

char *jk_get_worker_type(jk_map_t *m,
                         const char *wname);

int jk_get_worker_port(jk_map_t *m,
                       const char *wname,
                       int def);

int jk_get_worker_cache_size(jk_map_t *m, 
                             const char *wname,
                             int def);

void jk_set_log_format(char *logformat);

int jk_get_worker_list(jk_map_t *m,
                       char ***list,
                       unsigned *num_of_wokers);

double jk_get_lb_factor(jk_map_t *m, 
                        const char *wname);

int jk_get_lb_worker_list(jk_map_t *m, 
                          const char *lb_wname,
                          char ***list, 
                          unsigned *num_of_wokers);

int jk_get_worker_mx(jk_map_t *m, 
                     const char *wname,
                     unsigned *mx);

int jk_get_worker_ms(jk_map_t *m, 
                     const char *wname,
                     unsigned *ms);

int jk_get_worker_classpath(jk_map_t *m, 
                            const char *wname, 
                            char **cp);


int jk_get_worker_jvm_path(jk_map_t *m, 
                           const char *wname, 
                           char **vm_path);

int jk_get_worker_callback_dll(jk_map_t *m, 
                               const char *wname, 
                               char **cb_path);

int jk_get_worker_cmd_line(jk_map_t *m, 
                           const char *wname, 
                           char **cmd_line);

int jk_file_exists(const char *f);

int jk_is_path_poperty(const char *prp_name);

int jk_is_cmd_line_poperty(const char *prp_name);

int jk_get_worker_stdout(jk_map_t *m, 
                         const char *wname, 
                         char **stdout_name);

int jk_get_worker_stderr(jk_map_t *m, 
                         const char *wname, 
                         char **stderr_name);

int jk_get_worker_sysprops(jk_map_t *m, 
                           const char *wname, 
                           char **sysprops);

int jk_get_worker_libpath(jk_map_t *m, 
                          const char *wname, 
                          char **libpath);

char **jk_parse_sysprops(jk_pool_t *p, 
                         const char *sysprops);
        

void jk_append_libpath(jk_pool_t *p, 
                       const char *libpath);

void jk_init_ws_service(jk_ws_service_t *s);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _JK_UTIL_H */
