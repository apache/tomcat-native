/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2003 The Apache Software Foundation.          *
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
 * Description: ISAPI plugin for IIS/PWS                                   *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Larry Isaacs <larryi@apache.org>                           *
 * Author:      Ignacio J. Ortega <nacho@apache.org>                       *
 * Version:     $Revision$                                           *
 ***************************************************************************/

// This define is needed to include wincrypt,h, needed to get client certificates
#define _WIN32_WINNT 0x0400

#include <httpext.h>
#include <httpfilt.h>
#include <wininet.h>

#include "jk_global.h"
#include "jk_requtil.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_logger.h"
#include "jk_env.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "apr_general.h"
#include "jk_iis.h"
//#include "jk_uri_worker_map.h"

#define SERVER_ROOT_TAG         ("serverRoot")
#define EXTENSION_URI_TAG       ("extensionUri")
#define WORKERS_FILE_TAG        ("workersFile")
#define USE_AUTH_COMP_TAG       ("authComplete")
#define THREAD_POOL_TAG         ("threadPool")
#define SEND_GROUPS_TAG         ("sendGroups")


static char  file_name[_MAX_PATH];
static char  ini_file_name[MAX_PATH];
static int   using_ini_file = JK_FALSE;
static int   is_inited = JK_FALSE;
static int   is_mapread = JK_FALSE;
static int   was_inited = JK_FALSE;
static DWORD auth_notification_flags = 0;
static int   use_auth_notification_flags = 0;
int          send_groups = 0;

static jk_workerEnv_t *workerEnv;
apr_pool_t *jk_globalPool;

static char extension_uri[INTERNET_MAX_URL_LENGTH] = "/jakarta/isapi_redirector2.dll";
static char worker_file[MAX_PATH * 2] = "";
static char server_root[MAX_PATH * 2] = "";


static int init_jk(char *serverName);

static int initialize_extension();

static int read_registry_init_data(jk_env_t *env);
extern int jk_jni_status_code;

static int get_registry_config_parameter(HKEY hkey,
                                         const char *tag, 
                                         char *b,
                                         DWORD sz);


static jk_env_t* jk2_create_config();
static int get_auth_flags();

/*  ThreadPool support
 *
 */
int use_thread_pool = 0;

static void write_error_response(PHTTP_FILTER_CONTEXT pfc,char *status,char * msg)
{
    char crlf[3] = { (char)13, (char)10, '\0' };
    char *ctype = "Content-Type:text/html\r\n\r\n";
    DWORD len = strlen(msg);

    /* reject !!! */
    pfc->ServerSupportFunction(pfc, 
                               SF_REQ_SEND_RESPONSE_HEADER,
                               status,
                               (DWORD)crlf,
                               (DWORD)ctype);
    pfc->WriteClient(pfc, msg, &len, 0);
}

HANDLE jk2_starter_event;
HANDLE jk2_inited_event;
HANDLE jk2_starter_thread = NULL;

VOID jk2_isapi_starter( LPVOID lpParam ) 
{
    Sleep(1000);
    
    apr_initialize();
    apr_pool_create( &jk_globalPool, NULL );
    initialize_extension();
    if (is_inited) {
        if (init_jk(NULL))
            is_mapread = JK_TRUE;
    }
    SetEvent(jk2_inited_event);
    WaitForSingleObject(jk2_starter_event, INFINITE);

    if (is_inited) {
        was_inited = JK_TRUE;
        is_inited = JK_FALSE;
        if (workerEnv) {
            jk_env_t *env = workerEnv->globalEnv;
            jk2_iis_close_pool(env);
            workerEnv->close(env, workerEnv);
        }
        is_mapread = JK_FALSE;
    }
    apr_pool_destroy(jk_globalPool);
    apr_terminate();
    /* Clean up and die. */
    ExitThread(0); 
} 

