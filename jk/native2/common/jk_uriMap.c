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
 * Author:      Gal Shachor <shachor@il.ibm.com>                           
 * Version:     $Revision$                                           
 */

#include "jk_pool.h"
#include "jk_env.h"
#include "jk_uriMap.h"

int JK_METHOD jk2_uriMap_factory(jk_env_t *env, jk_pool_t *pool, void **result,
                                 const char *type, const char *name);

static int jk2_uriMap_init(jk_env_t *env, jk_uriMap_t *_this,
                           jk_workerEnv_t *workerEnv,
                           jk_map_t *init_data);

static jk_uriEnv_t *jk2_uriMap_addMapping(jk_env_t *env, jk_uriMap_t *_this,
                                          const char *vhost,
                                          const char *puri, 
                                          const char *pworker);

static INLINE const char *jk2_findExtension(jk_env_t *env, const char *uri);

static jk_uriEnv_t *jk2_uriMap_mapUri(jk_env_t *env, jk_uriMap_t *_this,
                                      const char *vhost,
                                      const char *uri);

static int jk2_uriMap_checkUri(jk_env_t *env, jk_uriMap_t *_this, 
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
static int jk2_uriMap_checkUri(jk_env_t *env, jk_uriMap_t *_this, 
                               const char *uri)
{
    int i;    

    for(i = 0 ; i < _this->size ; i++) {
        if(MATCH_TYPE_SUFFIX == _this->maps[i]->match_type) {
            char *suffix_start;
            for(suffix_start = strstr(uri, _this->maps[i]->suffix) ;
                suffix_start ;
                suffix_start =
                    strstr(suffix_start + 1, _this->maps[i]->suffix)) {
                
                if('.' != *(suffix_start - 1)) {
                    continue;
                } else {
                    char *after_suffix = suffix_start +
                        strlen(_this->maps[i]->suffix);
                
                    if((('.' == *after_suffix) ||
                        ('/' == *after_suffix) ||
                        (' ' == *after_suffix)) &&
                       (0 == strncmp(_this->maps[i]->prefix, uri,
                                     _this->maps[i]->prefix_len))) {
                        /* 
                         * Security violation !!!
                         * this is a fraud.
                         */
                        return i;
                    }
                }
            }
        }
    }

    return -1;
}


#define UW_INC_SIZE 4   /* 4 URI->WORKER STEP */

/*
 * Ensure there will be memory in context info to store Context Bases
 */
static int jk2_uriMap_realloc(jk_env_t *env,jk_uriMap_t *_this)
{
    if (_this->size == _this->capacity) {
        jk_uriEnv_t **uwr=NULL;
        int  capacity = _this->capacity + UW_INC_SIZE;

        uwr = (jk_uriEnv_t **)
            _this->pool->alloc(env, _this->pool,
                               sizeof(jk_uriEnv_t *) * capacity);

        if (! uwr)
            return JK_FALSE;

        if (_this->capacity && _this->maps)
            memcpy(uwr, _this->maps,
                   sizeof(jk_uriEnv_t *) * _this->capacity);

        _this->maps = uwr;
        _this->capacity = capacity;
    }

    return JK_TRUE;
}

static jk_uriEnv_t *jk2_uriMap_createUriEnv(jk_env_t *env, jk_uriMap_t *_this,
                                            const char *vhost, const char *uri)
{
    jk_uriEnv_t *uriEnv=(jk_uriEnv_t *)_this->pool->calloc(env, _this->pool,
                                                           sizeof(jk_uriEnv_t));
    uriEnv->workerEnv=_this->workerEnv;
    /* XXX search the real webapp
     */
    uriEnv->webapp=_this->workerEnv->rootWebapp;
    
    return uriEnv;
}

static jk_uriEnv_t *jk2_uriMap_addMapping(jk_env_t *env, jk_uriMap_t *_this,
                                          const char *vhost,
                                          const char *puri, 
                                          const char *pworker)
{
    jk_uriEnv_t *uwr;
    char *uri;
    char *worker;
    int err;
    char *asterisk;

    /* make sure we have space */
    err=jk2_uriMap_realloc(env, _this);
    if (err != JK_TRUE ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriMap.addMappint() OutOfMemoryException\n");
        return NULL;
    }

    uwr = jk2_uriMap_createUriEnv(env, _this,vhost,puri);
    
    uri = _this->pool->pstrdup(env, _this->pool, puri);
    uwr->uri = _this->pool->pstrdup(env, _this->pool, uri);
    
    worker = _this->pool->pstrdup(env, _this->pool, pworker);
    uwr->webapp->workerName = worker;

    if (uri==NULL || worker==NULL || uwr==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                 "uriMap.addMapping() NullPointerException\n");
        return NULL;
    }

    if ('/' != uri[0]) {
        /*
         * JFC: please check...
         * Not sure what to do, but I try to prevent problems.
         * I have fixed jk_mount_context() in apaches/mod_jk2.c so we should
         * not arrive here when using Apache.
         */
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriMap.addMapping() context must start with '/' in %s\n",
                      uri);
        return NULL;
    }

    _this->maps[_this->size] = uwr;
    _this->size++;

    asterisk = strchr(uri, '*');

    // set the mapping type
    if (!asterisk) {
        /* Something like:  JkMount /login/j_security_check ajp13 */
        uwr->prefix      = uri;
        uwr->prefix_len  =strlen( uwr->prefix );
        uwr->suffix      = NULL;
        uwr->match_type  = MATCH_TYPE_EXACT;
        if( _this->debug > 0 ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "uriMap.addMapping() exact mapping %s=%s was added\n",
                          uri, worker);
        }
        return uwr;
    }

    /*
     * Now, lets check that the pattern is /context/STAR.suffix
     * or /context/STAR
     * we need to have a '/' then a '*' and the a '.' or a
     * '/' then a '*'
     */
    asterisk--;
    if ('/' == asterisk[0]) {
        if ('.' == asterisk[2]) {
            /* suffix rule: /foo/bar/STAR.extension */
            asterisk[1]      = '\0';
            asterisk[2]      = '\0';
            uwr->prefix      = uri;
            uwr->prefix_len  =strlen( uwr->prefix );
            uwr->suffix      = asterisk + 3;
            uwr->match_type  = MATCH_TYPE_SUFFIX;
            if( _this->debug > 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriMap.addMapping() suffix mapping %s.%s=%s was added\n",
                              uri, asterisk + 3, worker);
            }
        } else if ('\0' != asterisk[2]) {
            /* general suffix rule /foo/bar/STARextraData */
            asterisk[1] = '\0';
            uwr->suffix  = asterisk + 2;
            uwr->prefix  = uri;
            uwr->prefix_len  =strlen( uwr->prefix );
            uwr->match_type = MATCH_TYPE_GENERAL_SUFFIX;
            if( _this->debug > 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                         "uriMap.addMapping() general suffix mapping %s.%s=%s\n",
                              uri, asterisk + 2, worker);
            }
        } else {
            /* context based /foo/bar/STAR  */
            asterisk[1]      = '\0';
            uwr->suffix      = NULL;
            uwr->prefix      = uri;
            uwr->prefix_len  =strlen( uwr->prefix );
            uwr->match_type  = MATCH_TYPE_CONTEXT;
            if( _this->debug > 0 ) {
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                              "uriMap.addMapping() prefix mapping %s=%s\n",
                              uri, worker);
            }
        }
    } else {
        /* Something like : JkMount /servlets/exampl* ajp13 */
        /* Is this valid ??? */
        uwr->prefix      = uri;
        uwr->prefix_len  =strlen( uwr->prefix );
        uwr->suffix      = NULL;
        uwr->match_type  = MATCH_TYPE_EXACT;
        if( _this->debug > 0 ) {
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                     "uriMap.addMapping() prefix mapping2 %s=%s\n",
                          uri, worker);
        }
    }

    return uwr;
}

