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
 * Description: Various utility functions                                  *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision$                                          *
 ***************************************************************************/
#ifndef _JK_UTIL_H
#define _JK_UTIL_H

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_logger.h"
#include "jk_service.h"

int jk_parse_log_level(const char *level);

int jk_open_file_logger(jk_logger_t **l, const char *file, int level);

int jk_close_file_logger(jk_logger_t **l);

int jk_log(jk_logger_t *l,
           const char *file, int line, const char *funcname, int level,
           const char *fmt, ...);

/* [V] Two general purpose functions. Should ease the function bloat. */
int jk_get_worker_str_prop(jk_map_t *m,
                           const char *wname, const char *pname, char **prop);

int jk_get_worker_int_prop(jk_map_t *m,
                           const char *wname, const char *pname, int *prop);

char *jk_get_worker_host(jk_map_t *m, const char *wname, const char *def);

char *jk_get_worker_type(jk_map_t *m, const char *wname);

int jk_get_worker_port(jk_map_t *m, const char *wname, int def);

int jk_get_worker_cache_size(jk_map_t *m, const char *wname, int def);

int jk_get_worker_socket_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_socket_keepalive(jk_map_t *m, const char *wname, int def);

int jk_get_worker_cache_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_recovery_opts(jk_map_t *m, const char *wname, int def);

int jk_get_worker_connect_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_reply_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_prepost_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_recycle_timeout(jk_map_t *m, const char *wname, int def);

char *jk_get_worker_secret_key(jk_map_t *m, const char *wname);

void jk_set_log_format(const char *logformat);

int jk_get_worker_list(jk_map_t *m, char ***list, unsigned *num_of_wokers);

int jk_get_lb_factor(jk_map_t *m, const char *wname);

int jk_get_is_sticky_session(jk_map_t *m, const char *wname);

int jk_get_is_local_worker(jk_map_t *m, const char *wname);

int jk_get_local_worker_only_flag(jk_map_t *m, const char *lb_wname);

int jk_get_lb_worker_list(jk_map_t *m,
                          const char *lb_wname,
                          char ***list, unsigned *num_of_wokers);

char *jk_get_worker_secret(jk_map_t *m, const char *wname);

int jk_get_worker_mx(jk_map_t *m, const char *wname, unsigned *mx);

int jk_get_worker_ms(jk_map_t *m, const char *wname, unsigned *ms);

int jk_get_worker_classpath(jk_map_t *m, const char *wname, char **cp);


int jk_get_worker_bridge_type(jk_map_t *m, const char *wname, unsigned *bt);

int jk_get_worker_jvm_path(jk_map_t *m, const char *wname, char **vm_path);

int jk_get_worker_callback_dll(jk_map_t *m,
                               const char *wname, char **cb_path);

int jk_get_worker_cmd_line(jk_map_t *m, const char *wname, char **cmd_line);

int jk_file_exists(const char *f);

int jk_is_path_poperty(const char *prp_name);

int jk_is_cmd_line_poperty(const char *prp_name);

int jk_get_worker_stdout(jk_map_t *m, const char *wname, char **stdout_name);

int jk_get_worker_stderr(jk_map_t *m, const char *wname, char **stderr_name);

int jk_get_worker_sysprops(jk_map_t *m, const char *wname, char **sysprops);

int jk_get_worker_libpath(jk_map_t *m, const char *wname, char **libpath);

char **jk_parse_sysprops(jk_pool_t *p, const char *sysprops);


void jk_append_libpath(jk_pool_t *p, const char *libpath);

void jk_init_ws_service(jk_ws_service_t *s);


#define TC32_BRIDGE_TYPE    32
#define TC33_BRIDGE_TYPE    33
#define TC40_BRIDGE_TYPE    40
#define TC41_BRIDGE_TYPE    41
#define TC50_BRIDGE_TYPE    50

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _JK_UTIL_H */
