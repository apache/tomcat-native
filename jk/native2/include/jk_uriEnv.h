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
 * URI enviroment. All properties associated with a particular URI will be
 * stored here. This coresponds to a per_dir structure in Apache. 
 *
 * Replaces uri_worker_map, etc.
 *
 * Author:      Costin Manolache
 */

#ifndef JK_URIENV_H
#define JK_URIENV_H

#include "jk_logger.h"
#include "jk_endpoint.h"
#include "jk_worker.h"
#include "jk_map.h"
#include "jk_uriMap.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct jk_worker;
struct jk_endpoint;
struct jk_env;
struct jk_uri_worker_map;
struct jk_map;

#define MATCH_TYPE_EXACT    (0)
#define MATCH_TYPE_CONTEXT  (1)
#define MATCH_TYPE_SUFFIX   (2)
#define MATCH_TYPE_GENERAL_SUFFIX (3) /* match all URIs of the form *ext */


struct jk_uriEnv {
    struct jk_workerEnv *workerEnv;

    /* Generic name/value properties. Some workers may use it.
     */
    struct jk_map *properties;
    
    /** Worker associated with this location.
        Inherited by virtual host, context ( i.e. you can override
        it if you want, the default will be the 'global' default worker,
        or the virtual host worker ).
    */
    struct jk_worker *worker;
    /** worker name - allow this to be set before the worker is defined.
     *  XXX do we need it ?
     */
    char *workerName; 
    
    /* Virtual server handled - "*" is all virtual. 
     */
    char *virtual;

    char *uri;
    
    /** Web application. No need to compute it again in tomcat.
     *  The 'id' is the index in the context table ( todo ), it
     *  reduce the ammount of trafic ( and string creation on java )
     */
    char *context;
    int contextId;

    /** Servlet. No need to compute it again in tomcat
     */
    char *servlet;
    int servletId;

    /* ---------- For our internal mapper use ---------- */
    char *suffix;
    unsigned int ctxt_len;
    int match_type;

    /** You can fine-tune the logging level per location
     */
    int log_level;

    /** Different apps can have different loggers.
     */
    jk_logger_t *l;

    /* SSL Support - you can fine tune this per application.
     * ( most likely you only do it per virtual host or globally )
     * XXX shouldn't SSL be enabled by default ???
     */
    int  ssl_enable;
    char *https_indicator;
    char *certs_indicator;
    char *cipher_indicator;
    char *session_indicator;
    char *key_size_indicator;

    /* Jk Options. Bitflag. 
     */
    int options;

    /* Environment variables support
     */
    int envvars_in_use;
    jk_map_t * envvars;

    /* -------------------- Methods -------------------- */

    /* get/setProperty */
};


typedef struct jk_uriEnv jk_uriEnv_t;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_URIENV_H */