static int jk2_uriMap_init(jk_env_t *env, jk_uriMap_t *_this,
                           jk_workerEnv_t *workerEnv,
                           jk_map_t *init_data)
{
    int rc=JK_TRUE;
    int sz;
    int err;
    int i;

    _this->workerEnv = workerEnv;
                                           
    sz = init_data->size(env, init_data);

    _this->debug=jk2_map_getIntProp(env,
                                   init_data,"urimap", "default", "debug", 0 );
    
    for(i = 0; i < sz ; i++) {
        char *name=init_data->nameAt(env, init_data, i);
        if( name!=NULL && name[0]=='/' ) {
            jk_uriEnv_t *uriEnv=_this->addMapping(env, _this,NULL,name,
                                    init_data->valueAt(env, init_data, i));
            if ( uriEnv==NULL) {
                env->l->jkLog(env, env->l, JK_LOG_ERROR,
                              "uriMap.init() error adding %s\n",
                         init_data->nameAt(env, init_data, i));
                rc=JK_FALSE;
            }
        }
    }

    if( _this->debug > 0 )  
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriMap.init(): rules=%d properties=\n",
                      _this->size, sz );

    /* Set uriEnv->worker ( can't be done earlier since we might not have
       the workers set up */
    for(i = 0 ; i < _this->size ; i++) {
        char *wname=_this->maps[i]->webapp->workerName;
        /* assert( wname != NULL ); */
        _this->maps[i]->webapp->worker=
            workerEnv->getWorkerForName( env, workerEnv, wname );
        if( _this->maps[i]->webapp->worker==NULL ) {
            env->l->jkLog(env, env->l, JK_LOG_ERROR,
                     "uriMap.init() map to invalid worker %s %s\n",
                          _this->maps[i]->uri, wname);
        }
    }
    
    
    return rc;
}