BOOL WINAPI GetFilterVersion(PHTTP_FILTER_VERSION pVer)
{
    DWORD dwThreadId;
    ULONG http_filter_revision = HTTP_FILTER_REVISION;

    jk2_inited_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    jk2_starter_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    jk2_starter_thread = CreateThread(NULL, 0,
                                      (LPTHREAD_START_ROUTINE)jk2_isapi_starter,
                                      NULL,
                                      0,
                                      &dwThreadId);
                        
    WaitForSingleObject(jk2_inited_event, INFINITE);
    if (!is_inited || !is_mapread) {
        return FALSE;
    }
    pVer->dwFilterVersion = pVer->dwServerFilterVersion;
    if (pVer->dwFilterVersion > http_filter_revision) {
        pVer->dwFilterVersion = http_filter_revision;
    }
    auth_notification_flags = get_auth_flags();
#ifdef SF_NOTIFY_AUTH_COMPLETE
    if (auth_notification_flags == SF_NOTIFY_AUTH_COMPLETE) {
        pVer->dwFlags = SF_NOTIFY_ORDER_HIGH        | 
                        SF_NOTIFY_SECURE_PORT       | 
                        SF_NOTIFY_NONSECURE_PORT    |
                        SF_NOTIFY_PREPROC_HEADERS   |
                        SF_NOTIFY_AUTH_COMPLETE;
    }
    else
#endif
    {
        pVer->dwFlags = SF_NOTIFY_ORDER_HIGH        | 
                        SF_NOTIFY_SECURE_PORT       | 
                        SF_NOTIFY_NONSECURE_PORT    |
                        SF_NOTIFY_PREPROC_HEADERS;   
    }

    strcpy(pVer->lpszFilterDesc, VERSION_STRING);

    return TRUE;
}

