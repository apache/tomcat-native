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
 * Facility: AOLserver plugin for Jakarta/Tomcat                         
 * Description: Implementation of JK2 HTTP request phases
 * Author:      Alexander Leykekh, aolserver@aol.net 
 * Version:     $Revision$                                           
 */

/*
 * Jakarta (jk_) include files
 */
#include "jk_ns.h"

#define NULL_FOR_EMPTY(x)   ((((x)!=NULL) && (strlen((x))!=0)) ? (x) : NULL ) 

static int JK_METHOD jk2_service_ns_head(jk_env_t *env, jk_ws_service_t *s )
{
    int h;
    int numheaders;
    Ns_Conn *conn;
    jk_map_t *headers;
    int debug=1;
    char *name, *val;
    time_t lastMod;
    
    if(env==NULL || env->l==NULL || env->l->jkLog==NULL)
       return JK_ERR;

    if(s==NULL ||  s->ws_private==NULL || s->headers_out==NULL || s->pool==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.head() NullPointerException\n");
        return JK_ERR;
    }
    
    if( s->uriEnv!=NULL && s->uriEnv->mbean!=NULL)
        debug=s->uriEnv->mbean->debug;

    conn = (Ns_Conn *)s->ws_private;
    if (conn == NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.head() no current connection\n");
	return JK_ERR;
    }

    if(s->msg==NULL) {
        s->msg = "";
    }

    headers=s->headers_out;

    numheaders = headers->size(env, headers);

    if( debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                      "service.head() %d %d %#lx\n", s->status,
                      numheaders, s->uriEnv);
    
    for(h = 0 ; h < numheaders; h++) {
        name=headers->nameAt( env, headers, h );
        val=headers->valueAt( env, headers, h );

	if (name==NULL || val==NULL) {
	  env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.head() NullPointerException\n");
	  return JK_ERR;
	}

        name=s->pool->pstrdup( env, s->pool, name );
        val=s->pool->pstrdup( env, s->pool, val );

	if (name==NULL || val==NULL) {
	  env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.head() NullPointerException\n");
	  return JK_ERR;
	}

        if( debug > 0 ) 
            env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                          "service.head() %s: %s %d %d\n", name, val, h, headers->size( env, headers ));

        if(!strcasecmp(name, "Content-type")) {
	    Ns_ConnSetTypeHeader (conn, Ns_StrToLower (val));

        } else if(!strcasecmp(name, "Last-Modified")) {
            /*
             * If the script gave us a Last-Modified header, we can't just
             * pass it on blindly because of restrictions on future values.
	     *
             */
	    lastMod = apr_date_parse_http (val);
	    Ns_ConnSetLastModifiedHeader (conn, &lastMod);

        } else {                
	    Ns_ConnSetHeaders (conn, name, val);
        }
    }
    
    if (Ns_ConnFlushHeaders (conn, s->status) != NS_OK) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.head() cannot flush headers\n");
	return JK_ERR;
    }

    s->response_started = JK_TRUE;
    
    return JK_OK;
}

/*
 * Read a chunk of the request body into a buffer.  Attempt to read len
 * bytes into the buffer.  Write the number of bytes actually read into
 * actually_read.
 *
 * Think of this function as a method of the AOLserver-specific subclass of
 * the jk_ws_service class.  Think of the *s param as a "this" or "self"
 * pointer.
 */
static int JK_METHOD jk2_service_ns_read(jk_env_t *env, jk_ws_service_t *s,
					 void *b, unsigned len,
					 unsigned *actually_read)
{
    int rv;

    if(s==NULL || s->ws_private==NULL ||  b==NULL || actually_read==NULL || s->ws_private==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.read() NullPointerException\n");
        return JK_ERR;
    }

    s->read_body_started = JK_TRUE;

    if ((rv = Ns_ConnRead((Ns_Conn *)s->ws_private, b, len)) == NS_ERROR) {
        *actually_read = 0;
    } else {
        *actually_read = (unsigned) rv;
    }

    return JK_OK;
}

/*
 * Write a chunk of response data back to the browser.  If the headers
 * haven't yet been sent over, send over default header values (Status =
 * 200, basically).
 *
 * Write len bytes from buffer b.
 *
 * Think of this function as a method of the AOLserver-specific subclass of
 * the jk_ws_service class.  Think of the *s param as a "this" or "self"
 * pointer.
 */

