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

/**
 * Description: IIS Jk2 Service 
 * Author:      Gal Shachor <shachor@il.ibm.com>                           
 *              Henri Gomez <hgomez@apache.org> 
 *              Ignacio J. Ortega <nacho@apache.org>
 */

// This define is needed to include wincrypt,h, needed to get client certificates
#define _WIN32_WINNT 0x0400

#include <httpext.h>
#include <httpfilt.h>
#include <wininet.h>

#include "jk_global.h"
#include "jk_requtil.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_env.h"
#include "jk_service.h"
#include "jk_worker.h"

#include "jk_iis.h"

#define ROLES_ATTRIBUTE_NAME  "org.apache.tomcat.jk.roles"

extern int send_groups;

static int JK_METHOD jk2_service_iis_head(jk_env_t *env, jk_ws_service_t *s ){
    static char crlf[3] = { (char)13, (char)10, '\0' };
    const char *reason;
    LPEXTENSION_CONTROL_BLOCK  lpEcb=(LPEXTENSION_CONTROL_BLOCK)s->ws_private;
    DWORD len_of_status;
    char *status_str;
    char *headers_str;
    int headerCount;
    
    env->l->jkLog(env,env->l, JK_LOG_DEBUG, 
                  "Into jk_ws_service_t::head\n");

    if (s->status< 100 || s->status > 1000) {
        env->l->jkLog(env,env->l, JK_LOG_ERROR, 
               "jk_ws_service_t::jk2_service_iis_head, invalid status %d\n", s->status);
        return JK_ERR;
    }
    
    if( lpEcb == NULL ) {
        env->l->jkLog(env,env->l, JK_LOG_ERROR, 
                      "jk_ws_service_t::head, no lpEcp\n");
        return JK_ERR;
    }
    
    s->response_started = JK_TRUE;
            
    /*
     * Create the status line
     */
    if (s->msg==NULL) {
        reason = "";
    } else {
        reason = s->msg;
    }

    status_str = (char *)_alloca((6 + strlen(reason)) * sizeof(char));
    sprintf(status_str, "%d %s", s->status, reason);
    len_of_status = strlen(status_str); 

    headerCount=s->headers_out->size( env, s->headers_out );
    /*
     * Create response headers string
     */
    if ( headerCount> 0 ) {
        int i;
        unsigned len_of_headers;
        
        for(i = 0 , len_of_headers = 0 ; i < headerCount ; i++) {
            len_of_headers += strlen(s->headers_out->nameAt(env,s->headers_out,i));
            len_of_headers += strlen(s->headers_out->valueAt(env,s->headers_out,i));
            len_of_headers += 4; /* extra for colon, space and crlf */
        }
        
        len_of_headers += 3;  /* crlf and terminating null char */
        headers_str = (char *)_alloca(len_of_headers * sizeof(char));
        headers_str[0] = '\0';
            
        for(i = 0 ; i < headerCount ; i++) {
            strcat(headers_str, s->headers_out->nameAt(env,s->headers_out,i));
            strcat(headers_str, ": ");
            strcat(headers_str, s->headers_out->valueAt(env,s->headers_out,i));
            strcat(headers_str, crlf);
        }
        strcat(headers_str, crlf);
    } else {
        headers_str = crlf;
    }

    if (!lpEcb->ServerSupportFunction(lpEcb->ConnID, 
                                      HSE_REQ_SEND_RESPONSE_HEADER,
                                      status_str,
                                      (LPDWORD)&len_of_status,
                                      (LPDWORD)headers_str)) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "jk_ws_service_t::head, ServerSupportFunction failed\n");
        return JK_ERR;
    }       
    
    return JK_OK;
}