static void jk2_uriMap_destroy(jk_env_t *env, jk_uriMap_t *_this)
{

    if( _this->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_INFO, "uriMap.destroy()\n"); 

    /* this can't be null ( or a NPE would have been generated */
    
    _this->pool->close(env, _this->pool);
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

static jk_uriEnv_t *jk2_uriMap_mapUri(jk_env_t *env, jk_uriMap_t *_this,
                                      const char *vhost,
                                      const char *uri)
{
    int i;
    int best_match = -1;
    int longest_match = 0;
    char * clean_uri = NULL;
    char *url_rewrite=NULL;
    const char *suffix;
    int uriLen;

    /* Ugly hack to avoid using non-thread safe code.
       Modify the uri in place for uri session encoding, then
       restore it to the original. That works since the processing
       happens in a single thred. A better solution is to allocate
       the jk_ws_service and it's pool and pass it as param */
    char origChar='\0';
    
    /* XXX - need to make sure prefix match take precedence over
       extension match ( now it doesn't )
    */
    
    if( _this == NULL || uri==NULL ) 
	return NULL;
    
    if( _this->size <= 0 )
	return NULL;

    if( _this->debug > 1 )
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriMap.mapUri() %s %s\n", vhost, uri);    

    if( '/' != uri[0]) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriMap.mapUri() uri must start with /\n");
        return NULL;
    }

    url_rewrite = strstr(uri, JK_PATH_SESSION_IDENTIFIER);
        
    if(url_rewrite) {
        origChar=*url_rewrite;
        *url_rewrite = '\0';
        if( _this->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "uriMap.mapUri() rewrote uri %s \n",uri );
    }

    uriLen=strlen( uri );
    
    /* Only once, no need to compute it for each extension match */
    suffix=jk2_findExtension( env, uri );

    for(i = 0 ; i < _this->size ; i++) {
        jk_uriEnv_t *uwr = _this->maps[i];
        
        if(uwr->prefix_len < longest_match) {
            /* This will also eliminate extension matches if
               a prefix match was already found */
            continue; /* can not be a best match anyway */
        }
        
        if(0 != strncmp(uwr->prefix, uri, uwr->prefix_len))
            continue;

        /* The prefix matches */
        
        if(MATCH_TYPE_EXACT == uwr->match_type &&
           uriLen == uwr->prefix_len) {
            if( _this->debug > 0 )
                env->l->jkLog(env, env->l, JK_LOG_INFO,
                              "uriMap.mapUri() exact match %s:%s \n",
                              uwr->webapp->worker->name, uwr->prefix );
            /* restore */
            if( url_rewrite ) *url_rewrite=origChar;
            return uwr;
        } else if(MATCH_TYPE_CONTEXT == uwr->match_type) {
            if(uwr->prefix_len > longest_match) {
                /* This takes care of 'shorter' matches */
                if( _this->debug > 0 )
                    env->l->jkLog(env, env->l, JK_LOG_INFO,
                             "uriMap.mapUri() tentative prefix match %s",
                                  uwr->prefix );
                longest_match = uwr->prefix_len;
                best_match = i;
            }
        } else if(MATCH_TYPE_GENERAL_SUFFIX == uwr->match_type) {
            if(uwr->prefix_len >= longest_match) {
                int suffix_start=jk2_last_index_of(uri,uwr->suffix[0]);
                if (suffix_start>=0 && 0==strcmp(uri+suffix_start,uwr->suffix)) {
                    if( _this->debug > 0 )
                        env->l->jkLog(env, env->l, JK_LOG_INFO,
                                    "uriMap.mapUri() general suffix match %s\n",
                                      uwr->suffix );
                    longest_match = uwr->prefix_len;
                    best_match = i;
                }
            }
        } else /* MATCH_TYPE_SUFFIX */ {
            if( suffix != NULL ) {
                /* for WinXX, fix the JsP != jsp problems */
#ifdef WIN32                        
                if(0 == strcasecmp(suffix, uwr->suffix))  {
#else
                    if(0 == strcmp(suffix, uwr->suffix)) {
#endif
                        if(uwr->prefix_len >= longest_match) {
                            if( _this->debug > 0 )
                                env->l->jkLog(env, env->l,JK_LOG_INFO,
                                           "uriMap.mapUri() suffix match %s\n",
                                              uwr->suffix );
                            longest_match = uwr->prefix_len;
                            best_match = i;
                        }
                        /* indentation trick */
#ifdef WIN32                        
                    }
#else
                }
#endif
            }
        }
    }

    /* restore */
    if( url_rewrite )
        *url_rewrite=origChar;
    
    if(-1 != best_match) {
        if( _this->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "uriMap.mapUri() matched %s %s\n",
                          uri, _this->maps[best_match]->webapp->worker->name ); 
        return _this->maps[best_match];
    }
    
    if( _this->debug > 1 )
        env->l->jkLog(env, env->l, JK_LOG_INFO,
                      "uriMap.mapUri() no match found\n"); 

    return NULL;
}

int JK_METHOD jk2_uriMap_factory(jk_env_t *env, jk_pool_t *pool, void **result,
                                 const char *type, const char *name)
{
    jk_uriMap_t *_this;
    jk_map_t *init_data;

    _this = (jk_uriMap_t *)pool->calloc(env, pool, sizeof(jk_uriMap_t));
    if(  ! _this) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "uriMap.factory() OutOfMemoryError\n");
        return JK_FALSE;
    }

    _this->size     = 0;
    _this->capacity = 0;

    _this->pool=pool;

    *result=_this;

    _this->init=jk2_uriMap_init;
    _this->destroy=jk2_uriMap_destroy;
    _this->addMapping=jk2_uriMap_addMapping;
    _this->checkUri=jk2_uriMap_checkUri;
    _this->mapUri=jk2_uriMap_mapUri;
    _this->maps = NULL;
    _this->debug= 1;
            
    return JK_TRUE;
}

