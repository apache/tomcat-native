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

/***************************************************************************
 * Description: URI to worker map object.                                  *
 * Maps can be                                                             *
 *                                                                         *
 * Exact Context -> /exact/uri=worker e.g. /examples/do *=ajp12             *
 * Context Based -> /context/ *=worker e.g. /examples/ *=ajp12               *
 * Context and suffix ->/context/*.suffix=worker e.g. /examples/ *.jsp=ajp12*
 *                                                                         *
 * This lets us either partition the work among the web server and the     *
 * servlet container.                                                      *
 *                                                                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_util.h"
#include "jk_env.h"
#include "jk_uriMap.h"

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
static int jk_uriMap_checkUri(jk_uriMap_t *_this, 
                              const char *uri )
{
    unsigned i;    

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
                       (0 == strncmp(_this->maps[i]->context, uri,
                                     _this->maps[i]->ctxt_len))) {
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


/* int uriMap_free(jk_uriMap_t **_this, */
/*                 jk_logger_t *l) */
/* { */
/*     l->jkLog(l, JK_LOG_DEBUG,  */
/*            "Into jk_uriMap_t::uriMap_free\n");     */

/*     if(_this && *_this) { */
/*         jk_uriMap_destroy(*_this, l);   */
/*         free(*_this); */
/*         *_this = NULL; */
/* 	return JK_TRUE; */
/*     } */
/*     else  */
/*     	l->jkLog(l, JK_LOG_ERROR,  */
/*            "In jk_uriMap_t::uriMap_free, NULL parameters\n");     */

/*     return JK_FALSE; */
/* } */

/*
 * Ensure there will be memory in context info to store Context Bases
 */

#define UW_INC_SIZE 4   /* 4 URI->WORKER STEP */