static int JK_METHOD jk2_service_iis_read(jk_env_t *env, jk_ws_service_t *s,
                                          void *b, unsigned len, unsigned *actually_read)
{
    env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                  "Into jk_ws_service_t::read\n");
    
    if (s && s->ws_private && b && actually_read) {
        LPEXTENSION_CONTROL_BLOCK  lpEcb=(LPEXTENSION_CONTROL_BLOCK)s->ws_private;
        
        *actually_read = 0;
        if ((s->content_read < (long)lpEcb->cbTotalBytes)&& !s->end_of_stream ){
            if (len) {
                char *buf = b;
                long already_read = (long)lpEcb->cbAvailable - s->content_read;
      
                if (already_read >= (long)len) {
                    memcpy(buf, lpEcb->lpbData + s->content_read, len);
                    *actually_read = len;
                } else {
                    /*
                     * Try to copy what we already have 
                     */
                    if (already_read > 0) {
                        memcpy(buf, lpEcb->lpbData + s->content_read, already_read);
                        buf   += already_read;
                        len   -= already_read;
//                        s->content_read = lpEcb->cbAvailable;
            
                        *actually_read = already_read;
                    }
                    if ((s->content_read+*actually_read)==lpEcb->cbTotalBytes) {
                        s->end_of_stream=JK_TRUE;
                    }
                    /*
                     * Now try to read from the client ...
                     */
                    if (!s->end_of_stream ) {
                        if ( lpEcb->ReadClient(lpEcb->ConnID, buf, &len)) {
                            *actually_read +=  len;            
                        } else {
                            env->l->jkLog(env,env->l, JK_LOG_ERROR, 
                                   "jk_ws_service_t::read, ReadClient failed\n");
                            return JK_OK;
                        }
                    }
                }
            }
        }
        if ((s->content_read+*actually_read)==lpEcb->cbTotalBytes) {
            s->end_of_stream=JK_TRUE;
        }
        return JK_OK;
    }
    
    env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                  "jk_ws_service_t::read, NULL parameters\n");
    return JK_ERR;
}

static int JK_METHOD jk2_service_iis_write(jk_env_t *env, jk_ws_service_t *s,
                                           const void *b, unsigned len)
{
    env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                  "Into jk_ws_service_t::write\n");
    
    if (s && s->ws_private && b) {
        LPEXTENSION_CONTROL_BLOCK  lpEcb=(LPEXTENSION_CONTROL_BLOCK)s->ws_private;
        
        if (len) {
            unsigned written = 0;           
            char *buf = (char *)b;
            
            if (!s->response_started) {
                s->head(env, s );
            }
            
            while(written < len) {
                DWORD try_to_write = len - written;
                if (!lpEcb->WriteClient(lpEcb->ConnID, 
                                        buf + written, 
                                        &try_to_write, 
                                        0)) {
                    env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                           "jk_ws_service_t::write, WriteClient failed\n");
                    return JK_ERR;
                }
                written += try_to_write;
            }
        }
        
        return JK_OK;
        
    }
    
    env->l->jkLog(env, env->l, JK_LOG_ERROR, 
           "jk_ws_service_t::write, NULL parameters\n");
    
    return JK_ERR;
}

int get_server_value(LPEXTENSION_CONTROL_BLOCK lpEcb,
                            char *name,
                            char  *buf,
                            DWORD bufsz,
                            char  *def_val)
{
    if (!lpEcb->GetServerVariable(lpEcb->ConnID, 
                                 name,
                                 buf,
                                 (LPDWORD)&bufsz)) {
        strcpy(buf, def_val);
        return JK_FALSE;
    }

    if (bufsz > 0) {
        buf[bufsz - 1] = '\0';
    }

    return JK_TRUE;
}

#define MAX_NAME 256

