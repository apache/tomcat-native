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

#define JK_CHECK_NULL( str ) ( ((str)==NULL) ? "null" : (str) )

#define DEFAULT_CSS ("BODY {COLOR: #000000; FONT-STYLE: normal; FONT-FAMILY: \"Times New Roman\", Times, serif; BACKGROUND-COLOR: #ffffff} H1 { COLOR: #0033cc; FONT-FAMILY: Arial, Helvetica, sans-serif} H2 { COLOR: #0033cc; FONT-FAMILY: Arial, Helvetica, sans-serif} H3 {FONT: 110% Arial, Helvetica, sans-serif; COLOR: #0033cc} B {FONT-WEIGHT: bold}" )

/** Display info for one endpoint
 */
static void jk2_worker_status_displayStat(jk_env_t *env, jk_ws_service_t *s,
                                          jk_stat_t *stat,
                                          int *totalReqP, int *totalErrP,
                                          unsigned long *totalTimeP, unsigned long *maxTimeP )
{
    int totalReq=*totalReqP;
    int totalErr=*totalErrP;
    unsigned long totalTime=*totalTimeP;
    unsigned long maxTime=*maxTimeP;
    char ctimeBuf[APR_CTIME_LEN];
    
    s->jkprintf(env, s, "<tr><td>%d</td><td>%d</td><td>%d</td>\n",
                stat->workerId, stat->reqCnt, stat->errCnt );
    s->jkprintf(env, s, "<td>%s</td>\n",  JK_CHECK_NULL(stat->active) );

    totalReq+=stat->reqCnt;
    totalErr+=stat->errCnt;

    apr_ctime( ctimeBuf, stat->connectedTime );
    s->jkprintf(env, s, "<td>%s</td>\n", ctimeBuf );

    s->jkprintf(env, s, "<td>%ld</td>\n", (long)stat->totalTime );
    s->jkprintf(env, s, "<td>%ld</td>\n", (long)stat->maxTime );

    if( stat->reqCnt + stat->errCnt > 0 ) 
        s->jkprintf(env, s, "<td>%ld</td>\n",
                    (long)(stat->totalTime / ( stat->reqCnt + stat->errCnt )) );
    else
        s->jkprintf(env, s, "<td>-</td>\n");

    s->jkprintf(env, s, "<td>%lu</td>\n", (long)stat->startTime );
    s->jkprintf(env, s, "<td>%ld</td>\n",
                (long)(stat->jkStartTime - stat->startTime) );
    s->jkprintf(env, s, "<td>%ld</td>\n",
                (long)(stat->endTime - stat->startTime) );

    totalTime += (long)stat->totalTime;
    if( maxTime < stat->maxTime )
        maxTime = (long)stat->maxTime;
    s->jkprintf(env, s, "</tr>\n");

    *maxTimeP=maxTime;
    *totalTimeP=totalTime;
    *totalReqP=totalReq;
    *totalErrP=totalErr;
}

/** Displays the totals
 */
static void jk2_worker_status_displayAggregate(jk_env_t *env, jk_ws_service_t *s,
                                               int totalReq, int totalErr,
                                               unsigned long totalTime, unsigned long maxTime )
{
    s->jkprintf(env, s, "Totals:\n");

    s->jkprintf(env, s, "<table border><tr><th>Req</th><th>Err</th><th>Max</th><th>Avg</th></tr>");

    s->jkprintf(env, s, "<tr><td>%d</td>\n", totalReq );
    s->jkprintf(env, s, "<td>%d</td>\n", totalErr );

    s->jkprintf(env, s, "<td>%ld</td>\n", maxTime );

    if( totalErr + totalReq > 0 ) {
        unsigned long avg=totalTime / ( totalReq + totalErr );
        s->jkprintf(env, s, "<td>%ld</td>\n", avg );
    } else {
        s->jkprintf(env, s, "<td>-</td>\n" );
    }

    s->jkprintf(env, s, "</tr></table>\n");
}

/** Information for the internal endpoints ( in this process ).
    Duplicates info from the scoreboard. This is used for debugging and for
    cases where scoreboard is not available. No longer included in the
    normal display to avoid confusion.
 */
