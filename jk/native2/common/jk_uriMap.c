/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2002 The Apache Software Foundation.          *
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
 * Description: URI to worker map object.                          
 * Maps can be                                                     
 *                                                                 
 * Exact Context -> /exact/uri=worker e.g. /examples/do[STAR]=ajp12
 * Context Based -> /context/[STAR]=worker e.g. /examples/[STAR]=ajp12
 * Context and suffix ->/context/[STAR].suffix=worker e.g. /examples/[STAR].jsp=ajp12
 *                                                                         
 * This lets us either partition the work among the web server and the     
 * servlet container.                                                      
 *                                                                         
 * @author: Gal Shachor <shachor@il.ibm.com>
 * @author: Costin Manolache
 */

#include "jk_pool.h"
#include "jk_env.h"
#include "jk_uriMap.h"
#include "jk_registry.h"

static INLINE const char *jk2_findExtension(jk_env_t *env, const char *uri);

static jk_uriEnv_t *jk2_uriMap_mapUri(jk_env_t *env, jk_uriMap_t *uriMap,
                                      const char *vhost,
                                      const char *uri);

static int jk2_uriMap_checkUri(jk_env_t *env, jk_uriMap_t *uriMap, 
                               const char *uri);

/*
 * We are now in a security nightmare, it maybe that somebody sent 
 * us a uri that looks like /top-secret.jsp. and the web server will 
 * fumble and return the jsp content. 
 *
 * To solve that we will check for path info following the suffix, we 
 * will also check that the end of the uri is not ".suffix.",
 * ".suffix/", or ".suffix ".
 *
 * Was: check_security_fraud
 */
static int jk2_uriMap_checkUri(jk_env_t *env, jk_uriMap_t *uriMap, 
                               const char *uri)
{
    int i;    

    for(i = 0 ; i < uriMap->maps->size( env, uriMap->maps ) ; i++) {
        jk_uriEnv_t *uriEnv=uriMap->maps->valueAt( env, uriMap->maps, i );
        
        if(MATCH_TYPE_SUFFIX == uriEnv->match_type) {
            char *suffix_start;
            for(suffix_start = strstr(uri, uriEnv->suffix) ;
                suffix_start ;
                suffix_start =
                    strstr(suffix_start + 1, uriEnv->suffix)) {
                
                if('.' != *(suffix_start - 1)) {
                    continue;
                } else {
                    char *after_suffix = suffix_start +
                        strlen(uriEnv->suffix);
                
                    if((('.' == *after_suffix) ||
                        ('/' == *after_suffix) ||
                        (' ' == *after_suffix)) &&
                       (0 == strncmp(uriEnv->prefix, uri,
                                     uriEnv->prefix_len))) {
                        /* 
                         * Security violation !!!
                         * this is a fraud.
                         */
                        return JK_ERR;
                    }
                }
            }
        }
    }

    return JK_OK;
}


/** Add a uri mapping. Called during uri: initialization. Will just copy the
    uri in the table ( XXX use a map keyed on name ). In init() we process this
    and set the right structures.
 */
static int jk2_uriMap_addUriEnv( jk_env_t *env, jk_uriMap_t *uriMap, jk_uriEnv_t *uriEnv )
{
    uriMap->maps->put( env, uriMap->maps, uriEnv->name, uriEnv, NULL );
    if( uriMap->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.addUriEnv() %s %s %s\n", uriEnv->name, uriEnv->virtual, uriEnv->uri);
    return JK_OK;
}

static int JK_METHOD jk2_uriMap_setProperty(jk_env_t *env, jk_bean_t *mbean,
                                  char *name, void *valueP)
{
    jk_uriMap_t *uriMap=mbean->object;
    char *value=valueP;
    
    return JK_OK;
}

static jk_uriEnv_t *jk2_uriMap_prefixMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                         jk_map_t *mapTable, const char *uri, 
                                         int uriLen, int reverse)
{
    int best_match=0;
    jk_uriEnv_t *match=NULL;
    int i;
    
    int sz=mapTable->size( env, mapTable);
    for(i = 0 ; i < sz ; i++) {
        jk_uriEnv_t *uwr=mapTable->valueAt( env, mapTable, i);

        if( uriLen < uwr->prefix_len ) continue;
        if( strncmp( uri, uwr->prefix, uwr->prefix_len ) == 0 ) {
            if( uwr->prefix_len > best_match ) {
                best_match=uwr->prefix_len;
                if (reverse > 0) {
                    if (uwr->reverse)
                        match=uwr;                
                }
                else if (!uwr->reverse)
                    match=uwr;
            }
        }
    }
    return match;
}