DWORD WINAPI HttpFilterProc(PHTTP_FILTER_CONTEXT pfc,
                            DWORD dwNotificationType, 
                            LPVOID pvNotification)
{
    jk_env_t *env=NULL; 
    jk_uriEnv_t *uriEnv=NULL;

    if (!is_inited) {
        initialize_extension();
    }

    /* Initialise jk */
    if (is_inited && !is_mapread) {
        char serverName[MAX_SERVERNAME];
        DWORD dwLen = sizeof(serverName);

        if (pfc->GetServerVariable(pfc, SERVER_NAME, serverName, &dwLen)){
            if (dwLen > 0) {
                serverName[dwLen-1] = '\0';
            }
            if (init_jk(serverName)){
                is_mapread = JK_TRUE;
            }
        }
        /* If we can't read the map we become dormant */
        if (!is_mapread)
            is_inited = JK_FALSE;
    }
    if (is_inited && is_mapread) {
        env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );

        if (auth_notification_flags == dwNotificationType)
        { 
            char uri[INTERNET_MAX_URL_LENGTH]; 
            char snuri[INTERNET_MAX_URL_LENGTH]="/";
            char Host[INTERNET_MAX_URL_LENGTH];
            char Translate[INTERNET_MAX_URL_LENGTH];
            char Port[INTERNET_MAX_URL_LENGTH];
            BOOL (WINAPI * GetHeader) 
                (struct _HTTP_FILTER_CONTEXT * pfc, LPSTR lpszName, LPVOID lpvBuffer, LPDWORD lpdwSize );
            BOOL (WINAPI * SetHeader) 
                (struct _HTTP_FILTER_CONTEXT * pfc, LPSTR lpszName, LPSTR lpszValue );
            BOOL (WINAPI * AddHeader) 
                (struct _HTTP_FILTER_CONTEXT * pfc, LPSTR lpszName,LPSTR lpszValue );
            char *query;
            DWORD sz = sizeof(uri);
            DWORD szHost = sizeof(Host);
            DWORD szTranslate = sizeof(Translate);
            DWORD szPort = sizeof(Port);
            int   nPort;
#ifdef SF_NOTIFY_AUTH_COMPLETE
            if (auth_notification_flags == SF_NOTIFY_AUTH_COMPLETE) {
                GetHeader=((PHTTP_FILTER_AUTH_COMPLETE_INFO)pvNotification)->GetHeader;
                SetHeader=((PHTTP_FILTER_AUTH_COMPLETE_INFO)pvNotification)->SetHeader;
                AddHeader=((PHTTP_FILTER_AUTH_COMPLETE_INFO)pvNotification)->AddHeader;
            } 
            else 
#endif
            {
                GetHeader=((PHTTP_FILTER_PREPROC_HEADERS)pvNotification)->GetHeader;
                SetHeader=((PHTTP_FILTER_PREPROC_HEADERS)pvNotification)->SetHeader;
                AddHeader=((PHTTP_FILTER_PREPROC_HEADERS)pvNotification)->AddHeader;
            }

            env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                   "HttpFilterProc started\n");


            /*
             * Just in case somebody set these headers in the request!
             */
            SetHeader(pfc, URI_HEADER_NAME, NULL);
            SetHeader(pfc, QUERY_HEADER_NAME, NULL);
            SetHeader(pfc, WORKER_HEADER_NAME, NULL);
            SetHeader(pfc, TOMCAT_TRANSLATE_HEADER_NAME, NULL);
        
            if (!GetHeader(pfc, "url", (LPVOID)uri, (LPDWORD)&sz)) {
                env->l->jkLog(env, env->l,  JK_LOG_ERROR, 
                       "HttpFilterProc error while getting the url\n");
                workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
                return SF_STATUS_REQ_ERROR;
            }

            if (strlen(uri)) {
                int rc;
                char *worker=0;
                query = strchr(uri, '?');
                if (query) {
                    *query++ = '\0';
                }

                rc = jk_requtil_unescapeUrl(uri);
                jk_requtil_getParents(uri);

                if (pfc->GetServerVariable(pfc, SERVER_NAME, (LPVOID)Host, (LPDWORD)&szHost)){
                    if (szHost > 0) {
                        Host[szHost-1] = '\0';
                    }
                }
                Port[0] = '\0';
                if (pfc->GetServerVariable(pfc, "SERVER_PORT", (LPVOID)Port, (LPDWORD)&szPort)){
                    if (szPort > 0) {
                        Port[szPort-1] = '\0';
                    }
                }
                nPort = atoi(Port);
                env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                            "In HttpFilterProc Virtual Host redirection of %s : %s\n", 
                            Host, Port);
                uriEnv = workerEnv->uriMap->mapUri(env, workerEnv->uriMap,Host, nPort, uri );

                if( uriEnv!=NULL ) {
                    char *forwardURI;

                    /* This is a servlet, should redirect ... */
                    /* First check if the request was invalidated at decode */
                    if (rc == BAD_REQUEST) {
                        env->l->jkLog(env, env->l,  JK_LOG_ERROR, 
                            "HttpFilterProc [%s] contains one or more invalid escape sequences.\n", 
                            uri);
                        write_error_response(pfc,"400 Bad Request", HTML_ERROR_400);
                        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
                        return SF_STATUS_REQ_FINISHED;
                    }
                    else if(rc == BAD_PATH) {
                        env->l->jkLog(env, env->l,  JK_LOG_EMERG, 
                            "HttpFilterProc [%s] contains forbidden escape sequences.\n", 
                            uri);
                        write_error_response(pfc,"403 Forbidden", HTML_ERROR_403);
                        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
                        return SF_STATUS_REQ_FINISHED;
                    }
                    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                           "HttpFilterProc [%s] is a servlet url - should redirect to %s\n", 
                           uri, uriEnv->workerName);
                
                    /* get URI we should forward */
                
                    if( workerEnv->options == JK_OPT_FWDURICOMPATUNPARSED ){
                        /* get original unparsed URI */
                        GetHeader(pfc, "url", (LPVOID)uri, (LPDWORD)&sz);
                        /* restore terminator for uri portion */
                        if (query)
                            *(query - 1) = '\0';
                        env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                               "HttpFilterProc fowarding original URI [%s]\n",uri);
                        forwardURI = uri;
                    } else if( workerEnv->options == JK_OPT_FWDURIESCAPED ){
                        if (!jk_requtil_escapeUrl(uri,snuri,INTERNET_MAX_URL_LENGTH)) {
                            env->l->jkLog(env, env->l,  JK_LOG_ERROR, 
                                   "HttpFilterProc [%s] re-encoding request exceeds maximum buffer size.\n", 
                                   uri);
                            write_error_response(pfc,"400 Bad Request", HTML_ERROR_400);
                            workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
                            return SF_STATUS_REQ_FINISHED;
                        }
                        env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                               "HttpFilterProc fowarding escaped URI [%s]\n",snuri);
                        forwardURI = snuri;
                    } else {
                        forwardURI = uri;
                    }

                    if(!AddHeader(pfc, URI_HEADER_NAME, forwardURI) || 
                       ( (query != NULL && strlen(query) > 0)
                               ? !AddHeader(pfc, QUERY_HEADER_NAME, query) : FALSE ) || 
                       !AddHeader(pfc, WORKER_HEADER_NAME, uriEnv->workerName) ||
                       !SetHeader(pfc, "url", extension_uri)) {
                        env->l->jkLog(env, env->l,  JK_LOG_ERROR, 
                               "HttpFilterProc error while adding request headers\n");
                        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
                        return SF_STATUS_REQ_ERROR;
                    }
                    
                    /* Move Translate: header to a temporary header so
                     * that the extension proc will be called.
                     * This allows the servlet to handle 'Translate: f'.
                     */
                    if(GetHeader(pfc, "Translate:", (LPVOID)Translate, (LPDWORD)&szTranslate) &&
                        Translate != NULL && szTranslate > 0) {
                        if (!AddHeader(pfc, TOMCAT_TRANSLATE_HEADER_NAME, Translate)) {
                            env->l->jkLog(env, env->l,  JK_LOG_ERROR, 
                              "HttpFilterProc error while adding Tomcat-Translate headers\n");
                            workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
                            return SF_STATUS_REQ_ERROR;
                        }
                        SetHeader(pfc, "Translate:", NULL);
                    }
                } else {
                    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                           "HttpFilterProc [%s] is not a servlet url\n", 
                           uri);
                }

                /*
                 * Check if somebody is feeding us with his own TOMCAT data headers.
                 * We reject such postings !
                 */
                env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                       "HttpFilterProc check if [%s] is pointing to the web-inf directory\n", 
                       uri);

                if(jk_requtil_uriIsWebInf(uri)) {
                    env->l->jkLog(env, env->l,  JK_LOG_EMERG, 
                           "HttpFilterProc [%s] points to the web-inf or meta-inf directory.\nSomebody try to hack into the site!!!\n", 
                           uri);

                    write_error_response(pfc,"403 Forbidden", HTML_ERROR_403);
                    workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
                    return SF_STATUS_REQ_FINISHED;
                }
            }
        }
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
    }
    return SF_STATUS_REQ_NEXT_NOTIFICATION;
}


