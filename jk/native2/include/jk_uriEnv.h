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
struct jk_webapp;

struct jk_uriEnv;
typedef struct jk_uriEnv jk_uriEnv_t;

/* Standard exact mapping */
#define MATCH_TYPE_EXACT    (0)

/* Standard prefix mapping */
#define MATCH_TYPE_PREFIX  (1)

/* Standard suffix mapping ( *.jsp ) */
#define MATCH_TYPE_SUFFIX   (2)

/* Special: match all URIs of the form *ext */
#define MATCH_TYPE_GENERAL_SUFFIX (3) 

/* Special: match all context path URIs with a path component suffix */
#define MATCH_TYPE_CONTEXT_PATH (4)

/* The uriEnv corresponds to a virtual host */
#define MATCH_TYPE_HOST  (5)

/* Top level context mapping. WEB-INF and META-INF will be checked,
   and the information will be passed to tomcat
*/
#define MATCH_TYPE_CONTEXT  (6)

/* Regular Expression match */ 
#define MATCH_TYPE_REGEXP  (7)

struct jk_uriEnv {
    struct jk_bean *mbean;
    
    struct jk_pool *pool;
    
    struct jk_workerEnv *workerEnv;

    struct jk_uriMap *uriMap;

    /* Generic name/value properties. 
     */
    struct jk_map *properties;

    /* -------------------- Properties extracted from the URI name ---------- */
    /** Full name */
    char *name;
    
    /* Virtual server handled - '*' means 'global' ( visible in all
     * virtual servers ). Part of the uri name.
     */
    char *virtual;

    /* Virtual server port - '0' means 'all' ( visible in all
     * ports on the virtual servers ). Part of the uri name.
     */
    int port;

    /* Original uri ( unparsed ). Part of the uri name.
     */
    char *uri;

    /* -------------------- Properties set using setAttribute ---------- */
    /** ContextPath. Set with 'context' attribute.
     */
    char *contextPath;
    int ctxt_len;
    
    /** ServletName. Set with 'servlet' attribute.
     */
    char *servlet;
    int servletId;

    /** Group, set with 'group' attribute. Defaults to 'lb'.
     */
    char *workerName; 
    struct jk_worker *worker;

    /** For MATCH_TYPE_HOST, the list of aliases for the virtual host.
     *  Set using (multi-value ) 'alias' attribute on vhost uris.
    */
    struct jk_map *aliases;

    /* If set we'll use apr_time to get the request time in microseconds and update
       the scoreboard to reflect that. 
    */
    int timing;
    
    /* -------------------- Properties extracted from the uri, at init() -------------------- */
    /* Extracted suffix, for extension-based mathces */
    char *suffix;
    int suffix_len;

    /* Prefix based mapping. Same a contextPath for MATCH_TYPE_CONTEXT
     */
    char *prefix;
    int prefix_len;

    int match_type;
    
    /* Regular Expression structure
     */ 
    void *regexp;
    /** For MATCH_TYPE_HOST, the list of webapps in that host
     */
    struct jk_map *webapps;

    /** For MATCH_TYPE_CONTEXT, the list of local mappings
     */
    struct jk_map *exactMatch;
    struct jk_map *prefixMatch;
    struct jk_map *suffixMatch;
    struct jk_map *regexpMatch;

    /** For MATCH_TYPE_CONTEXT, the config used to read properties
        for that context.
        For MATCH_TYPE_HOST, the config used to read contexts 
        For MATCH_TYPE_HOST/default it also contains all vhosts

        If NULL - no config was attached.
        ( this will be used in future for run-time deployment )
     */

    struct jk_config *config;

    /* -------------------- Other properties -------------------- */


    /** Different apps can have different loggers.
     */
    struct jk_logger *l;

    /* Environment variables support
     */
    int envvars_in_use;
    struct jk_map *envvars;

    int merged;

    int inherit_globals;
    
    /** XXX .
     */
/*     int status; */
/*     int virtualPort; */

    /* -------------------- Methods -------------------- */

    int (*init)( struct jk_env *env, struct jk_uriEnv *_this);

};



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JK_URIENV_H */