static void jk2_worker_status_displayEndpointInfo(jk_env_t *env, jk_ws_service_t *s,
                                                  jk_workerEnv_t *wenv)
{
    int i;
    int totalReq=0;
    int totalErr=0;
    unsigned long totalTime=0;
    unsigned long maxTime=0;

    s->jkprintf(env, s, "<h2>Endpoint info ( no shm )</h2>\n");
                
    s->jkprintf(env, s, "<table border>\n");

    s->jkprintf(env, s, "<tr><th>Worker</th><th>Req</th><th>Err</th>");
    s->jkprintf(env, s,"<th>LastReq</th>\n" );    
    s->jkprintf(env, s, "<th>ConnectionTime</th><th>TotalTime</th><th>MaxTime</th><th>AvgTime</th>" );
    s->jkprintf(env, s, "<th>ReqStart</th><th>+jk</th><th>+end</th>" );

    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        jk_endpoint_t *ep;

        if( mbean==NULL ) 
            continue;

        if( strncmp( "endpoint", mbean->type, 8 ) != 0 )
            continue;

        ep=mbean->object;
        if( ep->stats != NULL ){
            jk2_worker_status_displayStat( env, s, ep->stats,
                                           &totalReq, &totalErr, &totalTime, &maxTime);
        }
    }
    s->jkprintf(env, s, "</table>\n");

    jk2_worker_status_displayAggregate( env, s, 
                                        totalReq, totalErr, totalTime, maxTime);
}

/** Special case for endpoints - the info is stored in a shm segment, to be able
 *   to access info from all other processes in a multi-process server.
 *  For single process servers - the scoreboard is just a local char*.
 */
static void jk2_worker_status_displayScoreboardInfo(jk_env_t *env, jk_ws_service_t *s,
                                                    jk_workerEnv_t *wenv)
{
    jk_map_t *map=wenv->initData;
    int i;
    int j;
    int totalReq=0;
    int totalErr=0;
    unsigned long totalTime=0;
    unsigned long maxTime=0;
    int needHeader=JK_TRUE;
    
    if( wenv->shm==NULL || wenv->shm->head==NULL) {
		s->jkprintf(env, s, "<h3>No Scoreboard available</h3>\n"); 
        return;
    }

    s->jkprintf(env, s, "<h2>Scoreboard info (ver=%d slots=%d)</h2>\n", 
                wenv->shm->head->lbVer, wenv->shm->head->lastSlot );

    s->jkprintf(env, s, "<a href='jkstatus?scoreboard.reset'>reset</a>\n");
                
    s->jkprintf(env, s, "<table border>\n");

    for( i=1; i < wenv->shm->head->lastSlot; i++ ) {
        jk_shm_slot_t *slot= wenv->shm->getSlot( env, wenv->shm, i );

        if( slot==NULL ) continue;
        
        if( strncmp( slot->name, "epStat", 6 ) == 0 ) {
            /* This is an endpoint slot */
            void *data=slot->data;

            s->jkprintf(env, s, "<tr><th colspan='4'>%s</th>\n", JK_CHECK_NULL(slot->name) );
            s->jkprintf(env, s, "<th>Cnt=%d</th><th>size=%d</th>\n",
                        slot->structCnt, slot->structSize );
            
            s->jkprintf(env, s, "<tr><th>Worker</th><th>Req</th><th>Err</th>");
            s->jkprintf(env, s,"<th>LastReq</th>\n" );
            s->jkprintf(env, s, "<th>ConnectionTime</th><th>TotalTime</th><th>MaxTime</th><th>AvgTime</th>" );
            s->jkprintf(env, s, "<th>ReqStart</th><th>+jk</th><th>+end</th>" );
            
            /* XXX Add info about number of slots */
            for( j=0; j<slot->structCnt ; j++ ) {
                jk_stat_t *statArray=(jk_stat_t *)data;
                jk_stat_t *stat=statArray + j;
                
                jk2_worker_status_displayStat( env, s, stat,
                                               &totalReq, &totalErr, &totalTime, &maxTime);
            }

        }
    }
    s->jkprintf(env, s, "</table>\n");

    jk2_worker_status_displayAggregate( env, s, 
                                        totalReq, totalErr, totalTime, maxTime);
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
    int typeLen=strlen( type );

    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        int j;

        /* Don't display aliases */
        if( strchr(name, ':')==NULL )
            continue;
        
        if( mbean==NULL || mbean->getAttributeInfo==NULL ) 
            continue;

        if( mbean->getAttribute == NULL )
            continue;

        if( strncmp( type, mbean->type, typeLen ) != 0 )
            continue;

        if( mbean->localName == NULL || (strlen(mbean->localName) == 0) )
            continue;

        if( needHeader ) {
            s->jkprintf(env, s, "<H3>%s runtime info</H3>\n", JK_CHECK_NULL(type) );
            s->jkprintf(env, s, "<p>%s information, using getAttribute() </p>\n", JK_CHECK_NULL(type) );
            
            s->jkprintf(env, s, "<table border>\n");
            s->jkprintf(env, s, "<tr><th>id</th>\n");
            s->jkprintf(env, s, "<th>name</th>\n");
            for( j=0; mbean->getAttributeInfo[j] != NULL; j++ ) {
                char *pname=mbean->getAttributeInfo[j];
                
                s->jkprintf(env, s, "<th>%s</th>", JK_CHECK_NULL(pname) );
            }
            needHeader = JK_FALSE;
        } 
        
        s->jkprintf(env, s, "</tr><tr><td>%d</td><td>%s</td>\n", mbean->id,
                    JK_CHECK_NULL(mbean->localName));
        for( j=0; mbean->getAttributeInfo[j] != NULL; j++ ) {
            char *pname=mbean->getAttributeInfo[j];
            char *res=mbean->getAttribute( env, mbean, pname);

            s->jkprintf(env, s, "<td>%s</td>", JK_CHECK_NULL(res));
        }
    }
    if( ! needHeader ) {
        s->jkprintf( env,s , "</table>\n" );
    }
}