BOOL WINAPI GetExtensionVersion(HSE_VERSION_INFO  *pVer)
{
    pVer->dwExtensionVersion = MAKELONG( HSE_VERSION_MINOR,
                                         HSE_VERSION_MAJOR );

    strcpy(pVer->lpszExtensionDesc, VERSION_STRING);

    return TRUE;
}

DWORD WINAPI HttpExtensionProcWorker(LPEXTENSION_CONTROL_BLOCK  lpEcb, 
                                     jk_ws_service_t *service)
{   
    DWORD rc = HSE_STATUS_ERROR;
    jk_env_t *env;
    jk_ws_service_t sOnStack;
    jk_ws_service_t *s;
    char *worker_name;
    char huge_buf[16 * 1024]; /* should be enough for all */
    DWORD huge_buf_sz;        
    jk_worker_t *worker;
    jk_pool_t *rPool=NULL;
    int rc1;

    if (service)
        s = service;
    else
        s = &sOnStack;

    lpEcb->dwHttpStatusCode = HTTP_STATUS_SERVER_ERROR;
    env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );
    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                  "HttpExtensionProc started\n");

    huge_buf_sz = sizeof(huge_buf);
    get_server_value(lpEcb, HTTP_WORKER_HEADER_NAME, huge_buf, huge_buf_sz, "");
    worker_name = huge_buf;

    worker=env->getByName( env, worker_name);

    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
        "HttpExtensionProc %s a worker for name %s\n", 
        worker ? "got" : "could not get",
        worker_name);

    if( worker==NULL ){
        env->l->jkLog(env, env->l,  JK_LOG_ERROR, 
            "HttpExtensionProc worker is NULL\n");
        return rc;            
    }
    /* Get a pool for the request XXX move it in workerEnv to
    be shared with other server adapters */
    rPool= worker->rPoolCache->get( env, worker->rPoolCache );
    if( rPool == NULL ) {
        rPool=worker->mbean->pool->create( env, worker->mbean->pool, HUGE_POOL_SIZE );
        env->l->jkLog(env, env->l, JK_LOG_DEBUG,
            "HttpExtensionProc: new rpool\n");
    }

    jk2_service_iis_init( env, s );
    s->pool = rPool;

    /* reset the reco_status, will be set to INITED in LB mode */
    s->reco_status = RECO_NONE;
    
    s->is_recoverable_error = JK_FALSE;
    s->response_started = JK_FALSE;
    s->content_read = 0;
    s->ws_private = lpEcb;
    s->workerEnv = workerEnv;

    /* Initialize the ws_service structure */
    s->init( env, s, worker, lpEcb );

    if (JK_OK == worker->service(env, worker, s)){
        rc=HSE_STATUS_SUCCESS;
        lpEcb->dwHttpStatusCode = HTTP_STATUS_OK;
        env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
            "HttpExtensionProc service() returned OK\n");
    } else {
        env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
            "HttpExtensionProc service() Failed\n");
    }

    s->afterRequest(env, s);

    if (service != NULL) {
        lpEcb->ServerSupportFunction(lpEcb->ConnID, 
                                     HSE_REQ_DONE_WITH_SESSION, 
                                     NULL, 
                                     NULL, 
                                     NULL);
    }
    rPool->reset(env, rPool);
    rc1=worker->rPoolCache->put( env, worker->rPoolCache, rPool );

    workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );

    return rc;
}