static int uriMap_realloc(jk_uriMap_t *_this)
{
    if (_this->size == _this->capacity) {
        jk_uriEnv_t **uwr;
        int  capacity = _this->capacity + UW_INC_SIZE;

        uwr = (jk_uriEnv_t **)
            jk_pool_alloc(&_this->p, sizeof(jk_uriEnv_t *) * capacity);

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


static jk_uriEnv_t *jk_uriMap_addMapping(jk_uriMap_t *_this,
                                         char *vhost,
                                         char *puri, 
                                         char *pworker )
{
    jk_logger_t *l=_this->workerEnv->l;
    jk_uriEnv_t *uwr;
    char *uri;
    char *worker;
    int err;
    char *asterisk;
    
    /* make sure we have space */
    err=uriMap_realloc(_this);
    uwr = (jk_uriEnv_t *)jk_pool_alloc(&_this->p,
                                       sizeof(jk_uriEnv_t));
    
    uri = jk_pool_strdup(&_this->p, puri);
    worker = jk_pool_strdup(&_this->p, pworker);

    if (err || !uri || ! worker || ! uwr ) {
        l->jkLog(l, JK_LOG_ERROR,"Allocation error\n");
        return NULL;
    }

    if ('/' != uri[0]) {
        /*
         * JFC: please check...
         * Not sure what to do, but I try to prevent problems.
         * I have fixed jk_mount_context() in apaches/mod_jk.c so we should
         * not arrive here when using Apache.
         */
        l->jkLog(l, JK_LOG_ERROR,
               "uriMap.addMapping() context must start with '/' in %s\n",
               uri);
        return NULL;
    }

    uwr->ctxt_len = strlen(uwr->context);
    
    _this->maps[_this->size] = uwr;
    _this->size++;

    asterisk = strchr(uri, '*');

    // set the mapping type
    if (!asterisk) {
        /* Something like:  JkMount /login/j_security_check ajp13 */
        uwr->uri         = uri;
        uwr->workerName = worker;
        uwr->context     = uri;
        uwr->suffix      = NULL;
        uwr->match_type  = MATCH_TYPE_EXACT;
        l->jkLog(l, JK_LOG_DEBUG,
               "uriMap.addMapping() exact mapping %s=%s was added\n",
               uri, worker);
        return uwr;
    }

    uwr->uri = jk_pool_strdup(&_this->p, uri);

    if (!uwr->uri) {
        l->jkLog(l, JK_LOG_ERROR,"Allocation error\n");
        return NULL;
    }

    /*
     * Now, lets check that the pattern is /context/*.suffix
     * or /context/*
     * we need to have a '/' then a '*' and the a '.' or a
     * '/' then a '*'
     */
    asterisk--;
    if ('/' == asterisk[0]) {
        if ('.' == asterisk[2]) {
            /* suffix rule */
            asterisk[1]      = '\0';
            asterisk[2]      = '\0';
            uwr->workerName = worker;
            uwr->context     = uri;
            uwr->suffix      = asterisk + 3;
            uwr->match_type  = MATCH_TYPE_SUFFIX;
            l->jkLog(l, JK_LOG_DEBUG,
                   "uriMap.addMapping() suffix mapping %s.%s=%s was added\n",
                   uri, asterisk + 3, worker); 
        } else if ('\0' != asterisk[2]) {
            /* general suffix rule */
            asterisk[1] = '\0';
            uwr->workerName = worker;
            uwr->context = uri;
            uwr->suffix  = asterisk + 2;
            uwr->match_type = MATCH_TYPE_GENERAL_SUFFIX;
            l->jkLog(l, JK_LOG_DEBUG,
                   "uriMap.addMapping() general suffix mapping %s.%s=%s\n",
                   uri, asterisk + 2, worker);
        } else {
            /* context based */
            asterisk[1]      = '\0';
            uwr->workerName = worker;
            uwr->context     = uri;
            uwr->suffix      = NULL;
            uwr->match_type  = MATCH_TYPE_CONTEXT;
            l->jkLog(l, JK_LOG_DEBUG,
                   "uriMap.addMapping() context mapping %s=%s\n",
                   uri, worker);
        }
    } else {
        /* Something like : JkMount /servlets/exampl* ajp13 */
        uwr->uri         = uri;
        uwr->workerName = worker;
        uwr->context     = uri;
        uwr->suffix      = NULL;
        uwr->match_type  = MATCH_TYPE_EXACT;
        l->jkLog(l, JK_LOG_DEBUG,
                   "uriMap.addMapping() prefix mapping %s=%s\n",
               uri, worker);
    }
    
    return uwr;
}

int jk_uriMap_init(jk_uriMap_t *_this,
                   jk_workerEnv_t *workerEnv,
                   jk_map_t *init_data )
{
    int rc;
    int sz;
    int err;
    int i;
    jk_logger_t *l=workerEnv->l;

    _this->workerEnv = workerEnv;
                                           
    jk_open_pool(&_this->p,_this->buf, 
                 sizeof(jk_pool_atom_t) * SMALL_POOL_SIZE);
    jk_open_pool(&_this->tp,_this->tbuf,
                 sizeof(jk_pool_atom_t) * SMALL_POOL_SIZE);

    _this->size = 0;
    _this->maps = NULL;
        
    sz = map_size(init_data);

    l->jkLog(l, JK_LOG_DEBUG, "uriMap.open(): initializing %d rules\n", sz);

    if (sz <= 0) {
        return JK_TRUE;
    }
    
    rc = JK_TRUE;
    for(i = 0; i < sz ; i++) {
        jk_uriEnv_t *uriEnv=_this->addMapping(_this,NULL,
                                              map_name_at(init_data, i),
                                              map_value_at(init_data, i));
        if ( uriEnv==NULL) {
            l->jkLog(l, JK_LOG_ERROR, "Parsing error %s\n",
                   map_name_at(init_data, i));
            rc=JK_FALSE;
        }
    }

    return rc;
}

static void jk_uriMap_destroy(jk_uriMap_t *_this)
{
    jk_logger_t *l=_this->workerEnv->l;
    
    l->jkLog(l, JK_LOG_DEBUG, 
           "uriMap.destroy()\n"); 

    /* this can't be null ( or a NPE would have been generated */
    
    jk_close_pool(&_this->p);
    jk_close_pool(&_this->tp);
}


/* returns the index of the last occurrence of the 'ch' character
   if ch=='\0' returns the length of the string str  */
static INLINE int last_index_of(const char *str,char ch)
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
INLINE const char *findExtension( const char *uri ) {
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

jk_worker_t *jk_uriMap_mapUri(jk_uriMap_t *_this,
                              const char *vhost,
                              const char *uri )
{
    unsigned i;
    unsigned best_match = -1;
    unsigned longest_match = 0;
    char * clean_uri = NULL;
    char *url_rewrite;
    const char *suffix;
    jk_logger_t *l=_this->workerEnv->l;
    /* XXX - need to make sure prefix match take precedence over
       extension match ( now it doesn't )
    */
    
    if( _this == NULL || uri==NULL ) 
	return NULL;
    
    if( _this->size <= 0 )
	return NULL;

    l->jkLog(l, JK_LOG_DEBUG, 
           "uriMap.mapUri() %s %s\n", vhost, uri);    

    if( '/' != uri[0]) {
        l->jkLog(l, JK_LOG_ERROR, 
               "uriMap.mapUri() uri must start with /\n");
        return NULL;
    }

    url_rewrite = strstr(uri, JK_PATH_SESSION_IDENTIFIER);
        
    if(url_rewrite) {
        jk_reset_pool(&_this->tp);
        clean_uri = jk_pool_strdup(&_this->tp,uri);
        url_rewrite = strstr(clean_uri, JK_PATH_SESSION_IDENTIFIER);
        *url_rewrite = '\0';
        uri = clean_uri;
    }

    /* Only once, no need to compute it for each extension match */
    suffix=findExtension( uri );

    for(i = 0 ; i < _this->size ; i++) {
        jk_uriEnv_t *uwr = _this->maps[i];
        
        if(uwr->ctxt_len < longest_match) {
            continue; /* can not be a best match anyway */
        }
        
        if(0 != strncmp(uwr->context, uri, uwr->ctxt_len))
            continue;
        
        if(MATCH_TYPE_EXACT == uwr->match_type) {
            if(strlen(uri) == uwr->ctxt_len) {
                l->jkLog(l, JK_LOG_DEBUG,
                       "uriMap.mapUri() exact match %s:%s \n",
                       uwr->worker->name, uwr->context );
                return uwr->worker;
            }
        } else if(MATCH_TYPE_CONTEXT == uwr->match_type) {
            if(uwr->ctxt_len > longest_match) {
                l->jkLog(l, JK_LOG_DEBUG,
                       "uriMap.mapUri() tentative prefix match %s",
                       uwr->context );
                longest_match = uwr->ctxt_len;
                best_match = i;
            }
        } else if(MATCH_TYPE_GENERAL_SUFFIX == uwr->match_type) {
            int suffix_start=last_index_of(uri,uwr->suffix[0]);
            if (suffix_start>=0 && 0==strcmp(uri+suffix_start,uwr->suffix)) {
                if(uwr->ctxt_len >= longest_match) {
                    l->jkLog(l, JK_LOG_DEBUG,
                           "uriMap.mapUri() general suffix match %s\n",
                           uwr->suffix );
                    longest_match = uwr->ctxt_len;
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
                        if(uwr->ctxt_len >= longest_match) {
                            l->jkLog(l,JK_LOG_DEBUG,
                                   "uriMap.mapUri() suffix match %s\n",
                                   uwr->suffix );
                            longest_match = uwr->ctxt_len;
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

    if(-1 != best_match) {
        return _this->maps[best_match]->worker;
    }
    
    l->jkLog(l, JK_LOG_DEBUG, 
           "uriMap.mapUri() no match found\n"); 

    return NULL;
}

int JK_METHOD jk_uriMap_factory( jk_env_t *env, void **result,
                                 char *type, char *name)
{
    jk_uriMap_t *_this;
    jk_map_t *init_data;
    jk_logger_t *l;

    _this = (jk_uriMap_t *)calloc(1,sizeof(jk_uriMap_t));
    _this->size     = 0;
    _this->capacity = 0;

    if(  ! _this) {
        l->jkLog(l, JK_LOG_ERROR, "Allocation error\n");
        return JK_FALSE;
    }
    *result=_this;

    _this->init=jk_uriMap_init;
    _this->destroy=jk_uriMap_destroy;
    _this->addMapping=jk_uriMap_addMapping;
    _this->checkUri=jk_uriMap_checkUri;
    _this->mapUri=jk_uriMap_mapUri;
    
    
    return JK_TRUE;
}

