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

#include "jk_iis.h"
//#include "jk_uri_worker_map.h"

#define WORKERS_FILE_TAG       ("worker_file")
#define SERVER_ROOT_TAG          ("server_root")
#define URI_SELECT_TAG          ("uri_select")
#define EXTENSION_URI_TAG       ("extension_uri")

#define URI_SELECT_PARSED_VERB      ("parsed")
#define URI_SELECT_UNPARSED_VERB    ("unparsed")
#define URI_SELECT_ESCAPED_VERB     ("escaped")


static char  ini_file_name[MAX_PATH];
static int   using_ini_file = JK_FALSE;
static int   is_inited = JK_FALSE;
static int	 is_mapread	= JK_FALSE;
static int   iis5 = -1;

static jk_workerEnv_t *workerEnv;


static char extension_uri[INTERNET_MAX_URL_LENGTH] = "/jakarta/isapi_redirector2.dll";
static char worker_file[MAX_PATH * 2];
static char server_root[MAX_PATH * 2];



#define URI_SELECT_OPT_PARSED       0
#define URI_SELECT_OPT_UNPARSED     1
#define URI_SELECT_OPT_ESCAPED      2

static int uri_select_option = URI_SELECT_OPT_PARSED;


static int init_jk(char *serverName);

static int initialize_extension(void);

static int read_registry_init_data(void);

static int get_registry_config_parameter(HKEY hkey,
                                         const char *tag, 
                                         char *b,
                                         DWORD sz);


static jk_env_t* jk2_create_config();



static void write_error_response(PHTTP_FILTER_CONTEXT pfc,char *status,char * msg)
{
    char crlf[3] = { (char)13, (char)10, '\0' };
    char ctype[30];
    DWORD len = strlen(msg);

    sprintf(ctype, 
            "Content-Type:text/html%s%s", 
            crlf, 
            crlf);

    /* reject !!! */
    pfc->ServerSupportFunction(pfc, 
                               SF_REQ_SEND_RESPONSE_HEADER,
                               status,
                               (DWORD)crlf,
                               (DWORD)ctype);
    pfc->WriteClient(pfc, msg, &len, 0);
}


BOOL WINAPI GetFilterVersion(PHTTP_FILTER_VERSION pVer)
{
    ULONG http_filter_revision = HTTP_FILTER_REVISION;


    pVer->dwFilterVersion = pVer->dwServerFilterVersion;
                        
    if (pVer->dwFilterVersion > http_filter_revision) {
        pVer->dwFilterVersion = http_filter_revision;
    }

    pVer->dwFlags = SF_NOTIFY_ORDER_HIGH        | 
                    SF_NOTIFY_SECURE_PORT       | 
                    SF_NOTIFY_NONSECURE_PORT    |
                    SF_NOTIFY_PREPROC_HEADERS   |
                    SF_NOTIFY_AUTH_COMPLETE;
                    
    strcpy(pVer->lpszFilterDesc, VERSION_STRING);

    if (!is_inited) {
        return initialize_extension();
    }

    return TRUE;
}