DWORD WINAPI HttpExtensionProc(LPEXTENSION_CONTROL_BLOCK  lpEcb)
{
    if (is_inited) {
        if (!use_thread_pool) {
            return HttpExtensionProcWorker(lpEcb, NULL);
        }
        else {
            /* Pass the request to the thread pool */
            jk2_iis_thread_pool(lpEcb);
            return HSE_STATUS_PENDING;
        }
    }
    return HSE_STATUS_ERROR;
}

BOOL WINAPI TerminateExtension(DWORD dwFlags) 
{
    return TerminateFilter(dwFlags);
}

BOOL WINAPI TerminateFilter(DWORD dwFlags) 
{
    /* detatch the starter thread */
    SetEvent(jk2_starter_event);
    WaitForSingleObject(jk2_starter_thread, 3000);
    CloseHandle(jk2_starter_thread);
    jk2_starter_thread = INVALID_HANDLE_VALUE;
    return TRUE;
}



BOOL WINAPI DllMain(HINSTANCE hInst,        // Instance Handle of the DLL
                    ULONG ulReason,         // Reason why NT called this DLL
                    LPVOID lpReserved)      // Reserved parameter for future use
{
    BOOL fReturn = TRUE; 
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char fname[_MAX_FNAME];

    switch (ulReason) {
        case DLL_PROCESS_ATTACH:
            if (GetModuleFileName( hInst, file_name, sizeof(file_name))) {
                _splitpath( file_name, drive, dir, fname, NULL );
                _makepath( ini_file_name, drive, dir, fname, ".properties" );
                
            } else {
                fReturn = JK_FALSE;
            }
        break;
        default:
        break;
    } 
    return fReturn;
}