static jk_uriEnv_t *jk2_uriMap_exactMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                        jk_map_t *mapTable, const char *uri, 
                                        int uriLen, int reverse)
{
    int i;
    int sz=mapTable->size( env, mapTable);
    for(i = 0 ; i < sz ; i++) {
        jk_uriEnv_t *uwr=mapTable->valueAt( env, mapTable, i);
        
        if( uriLen != uwr->prefix_len ) continue;
        if( strncmp( uri, uwr->prefix, uriLen ) == 0 ) {
            if (reverse > 0) {
                if (uwr->reverse)
                    return uwr;                
            }
            else if (!uwr->reverse)
                return uwr;
        }
    }
    return NULL;
}

static jk_uriEnv_t *jk2_uriMap_suffixMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                         jk_map_t *mapTable, const char *suffix, 
                                         int suffixLen, int reverse)
{
    int i;
    int sz=mapTable->size( env, mapTable);
    for(i = 0 ; i < sz ; i++) {
        jk_uriEnv_t *uwr=mapTable->valueAt( env, mapTable, i);

        /* for WinXX, fix the JsP != jsp problems */
#ifdef WIN32                        
        if(0 == strcasecmp(suffix, uwr->suffix))  {
#else
            if(0 == strcmp(suffix, uwr->suffix)) {
#endif
                if( uriMap->mbean->debug > 0 ) {
                    env->l->jkLog(env, env->l,JK_LOG_DEBUG,
                                  "uriMap.mapUri() suffix match %s\n",
                                  uwr->suffix );
                }
                if (reverse > 0) {
                    if (uwr->reverse)
                        return uwr;                
                }
                else if (!uwr->reverse)
                    return uwr;
            /* indentation trick */
#ifdef WIN32                        
            }
#else
        }
#endif
    }
    return NULL;
}

/* Find the vhost */
static jk_uriEnv_t *jk2_uriMap_hostMap(jk_env_t *env, jk_uriMap_t *uriMap,
                                       const char *vhost)
{
    int i;
    
    if( vhost!=NULL ) {
        int sz=uriMap->vhosts->size( env, uriMap->vhosts);
        for(i = 0 ; i < sz ; i++) {
            char *name=uriMap->vhosts->nameAt( env, uriMap->vhosts, i);
            
            /* Host name is not case sensitive */
            if( strcasecmp( name, vhost ) == 0 ) {
                return uriMap->vhosts->valueAt( env, uriMap->vhosts, i);
            }
        }
        /* Can't find vhost, return default */
    }
    return uriMap->defaultVhost;
}

static void jk2_uriMap_correctHosts(jk_env_t *env, jk_uriMap_t *uriMap) {
    int i;
    jk_bean_t *mbean;

    /* XXX Make sure all vhosts are created and we didn't miss any */
    for(i = 0 ; i < uriMap->maps->size( env, uriMap->maps ) ; i++) {
        jk_uriEnv_t *uriEnv=uriMap->maps->valueAt( env, uriMap->maps, i );

        char *vhost= uriEnv->virtual;
        if( vhost != NULL ) {
            jk_env_t *hostEnv=uriMap->vhosts->get( env, uriMap->vhosts,
                                                   vhost );
            if( hostEnv == NULL ) {
                env->l->jkLog( env, env->l, JK_LOG_INFO,
                               "uriMap: creating vhost %s\n", vhost);
                mbean=env->getBean2( env, "uri", vhost );
                if( mbean==NULL )
                    mbean=env->createBean2(env, uriMap->pool,"uri", vhost);
                if( mbean==NULL || mbean->object==NULL ) {
                    env->l->jkLog( env, env->l, JK_LOG_ERROR,
                                   "uriMap: can't create vhost object %s\n", vhost);
                    continue;
                } 
                uriMap->vhosts->put( env, uriMap->vhosts,
                                     vhost, mbean->object, NULL );
            }
        }
    }
}