static int JK_METHOD jk2_service_ns_write(jk_env_t *env, jk_ws_service_t *s,
                                               const void *b, unsigned len)
{
    int r = 0;
    char *bb=(char *)b;
    Ns_Conn *conn;
    Ns_Set *outHeaders;
    int i;
    int headerCount;
    int debug=1;
    int rc;

    if (env->l->jkLog == NULL)
        return JK_ERR;

    if(s==NULL  || s->ws_private == NULL ||  b==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.write() NullPointerException\n");
        return JK_ERR;
    }
    if( s->uriEnv != NULL )
        debug=s->uriEnv->mbean->debug;
    
    if(len==0 ) {
        return JK_OK;
    }

    conn=(Ns_Conn *)s->ws_private;
    
    if(!s->response_started) {
        if( debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                          "service.write() default head\n");
        rc=s->head(env, s);
        if( rc != JK_OK) {
            return rc;
        }
        
	if (debug > 0) {
	    outHeaders = Ns_ConnOutputHeaders (conn);
	    headerCount = Ns_SetSize (outHeaders);

	    if (outHeaders && headerCount) {
	        for (i = 0 ; i < headerCount ; i++) {
		    env->l->jkLog(env, env->l, JK_LOG_DEBUG, "OutHeaders %s: %s\n",
				  Ns_SetKey (outHeaders, i), Ns_SetValue (outHeaders, i));
		}
	    }
	}
    }
    
    while( len > 0 ) {
        r = Ns_ConnWrite (conn, bb, len);
	
	if (r == -1)
	    return JK_ERR;

        len -= r;
        bb += r;
    } 
        
    return JK_OK;
}

/* ========================================================================= */
/* Utility functions                                                         */
/* ========================================================================= */

static int JK_METHOD jk2_init_ws_service(jk_env_t *env, jk_ws_service_t *s,
                                         jk_worker_t *worker, void *serverObj)
{
    jk_workerEnv_t *workerEnv;
    Ns_Conn *conn=serverObj;
    int need_content_length_header=JK_FALSE;
    char *query_str_begin, *query_str_end;
    Ns_DString dstr;
    int count, i;
    char *name, *val;
    Ns_Set *reqHdrs = Ns_ConnHeaders (conn);
    char* override = s->server_name; /* jk2_requtil_initRequest will null s->server_name */

    if (env == NULL)
        return JK_ERR;

    if (s==NULL || s->pool==NULL || worker==NULL || serverObj==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "init_ws_service() NullPointerException\n");
        return JK_ERR;
    }

    workerEnv = worker->workerEnv;
    /* Common initialization */
    jk2_requtil_initRequest(env, s);

    s->ws_private = conn;
    s->response_started = JK_FALSE;
    s->read_body_started = JK_FALSE;
    s->workerEnv=workerEnv;

    /* Unless s->realWorker is set, JNI channel will not detach thread from JVM at
       the end of request
    s->jvm_route = worker->route;
    s->realWorker = worker;
    */

    s->remote_user  = NULL_FOR_EMPTY(Ns_ConnAuthUser (conn));
    s->auth_type    = (s->remote_user==NULL? NULL : "Basic");

    /* conn->request->line has HTTP request line */
    if (conn->request!=NULL && conn->request->line!=NULL)
        s->protocol = strrchr (conn->request->line, ' ');

    if (s->protocol != NULL)
        s->protocol++;
    else
        s->protocol = "HTTP/1.0";

    s->remote_host  = s->remote_addr = NULL_FOR_EMPTY(Ns_ConnPeer (conn));

    /* get server name */
    if (override != NULL)
        s->server_name = override;

    else { 
        if (reqHdrs != NULL)
	    s->server_name = Ns_SetIGet (reqHdrs, "Host");

	if (s->server_name == NULL)
	    s->server_name= Ns_ConnServer (conn);
    }

    /* get the real port (otherwise redirect failed) */
    s->server_port = Ns_ConnPort (conn);

    s->server_software = Ns_InfoServerVersion ();

    s->method         = conn->request->method;
    s->content_length = Ns_ConnContentLength (conn);
    s->is_chunked     = JK_FALSE;
    s->no_more_chunks = JK_FALSE;
    s->query_string   = conn->request->query;

    s->startTime = time (NULL);
    /*
     * The 2.2 servlet spec errata says the uri from
     * HttpServletRequest.getRequestURI() should remain encoded.
     * [http://java.sun.com/products/servlet/errata_042700.html]
     *
     * We use JkOptions to determine which method to be used
     *
     */

    switch (workerEnv->options & JK_OPT_FWDURIMASK) {

        case JK_OPT_FWDURICOMPATUNPARSED :
	    /* Parse away method, query and protocol from request line, e.g.,
	     * 
	     * GET /x/y?q HTTP/1.1
	     *
	     */
	    query_str_begin = strchr (conn->request->line, ' ');
	    if (query_str_begin == NULL) {
	        s->req_uri = NULL;
		break;
	    }

	    query_str_end = strchr (++query_str_begin, '?'); /* exclude query */
	    if (query_str_end == NULL) {
	      query_str_end = strchr (query_str_begin, ' '); /* no query? find where URI ends */
		
		if (query_str_end == NULL)
		    query_str_end = conn->request->line+strlen(conn->request->line);
	    }

	    s->req_uri = s->pool->alloc (env, s->pool, query_str_end-query_str_begin);
	    if (s->req_uri == NULL)
	        break;

            memcpy (s->req_uri, query_str_begin, query_str_end-query_str_begin+1); /* include 0 terminator */
	    

        break;

        case JK_OPT_FWDURICOMPAT :
            s->req_uri = conn->request->url;
        break;

        case JK_OPT_FWDURIESCAPED :
	    Ns_DStringInit (&dstr);
	    query_str_begin = Ns_EncodeUrl (&dstr, conn->request->url);
	    
	    if (query_str_begin != NULL) {
	        s->req_uri = s->pool->pstrdup (env, s->pool, query_str_begin+1); /* +1 because string starts with ? */
 	    } else {
	        s->req_uri = NULL;
	    }

	    Ns_DStringFree (&dstr);
        break;

        default :
	    return JK_ERR;
    }

    s->is_ssl       = JK_FALSE;
    s->ssl_cert     = NULL;
    s->ssl_cert_len = 0;
    s->ssl_cipher   = NULL;        /* required by Servlet 2.3 Api, 
                                   allready in original ajp13 */
    s->ssl_session  = NULL;
    s->ssl_key_size = -1;        /* required by Servlet 2.3 Api, added in jtc */

    if (workerEnv->ssl_enable || workerEnv->envvars_in_use) {
        if (workerEnv->ssl_enable) {

	    name = Ns_ConnDriverName (conn);

	    if (name!=NULL && strcmp(name, "nsssl")==0)
	        s->is_ssl = JK_TRUE;
	    else 
	        s->is_ssl = JK_FALSE;
		
	    if (s->is_ssl == JK_TRUE) {
	        s->ssl_cert     = NULL; /* workerEnv->certs_indicator "SSL_CLIENT_CERT" */

		if (s->ssl_cert) {
		    s->ssl_cert_len = strlen(s->ssl_cert);
		}

		/* Servlet 2.3 API */
		s->ssl_cipher   = NULL; /* workerEnv->cipher_indicator "HTTPS_CIPHER"/"SSL_CIPHER" */
		s->ssl_session  = NULL; /* workerEnv->session_indicator "SSL_SESSION_ID" */

		if (workerEnv->options & JK_OPT_FWDKEYSIZE) {
		    /* Servlet 2.3 API */
		    s->ssl_key_size = 0;
		}
	    }
	}

	jk2_map_default_create(env, &s->attributes, s->pool );
        
	if (workerEnv->envvars_in_use && reqHdrs) {
	    count = workerEnv->envvars->size( env, workerEnv->envvars );
	
	    for ( i=0; i< count ; i++ ) {
	        name= workerEnv->envvars->nameAt( env, workerEnv->envvars, i );
		val= Ns_SetIGet (reqHdrs, name);

		if (val==NULL) {
		    val=workerEnv->envvars->valueAt( env, workerEnv->envvars, i );
		}

		s->attributes->put( env, s->attributes, name, val, NULL );
	    }
	}
    }

    jk2_map_default_create(env, &s->headers_in, s->pool );

    if (reqHdrs && (count=Ns_SetSize (reqHdrs))) {
        for (i = 0 ; i < count ; i++) {
	     s->headers_in->add( env, s->headers_in, Ns_SetKey (reqHdrs, i), Ns_SetValue (reqHdrs, i));
	}
    }

    if(!s->is_chunked && s->content_length == 0) {
        /* if r->contentLength == 0 I assume there's no header
           or a header with '0'. In the second case, put will override it 
         */
        s->headers_in->put( env, s->headers_in, "content-length", "0", NULL );
    }

    jk2_map_default_create(env, &s->headers_out, s->pool );

    return JK_OK;
}

