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
 * Pool using APR.
 *
 * @author Costin Manolache
 */

#include "jk_pool.h"
#include "jk_env.h"
#include "apr_pools.h"
#include "apr_strings.h"

#include "jk_apache2.h"

int jk_pool_apr_create( jk_pool_t **newPool, jk_pool_t *parent, apr_pool_t *aprPool );

int JK_METHOD jk_pool_apr_factory(jk_env_t *env,
                                  jk_pool_t *pool,
                                  void **result,
                                  char *type, char *name);

/** Nothing - apache will take care ??
 */
static void jk_pool_apr_close(jk_pool_t *p)
{
    
}

/** Nothing - apache will take care.
    XXX with jk pools we can implement 'recycling',
    not sure what's the equivalent for apache
*/
static void jk_pool_apr_reset(jk_pool_t *p)
{
    apr_pool_clear(p->_private);
}

static void *jk_pool_apr_calloc(jk_pool_t *p, 
                           size_t size)
{
    /* assert( p->_private != NULL ) */
    return apr_pcalloc( (apr_pool_t *)p->_private, (apr_size_t)size);
}

static void *jk_pool_apr_alloc(jk_pool_t *p, 
                               size_t size)
{
    return apr_palloc( (apr_pool_t *)p->_private, (apr_size_t)size);
}

static void *jk_pool_apr_realloc(jk_pool_t *p, 
                             size_t sz,
                             const void *old,
                             size_t old_sz)
{
    void *rc;

    if(!p || (!old && old_sz)) {
        return NULL;
    }

    rc = jk_pool_apr_alloc(p, sz);
    if(rc) {
        memcpy(rc, old, old_sz);
    }

    return rc;
}

static void *jk_pool_apr_strdup(jk_pool_t *p, 
                                const char *s)
{
    return apr_pstrdup( (apr_pool_t *)p->_private, s);
}



static jk_pool_t *jk_pool_apr_createChild( jk_pool_t *_this, int sizeHint ) {
    apr_pool_t *parentAprPool=_this->_private;
    apr_pool_t *childAprPool;
    jk_pool_t *newPool;

    apr_pool_create( &childAprPool, parentAprPool );

    jk_pool_apr_create( &newPool, _this, childAprPool );
    
    return newPool;
}


int jk_pool_apr_create( jk_pool_t **newPool, jk_pool_t *parent, apr_pool_t *aprPool)
{
    jk_pool_t *_this=(jk_pool_t *)apr_palloc(aprPool, sizeof( jk_pool_t ));
    
    _this->_private=aprPool;
    
    *newPool = _this;

    /* methods */
    _this->create=jk_pool_apr_createChild;
    _this->close=jk_pool_apr_close;
    _this->reset=jk_pool_apr_reset;
    _this->alloc=jk_pool_apr_alloc;
    _this->calloc=jk_pool_apr_calloc;
    _this->pstrdup=jk_pool_apr_strdup;
    _this->realloc=jk_pool_apr_realloc;
    
    return JK_TRUE;
}

/* Not used yet */
int  jk_pool_apr_factory(jk_env_t *env, jk_pool_t *pool,
                         void **result,
                         char *type, char *name)
{
    jk_pool_t *_this=(jk_pool_t *)calloc( 1, sizeof(jk_pool_t));

    *result=_this;
    
    return JK_TRUE;
}