DWORD WINAPI HttpFilterProc(PHTTP_FILTER_CONTEXT pfc,
                            DWORD dwNotificationType, 
                            LPVOID pvNotification)
{
    jk_env_t *env=NULL; 
    jk_uriEnv_t *uriEnv=NULL;

    /* Initialise jk */
    if (is_inited && !is_mapread) {
        char serverName[MAX_SERVERNAME];
        DWORD dwLen = sizeof(serverName);

        if (pfc->GetServerVariable(pfc, SERVER_NAME, serverName, &dwLen)){
            if (dwLen > 0) serverName[dwLen-1] = '\0';
            if (init_jk(serverName))
                is_mapread = JK_TRUE;
        }
        /* If we can't read the map we become dormant */
        if (!is_mapread)
            is_inited = JK_FALSE;
    }
    env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );
    if (is_inited && (iis5 < 0) ) {
        char serverSoftware[256];
        DWORD dwLen = sizeof(serverSoftware);
		iis5=0;
        if (pfc->GetServerVariable(pfc,SERVER_SOFTWARE, serverSoftware, &dwLen)){
			iis5=(atof(serverSoftware + 14) >= 5.0);
			if (iis5) {
				env->l->jkLog(env, env->l,  JK_LOG_INFO,"Detected IIS >= 5.0\n");
			} else {
				env->l->jkLog(env, env->l,  JK_LOG_INFO,"Detected IIS < 5.0\n");
			}
        }
    }
    if (is_inited &&
         (((SF_NOTIFY_PREPROC_HEADERS == dwNotificationType) && !iis5) ||
		  ((SF_NOTIFY_AUTH_COMPLETE   == dwNotificationType) &&  iis5)
		  )
		)
	{ 
        char uri[INTERNET_MAX_URL_LENGTH]; 
        char snuri[INTERNET_MAX_URL_LENGTH]="/";
        char Host[INTERNET_MAX_URL_LENGTH];
        char Translate[INTERNET_MAX_URL_LENGTH];
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

		if (iis5) {
			GetHeader=((PHTTP_FILTER_AUTH_COMPLETE_INFO)pvNotification)->GetHeader;
			SetHeader=((PHTTP_FILTER_AUTH_COMPLETE_INFO)pvNotification)->SetHeader;
			AddHeader=((PHTTP_FILTER_AUTH_COMPLETE_INFO)pvNotification)->AddHeader;
		} else {
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
            if (rc == BAD_REQUEST) {
                env->l->jkLog(env, env->l,  JK_LOG_ERROR, 
                       "HttpFilterProc [%s] contains one or more invalid escape sequences.\n", 
                       uri);
                write_error_response(pfc,"400 Bad Request",
                        "<HTML><BODY><H1>Request contains invalid encoding</H1></BODY></HTML>");
                workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
                return SF_STATUS_REQ_FINISHED;
            }
            else if(rc == BAD_PATH) {
                env->l->jkLog(env, env->l,  JK_LOG_EMERG, 
                       "HttpFilterProc [%s] contains forbidden escape sequences.\n", 
                       uri);
                write_error_response(pfc,"403 Forbidden",
                        "<HTML><BODY><H1>Access is Forbidden</H1></BODY></HTML>");
                workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
                return SF_STATUS_REQ_FINISHED;
            }
            jk_requtil_getParents(uri);


            if(GetHeader(pfc, "Host:", (LPVOID)Host, (LPDWORD)&szHost)) {
                env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                       "In HttpFilterProc Virtual Host redirection of %s\n", 
                       Host);
                uriEnv = workerEnv->uriMap->mapUri(env, workerEnv->uriMap,Host,uri );
            }

            if (uriEnv==NULL) {
                env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                       "In HttpFilterProc test Default redirection of %s\n", 
                       uri);
                uriEnv = workerEnv->uriMap->mapUri(env, workerEnv->uriMap,NULL,uri );
            }

            if (uriEnv!=NULL) {
                char *forwardURI;

                /* This is a servlet, should redirect ... */
                env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                       "HttpFilterProc [%s] is a servlet url - should redirect to %s\n", 
                       uri, uriEnv->workerName);
                
                /* get URI we should forward */
                if (uri_select_option == URI_SELECT_OPT_UNPARSED) {
                    /* get original unparsed URI */
                    GetHeader(pfc, "url", (LPVOID)uri, (LPDWORD)&sz);
                    /* restore terminator for uri portion */
                    if (query)
                        *(query - 1) = '\0';
                    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
                           "HttpFilterProc fowarding original URI [%s]\n",uri);
                    forwardURI = uri;
                } else if (uri_select_option == URI_SELECT_OPT_ESCAPED) {
                    if (!jk_requtil_escapeUrl(uri,snuri,INTERNET_MAX_URL_LENGTH)) {
                        env->l->jkLog(env, env->l,  JK_LOG_ERROR, 
                               "HttpFilterProc [%s] re-encoding request exceeds maximum buffer size.\n", 
                               uri);
                        write_error_response(pfc,"400 Bad Request",
                                "<HTML><BODY><H1>Request contains too many characters that need to be encoded.</H1></BODY></HTML>");
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

                write_error_response(pfc,"403 Forbidden",
                        "<HTML><BODY><H1>Access is Forbidden</H1></BODY></HTML>");
                workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
                return SF_STATUS_REQ_FINISHED;
            }
        }
    }
    workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
    return SF_STATUS_REQ_NEXT_NOTIFICATION;
}


BOOL WINAPI GetExtensionVersion(HSE_VERSION_INFO  *pVer)
{
    pVer->dwExtensionVersion = MAKELONG( HSE_VERSION_MINOR,
                                         HSE_VERSION_MAJOR );

    strcpy(pVer->lpszExtensionDesc, VERSION_STRING);


    if (!is_inited) {
        return initialize_extension();
    }
    return TRUE;
}

