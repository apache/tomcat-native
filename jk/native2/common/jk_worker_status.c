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
#include "jk_endpoint.h"


static void jk2_worker_status_displayScoreboardInfo(jk_env_t *env, jk_ws_service_t *s,
                                                    jk_workerEnv_t *wenv)
{
    jk_map_t *map=wenv->initData;
    int i;
    int j;
    int totalReq=0;
    int totalErr=0;
    long totalTime=0;
    long maxTime=0;
    int needHeader=JK_TRUE;
    
    if( wenv->shm==NULL || wenv->shm->head==NULL)
        return;

    s->jkprintf(env, s, "<h2>Scoreboard info (ver=%d slots=%d)</h2>\n", 
                wenv->shm->head->lbVer, wenv->shm->head->lastSlot );
                
    s->jkprintf(env, s, "<table border>\n");

    for( i=1; i < wenv->shm->head->lastSlot; i++ ) {
        jk_shm_slot_t *slot= wenv->shm->getSlot( env, wenv->shm, i );

        if( slot==NULL ) continue;
        
        if( strncmp( slot->name, "epStat", 6 ) == 0 ) {
            /* This is an endpoint slot */
            void *data=slot->data;

            s->jkprintf(env, s, "<tr><th colspan='4'>%s</th>\n", slot->name );
            s->jkprintf(env, s, "<th>Cnt=%d</th><th>size=%d</th>\n",
                        slot->structCnt, slot->structSize );
            
            s->jkprintf(env, s, "<tr><th>Req</th><th>Err</th>");
            s->jkprintf(env, s,"<th>LastReq</th>\n" );
            
#ifdef HAVE_APR
            s->jkprintf(env, s, "<th>ConnectionTime</th><th>TotalTime</th><th>MaxTime</th><th>AvgTime</th>" );
            s->jkprintf(env, s, "<th>ReqStart</th><th>+jk</th><th>+end</th>" );
#endif
            
            /* XXX Add info about number of slots */
            for( j=0; j<slot->structCnt ; j++ ) {
                jk_stat_t *stat=(jk_stat_t *) ( data + j * sizeof( jk_stat_t ));

                s->jkprintf(env, s, "<tr><td>%d</td><td>%d</td>\n", stat->reqCnt, stat->errCnt );
                s->jkprintf(env, s, "<td>%s</td>\n",  stat->active );

                totalReq+=stat->reqCnt;
                totalErr+=stat->errCnt;
#ifdef HAVE_APR
                {
                    char ctimeBuf[APR_CTIME_LEN];
                    apr_ctime( ctimeBuf, stat->connectedTime );
                    s->jkprintf(env, s, "<td>%s</td>\n", ctimeBuf );

                    s->jkprintf(env, s, "<td>%ld</td>\n", stat->totalTime );
                    s->jkprintf(env, s, "<td>%ld</td>\n", stat->maxTime );

                    if( stat->reqCnt + stat->errCnt > 0 ) 
                        s->jkprintf(env, s, "<td>%ld</td>\n",
                                    (stat->totalTime / ( stat->reqCnt + stat->errCnt )) );
                    else
                        s->jkprintf(env, s, "<td>-</td>\n");

                    s->jkprintf(env, s, "<td>%ld</td>\n", stat->startTime );
                    s->jkprintf(env, s, "<td>%ld</td>\n",
                                stat->jkStartTime - stat->startTime );
                    s->jkprintf(env, s, "<td>%ld</td>\n",
                                stat->endTime - stat->startTime );

                    totalTime += stat->totalTime;
                    if( maxTime < stat->maxTime )
                        maxTime=stat->maxTime;
                }
#endif
                s->jkprintf(env, s, "</tr>\n");
            }

        }
    }
    s->jkprintf(env, s, "</table>\n");

    s->jkprintf(env, s, "Totals:\n");

    s->jkprintf(env, s, "<table border><tr><th>Req</th><th>Err</th><th>Max</th><th>Avg</th></tr>");

    s->jkprintf(env, s, "<tr><td>%d</td>\n", totalReq );
    s->jkprintf(env, s, "<td>%d</td>\n", totalErr );

    s->jkprintf(env, s, "<td>%ld</td>\n", maxTime );

    if( totalErr + totalReq > 0 ) {
        long avg=totalTime / ( totalReq + totalErr );
        s->jkprintf(env, s, "<td>%ld</td>\n", avg );
    } else {
        s->jkprintf(env, s, "<td>-</td>\n" );
    }

    s->jkprintf(env, s, "</tr></table>\n");
}

/** Use 'introspection' data to find what getters an type support,
 *  and display the information in a table
 */
static void jk2_worker_status_displayRuntimeType(jk_env_t *env, jk_ws_service_t *s,
                                                 jk_workerEnv_t *wenv, char *type)
{
    jk_map_t *map=wenv->initData;
    int i;
    int needHeader=JK_TRUE;

    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        int j;
        int propCount;

        /* Don't display aliases */
        if( strchr(name, ':')==NULL )
            continue;
        
        if( mbean==NULL || mbean->getAttributeInfo==NULL ) 
            continue;

        if( mbean->getAttribute == NULL )
            continue;

        if( strncmp( type, mbean->type, 5 ) != 0 )
            continue;

        if( needHeader ) {
            s->jkprintf(env, s, "<H3>%s runtime info</H3>\n", type);
            s->jkprintf(env, s, "<p>%s information, using getAttribute() </p>\n", type);
            
            s->jkprintf(env, s, "<table border>\n");
            s->jkprintf(env, s, "<tr><th>name</th>\n");
            for( j=0; mbean->getAttributeInfo[j] != NULL; j++ ) {
                char *pname=mbean->getAttributeInfo[j];
                
                s->jkprintf(env, s, "<th>%s</th>", pname );
            }
            needHeader = JK_FALSE;
        } 
        
        s->jkprintf(env, s, "</tr><tr><td>%s</td>\n", mbean->localName);
        for( j=0; mbean->getAttributeInfo[j] != NULL; j++ ) {
            char *pname=mbean->getAttributeInfo[j];

            s->jkprintf(env, s, "<td>%s</td>",
                        mbean->getAttribute( env, mbean, pname));
        }
    }
    if( ! needHeader ) {
        s->jkprintf( env,s , "</table>\n" );
    }
}