/*
 * If the servlet engine didn't consume all of the
 * request data, consume and discard all further
 * characters left to read from client
 *
 */
static void JK_METHOD jk2_service_ns_afterRequest(jk_env_t *env, jk_ws_service_t *s )
{
    Ns_Conn *conn;
    char buff[2048];
    int rd;
    struct jk_worker *w;

    if (env==NULL)
        return;

    if (s==NULL) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.afterRequest() NullPointerException\n");
        return;
    }

    if (s->content_read < s->content_length ||
        (s->is_chunked && ! s->no_more_chunks)) {
        
         conn = s->ws_private;

	 while ((rd = Ns_ConnRead (conn, buff, sizeof(buff))) > 0) {
	     s->content_read += rd;
	 }
    }

    if (s->realWorker) {
        w = s->realWorker;

        if (w != NULL && w->channel != NULL && w->channel->afterRequest != NULL) {
            w->channel->afterRequest( env, w->channel, w, NULL, s );
        }
    }
}



int JK_METHOD jk2_service_ns_init(jk_env_t *env, jk_ws_service_t *s, char* serverNameOverride)
{
    if(s==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "service.init() NullPointerException\n");
        return JK_ERR;
    }

    s->head   = jk2_service_ns_head;
    s->read   = jk2_service_ns_read;
    s->write  = jk2_service_ns_write;
    s->init   = jk2_init_ws_service;
    s->afterRequest     = jk2_service_ns_afterRequest;
    s->server_name = serverNameOverride;

    return JK_OK;
}