DWORD WINAPI HttpExtensionProc(LPEXTENSION_CONTROL_BLOCK  lpEcb)
{   
    DWORD rc = HSE_STATUS_ERROR;
    jk_env_t *env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );
    lpEcb->dwHttpStatusCode = HTTP_STATUS_SERVER_ERROR;

    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
           "HttpExtensionProc started\n");

	/* Initialise jk */
	if (is_inited && !is_mapread) {
		char serverName[MAX_SERVERNAME];
		DWORD dwLen = sizeof(serverName);
		if (lpEcb->GetServerVariable(lpEcb->ConnID, SERVER_NAME, serverName, &dwLen)){
			if (dwLen > 0) serverName[dwLen-1] = '\0';
			if (init_jk(serverName))
				is_mapread = JK_TRUE;
		}
		if (!is_mapread)
			is_inited = JK_FALSE;
	}

	if (is_inited) {
        jk_ws_service_t sOnStack;
        jk_ws_service_t *s=&sOnStack;
        char *worker_name;
        char huge_buf[16 * 1024]; /* should be enough for all */
        DWORD huge_buf_sz;

        
        jk_worker_t *worker;
        jk_pool_t *rPool=NULL;
        int rc1;

        GET_SERVER_VARIABLE_VALUE(workerEnv->pool,HTTP_WORKER_HEADER_NAME, ( worker_name ));
        worker=env->getByName( env, worker_name);

        env->l->jkLog(env, env->l,  JK_LOG_DEBUG, 
               "HttpExtensionProc %s a worker for name %s\n", 
               worker ? "got" : "could not get",
               worker_name);
        
        if( worker==NULL ){
            env->l->jkLog(env, env->l,  JK_LOG_INFO, 
                   "HttpExtensionProc worker is NULL\n");
            return rc;            
        }
        /* Get a pool for the request XXX move it in workerEnv to
           be shared with other server adapters */
        rPool= worker->rPoolCache->get( env, worker->rPoolCache );
        if( rPool == NULL ) {
            rPool=worker->mbean->pool->create( env, worker->mbean->pool, HUGE_POOL_SIZE );
            env->l->jkLog(env, env->l, JK_LOG_INFO,
                          "HttpExtensionProc: new rpool\n");
        }

        jk2_service_iis_init( env, s );
        s->pool = rPool;
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
        
        rPool->reset(env, rPool);
        
        rc1=worker->rPoolCache->put( env, worker->rPoolCache, rPool );
        
    } else {
        env->l->jkLog(env, env->l,  JK_LOG_ERROR, 
               "HttpExtensionProc error, not initialized\n");
    }
    return rc;
}

    

BOOL WINAPI TerminateExtension(DWORD dwFlags) 
{
    return TerminateFilter(dwFlags);
}

BOOL WINAPI TerminateFilter(DWORD dwFlags) 
{
    if (is_inited) {
        is_inited = JK_FALSE;

/* XXX Here goes a graceful shutdown of jk2, Free resources and pools
*/
    }
    
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
    char file_name[_MAX_PATH];

    switch (ulReason) {
        case DLL_PROCESS_DETACH:
            __try {
                TerminateFilter(HSE_TERM_MUST_UNLOAD);
            } __except(1) {
            }
        break;

        default:
        break;
    } 
    if (GetModuleFileName( hInst, file_name, sizeof(file_name))) {
        _splitpath( file_name, drive, dir, fname, NULL );
        _makepath( ini_file_name, drive, dir, fname, ".properties" );
    } else {
        fReturn = JK_FALSE;
    }
    return fReturn;
}

static int init_jk(char *serverName)
{
    int rc = JK_FALSE;  
       
    jk_env_t *env=jk2_create_config();   

    rc=(JK_OK == workerEnv->config->setPropertyString( env, workerEnv->config, "config.file", worker_file ));

    /* Logging the initialization type: registry or properties file in virtual dir
    */
    if (using_ini_file) {
        env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using ini file %s.\n", ini_file_name);
    } else {
        env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using registry.\n");
    }
    //env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using log file %s.\n", log_file);
    //env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using log level %d.\n", log_level);
    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using extension uri %s.\n", extension_uri);
    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using server root %s.\n", server_root);
    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using worker file %s.\n", worker_file);
    env->l->jkLog(env, env->l,  JK_LOG_DEBUG, "Using uri select %d.\n", uri_select_option);
    workerEnv->init(env,workerEnv);
    return rc;
}