/** That's the 'bulk' data - everything that was configured, after substitution
 */
static void jk2_worker_status_displayActiveProperties(jk_env_t *env, jk_ws_service_t *s,
                                                      jk_workerEnv_t *wenv)
{
    jk_map_t *map=wenv->initData;
    int i;

    s->jkprintf(env, s, "<H3>Processed config</H3>\n");
    s->jkprintf(env, s, "<p>All settings ( automatic and configured ), after substitution</p>\n");

    s->jkprintf(env, s, "<table border>\n");
    s->jkprintf(env, s, "<tr><th>Name</th><th>Value</td></tr>\n");
    for( i=0; i< map->size( env, map ) ; i++ ) {
        char *name=map->nameAt( env, map, i );
        char *value=(char *)map->valueAt( env, map,i );
        s->jkprintf(env, s, "<tr><td>%s</td><td>%s</td></tr>", name,
                    value);
    }
    s->jkprintf(env, s, "</table>\n");
}

/** persistent configuration
 */
static void jk2_worker_status_displayConfigProperties(jk_env_t *env, jk_ws_service_t *s,
                                            jk_workerEnv_t *wenv)
{
    int i;
    
    s->jkprintf(env, s, "<H3>Configured Properties</H3>\n");
    s->jkprintf(env, s, "<p>Original data set by user</p>\n");
    s->jkprintf(env, s, "<table border>\n");
    s->jkprintf(env, s, "<tr><th>Object name</th><th>Property</th><th>Value</td></tr>\n");

    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        int j;
        int propCount;

        /* Don't display aliases */
        if( strchr(name, ':')==NULL )
            continue;
        
        if( mbean==NULL || mbean->settings==NULL ) 
            continue;
        
        propCount=mbean->settings->size( env, mbean->settings );

        if( propCount==0 ) {
            s->jkprintf(env, s, "<tr><th>%s</th><td></td></tr>", mbean->name );
        } else {
            s->jkprintf(env, s, "<tr><th rowspan='%d'>%s</th><td>%s</td><td>%s</td></tr>",
                        propCount, mbean->name,
                        mbean->settings->nameAt( env, mbean->settings, 0),
                        mbean->settings->valueAt( env, mbean->settings, 0));
            for( j=1; j < propCount ; j++ ) {
                char *pname=mbean->settings->nameAt( env, mbean->settings, j);
                /* Don't save redundant information */
                if( strcmp( pname, "name" ) != 0 ) {
                    s->jkprintf(env, s, "<tr><td>%s</td><td>%s</td></tr>",
                                pname, mbean->settings->valueAt( env, mbean->settings, j));
                }
            }
        }
    }
    s->jkprintf( env,s , "</table>\n" );
}
 
static int JK_METHOD jk2_worker_status_service(jk_env_t *env,
                                               jk_worker_t *w, 
                                               jk_ws_service_t *s)
{
    char *uri=s->req_uri;
    jk_map_t *queryMap;
    int status;
    int didUpdate;

    if( w->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_INFO, "status.service() %s %s\n",
                      uri, s->query_string);

    /* Generate the header */
    s->status=200;
    s->msg="OK";
    s->headers_out->put(env, s->headers_out,
                        "Content-Type", "text/html", NULL);
    s->head(env, s );

    /** Process the query string.
     */
    if( s->query_string == NULL ) {
        s->query_string="get=*";
    }

    w->workerEnv->config->update( env, w->workerEnv->config, &didUpdate );
    if( didUpdate ) {
        jk_shm_t *shm=w->workerEnv->shm;
        
        /* Update the scoreboard's version - all other
           jk2 processes will see this and update
        */
        if( shm!=NULL && shm->head!=NULL )
            shm->head->lbVer++;
    }
    
    s->jkprintf(env, s, "Status information for child %d", s->workerEnv->childId  );
    
    /* Body */
    jk2_worker_status_displayRuntimeType(env, s, s->workerEnv, "ajp13" );
    jk2_worker_status_displayScoreboardInfo(env, s, s->workerEnv );
/*     jk2_worker_status_displayRuntimeType(env, s, s->workerEnv, "endpoint" ); */
    jk2_worker_status_displayRuntimeType(env, s, s->workerEnv, "uri" );
    jk2_worker_status_displayConfigProperties(env, s, s->workerEnv );
    jk2_worker_status_displayActiveProperties(env, s, s->workerEnv );
    
    s->afterRequest( env, s);
    return JK_OK;
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
        return JK_ERR;
    }

    _this->service        = jk2_worker_status_service;

    result->object=_this;
    _this->mbean=result;

    _this->workerEnv=env->getByName( env, "workerEnv" );
    _this->workerEnv->addWorker( env, _this->workerEnv, _this );

    return JK_OK;
}

