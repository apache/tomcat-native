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
 * Web application, coresponding to a tomcat Context.  
 * 
 *
 * @author Henri Gomez
 * @author Costin Manolache
 */

#ifndef JK_WEBAPP_H
#define JK_WEBAPP_H

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
struct jk_uriMap;
struct jk_logger;

struct jk_webapp;
typedef struct jk_webapp jk_webapp_t;

struct jk_webapp {
    struct jk_workerEnv *workerEnv;

    /** Pool where the app was created. Will be reset when
     *  the webapp is reloaded.
     */
    struct jk_pool *pool;

    /* Generic name/value properties. Some workers may use it.
     */
    struct jk_map *properties;
    
    /** Worker associated with this application.
        Inherited by virtual host. Defaults to the first declared worker.
        ( if null == use default worker ).
    */
    struct jk_worker *worker;
    
    /** worker name - allow this to be set before the worker is defined.
     *  worker will be set on init.
     */
    char *workerName; 

    /* 'Local' mappings. This is the information from web.xml, either
       read from file or received from tomcat
    */
    struct jk_uriMap *uriMap;
    
    /* Virtual server handled - NULL means 'global' ( visible in all
     * virtual servers ). This is the canonical name.
     */
    char *virtual;
    int virtualPort;

    /** Uri we're monted on
     *  The 'id' is the index in the context table ( todo ), it
     *  reduce the ammount of trafic ( and string creation on java )
     */
    char *context;
    int ctxt_len;
    int contextId;

    /** Document base. XXX In apache2 we can add an "Alias" on the fly
     */
    char *docbase;

    /** XXX Implement.
     *   
     */
    int status;
    
    /** You can fine-tune the logging level per location
     */
    int log_level;

    /** Different apps can have different loggers.
     */
    struct jk_logger *l;

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
    struct jk_map *envvars;

    /* -------------------- Methods -------------------- */

    /* get/setProperty */
};


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif 