char * jk2_service_iis_get_roles(jk_env_t *env, jk_ws_service_t *s)
{
    LPEXTENSION_CONTROL_BLOCK  lpEcb=(LPEXTENSION_CONTROL_BLOCK)s->ws_private;
    HANDLE h;
    DWORD len=0;
    PTOKEN_GROUPS g=NULL;
    unsigned i;
    char *roles=NULL;
    if ( lpEcb->ServerSupportFunction(lpEcb->ConnID,
                                      HSE_REQ_GET_IMPERSONATION_TOKEN,
                                      (LPVOID)&h, NULL,NULL) != FALSE ){
        // First get the length of the user's groups array and gets the memory 
        if ( !GetTokenInformation(h, TokenGroups, NULL, len , &len ) ) {
            if ( ERROR_INSUFFICIENT_BUFFER == GetLastError() ){
                g = (PTOKEN_GROUPS)s->pool->calloc(env,s->pool,len);
            }
        }
        if ( g != NULL ){
            if ( GetTokenInformation(h, TokenGroups, g, len, &len)) {
                roles=s->pool->calloc(env,s->pool,(g->GroupCount)*MAX_NAME);
                for (i=0; i < g->GroupCount ; i++){
                    char name[MAX_NAME],domain[MAX_NAME];
                    DWORD nLen = MAX_NAME, dLen = MAX_NAME;
                    SID_NAME_USE eUse;
                    // Get  the user name and the domain from the SID.
                    env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                           "jk2_service_iis_get_roles requesting name for member:%d attributes:%#lx SID:%#lx \n",
                            i,g->Groups[i].Attributes, g->Groups[i].Sid );
                    if ( ! LookupAccountSid(NULL,g->Groups[i].Sid,name,&nLen,domain,&dLen,&eUse) ){
                        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                               "jk2_service_iis_get_roles problems requesting name for member:%d attributes:%#lx SID:%#lx \n",
                                i,g->Groups[i].Attributes, g->Groups[i].Sid );
                            
                    } else {
                        strcpy(roles+strlen(roles),name);
                        roles[strlen(roles)]=',';
                        roles[strlen(roles)+1]='\0';
                        env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                               "jk2_service_iis_get_roles member:%d attributes:%#lx SID:%#lx name:%s\n",
                                i,g->Groups[i].Attributes, g->Groups[i].Sid,name );
                    }
                }
                roles[strlen(roles)-1]='\0';
                env->l->jkLog(env, env->l, JK_LOG_INFO, 
                       "jk_ws_service_t::jk2_service_iis_get_roles roles:%s \n",
                        roles );
            }
            return roles;
        } else {
            return NULL;
        }
    }
    return NULL;
}

static int JK_METHOD jk2_service_iis_initService( struct jk_env *env, jk_ws_service_t *s,
                 struct jk_worker *w, void *serverObj )
/* */
{
    LPEXTENSION_CONTROL_BLOCK  lpEcb=(LPEXTENSION_CONTROL_BLOCK)serverObj;
    char huge_buf[16 * 1024]; /* should be enough for all */

    DWORD huge_buf_sz;

    s->jvm_route = NULL;

    GET_SERVER_VARIABLE_VALUE(s->pool,HTTP_URI_HEADER_NAME, s->req_uri);     
    GET_SERVER_VARIABLE_VALUE(s->pool,HTTP_QUERY_HEADER_NAME, s->query_string);     
    
    if (s->req_uri == NULL) {
        s->query_string = lpEcb->lpszQueryString;
        /* *worker_name    = DEFAULT_WORKER_NAME; */
        GET_SERVER_VARIABLE_VALUE(s->pool,"URL", s->req_uri);       
        if (jk_requtil_unescapeUrl(s->req_uri) < 0)
            return JK_ERR;
        jk_requtil_getParents(s->req_uri);
    }
    
    GET_SERVER_VARIABLE_VALUE(s->pool,"AUTH_TYPE", s->auth_type);
    GET_SERVER_VARIABLE_VALUE(s->pool,"REMOTE_USER", s->remote_user);
    GET_SERVER_VARIABLE_VALUE(s->pool,"SERVER_PROTOCOL", s->protocol);
    GET_SERVER_VARIABLE_VALUE(s->pool,"REMOTE_HOST", s->remote_host);
    GET_SERVER_VARIABLE_VALUE(s->pool,"REMOTE_ADDR", s->remote_addr);
    GET_SERVER_VARIABLE_VALUE(s->pool,SERVER_NAME, s->server_name);
    GET_SERVER_VARIABLE_VALUE_INT("SERVER_PORT", s->server_port, 80);
    GET_SERVER_VARIABLE_VALUE(s->pool,SERVER_SOFTWARE, s->server_software);
    GET_SERVER_VARIABLE_VALUE_INT("SERVER_PORT_SECURE", s->is_ssl, 0);

    s->method           = lpEcb->lpszMethod;
    s->content_length   = lpEcb->cbTotalBytes;
    s->end_of_stream    = JK_FALSE;

    s->ssl_cert     = NULL;
    s->ssl_cert_len = 0;
    s->ssl_cipher   = NULL;
    s->ssl_session  = NULL;
    s->ssl_key_size = -1;

    if (JK_OK!=jk2_map_default_create(env, &s->headers_out, s->pool )){
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
               "jk_ws_service_t::init, Failed to create headers_out map \n");
        return JK_ERR;

    }
    if (JK_OK!=jk2_map_default_create(env, &s->attributes, s->pool )){
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
               "jk_ws_service_t::init, Failed to create attributes map \n");
        return JK_ERR;

    }
    if (JK_OK!=jk2_map_default_create(env, &s->headers_in, s->pool )){
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
               "jk_ws_service_t::init, Failed to create headers_in map \n");
        return JK_ERR;
    }
