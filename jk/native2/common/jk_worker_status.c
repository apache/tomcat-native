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
 * Status worker. It'll not connect to tomcat, but just generate response
 * itself, containing a simple xhtml page with the current jk info.
 *
 * Note that the html tags are using 'class' attribute. Someone with some
 * color taste can do a nice CSS and display it nicely, but more important is
 * that it should be easy to grep/xpath it programmatically.
 * 
 * @author Costin Manolache
 */

#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_logger.h"
#include "jk_env.h"
#include "jk_requtil.h"
#include "jk_registry.h"

static void jk2_worker_status_displayWorkers(jk_env_t *env, jk_ws_service_t *s,
                                             jk_workerEnv_t *wenv)
{
    jk_map_t *map=wenv->worker_map;
    int i;
    int count=map->size( env, map );
    
    s->jkprintf(env, s, "<H2>Active workers</H2>\n" );
    s->jkprintf(env, s, "<table border>\n");

    s->jkprintf(env, s, "<tr><th>Name</th><th>Type</th>"
              "<th>Channel</th><th>Route</th>"
              "<th>Error state</th><th>Recovery</th>"
              "<th>Endpoint cache</th></tr>");
    
    for( i=0; i< count ; i++ ) {
        char *name=map->nameAt( env, map, i );
        jk_worker_t *worker=(jk_worker_t *)map->valueAt( env, map,i );

        s->jkprintf(env, s, "<tr><td>%s</td>", name );
        s->jkprintf(env, s, "<td>%s</td>", worker->mbean->type );
        if( worker->channel != NULL ) {
            s->jkprintf(env, s, "<td>%s</td>",worker->channel->mbean->name );
        } else {
            s->jkprintf(env, s, "<td>&nbsp;</td>" );
        }
        if( worker->route != NULL ) {
            s->jkprintf(env, s, "<td>%s</td>",worker->route );
        } else {
            s->jkprintf(env, s, "<td>&nbsp;</td>" );
        }
        s->jkprintf(env, s, "<td class='in_error'>%d</td>",
                  worker->in_error_state );
        s->jkprintf(env, s, "<td class='in_recovering'>%d</td>",
                  worker->in_recovering );
        s->jkprintf(env, s, "<td class='epCount'>%d</td>",
                  ( worker->endpointCache == NULL ? 0 :
                    worker->endpointCache->count ));

        /* Endpoint cache ? */

        /* special case for status worker */
        
        s->jkprintf(env, s, "</tr>" );
    }
    s->jkprintf(env, s, "</table>\n");
}

static void jk2_worker_status_displayWorkerEnv(jk_env_t *env, jk_ws_service_t *s,
                                               jk_workerEnv_t *wenv)
{
    jk_map_t *map=wenv->initData;
    int i;

    s->jkprintf(env, s, "<H2>Worker Env Info</H2>\n");

    s->jkprintf(env, s, "<table border>\n");
    /* Could be modified dynamically */
    s->jkprintf(env, s, "<tr><th>LogLevel</th>"
              "<td id='logLevel'>%d</td></tr>\n",
              env->l->level);
    
    s->jkprintf(env, s, "</table>\n");

    s->jkprintf(env, s, "<H3>Active Properties ( after substitution )</H3>\n");
    s->jkprintf(env, s, "<table border>\n");
    s->jkprintf(env, s, "<tr><th>Name</th><th>Value</td></tr>\n");
    for( i=0; i< map->size( env, map ) ; i++ ) {
        char *name=map->nameAt( env, map, i );
        char *value=(char *)map->valueAt( env, map,i );
        /* Don't display worker properties or uris, those are displayed separately
           for each worker */
/*         if( strncmp( name, "worker", 6 ) !=0 && */
/*             name[0] != '/' ) { */
            s->jkprintf(env, s, "<tr><td>%s</td><td>%s</td></tr>", name,
                       value);
/*         } */
    }
    s->jkprintf(env, s, "</table>\n");


    s->jkprintf(env, s, "<H3>Configured Properties</H3>\n");
    s->jkprintf(env, s, "<table border>\n");
    s->jkprintf(env, s, "<tr><th>Object name</th><th>Property</th><th>Value</td></tr>\n");

    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        int j;
        
        if( mbean==NULL || mbean->settings==NULL ) 
            continue;
        
        s->jkprintf(env, s, "<tr><td>%s</td>", mbean->name );
        
        for( j=0; j < mbean->settings->size( env, mbean->settings ); j++ ) {
            char *pname=mbean->settings->nameAt( env, mbean->settings, j);
            /* Don't save redundant information */
            if( strcmp( pname, "name" ) != NULL ) {
                s->jkprintf( env, s, "<td>%s</td><td>%s</td>\n",
                             pname,
                             mbean->settings->valueAt( env, mbean->settings, j));
            }
        }
        s->jkprintf( env,s , "</tr>\n" );
    }
    s->jkprintf( env,s , "</table>\n" );

}
 