static int init_jk(char *serverName)
{
    int rc = JK_TRUE;  
    /* XXX this need review, works well because the initializations are done at the first request 
       but in case inits should be splited another time using directly globalEnv here could lead 
       to subtle problems.. 
    */   
    jk_env_t *env = workerEnv->globalEnv;
    workerEnv->initData->add( env, workerEnv->initData, "serverRoot",
                              workerEnv->pool->pstrdup( env, workerEnv->pool, server_root));
    /* Logging the initialization type: registry or properties file in virtual dir
    */
    if(strlen(worker_file)){
        rc=(JK_OK == workerEnv->config->setPropertyString( env, workerEnv->config, "config.file", worker_file ));
    }
    workerEnv->init(env,workerEnv);
    env->l->jkLog(env, env->l, JK_LOG_INFO, "Set serverRoot %s\n", server_root);
    if (using_ini_file) {
        env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using ini file %s.\n", ini_file_name);
    } else {
        env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using registry.\n");
    }
    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using extension uri %s.\n", extension_uri);
    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using server root %s.\n", server_root);
    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using worker file %s.\n", worker_file);
    return rc;
}

static int initialize_extension()
{
    jk_env_t *env=jk2_create_config();   
    if (read_registry_init_data(env)) {
        jk2_iis_init_pool(env);
        is_inited = JK_TRUE;
    }
    return is_inited;
}

static int read_registry_init_data(jk_env_t *env)
{
    char tmpbuf[INTERNET_MAX_URL_LENGTH];
    HKEY hkey;
    long rc;
    int  ok = JK_TRUE; 
    char *tmp;
    jk_map_t *map;
   
    if (JK_OK==jk2_map_default_create(env, &map, workerEnv->pool )) {
        if (JK_OK==jk2_config_file_read(env,map, ini_file_name)) {
            tmp = map->get(env,map,EXTENSION_URI_TAG);
            if (tmp) {
                strcpy(extension_uri, tmp);
            } else {
                ok = JK_FALSE;
            }
            tmp = map->get(env,map,SERVER_ROOT_TAG);
            if (tmp) {
                strcpy(server_root, tmp);
            } else {
                ok = JK_FALSE;
            }
            tmp = map->get(env,map,WORKERS_FILE_TAG);
            if (tmp) {
                strcpy(worker_file, tmp);
            }
            tmp = map->get(env,map,THREAD_POOL_TAG);
            if (tmp) {
                use_thread_pool = atoi(tmp);
                if (use_thread_pool < 10)
                    use_thread_pool = 0;
            }
            tmp = map->get(env,map,USE_AUTH_COMP_TAG);
            if (tmp) {
                use_auth_notification_flags = atoi(tmp);
            }
            tmp = map->get(env,map,SEND_GROUPS_TAG);
            if (tmp) {
                send_groups = atoi(tmp);
            }
            using_ini_file=JK_TRUE;            
            return ok;
        }     
    } else {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
               "read_registry_init_data, Failed to create map \n");
    }
    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                      REGISTRY_LOCATION,
                      (DWORD)0,
                      KEY_READ,
                      &hkey);
    if(ERROR_SUCCESS != rc) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
               "read_registry_init_data, Failed Registry OpenKey %s\n",REGISTRY_LOCATION);
        return JK_FALSE;
    } 

    if(get_registry_config_parameter(hkey,
                                     EXTENSION_URI_TAG,
                                     tmpbuf,
                                     sizeof(extension_uri))) {
        strcpy(extension_uri, tmpbuf);
    } else {
        ok = JK_FALSE;
    }

    if(get_registry_config_parameter(hkey,
                                     SERVER_ROOT_TAG,
                                     tmpbuf,
                                     sizeof(server_root))) {
        strcpy(server_root, tmpbuf);
    } else {
        ok = JK_FALSE;
    }
    if(get_registry_config_parameter(hkey,
                                     WORKERS_FILE_TAG,
                                     tmpbuf,
                                     sizeof(server_root))) {
        strcpy(worker_file, tmpbuf);
    } else {
        ok = JK_FALSE;
    }
    if(get_registry_config_parameter(hkey,
                                     THREAD_POOL_TAG,
                                     tmpbuf,
                                     8)) {
        use_thread_pool = atoi(tmpbuf);
        if (use_thread_pool < 10) {
            use_thread_pool = 0;
            env->l->jkLog(env, env->l, JK_LOG_INFO, 
                          "read_registry_init_data, ThreadPool must be set to the value 10 or higher\n");
        }
    }
    if(get_registry_config_parameter(hkey,
                                     USE_AUTH_COMP_TAG,
                                     tmpbuf,
                                     8)) {
        use_auth_notification_flags = atoi(tmpbuf);
    }

    if(get_registry_config_parameter(hkey,
                                     SEND_GROUPS_TAG,
                                     tmpbuf,
                                     8)) {
        send_groups = atoi(tmpbuf);
    }

    RegCloseKey(hkey);
    return ok;
} 