static void jk2_worker_status_resetScoreboard(jk_env_t *env, jk_ws_service_t *s,
                                              jk_workerEnv_t *wenv)
{
    int i, j;
    
    if( wenv->shm==NULL || wenv->shm->head==NULL) {
        return;
    }
    
    for( i=1; i < wenv->shm->head->lastSlot; i++ ) {
        jk_shm_slot_t *slot= wenv->shm->getSlot( env, wenv->shm, i );
        
        if( slot==NULL ) continue;
        
        if( strncmp( slot->name, "epStat", 6 ) == 0 ) {
            /* This is an endpoint slot */
            void *data=slot->data;

            for( j=0; j<slot->structCnt ; j++ ) {
                jk_stat_t *statArray=(jk_stat_t *)data;
                jk_stat_t *stat=statArray + j;

                stat->reqCnt=0;
                stat->errCnt=0;
                stat->totalTime=0;
                stat->maxTime=0;
            }
        }
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
        s->jkprintf(env, s, "<tr><td>%s</td><td>%s</td></tr>", JK_CHECK_NULL(name),
                    JK_CHECK_NULL(value));
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
            s->jkprintf(env, s, "<tr><th>%s</th><td></td></tr>", JK_CHECK_NULL(mbean->name) );
        } else {
            s->jkprintf(env, s, "<tr><th rowspan='%d'>%s</th><td>%s</td><td>%s</td></tr>",
                        propCount, JK_CHECK_NULL(mbean->name),
                        JK_CHECK_NULL(mbean->settings->nameAt( env, mbean->settings, 0)),
                        JK_CHECK_NULL(mbean->settings->valueAt( env, mbean->settings, 0)));
            for( j=1; j < propCount ; j++ ) {
                char *pname=mbean->settings->nameAt( env, mbean->settings, j);
                /* Don't save redundant information */
                if( strcmp( pname, "name" ) != 0 ) {
                    s->jkprintf(env, s, "<tr><td>%s</td><td>%s</td></tr>",
                                JK_CHECK_NULL(pname),
                                JK_CHECK_NULL(mbean->settings->valueAt( env, mbean->settings, j)));
                }
            }
        }
    }
    s->jkprintf( env,s , "</table>\n" );
}

/** List metadata for endpoints ( from scoreboard ) using the remote-jmx style
  */
static void jk2_worker_status_lstEndpoints(jk_env_t *env, jk_ws_service_t *s,
                                           jk_workerEnv_t *wenv)
{
    jk_map_t *map=wenv->initData;
    int i;
    int j;
    int totalReq=0;
    int totalErr=0;
    unsigned long totalTime=0;
    unsigned long maxTime=0;
    int needHeader=JK_TRUE;
    
    if( wenv->shm==NULL || wenv->shm->head==NULL) {
        return;
    }

    for( i=1; i < wenv->shm->head->lastSlot; i++ ) {
        jk_shm_slot_t *slot= wenv->shm->getSlot( env, wenv->shm, i );

        if( slot==NULL ) continue;
        
        if( strncmp( slot->name, "epStat", 6 ) == 0 ) {
            /* This is an endpoint slot */
            void *data=slot->data;
            char *name=JK_CHECK_NULL(slot->name);

            for( j=0; j<slot->structCnt ; j++ ) {
                jk_stat_t *statArray=(jk_stat_t *)data;
                jk_stat_t *stat=statArray + j;

                s->jkprintf(env, s, "[endpoint:%s%d]\n", name, j );
                s->jkprintf(env, s, "T=endpoint\n" );

                s->jkprintf(env, s, "G=id\n"); 
                s->jkprintf(env, s, "G=workerId\n"); 
                s->jkprintf(env, s, "G=requests\n"); 
                s->jkprintf(env, s, "G=errors\n"); 
                s->jkprintf(env, s, "G=lastRequest\n"); 
                s->jkprintf(env, s, "G=lastConnectionTime\n"); 
                s->jkprintf(env, s, "G=totalTime\n"); 
                s->jkprintf(env, s, "G=maxTime\n"); 
                s->jkprintf(env, s, "G=requestStart\n"); 
                s->jkprintf(env, s, "G=jkTime\n"); 
                s->jkprintf(env, s, "G=requestEnd\n\n");
            }
        }
    }
}