static void jk2_worker_status_displayMappings(jk_env_t *env, jk_ws_service_t *s,
                                             jk_workerEnv_t *wenv)
{
    jk_uriEnv_t **maps=wenv->uriMap->maps;
    int size=wenv->uriMap->size;
    int i;

    s->jkprintf(env, s, "<H2>Mappings</H2>\n");

    if( maps==NULL ) {
        s->jkprintf(env, s, "None\n");
        return;
    }
    
    s->jkprintf(env, s, "<table class='mappings' border>\n");
    
    s->jkprintf(env, s, "<tr><th>Host</th><th>Uri</th>"
              "<th>Worker</th></tr>");
    
    for( i=0; i< size ; i++ ) {
        jk_uriEnv_t *uriEnv=maps[i];

        s->jkprintf(env, s, "<tr>" );
        s->jkprintf(env, s, "<td class='host'>%s</td>",
                   (uriEnv->virtual==NULL) ? "*" : uriEnv->virtual );
        s->jkprintf(env, s, "<td class='uri'>%s</td>",
                   uriEnv->uri);
        s->jkprintf(env, s, "<td class='worker'>%s</td>",
                   (uriEnv->workerName==NULL) ? "DEFAULT" : uriEnv->workerName );
        s->jkprintf(env, s, "</tr>" );
    }

    
    s->jkprintf(env, s, "</table>\n");
}

/* Channels and connections, including 'pooled' ones
 */
static void jk2_worker_status_displayConnections(jk_env_t *env, jk_ws_service_t *s,
                                                 jk_workerEnv_t *wenv)
{
        s->jkprintf(env, s, "<H2>Active connections</H2>\n");
        s->jkprintf(env, s, "<table border>\n");
        
    
        s->jkprintf(env, s, "</table>\n");

}

static int JK_METHOD jk2_worker_status_service(jk_env_t *env,
                                               jk_worker_t *w, 
                                               jk_ws_service_t *s)
{
    char *uri=s->req_uri;
    jk_map_t *queryMap;
    
    env->l->jkLog(env, env->l, JK_LOG_INFO, "status.service() %s %s\n",
                  uri, s->query_string);

    /* Generate the header */
    s->status=200;
    s->msg="OK";
    s->headers_out->put(env, s->headers_out,
                        "Content-Type", "text/html", NULL);
    fprintf(stderr, "Writing head \n");
    s->head(env, s );

    /** Process the query string.
     */
    if( s->query_string == NULL ) {
        s->query_string="get=*";
    }
    
    /* Body */
    jk2_worker_status_displayWorkerEnv(env, s, s->workerEnv );
    jk2_worker_status_displayWorkers(env, s, s->workerEnv );
    jk2_worker_status_displayMappings(env, s, s->workerEnv );
    jk2_worker_status_displayConnections(env, s, s->workerEnv );
    
    s->afterRequest( env, s);
    fprintf(stderr, "After req %s \n", s);
    return JK_TRUE;
}


int JK_METHOD jk2_worker_status_factory(jk_env_t *env, jk_pool_t *pool,
                                        jk_bean_t *result,
                                        const char *type, const char *name)
{
    jk_worker_t *_this;
    
    _this = (jk_worker_t *)pool->calloc(env, pool, sizeof(jk_worker_t));

    if(_this==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR,
                      "status_worker.factory() OutOfMemoryException\n");
        return JK_FALSE;
    }

    _this->pool           = pool;

    _this->service        = jk2_worker_status_service;

    result->object=_this;
    _this->mbean=result;

    _this->workerEnv=env->getByName( env, "workerEnv" );
    _this->workerEnv->addWorker( env, _this->workerEnv, _this );

    return JK_TRUE;
}