static void jk2_uriMap_correctWebapps(jk_env_t *env, jk_uriMap_t *uriMap) {
    int i;
    jk_bean_t *mbean;

    /* Init all contexts */
    /* For each context, init the local uri maps */
    for(i = 0 ; i < uriMap->maps->size( env, uriMap->maps ) ; i++) {
        jk_uriEnv_t *uriEnv=uriMap->maps->valueAt( env, uriMap->maps, i );
        
        char *vhost= uriEnv->virtual;
        jk_uriEnv_t *hostEnv=jk2_uriMap_hostMap( env, uriMap, vhost );
        char *context= uriEnv->contextPath;
        jk_uriEnv_t *ctxEnv;
        
        if( context==NULL ) {
            env->l->jkLog( env, env->l, JK_LOG_ERROR,
                           "uriMap: no context %s\n", uriEnv->uri );
            continue;
        }
        
        ctxEnv=jk2_uriMap_prefixMap( env, uriMap, hostEnv->webapps, context, strlen( context), -1);
        /* if not alredy created, create it */
        if( ctxEnv == NULL ) {
            env->l->jkLog( env, env->l, JK_LOG_INFO,
                           "uriMap: creating context %s\n", vhost);
            mbean=env->getBean2( env, "uri", context );
            if( mbean==NULL )
                mbean=env->createBean2(env, uriMap->pool,"uri", context );
            if( mbean==NULL || mbean->object==NULL ) {
                env->l->jkLog( env, env->l, JK_LOG_ERROR,
                               "uriMap: can't create context object %s\n",context);
                continue;
            }
            ctxEnv=mbean->object;
            ctxEnv->match_type = MATCH_TYPE_CONTEXT;
            hostEnv->webapps->put( env, hostEnv->webapps, context, ctxEnv, NULL );
        }
    }
}