/** List values for endpoints ( from scoreboard ) using the remote-jmx style
  */
static void jk2_worker_status_dmpEndpoints(jk_env_t *env, jk_ws_service_t *s,
                                           jk_workerEnv_t *wenv)
{
    jk_map_t *map=wenv->initData;
    int i;
    int j;
    int totalReq=0;
    int totalErr=0;
    unsigned long totalTime=0;
    unsigned long maxTime=0;
    int needHeader=JK_TRUE;

    if( wenv->shm==NULL || wenv->shm->head==NULL) {
        return;
    }

    for( i=1; i < wenv->shm->head->lastSlot; i++ ) {
        jk_shm_slot_t *slot= wenv->shm->getSlot( env, wenv->shm, i );

        if( slot==NULL ) continue;
        
        if( strncmp( slot->name, "epStat", 6 ) == 0 ) {
            /* This is an endpoint slot */
            void *data=slot->data;
            char *name=JK_CHECK_NULL(slot->name);
            char ctimeBuf[APR_CTIME_LEN];

            /* XXX Add info about number of slots */
            for( j=0; j<slot->structCnt ; j++ ) {
                jk_stat_t *statArray=(jk_stat_t *)data;
                
                jk_stat_t *stat=statArray + j;
                s->jkprintf(env, s, "[endpoint:%s%d]\n", name, j);
            
                s->jkprintf(env, s, "workerId=%d\n", stat->workerId);
                s->jkprintf(env, s, "id=%d\n", stat->workerId); 
                s->jkprintf(env, s, "requests=%d\n", stat->reqCnt);
                s->jkprintf(env, s, "errors=%d\n", stat->errCnt);
                s->jkprintf(env, s, "lastRequest=%s\n", JK_CHECK_NULL(stat->active)); 
                apr_ctime( ctimeBuf, stat->connectedTime );
                s->jkprintf(env, s, "lastConnectionTime=%s\n", ctimeBuf);

                s->jkprintf(env, s, "totalTime=%ld\n", (long)stat->totalTime );
                s->jkprintf(env, s, "maxTime=%ld\n", (long)stat->maxTime ); 

                s->jkprintf(env, s, "requestStart=%lu\n", (long)stat->startTime); 
                s->jkprintf(env, s, "jkTime=%ld\n", 
                            (long)(stat->jkStartTime - stat->startTime) );
                s->jkprintf(env, s, "requestEnd=%ld\n", 
                            (long)(stat->endTime - stat->startTime) );
                s->jkprintf(env, s, "\n"); 
            }

        }
    }
}