static int get_registry_config_parameter(HKEY hkey,
                                         const char *tag, 
                                         char *b,
                                         DWORD sz)
{   
    DWORD type = 0;
    LONG  lrc;

    lrc = RegQueryValueEx(hkey,     
                          tag,      
                          (LPDWORD)0,
                          &type,    
                          (LPBYTE)b,
                          &sz); 
    if ((ERROR_SUCCESS != lrc) || (type != REG_SZ)) {
        return JK_FALSE;        
    }
    
    b[sz] = '\0';

    return JK_TRUE;     
}


/** Basic initialization for jk2.
 */
static  jk_env_t*  jk2_create_workerEnv (void) {

    jk_logger_t *l;
    jk_pool_t *globalPool;
    jk_bean_t *jkb;
    jk_env_t *env;

    jk2_pool_apr_create( NULL, &globalPool, NULL, jk_globalPool );

    /** Create the global environment. This will register the default
        factories
    */
    env=jk2_env_getEnv( NULL, globalPool );

    /* Optional. Register more factories ( or replace existing ones ) */
    /* Init the environment. */
    
    /* Create the logger */
    jkb=env->createBean2( env, env->globalPool, "logger.win32", "");
    env->alias( env, "logger.win32:", "logger");
    l = jkb->object;
    
    env->l=l;
    env->soName=env->globalPool->pstrdup(env, env->globalPool, file_name );
    
    if( env->soName == NULL ){
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "Error creating env->soName\n");
        return env;
    }
    env->l->init(env,env->l);

    /* We should make it relative to JK_HOME or absolute path.
       ap_server_root_relative(cmd->pool,opt); */
    
    /* Create the workerEnv */
    jkb=env->createBean2( env, env->globalPool,"workerEnv", "");
    workerEnv= jkb->object;
    env->alias( env, "workerEnv:" , "workerEnv");
    
    if( workerEnv==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "Error creating workerEnv\n");
        return env;
    }

    workerEnv->childId = 0;
/* XXX 
    
    Detect install dir, be means of service configs, */

    
    return env;
}

static jk_env_t * jk2_create_config()
{
    jk_env_t *env;
    if(  workerEnv==NULL ) {
        env = jk2_create_workerEnv();
        env->l->jkLog(env, env->l, JK_LOG_INFO, "JK2 Config Created");
    } else {
        env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );
        env->l->jkLog(env, env->l, JK_LOG_INFO, "JK2 Config Reused");
    }

    
   
    return env;
}

#ifdef SF_NOTIFY_AUTH_COMPLETE
static int get_auth_flags()
{
    HKEY hkey;
    long rc;
    int maj, sz;
    int rv = SF_NOTIFY_PREPROC_HEADERS;
    int use_auth = JK_FALSE;
    /* Retreive the IIS version Major*/
    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                      W3SVC_REGISTRY_KEY,
                      (DWORD)0,
                      KEY_READ,
                      &hkey);
    if(ERROR_SUCCESS != rc) {
        return rv;
    } 
    sz = sizeof(int);
    rc = RegQueryValueEx(hkey,     
                         "MajorVersion",      
                          NULL,
                          NULL,    
                          (LPBYTE)&maj,
                          &sz); 
    if (ERROR_SUCCESS != rc) {
        CloseHandle(hkey);
        return rv;        
    }
    CloseHandle(hkey);
    if (use_auth_notification_flags && maj > 4)
        rv = SF_NOTIFY_AUTH_COMPLETE;

    return rv;
}
#else
static int get_auth_flags()
{
    return SF_NOTIFY_PREPROC_HEADERS;
}
#endif