//    s->headers_values   = NULL;
//  s->num_headers      = 0;
    
    /*
     * Add SSL IIS environment
     */

    if ( send_groups && *s->remote_user ){
        char *groups=jk2_service_iis_get_roles(env, s);
        if( groups != NULL){
            s->attributes->put( env, s->attributes,ROLES_ATTRIBUTE_NAME,groups,NULL);
        }
    }

    if (s->is_ssl) {         
        char *ssl_env_names[9] = {
            "CERT_ISSUER", 
            "CERT_SUBJECT", 
            "CERT_COOKIE", 
            "HTTPS_SERVER_SUBJECT", 
            "CERT_FLAGS", 
            "HTTPS_SECRETKEYSIZE", 
            "CERT_SERIALNUMBER", 
            "HTTPS_SERVER_ISSUER", 
            "HTTPS_KEYSIZE"
        };
        char *ssl_env_values[9] = {
            NULL, 
            NULL, 
            NULL, 
            NULL, 
            NULL, 
            NULL, 
            NULL, 
            NULL, 
            NULL
        };
        unsigned i;
        unsigned num_of_vars = 0;

        for(i = 0 ; i < 9 ; i++) {
            GET_SERVER_VARIABLE_VALUE(s->pool,ssl_env_names[i], ssl_env_values[i]);
            if (ssl_env_values[i]) {
                num_of_vars++;
            }
        }
        if (num_of_vars) {
            unsigned j=0;
            for(i = 0 ; i < 9 ; i++) {                
                if (ssl_env_values[i]) {
                    s->attributes->put( env, s->attributes, 
                                        ssl_env_names[i], ssl_env_values[i],NULL);
                    j++;
                }
            }
            if (ssl_env_values[4] && ssl_env_values[4][0] == '1') {
                CERT_CONTEXT_EX cc;
                DWORD cc_sz = sizeof(cc);
                cc.cbAllocated = sizeof(huge_buf);
                cc.CertContext.pbCertEncoded = (BYTE*) huge_buf;
                cc.CertContext.cbCertEncoded = 0;
                
                if (lpEcb->ServerSupportFunction(lpEcb->ConnID,
                                                 (DWORD)HSE_REQ_GET_CERT_INFO_EX,                               
                                                 (LPVOID)&cc,NULL,NULL) != FALSE)
                    {
                        env->l->jkLog(env, env->l, JK_LOG_DEBUG,"Client Certificate encoding:%d sz:%d flags:%ld\n",
                                      cc.CertContext.dwCertEncodingType & X509_ASN_ENCODING ,
                                      cc.CertContext.cbCertEncoded,
                                      cc.dwCertificateFlags);
                        s->ssl_cert=s->pool->alloc(env, s->pool,
                                                   jk_requtil_base64CertLen(cc.CertContext.cbCertEncoded));
                        
                        s->ssl_cert_len = jk_requtil_base64EncodeCert(s->ssl_cert,
                                                                      huge_buf,cc.CertContext.cbCertEncoded) - 1;
                    }
            }
        }
    }

    
    huge_buf_sz = sizeof(huge_buf);         
    if (get_server_value(lpEcb,
                         "ALL_HTTP",             
                         huge_buf,           
                         huge_buf_sz,        
                         "")) {              
        unsigned cnt = 0;
        char *tmp;

        for(tmp = huge_buf ; *tmp ; tmp++) {
            if (*tmp == '\n'){
                cnt++;
            }
        }
        
        if (cnt) {
            char *headers_buf = s->pool->pstrdup(env, s->pool, huge_buf);
            unsigned i;
            unsigned len_of_http_prefix = strlen("HTTP_");
            BOOL need_content_length_header = (s->content_length == 0);
            
            cnt -= 2; /* For our two special headers */
            /* allocate an extra header slot in case we need to add a content-length header */
            for(i = 0, tmp = headers_buf ; *tmp && i < cnt ; ) {
                int real_header = JK_TRUE;
                char *headerName;
                /* Skipp the HTTP_ prefix to the beginning of th header name */
                tmp += len_of_http_prefix;

                if (!strnicmp(tmp, URI_HEADER_NAME, strlen(URI_HEADER_NAME)) ||
                    !strnicmp(tmp, WORKER_HEADER_NAME, strlen(WORKER_HEADER_NAME))) {
                    real_header = JK_FALSE;
                } else if(need_content_length_header &&
                          !strnicmp(tmp, CONTENT_LENGTH, strlen(CONTENT_LENGTH))) {
                    need_content_length_header = FALSE;
                    headerName  = tmp;
                } else if (!strnicmp(tmp, TOMCAT_TRANSLATE_HEADER_NAME,
                                     strlen(TOMCAT_TRANSLATE_HEADER_NAME))) {
                    tmp += 6; /* TOMCAT */
                    headerName  = tmp;
                } else {
                    headerName  = tmp;
                }
                
                while(':' != *tmp && *tmp) {
                    if ('_' == *tmp) {
                        *tmp = '-';
                    } else {
                        *tmp = tolower(*tmp);
                    }
                    tmp++;
                }
                *tmp = '\0';
                tmp++;

                /* Skip all the WS chars after the ':' to the beginning of th header value */
                while(' ' == *tmp || '\t' == *tmp || '\v' == *tmp) {
                    tmp++;
                }

                if (real_header) {
                    s->headers_in->put( env, s->headers_in, headerName, tmp, NULL );
                }
                
                while(*tmp != '\n' && *tmp != '\r') {
                    tmp++;
                }
                *tmp = '\0';
                tmp++;
                
                /* skipp CR LF */
                while(*tmp == '\n' || *tmp == '\r') {
                    tmp++;
                }
                
                if (real_header) {
                    i++;
                }
            }
            /* Add a content-length = 0 header if needed.
             * Ajp13 assumes an absent content-length header means an unknown,
             * but non-zero length body.
             */
            if(need_content_length_header) {
                s->headers_in->put( env, s->headers_in,
                                    "content-length", "0",NULL);
                cnt++;
            }
        } else {
            /* We must have our two headers */
            return JK_ERR;
        }
    } else {
        return JK_ERR;
    }
    
    return JK_OK;
}

static void JK_METHOD jk2_service_iis_afterRequest(jk_env_t *env, jk_ws_service_t *s )
{
    
    if (s->content_read < s->content_length ||
        (s->is_chunked && ! s->no_more_chunks)) {

        LPEXTENSION_CONTROL_BLOCK  lpEcb=(LPEXTENSION_CONTROL_BLOCK)s->ws_private;

#if 0
        char *buff = s->pool->calloc(env,s->pool, 2048);
        if (buff != NULL) {
            /* int rd; */
            /* FIXME Is there a IIS equivalent ? */
            /*             while ((rd = ap_get_client_block(r, buff, 2048)) > 0) { */
            /*                 s->content_read += rd; */
            /*             } */
        }
#endif
    }
}


int jk2_service_iis_init(jk_env_t *env, jk_ws_service_t *s)
{
    if(s==NULL ) {
        return JK_ERR;
    }
    jk2_requtil_initRequest(env, s);
    s->head   = jk2_service_iis_head;
    s->read   = jk2_service_iis_read;
    s->write  = jk2_service_iis_write;
    s->init   = jk2_service_iis_initService;
    s->afterRequest = jk2_service_iis_afterRequest;
    
    return JK_OK;
}