static int JK_METHOD jk2_worker_status_list(jk_env_t *env,
                                            jk_worker_t *w, 
                                            jk_ws_service_t *s)
{
    char *cName=s->query_string + 4;
    int i;
    int qryLen=0;
    int exact=1;
    
        /* Dump all attributes for the beans */
    if( strcmp( cName, "*" )==0 ) {
        *cName='\0';
        qryLen=0;
    } else {
        qryLen=strlen( cName );
    }
    if( qryLen >0 ) {
        if( cName[strlen(cName)-1] == '*' ) {
/*             printf("Exact match off %s\n", cName ); */
            cName[strlen(cName)-1]='\0';
            exact=0;
            qryLen--;
        }
    }
    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        char **getAtt=mbean->getAttributeInfo;
        char **setAtt=mbean->setAttributeInfo;
        
        /* That's a bad name, created for backward compat. It should be deprecated.. */
        if( strchr( name, ':' )==NULL )
            continue;
        
        /* Prefix */
        if( (! exact)  && qryLen != 0 && strncmp( name, cName, qryLen )!= 0 )
            continue;

        /* Endpoints are treated specially ( scoreboard to get the ep from other
            processes */
        if( strncmp( "endpoint", mbean->type, 8 ) == 0 )
            continue;

        if( strncmp( "threadMutex", mbean->type, 11 ) == 0 )
            continue;

        /* Exact */
        if( exact && qryLen != 0 && strcmp( name, cName )!= 0 )
            continue;
        
        if( mbean==NULL ) 
            continue;
        s->jkprintf(env, s, "[%s]\n", name);
        s->jkprintf(env, s, "T=%s\n", mbean->type);
        
        s->jkprintf(env, s, "G=ver\n");
        s->jkprintf(env, s, "G=disabled\n");
        s->jkprintf(env, s, "G=debug\n");
        while( getAtt != NULL && *getAtt != NULL && **getAtt!='\0' ) {
            if( strcmp( *getAtt,"ver" )==0 ||
                strcmp( *getAtt,"debug" )==0 ||
                strcmp( *getAtt,"disabled" )==0 ) {
                getAtt++;
                continue;
            }
            s->jkprintf(env, s, "G=%s\n", *getAtt);
            getAtt++;
        }
        
        s->jkprintf(env, s, "S=ver\n");
        s->jkprintf(env, s, "S=disabled\n");
        s->jkprintf(env, s, "S=debug\n");
        while( setAtt != NULL && *setAtt != NULL && **setAtt!='\0' ) {
            s->jkprintf(env, s, "S=%s\n", *setAtt);
            setAtt++;
        }
        s->jkprintf(env, s, "M=init\n");
        s->jkprintf(env, s, "M=destroy\n");

        s->jkprintf(env, s, "\n", name);
    }
    jk2_worker_status_lstEndpoints( env, s, s->workerEnv);
    return JK_OK;
}

static int JK_METHOD jk2_worker_status_dmp(jk_env_t *env,
                                           jk_worker_t *w, 
                                           jk_ws_service_t *s)
{
    char *cName=s->query_string + 4;
    int i;
    int qryLen=0;
    int exact=1;
    
    /* Dump all attributes for the beans */
    if( strcmp( cName, "*" )==0 ) {
        *cName='\0';
        qryLen=0;
    } else {
        qryLen=strlen( cName );
    }
    if( qryLen >0 ) {
        if( cName[strlen(cName)-1] == '*' ) {
/*             printf("Exact match off %s\n", cName ); */
            cName[strlen(cName)-1]='\0';
            exact=0;
            qryLen--;
        }
    }
    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        char **getAtt=mbean->getAttributeInfo;
        char **setAtt=mbean->setAttributeInfo;
        
        /* That's a bad name, created for backward compat. It should be deprecated.. */
        if( strchr( name, ':' )==NULL )
            continue;
        
        /* Endpoints are treated specially ( scoreboard to get the ep from other
            processes */
        if( strncmp( "endpoint", mbean->type, 8 ) == 0 )
            continue;

        if( strncmp( "threadMutex", mbean->type, 11 ) == 0 )
            continue;

        /* Prefix */
        if( ! exact  && qryLen != 0 && strncmp( name, cName, qryLen )!= 0 )
            continue;
        
        /* Exact */
        if( exact && qryLen != 0 && strcmp( name, cName )!= 0 )
            continue;
        
        if( mbean==NULL ) 
            continue;
        s->jkprintf(env, s, "[%s]\n", name );

        s->jkprintf(env, s, "Id=%lp\n", mbean->object ); 
        
        s->jkprintf(env, s, "ver=%d\n", mbean->ver);
        s->jkprintf(env, s, "debug=%d\n", mbean->debug);
        s->jkprintf(env, s, "disabled=%d\n", mbean->disabled);
        while( getAtt != NULL && *getAtt != NULL && **getAtt!='\0' ) {
            char *attName=*getAtt;
            char *val=mbean->getAttribute(env, mbean, *getAtt );
            if( strcmp( attName,"ver" )==0 ||
                strcmp( attName,"debug" )==0 ||
                strcmp( attName,"disabled" )==0 ) {
                getAtt++;
                continue;
            }
            s->jkprintf(env, s, "%s=%s\n", *getAtt, (val==NULL)? "NULL": val);
            getAtt++;
        }
        s->jkprintf(env, s, "\n" );
    }
    jk2_worker_status_dmpEndpoints( env, s, s->workerEnv);
    return JK_OK;
}