static int jk2_uriMap_init(jk_env_t *env, jk_uriMap_t *uriMap)
{
    int rc=JK_OK;
    int i;
    jk_workerEnv_t *workerEnv=uriMap->workerEnv;
    jk_bean_t *mbean;
    jk_uriEnv_t *uriEnv;

    mbean=env->getBean2( env, "uri", "" );
    if( mbean==NULL )
        mbean=env->createBean2(env, workerEnv->pool,"uri", "");
    if( mbean==NULL || mbean->object==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriMap.factory() Fail to create default host\n");
        return JK_ERR;
    }
    uriMap->defaultVhost=mbean->object;
    
    if( uriMap->mbean->debug > 5 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "uriMap.init() set default host\n"); 
    /* XXX Initializes vhosts from uris */
    jk2_uriMap_correctHosts(env,uriMap);
    /* Initialize the vhosts table */
    for(i = 0 ; i < uriMap->maps->size( env, uriMap->maps ) ; i++) {
        uriEnv=uriMap->maps->valueAt( env, uriMap->maps, i );

        if( uriEnv == NULL ) continue;
        if( uriEnv->match_type== MATCH_TYPE_HOST ) {
            jk2_map_default_create( env, & uriEnv->webapps, uriMap->pool );
            if( uriEnv->virtual!=NULL  ) {
                uriMap->vhosts->put( env, uriMap->vhosts,
                                     uriEnv->virtual, uriEnv, NULL );
            }
            if( uriMap->mbean->debug > 5 ) 
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriMap.init() loaded host %s\n",uriEnv->virtual); 
        }
    }        



    /* Add the vhost aliases ( for each vhost, by looking in the aliases ) */
    for(i = 0 ; i < uriMap->maps->size( env, uriMap->maps ) ; i++) {
        uriEnv=uriMap->maps->valueAt( env, uriMap->maps, i );
        if( uriEnv->match_type== MATCH_TYPE_HOST  &&
            uriEnv->virtual!=NULL  ) {
            
            
            /* XXX TODO */
            
        }
    }    
    
    /** Make sure each vhost has a default context
     */
    for(i = 0 ; i < uriMap->vhosts->size( env, uriMap->vhosts ) ; i++) {
        jk_uriEnv_t *hostEnv=uriMap->vhosts->valueAt( env, uriMap->vhosts, i );
        jk_uriEnv_t *rootCtx;
        char *uriPath;
        
        if( hostEnv->virtual != NULL ) {
            uriPath=env->tmpPool->calloc( env, env->tmpPool,
                                          strlen( hostEnv->virtual ) + 3 );
            strcpy( uriPath, hostEnv->virtual );
            strcat( uriPath, "/" );
        } else {
            uriPath="/";
        }
        
        rootCtx=env->getByName2( env, "uri", uriPath );
        if( rootCtx==NULL ) {
            env->createBean2( env, uriMap->mbean->pool, "uri", uriPath );
            rootCtx=env->getByName2( env, "uri", uriPath );
            if( uriMap->mbean->debug > 0 ) 
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriMap.init() Create default context %s\n", uriPath );
            rootCtx->mbean->setAttribute( env, rootCtx->mbean, "context", "/" );
        }
    }

    uriEnv=env->getByName2( env, "uri", "/" );
    if( uriEnv==NULL ) {
        env->createBean2( env, uriMap->mbean->pool, "uri", "/" );
        uriEnv=env->getByName2( env, "uri", "/" );
        if( uriMap->mbean->debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriMap.init() Create default context / ( for default host )\n" );
        uriEnv->mbean->setAttribute( env, uriEnv->mbean, "context", "/" );
    }
    
    /* Init all contexts */
    /* For each context, init the local uri maps */
    for(i = 0 ; i < uriMap->maps->size( env, uriMap->maps ) ; i++) {
        jk_uriEnv_t *uriEnv=uriMap->maps->valueAt( env, uriMap->maps, i );
        char *uri;
        char *context;
        if( uriEnv==NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "uriMap.init() NPE\n");
        }
        uri=uriEnv->uri;
        context= uriEnv->contextPath;

        if( uri!=NULL && context!=NULL && strcmp( uri, context ) == 0 ) {
            char *vhost= uriEnv->virtual;
            jk_uriEnv_t *hostEnv=jk2_uriMap_hostMap( env, uriMap, vhost );

            if( uriMap->mbean->debug > 5 ) 
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriMap.init() loaded context %s %s %#lx %#lx %#lx\n",
                              uriEnv->virtual, context, hostEnv, hostEnv->webapps,
                              uriMap->pool); 
            uriEnv->match_type=MATCH_TYPE_CONTEXT;

            uriEnv->prefix=context;
            uriEnv->prefix_len=strlen( context );
            hostEnv->webapps->put( env, hostEnv->webapps, context, uriEnv, NULL );
            jk2_map_default_create( env, & uriEnv->exactMatch, uriMap->pool );
            jk2_map_default_create( env, & uriEnv->prefixMatch, uriMap->pool );
            jk2_map_default_create( env, & uriEnv->suffixMatch, uriMap->pool );

            /* add default mappings for WEB-INF, META-INF, servlet/, .jsp */
        }
    }

    if( uriMap->mbean->debug > 5 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.init() processing mappings\n");

    /* All other mappings are added in the right context leaf.
       XXX We should also sort prefix mappings and maybe use binary search - but
       it won't have too bigger benefits, the number of mappings per ctx is typically 
       small
     */
    for(i = 0 ; i < uriMap->maps->size( env, uriMap->maps ) ; i++) {
        jk_uriEnv_t *uriEnv=uriMap->maps->valueAt( env, uriMap->maps, i );

        char *vhost= uriEnv->virtual;
        jk_uriEnv_t *hostEnv=jk2_uriMap_hostMap( env, uriMap, vhost );
        
        char *uri= uriEnv->uri;
        jk_uriEnv_t *ctxEnv;

        if( uri==NULL ) continue;
        uriEnv->uriMap=uriMap;
        uriEnv->init( env, uriEnv );

        if( uri==NULL ) continue;
        
        ctxEnv=jk2_uriMap_prefixMap( env, uriMap, hostEnv->webapps, uri, strlen( uri ), -1 );

        if( ctxEnv==NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO, "uriMap.init() no context for %s\n", uri); 
            return JK_ERR;
        }
        
        if( MATCH_TYPE_EXACT == uriEnv->match_type ) {
            ctxEnv->exactMatch->add( env, ctxEnv->exactMatch, uri, uriEnv );
        } else if( MATCH_TYPE_SUFFIX == uriEnv->match_type ) {
            ctxEnv->suffixMatch->add( env, ctxEnv->suffixMatch, uri, uriEnv );
        } else if (MATCH_TYPE_PREFIX== uriEnv->match_type ) {
            ctxEnv->prefixMatch->add( env, ctxEnv->prefixMatch, uri, uriEnv );
        }
    }
    return rc;
}

static void jk2_uriMap_destroy(jk_env_t *env, jk_uriMap_t *uriMap)
{

    if( uriMap->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "uriMap.destroy()\n"); 

    /* this can't be null ( or a NPE would have been generated */
    uriMap->pool->close(env, uriMap->pool);
}


/* returns the index of the last occurrence of the 'ch' character
   if ch=='\0' returns the length of the string str  */
static INLINE int jk2_last_index_of(const char *str,char ch)
{
    const char *str_minus_one=str-1;
    const char *s=str+strlen(str);
    while(s!=str_minus_one && ch!=*s) {
	--s;
    }
    return (s-str);
}