static int initialize_extension(void)
{
    if (read_registry_init_data()) {
        is_inited = JK_TRUE;
    }
    return is_inited;
}

int parse_uri_select(const char *uri_select)
{
    if(0 == strcasecmp(uri_select, URI_SELECT_PARSED_VERB)) {
        return URI_SELECT_OPT_PARSED;
    }

    if(0 == strcasecmp(uri_select, URI_SELECT_UNPARSED_VERB)) {
        return URI_SELECT_OPT_UNPARSED;
    }

    if(0 == strcasecmp(uri_select, URI_SELECT_ESCAPED_VERB)) {
        return URI_SELECT_OPT_ESCAPED;
    }

    return -1;
}

static int read_registry_init_data(void)
{
    char tmpbuf[INTERNET_MAX_URL_LENGTH];
    HKEY hkey;
    long rc;
    int  ok = JK_TRUE; 
/*  
    char *tmp;
    jk_map_t *map;

    if (map_alloc(&map)) {
        if (map_read_properties(map, ini_file_name)) {
            using_ini_file = JK_TRUE;
		}
    }
    if (using_ini_file) {
        tmp = map_get_string(map, JK_LOG_FILE_TAG, NULL);
        if (tmp) {
            strcpy(log_file, tmp);
        } else {
            ok = JK_FALSE;
        }
        tmp = map_get_string(map, JK_LOG_LEVEL_TAG, NULL);
        if (tmp) {
            log_level = jk2_logger_file_parseLogLevel(tmp);
        } else {
            ok = JK_FALSE;
        }
        tmp = map_get_string(map, EXTENSION_URI_TAG, NULL);
        if (tmp) {
            strcpy(extension_uri, tmp);
        } else {
            ok = JK_FALSE;
        }
        tmp = map_get_string(map, SERVER_ROOT_TAG, NULL);
        if (tmp) {
            strcpy(server_root, tmp);
        } else {
            ok = JK_FALSE;
        }
        tmp = map_get_string(map, URI_SELECT_TAG, NULL);
        if (tmp) {
            int opt = parse_uri_select(tmp);
            if (opt >= 0) {
                uri_select_option = opt;
            } else {
                ok = JK_FALSE;
            }
        }
    
    } else  */{
        rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                          REGISTRY_LOCATION,
                          (DWORD)0,
                          KEY_READ,
                          &hkey);
        if(ERROR_SUCCESS != rc) {
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
                                         JK_WORKER_FILE_TAG,
                                         tmpbuf,
                                         sizeof(worker_file))) {
            strcpy(worker_file, tmpbuf);
        } else {
            ok = JK_FALSE;
        }

        if(get_registry_config_parameter(hkey,
                                         URI_SELECT_TAG, 
                                         tmpbuf,
                                         sizeof(tmpbuf))) {
            int opt = parse_uri_select(tmpbuf);
            if (opt >= 0) {
                uri_select_option = opt;
            } else {
                ok = JK_FALSE;
            }
        }

        RegCloseKey(hkey);
    }
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
    /** First create a pool. Compile time option
     */
    jk2_pool_create( NULL, &globalPool, NULL, 2048 );

    /** Create the global environment. This will register the default
        factories
    */
    env=jk2_env_getEnv( NULL, globalPool );

    /* Optional. Register more factories ( or replace existing ones ) */
    /* Init the environment. */
    
    /* Create the logger */
    jkb=env->createBean2( env, env->globalPool, "logger.file", "");
    env->alias( env, "logger.file:", "logger");
    l = jkb->object;
    
    env->l=l;
    
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

/* XXX 
	
	Detect install dir, be means of service configs, */

    
    workerEnv->initData->add( env, workerEnv->initData, "serverRoot",
                              workerEnv->pool->pstrdup( env, workerEnv->pool, server_root));
    env->l->jkLog(env, env->l, JK_LOG_ERROR, "Set serverRoot %s\n", server_root);

    return env;
}

static jk_env_t * jk2_create_config()
{
    jk_uriEnv_t *newUri;
    jk_bean_t *jkb;
    jk_env_t *env;
    if(  workerEnv==NULL ) {
        env=jk2_create_workerEnv();
    }

    env->l->jkLog(env, env->l, JK_LOG_ERROR, "JK2 Config Created");
   
    return env;
}