static int JK_METHOD jk2_worker_status_qry(jk_env_t *env,
                                           jk_worker_t *w, 
                                           jk_ws_service_t *s)
{
    char *cName=s->query_string + 4;
    int i;
    int qryLen=0;
    int exact=1;
    char localName[256];
    int needQuote=0;
    
    /* Dump all attributes for the beans */
    if( strcmp( cName, "*" )==0 ) {
        *cName='\0';
        qryLen=0;
    } else {
        qryLen=strlen( cName );
    }
    if( qryLen >0 ) {
        if( cName[strlen(cName)-1] == '*' ) {
            cName[strlen(cName)-1]='\0';
            exact=0;
            qryLen--;
        }
    }
    /* Create a top section - not used currently */
    s->jkprintf(env, s, "MXAgent: mod_jk2\n" );
    s->jkprintf(env, s, "\n" );
    
    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        char **getAtt=mbean->getAttributeInfo;
        char **setAtt=mbean->setAttributeInfo;
        int j,k;
        
        /* That's a bad name, created for backward compat. It should be deprecated.. */
        if( strchr( name, ':' )==NULL )
            continue;
        
        /* Endpoints are treated specially ( scoreboard to get the ep from other
            processes */
        if( strncmp( "endpoint", mbean->type, 8 ) == 0 )
            continue;

        if( strncmp( "threadMutex", mbean->type, 11 ) == 0 )
            continue;

        /* Prefix */
        if( ! exact  && qryLen != 0 && strncmp( name, cName, qryLen )!= 0 )
            continue;
        
        /* Exact */
        if( exact && qryLen != 0 && strcmp( name, cName )!= 0 )
            continue;
        
        if( mbean==NULL ) 
            continue;

        if( mbean->localName==NULL ) {
            s->jkprintf(env, s, "Name: modjk:type=%s\n", mbean->type );
        } else if( mbean->localName[0]=='\0' ) {
            s->jkprintf(env, s, "Name: modjk:type=%s\n", mbean->type );
        } else {
            needQuote=0;
            localName[0]='\0';
            for( j=0, k=0; k<255; j++,k++ ) {
                char c;
                c=mbean->localName[j];
                if( c=='\n' ) {
                    localName[k++]='\\';
                    localName[k]='n';
                } else if( c=='*' || c=='"' || c=='\\' || c=='?' ) {
                    localName[k++]='\\';
                    localName[k]=c;
                    needQuote=1;
                } else {
                    localName[k]=c;
                }
            }
            localName[k]='\0';
            if( needQuote ) {
                s->jkprintf(env, s, "Name: modjk:type=%s,name=\"%s\"\n", mbean->type, localName );
            } else {
                s->jkprintf(env, s, "Name: modjk:type=%s,name=%s\n", mbean->type, localName );
            }
        }
        

        /** Will be matched against modeler-mbeans.xml in that dir */
        s->jkprintf(env, s, "modelerType: org.apache.jk.modjk.%s\n", mbean->type );
        
        s->jkprintf(env, s, "Id: %lp\n", mbean->object ); 
        s->jkprintf(env, s, "ver: %d\n", mbean->ver);
        s->jkprintf(env, s, "debug: %d\n", mbean->debug);
        s->jkprintf(env, s, "disabled: %d\n", mbean->disabled);

        while( getAtt != NULL && *getAtt != NULL && **getAtt!='\0' ) {
            char *attName=*getAtt;
            char *val=mbean->getAttribute(env, mbean, *getAtt );
            if( strcmp( attName,"ver" )==0 ||
                strcmp( attName,"debug" )==0 ||
                strcmp( attName,"disabled" )==0 ) {
                getAtt++;
                continue;
            }

            if( val!=NULL && strchr( val, '\n' ) != NULL ) {
                /* XXX escape ? */
                continue;
            }
            s->jkprintf(env, s, "%s: %s\n", *getAtt, (val==NULL)? "NULL": val);
            getAtt++;
        }
        s->jkprintf(env, s, "\n" );
    }
    jk2_worker_status_dmpEndpoints( env, s, s->workerEnv);
    return JK_OK;
}


static int JK_METHOD jk2_worker_status_get(jk_env_t *env,
                                           jk_worker_t *w, 
                                           jk_ws_service_t *s)
{
    char *cName=s->query_string + 4;
    char *attName=strrchr(cName, '|' );
    int i;
    
    if( attName == NULL ) {
        s->jkprintf( env, s, "ERROR: no attribute value found %s\n", cName);
        return JK_OK;
    }
    *attName='\0';
    attName++;
    
    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        
        if( mbean==NULL ) 
            continue;
        
        if( strcmp( name, cName ) == 0 &&
            mbean->getAttribute != NULL ) {
            void *result=mbean->getAttribute( env, mbean, attName );
            s->jkprintf( env, s, "OK|%s|%s", cName, attName );
            s->jkprintf( env, s, "%s", result );
            return JK_OK;
        }
    }
    s->jkprintf( env, s, "ERROR|mbean not found|%s\n", cName );
    return JK_OK;
}