/* find the suffix - only once. We also make sure
   we check only the last component, as required by
   servlet spec
*/
static INLINE const char *jk2_findExtension(jk_env_t *env, const char *uri) {
    int suffix_start;
    const char *suffix;
    
    for(suffix_start = strlen(uri) - 1 ; 
        suffix_start > 0; 
        suffix_start--) {
        if( '.' == uri[suffix_start] ||
            '/' == uri[suffix_start] )
            break;
    }
    if( '.' != uri[suffix_start] ) {
        suffix_start=-1;
        suffix = NULL;
    } else {
        suffix_start++;
        suffix = uri + suffix_start;
    }
    return suffix;
}

#define SAFE_URI_SIZE 8192

static jk_uriEnv_t *jk2_uriMap_map(jk_env_t *env, jk_uriMap_t *uriMap,
                                   const char *vhost,
                                   const char *uri, int reverse)
{
    int best_match = -1;
    int longest_match = 0;
    char * clean_uri = NULL;
    char *url_rewrite=NULL;
    const char *suffix;
    int uriLen;
    jk_uriEnv_t *hostEnv;
    jk_uriEnv_t *ctxEnv;
    jk_uriEnv_t *match;
    
    /* Ugly hack to avoid using non-thread safe code.
       Modify the uri in place for uri session encoding, then
       restore it to the original. That works since the processing
       happens in a single thred. A better solution is to allocate
       the jk_ws_service and it's pool and pass it as param */
    char origChar='\0';
    
    /* XXX - need to make sure prefix match take precedence over
       extension match ( now it doesn't )
    */
    
    if( uriMap == NULL || uri==NULL ) 
	return NULL;
    
    if( uriMap->mbean->debug > 1 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.mapUri() %s %s\n", vhost, uri);    

    if( '/' != uri[0]) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriMap.mapUri() uri must start with /\n");
        return NULL;
    }

    hostEnv=jk2_uriMap_hostMap( env, uriMap, vhost );

    if( uriMap->mbean->debug > 1 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.mapUri() found host %s\n", hostEnv->virtual);    

    url_rewrite = strstr(uri, JK_PATH_SESSION_IDENTIFIER);
        
    if(url_rewrite) {
        origChar=*url_rewrite;
        *url_rewrite = '\0';
        if( uriMap->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriMap.mapUri() rewrote uri %s \n",uri );
    }

    uriLen=strlen( uri );

    /* Map the context */
    ctxEnv=jk2_uriMap_prefixMap( env, uriMap, hostEnv->webapps, uri, uriLen, 0 );

    if( ctxEnv==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriMap.mapUri() no context %s\n", uri);    
        return NULL;
    }
    
    if( uriMap->mbean->debug > 1 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.mapUri() found ctx %s\n", ctxEnv->uri);    

    /* As per Servlet spec, do exact match first */
    match=jk2_uriMap_exactMap( env, uriMap, ctxEnv->exactMatch, uri, uriLen, reverse );
    if( match != NULL && 
            (( match->virtual==NULL && hostEnv->virtual==NULL ) || 
             ( hostEnv->virtual!=NULL && strcasecmp(match->virtual,hostEnv->virtual)==0 ))
       ) {
        /* restore */
        if( url_rewrite ) *url_rewrite=origChar;
        if( uriMap->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "uriMap.mapUri() exact match %s %s\n",
                          uri, match->workerName ); 
        return match;
    }
    
    /* Then prefix match */
    match=jk2_uriMap_prefixMap( env, uriMap, ctxEnv->prefixMatch, uri, uriLen, reverse );
    if( match != NULL && 
            (( match->virtual==NULL && hostEnv->virtual==NULL ) || 
             ( hostEnv->virtual!=NULL && strcasecmp(match->virtual,hostEnv->virtual)==0 ))
       ) {
        char c=uri[match->prefix_len];
        /* XXX Filter prefix matches to allow only exact 
               matches with an optional path_info or query string at end.
               Fixes Bugzilla#12141, needs review..
        */
        if (( uriLen > match->prefix_len && ( c=='/' || c=='?' ) ) ||
              uriLen == match->prefix_len ) {
            /* restore */
            if( url_rewrite ) *url_rewrite=origChar;
            if( uriMap->mbean->debug > 0 )
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriMap.mapUri() prefix match %s %s\n",
                              uri, match->workerName ); 
            return match;
        }
    }

    /* And extension match at the end */
    /* Only once, no need to compute it for each extension match */
    suffix=jk2_findExtension( env, uri );
    if( suffix!=NULL ) {
        match=jk2_uriMap_suffixMap( env, uriMap, ctxEnv->suffixMatch,
                                    suffix, strlen( suffix ), reverse);
        if( match != NULL && 
                (( match->virtual==NULL && hostEnv->virtual==NULL ) || 
                 ( hostEnv->virtual!=NULL && strcasecmp(match->virtual,hostEnv->virtual)==0 ))
           ) {
            /* restore */
            if( url_rewrite ) *url_rewrite=origChar;
            if( uriMap->mbean->debug > 0 )
                env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                              "uriMap.mapUri() extension match %s %s\n",
                              uri, match->workerName ); 
            return match;
        }
    }
        
/*     for(i = 0 ; i < ctxEnv->ctxMap->size( env, ctxEnv->ctxMap ) ; i++) { */
/*         jk_uriEnv_t *uwr=ctxEnv->ctxMap->valueAt( env, ctxEnv->ctxMap, i ); */
        
/*         if(MATCH_TYPE_GENERAL_SUFFIX == uwr->match_type) { */
/*             if(uwr->prefix_len >= longest_match) { */
/*                 int suffix_start=jk2_last_index_of(uri,uwr->suffix[0]); */
/*                 if (suffix_start>=0 && 0==strcmp(uri+suffix_start,uwr->suffix)) { */
/*                     if( uriMap->mbean->debug > 0 ) */
/*                         env->l->jkLog(env, env->l, JK_LOG_INFO, */
/*                                     "uriMap.mapUri() general suffix match %s\n", */
/*                                       uwr->suffix ); */
/*                     longest_match = uwr->prefix_len; */
/*                     best_match = i; */
/*                 } */
/*             } */
/*         } else if(MATCH_TYPE_CONTEXT_PATH == uwr->match_type) { */
/*             char *suffix_path = NULL; */
/*             if (strlen(uri) > 1 && (suffix_path = strchr(uri+1,'/')) != NULL) { */
/*                 if (0 == strncmp(suffix_path,uwr->suffix, strlen(uwr->suffix))) { */
/*                     if(uwr->prefix_len >= longest_match) { */
/*                         env->l->jkLog(env, env->l, JK_LOG_INFO, */
/*                                       "jk_uri_worker_map_t::map_uri_to_worker, " */
/*                                       "Found a general context path match %s -> *%s\n", */
/*                                       uwr->worker_name, uwr->suffix );    */
/*                         longest_match = uwr->prefix_len; */
/*                         best_match = i; */
/*                     } */
/*                 } */
/*             } */
/*         } */
/*     } */

    /* restore */
    if( url_rewrite )
        *url_rewrite=origChar;
        
    if( uriMap->mbean->debug > 1 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "uriMap.mapUri() no match found\n"); 

    return NULL;
}

static jk_uriEnv_t *jk2_uriMap_mapUri(jk_env_t *env, jk_uriMap_t *uriMap,
                                      const char *vhost, const char *uri)
{
    jk_uriEnv_t *match;
    
    match = jk2_uriMap_map(env, uriMap, vhost, uri, 0);
    if (match) {
        jk_uriEnv_t *rmatch = jk2_uriMap_map(env, uriMap, vhost, uri, 1);
        if (rmatch)
            return NULL;
    }
    return match;
}

int JK_METHOD jk2_uriMap_factory(jk_env_t *env, jk_pool_t *pool, jk_bean_t *result,
                                 const char *type, const char *name)
{
    jk_uriMap_t *uriMap;

    uriMap = (jk_uriMap_t *)pool->calloc(env, pool, sizeof(jk_uriMap_t));
    if(  ! uriMap) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriMap.factory() OutOfMemoryError\n");
        return JK_ERR;
    }

    uriMap->pool=pool;

    jk2_map_default_create( env, &uriMap->maps, pool );
    jk2_map_default_create( env, &uriMap->vhosts, pool );

    uriMap->init=jk2_uriMap_init;
    uriMap->destroy=jk2_uriMap_destroy;
    uriMap->addUriEnv=jk2_uriMap_addUriEnv;
    uriMap->checkUri=jk2_uriMap_checkUri;
    uriMap->mapUri=jk2_uriMap_mapUri;
            
    result->object=uriMap;
    result->setAttribute=jk2_uriMap_setProperty;
    uriMap->mbean=result;

    return JK_OK;
}