static int JK_METHOD jk2_worker_status_set(jk_env_t *env,
                                           jk_worker_t *w, 
                                           jk_ws_service_t *s)
{
    char *cName=s->query_string + 4;
    char *attVal=strrchr(cName, '|' );
    char *attName;
    int i;
    
    if( attVal == NULL ) {
        s->jkprintf( env, s, "ERROR: no attribute value found %s\n", cName);
        return JK_OK;
    }
    *attVal='\0';
    attVal++;
    
    attName=strrchr( cName, '|' );
    if( attName == NULL ) {
        s->jkprintf( env, s, "ERROR: attribute name not found\n", cName);
        return JK_OK;
    }
    *attName='\0';
    attName++;
    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        
        if( mbean==NULL ) 
            continue;
        
        if( strcmp( name, cName ) == 0 &&
            mbean->setAttribute != NULL ) {
            int res;

            jk_shm_t *shm=w->workerEnv->shm;

            /* Found the object */
/*             int res=mbean->setAttribute( env, mbean, attName, attVal ); */
/*             if( w->mbean->debug > 0 )  */
                env->l->jkLog(env, env->l, JK_LOG_DEBUG, "status.set() %s %s\n",
                              cName, attName);

            res=jk2_config_setProperty(env, w->workerEnv->config,
                                           mbean, attName, attVal);
                
            /* Increment the version */
/*             jk2_config_setProperty(env, w->workerEnv->config, */
            mbean->ver++;

            /* Save */
            w->workerEnv->config->save( env, w->workerEnv->config, NULL );

        
            /* Update the scoreboard's version - all other
               jk2 processes will see this and update
            */
            if( shm!=NULL && shm->head!=NULL )
                shm->head->lbVer++;
            
            s->jkprintf( env, s, "OK|%s|%s|%d", cName, attName, res );
            return JK_OK;
        }
    }
    s->jkprintf( env, s, "ERROR|not found|%s|%s|%s\n", cName, attName, attVal );
    return JK_OK;
}

static int JK_METHOD jk2_worker_status_invoke(jk_env_t *env,
                                              jk_worker_t *w, 
                                              jk_ws_service_t *s)
{
    char *cName=s->query_string + 4;
    char *attName;
    int i;
    
    attName=strrchr( cName, '|' );
    if( attName == NULL ) {
        s->jkprintf( env, s, "ERROR: attribute name not found\n", cName);
        return JK_OK;
    }
    *attName='\0';
    attName++;

    for( i=0; i < env->_objects->size( env, env->_objects ); i++ ) {
        char *name=env->_objects->nameAt( env, env->_objects, i );
        jk_bean_t *mbean=env->_objects->valueAt( env, env->_objects, i );
        int res;
        
        if( mbean==NULL ) 
            continue;
        
        if( strcmp( name, cName ) == 0 ) {

            if( strcmp( attName, "init" ) ) {
                if( mbean->init != NULL )
                    res=mbean->init( env, mbean );
            }
            if( strcmp( attName, "destroy" ) ) {
                if( mbean->destroy != NULL )
                    res=mbean->destroy( env, mbean );
            }
            
            s->jkprintf( env, s, "OK|%s|%s|%d", cName, attName, res );
            return JK_OK;
        }
    }
    s->jkprintf( env, s, "ERROR|not found|%s|%s\n", cName, attName );
    return JK_OK;
}


static int JK_METHOD jk2_worker_status_service(jk_env_t *env,
                                               jk_worker_t *w, 
                                               jk_ws_service_t *s)
{
    char *uri=s->req_uri;
    int didUpdate;

    if( w->mbean->debug > 0 ) 
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, "status.service() %s %s\n",
                      JK_CHECK_NULL(uri), JK_CHECK_NULL(s->query_string));

    /* Generate the header */
    s->status=200;
    s->msg="OK";
    if( s->query_string != NULL &&
        strncmp( s->query_string, "qry=", 4) == 0 ) {
        s->headers_out->put(env, s->headers_out,
                            "Content-Type", "text/plain", NULL);
    } else {
        s->headers_out->put(env, s->headers_out,
                            "Content-Type", "text/html", NULL);
    }
    s->head(env, s );

	s->jkprintf(env, s, "<style>%s</style>\n", DEFAULT_CSS );

    /** Process the query string.
     */
    if( s->query_string == NULL ) {
        s->query_string="all";
    }

    if( strcmp( s->query_string, "scoreboard.reset" ) == 0 ) {
        jk2_worker_status_resetScoreboard(env, s, s->workerEnv );
        s->jkprintf(env, s, "Scoreboard reset\n"  );
    }

    /** Updating the config is the first thing we do.
        All other operations will happen on the update config.
      */
    w->workerEnv->config->update( env, w->workerEnv->config, &didUpdate );
    env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                      "status.update check %d\n", didUpdate );
    if( didUpdate ) {
        jk_shm_t *shm=w->workerEnv->shm;
        
        /* Update the scoreboard's version - all other
           jk2 processes will see this and update
        */
        if( shm!=NULL && shm->head!=NULL ) {
            shm->head->lbVer++;
            s->jkprintf(env, s, "Updated config version to %d\n", shm->head->lbVer  );
        } else {
            s->jkprintf(env, s, "Update detected. No scoreboard.\n" );
        }
    }
    

    /* Increment the scoreboard version counter, reload */
    if( strncmp( s->query_string, "rld=",4 ) == 0 ) {
        jk_shm_t *shm=w->workerEnv->shm;
        
        /* Update the scoreboard's version - all other
           jk2 processes will see this and update
        */
        if( shm!=NULL && shm->head!=NULL ) {
            shm->head->lbVer++;
            s->jkprintf(env, s, "Updated config version to %d\n", shm->head->lbVer  );
        } else {
            s->jkprintf(env, s, "Reload requested. No scoreboard.\n" );
        }
        return JK_OK;
    }

    /** list all mbeans and their attributes  */
    if( strncmp( s->query_string, "lst=", 4) == 0 ) {
        return jk2_worker_status_list(env, w, s);
    }

    /** Dump multiple attributes  */
    if( strncmp( s->query_string, "dmp=", 4) == 0 ) {
        return jk2_worker_status_dmp(env, w, s);
    }

    /** Commons-modeler  */
    if( strncmp( s->query_string, "qry=", 4) == 0 ) {
        return jk2_worker_status_qry(env, w, s);
    }

    /** Get a single attribute. This also works for attributes that are not listed in getAtt **/
    if( strncmp( s->query_string, "get=", 4) == 0 ) {
        return jk2_worker_status_get(env, w, s);
    }

    /** Set an attribute. Works for attributes that are not listed in setAtt **/
    if( strncmp( s->query_string, "set=", 4) == 0 ) {
        return jk2_worker_status_set(env, w, s);
    }

    if( strncmp( s->query_string, "inv=", 4) == 0 ) {
        return jk2_worker_status_invoke(env, w, s);
    }

    s->jkprintf(env, s, "Status information for child %d<br>", s->workerEnv->childId  );
    s->jkprintf(env, s, " <a href='jkstatus?cfgOrig=1'>[Original config]</a>\n");
    s->jkprintf(env, s, " <a href='jkstatus?cfgParsed=1'>[Processed config]</a>\n");
    s->jkprintf(env, s, " <a href='jkstatus?scoreboard=1'>[Scoreboard info]</a>\n");
    s->jkprintf(env, s, " <a href='jkstatus'>[Workers, Channels and URIs]</a>\n");
    
    if( strncmp( s->query_string, "cfgOrig=", 8) == 0 ) {
        jk2_worker_status_displayConfigProperties(env, s, s->workerEnv );
        return JK_OK;
    }

    if( strncmp( s->query_string, "cfgParsed=", 10) == 0 ) {
        jk2_worker_status_displayActiveProperties(env, s, s->workerEnv );
        return JK_OK;
    }

    if( strncmp( s->query_string, "scoreboard=", 11) == 0 ) {
        jk2_worker_status_displayScoreboardInfo(env, s, s->workerEnv );
        return JK_OK;
    }
    if( strncmp( s->query_string, "endpoints=", 11) == 0 ) {
        jk2_worker_status_displayEndpointInfo( env, s, s->workerEnv );
        return JK_OK;
    }

    /* Body */
    jk2_worker_status_displayRuntimeType(env, s, s->workerEnv, "ajp13" );
    jk2_worker_status_displayRuntimeType(env, s, s->workerEnv, "channel.socket" );
    jk2_worker_status_displayRuntimeType(env, s, s->workerEnv, "channel.un" );
    jk2_worker_status_displayRuntimeType(env, s, s->workerEnv, "channel.jni" );
    jk2_worker_status_displayRuntimeType(env, s, s->workerEnv, "uri" );
    
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

