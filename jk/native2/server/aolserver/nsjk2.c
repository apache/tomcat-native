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
/*
 * Module jk2: keeps all servlet/jakarta related ramblings together.
 *
 * Author: Alexander Leykekh (aolserver@aol.net)
 *
 */


#include "ns.h"
#include "jk_ns.h"
#include <jni.h>

#define LOG_ERROR(msg) Ns_Log(Error,"nsjk2: %s:%d:$s",__FILE__,__LINE__,msg)
#define LOG_ERROR2(msg1,msg2) Ns_Log(Error,"nsjk2: %s:%d:$s:%s",__FILE__,__LINE__,msg1,msg2)

/* Context allows request handler extract worker and vhost for the URI quickly */
typedef struct {
    char* serverName;
    jk_uriEnv_t* uriEnv;
} uriContext;
  

#ifdef WIN32
static char  file_name[_MAX_PATH];
#endif


/* Standard AOLserver module global */
int Ns_ModuleVersion = 1;

/* Globals 
 *
 * There is one instance of JVM worker environment per process, regardless of 
 * virtual servers in AOLserver's nsd.tcl file or their counterparts in
 * Tomcat's server.xml. The only thing that AOLserver virtual servers do separately
 * is map URIs for Tomcat to process.
 *
 */
static jk_workerEnv_t *workerEnv = NULL; /* JK2 environment */
static JavaVM* jvmGlobal = NULL; /* JVM instance */
static Ns_Tls jkTls; /* TLS destructor detaches request thread from JVM */
static unsigned jkInitCount =0; /* number of running virtual servers using this module */

/** Basic initialization for jk2.
 */
static int jk2_create_workerEnv(apr_pool_t *p, char* serverRoot) {
    jk_env_t *env;
    jk_logger_t *l;
    jk_pool_t *globalPool;
    jk_bean_t *jkb;
    
    if (jk2_pool_apr_create( NULL, &globalPool, NULL, p) != JK_OK) {
        LOG_ERROR("Cannot create apr pool");
	return JK_ERR;
    }

    /** Create the global environment. This will register the default
        factories
    */
    env=jk2_env_getEnv( NULL, globalPool );
    if (env == NULL) {
        LOG_ERROR("jk2_create_workerEnv: Cannot get environment");
	return JK_ERR;
    }
        
    /* Optional. Register more factories ( or replace existing ones ) */
    /* Init the environment. */
    
    /* Create the logger */
    env->registerFactory( env, "logger.ns",  jk2_logger_ns_factory );

    jkb=env->createBean2( env, env->globalPool, "logger.ns", "");
    if (jkb == NULL) {
        LOG_ERROR("Cannot create logger bean (with factory)");
	return JK_ERR;
    }

    env->alias( env, "logger.ns:", "logger");
    l = jkb->object;
    env->l=l;
    
#ifdef WIN32
    env->soName=env->globalPool->pstrdup(env, env->globalPool, file_name);
    
    if( env->soName == NULL ){
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "Error creating env->soName\n");
        return JK_ERR;
    }
#else 
    env->soName=NULL;
#endif
    
    /* Create the workerEnv */
    jkb=env->createBean2( env, env->globalPool,"workerEnv", "");
    if (jkb == NULL) {
        LOG_ERROR("Cannot create worker environment bean");
	return JK_ERR;
    }

    workerEnv= jkb->object;

    if( workerEnv==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, "Error creating workerEnv\n");
        return JK_ERR;
    }

    env->alias( env, "workerEnv:" , "workerEnv");

    /* AOLserver is a single-process environment, but leaving childId as -1 breaks
       JNI initialization
    */
    workerEnv->childId=0;
    
    workerEnv->initData->add( env, workerEnv->initData, "serverRoot",
                              workerEnv->pool->pstrdup( env, workerEnv->pool, serverRoot));

    env->l->jkLog(env, env->l, JK_LOG_INFO, "Set JK2 serverRoot %s\n", serverRoot);
    
    return JK_OK;
}


/**
 * Shutdown callback for AOLserver
 */
void jk2_shutdown_system (void* data)
{
    if (--jkInitCount == 0) {

	apr_pool_terminate ();
	apr_terminate ();
    }
}    


/**
 * JK2 module shutdown callback for APR (Apache Portable Runtime)
 */
static apr_status_t jk2_shutdown(void *data)
{
    jk_env_t *env;
    if (workerEnv) {
        env=workerEnv->globalEnv;
        /* bug in APR?? results in SEGV. Not important, the process is going away

	workerEnv->close(env, workerEnv);
	*/

	if( workerEnv->vm != NULL  && !workerEnv->vm->mbean->disabled)
	    workerEnv->vm->destroy( env, workerEnv->vm );

        workerEnv = NULL;
    }
    return APR_SUCCESS;
}


/* ========================================================================= */
/* The JK module handlers                                                    */
/* ========================================================================= */

/** Main service method, called to forward a request to tomcat
 */
static int jk2_handler(void* context, Ns_Conn *conn)
{   
    jk_logger_t      *l=NULL;
    int              rc;
    jk_worker_t *worker=NULL;
    jk_endpoint_t *end = NULL;
    jk_env_t *env;
    char* workerName;
    jk_ws_service_t *s=NULL;
    jk_pool_t *rPool=NULL;
    int rc1;
    jk_uriEnv_t *uriEnv;
    uriContext* uriCtx = (uriContext*)context;


    /* must make this call to assure TLS destructor runs when thread exists */
    Ns_TlsSet (&jkTls, jvmGlobal);

    uriEnv= uriCtx->uriEnv;
    /* there is a chance of dynamic reconfiguration, so instead of doing above statement, might map
       the URI to its context object on the fly, like this:

       uriEnv = workerEnv->uriMap->mapUri (workerEnv->globalEnv, workerEnv->uriMap, conn->request->host, conn->request->url);
    */

    /* Get an env instance */
    env = workerEnv->globalEnv->getEnv( workerEnv->globalEnv );

    worker = uriEnv->worker;

    if (worker == NULL) /* use global default */
        worker=workerEnv->defaultWorker;

    workerName = uriEnv->mbean->getAttribute (env, uriEnv->mbean, "group");

    if( worker==NULL && workerName!=NULL ) {
        worker=env->getByName( env, workerName);
        env->l->jkLog(env, env->l, JK_LOG_INFO, 
                      "finding worker for %#lx %#lx %s\n",
                      worker, uriEnv, workerName);
        uriEnv->worker=worker;
    }

    if(worker==NULL || worker->mbean==NULL || worker->mbean->localName==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "No worker for %s\n", conn->request->url); 
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
        return NS_FILTER_RETURN;
    }

    if( uriEnv->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                      "serving %s with %#lx %#lx %s\n",
                      uriEnv->mbean->localName, worker, worker->mbean, worker->mbean->localName );

    /* Get a pool for the request */
    rPool= worker->rPoolCache->get( env, worker->rPoolCache );
    if( rPool == NULL ) {
        rPool=worker->mbean->pool->create( env, worker->mbean->pool, HUGE_POOL_SIZE );
        if( uriEnv->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG, "new rpool %#lx\n", rPool );
    }
    
    s=(jk_ws_service_t *)rPool->calloc( env, rPool, sizeof( jk_ws_service_t ));
    
    jk2_service_ns_init( env, s, uriCtx->serverName);
    
    s->pool = rPool;
    s->init( env, s, worker, conn );
    s->is_recoverable_error = JK_FALSE;
    s->uriEnv = uriEnv; 
    rc = worker->service(env, worker, s);    
    s->afterRequest(env, s);
    rPool->reset(env, rPool);

    rc1=worker->rPoolCache->put( env, worker->rPoolCache, rPool );

    if( rc1 == JK_OK ) {
        rPool=NULL;
    }

    if( rPool!=NULL ) {
        rPool->close(env, rPool);
    }

    if(rc==JK_OK) {
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
        return NS_OK;
    }

    env->l->jkLog(env, env->l, JK_LOG_ERROR, "Error connecting to tomcat %d\n", rc);
    workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
    Ns_ConnReturnInternalError (conn);
    return NS_FILTER_RETURN;
}


/*
 * TLS destructor. Sole purpose is to detach current thread from JVM before the thread exits
 */
static void jkTlsDtor (void* arg) {
    void* env = NULL;
    jint err;

    /* check if attached 1st */
    if (jvmGlobal!=NULL && 
	JNI_OK==((*jvmGlobal)->GetEnv(jvmGlobal, &env, JNI_VERSION_1_2 ))) 
        {
        (*jvmGlobal)->DetachCurrentThread (jvmGlobal);
	}
}


/** Register a URI pattern with AOLserver
 */
static void registerURI (jk_env_t* env, jk_uriEnv_t* uriEnv, char* vserver, char* serverName) {
    uriContext* ctx;
    char* uriname;

    if (uriEnv==NULL || uriEnv->mbean==NULL)
	return;

    uriname=uriEnv->mbean->getAttribute (env, uriEnv->mbean, "uri");
    if (uriname==NULL || strcmp(uriname, "/")==0)
        return;

    ctx = Ns_Malloc (sizeof (uriContext));
    if (ctx == NULL)
        return;

    ctx->serverName = serverName;
    ctx->uriEnv = uriEnv;

    if (uriEnv->mbean->debug > 0)
        env->l->jkLog (env, env->l, JK_LOG_DEBUG, "registering URI %s", uriname);

    Ns_RegisterRequest(vserver, "GET", uriname, jk2_handler, NULL, ctx, 0);
    Ns_RegisterRequest(vserver, "HEAD", uriname, jk2_handler, NULL, ctx, 0);
    Ns_RegisterRequest(vserver, "POST", uriname, jk2_handler, NULL, ctx, 0);
}


/** Standard AOLserver callback.
 * Initialize jk2, unless already done in another server. Register URI mappping for Tomcat
 * If multiple virtual servers use this module, calls to Ns_ModuleInit will be made serially
 * in order of appearance of those servers in nsd.tcl
 */
int Ns_ModuleInit(char *server, char *module)
{
    jk_env_t *env;
    
    /* configuration-related */
    char* serverName=NULL;
    char* confPath;
    static char cwdBuf[PATH_MAX];
    static char* serverRoot = NULL;

    /* APR-related */
    apr_status_t aprrc;
    char errbuf[512];
    apr_pool_t *jk_globalPool = NULL;

    /* URI registration */
    char *hosts[2] = {"*", NULL};
    jk_map_t *vhosts;
    int i, j, k, l, cnt1, cnt2;
    jk_map_t *uriMap, *webapps, *uriMaps[3];
    jk_uriEnv_t *uriEnv, *hostEnv, *appEnv;


    if (jkInitCount++ == 0) {

        /* Get Tomcat installation root - this value is same for all virtual servers*/
        if (serverRoot == NULL) {
            confPath = Ns_ConfigGetPath (NULL, module, NULL);
            serverRoot = (confPath? Ns_ConfigGetValue (confPath, "serverRoot") : NULL);
        }

        /* not configured in nsd.tcl? try env. variable */
        if (serverRoot == NULL) {
            serverRoot = getenv ("TOMCAT_HOME");
        }

        /* not in env. variables? get it from CWD */
        if (serverRoot == NULL) {
            serverRoot = getcwd (cwdBuf, sizeof(cwdBuf));
        }

	/* Initialize APR */
	if ((aprrc=apr_initialize()) != APR_SUCCESS) {
	    LOG_ERROR2 ("Cannot initialize APR", apr_strerror (aprrc, errbuf, sizeof(errbuf)));
	    return NS_ERROR;
	}

	if ((aprrc=apr_pool_create(&jk_globalPool, NULL)) != APR_SUCCESS) {
	    LOG_ERROR2 ("Cannot create global APR pool", apr_strerror (aprrc, errbuf, sizeof(errbuf)));
	    return NS_ERROR;
	}

	/* Initialize JNI */
	if (workerEnv==NULL && jk2_create_workerEnv(jk_globalPool, serverRoot)==JK_ERR) {
	    return NS_ERROR;
	}

	env=workerEnv->globalEnv;
	env->setAprPool(env, jk_globalPool);

	/* Initialize JK2 */
	if (workerEnv->init(env, workerEnv ) != JK_OK) {
	    LOG_ERROR("Cannot initialize worker environment");
	    return NS_ERROR;
	}

	workerEnv->server_name = apr_pstrcat (jk_globalPool, Ns_InfoServerName(), " ", Ns_InfoServerVersion (), NULL);
	apr_pool_cleanup_register(jk_globalPool, NULL, jk2_shutdown, apr_pool_cleanup_null);

	workerEnv->was_initialized = JK_TRUE; 
    
	if ((aprrc=apr_pool_userdata_set( "INITOK", "Ns_ModuleInit", NULL, jk_globalPool )) != APR_SUCCESS) {
	    LOG_ERROR2 ("Cannot set APR pool user data", apr_strerror (aprrc, errbuf, sizeof(errbuf)));
	    return NS_ERROR;
	}

	if (workerEnv->parentInit( env, workerEnv) != JK_OK) {
	    LOG_ERROR ("Cannot initialize global environment");
	    return NS_ERROR;
	}

	/* Obtain TLS slot - it's destructor will detach threads from JVM */
	jvmGlobal = workerEnv->vm->jvm;
	Ns_TlsAlloc (&jkTls, jkTlsDtor);

	Ns_Log (Notice, "nsjk2: Initialized JK2 environment");

    } else {
      
      env = workerEnv->globalEnv;
    }

    Ns_RegisterShutdown (jk2_shutdown_system, NULL);

    /* Register URI patterns from workers2.properties with AOLserver 
     *  
     * Worker environment has a list of vhosts, including "*" vhost.
     * Each vhost has a list of web applications (contexts) associated with it.
     * Each webapp has a list of exact, prefix, suffix and regexp URI patterns.
     *
     * Will register URIs that are either in vhost "*", or one with name matching
     * this AOLserver virtual server. Will ignore regexp patterns. Will register 
     * exact webapp URIs (context root) as JK2 somehow doesn't include them in URI
     * maps, even if specified in workers2.properties.
     *
     */

    /* virtual server name override if specified */
    confPath = Ns_ConfigGetPath (server, module, NULL);
    if (confPath != NULL)
        serverName = Ns_ConfigGetValue (confPath, "serverName");

    if (serverName == NULL)
        serverName = server;
    
    vhosts=workerEnv->uriMap->vhosts;
    hosts[1]= serverName;

    for (i=0; i<sizeof(hosts)/sizeof(*hosts); i++) {
        hostEnv=vhosts->get (env, vhosts, hosts[i]);

	if (hostEnv==NULL || hostEnv->webapps==NULL)
	    continue;

	webapps=hostEnv->webapps;
	cnt1=webapps->size(env, webapps);

	for (j=0; j<cnt1; j++) {
	    appEnv = webapps->valueAt (env, webapps, j);
	    if (appEnv == NULL)
	        continue;

	    /* register webapp root - registerURI checks if it is "/" */
	    registerURI (env, appEnv, server, serverName);
	    
	    uriMaps[0] = appEnv->exactMatch;
	    uriMaps[1] = appEnv->prefixMatch;
	    uriMaps[2] = appEnv->suffixMatch;

	    for (k=0; k<sizeof(uriMaps)/sizeof(*uriMaps); k++) {
	        if (uriMaps[k] == NULL)
		    continue;

	        cnt2 = uriMaps[k]->size (env, uriMaps[k]);
		
		for (l=0; l<cnt2; l++) {
		     registerURI (env, uriMaps[k]->valueAt (env, uriMaps[k], l), server, serverName);
		}
	    }
	}
    }

    Ns_Log (Notice, "nsjk2: Initialized on %s", server);

    return NS_OK;
}



#ifdef WIN32

BOOL WINAPI DllMain(HINSTANCE hInst,        // Instance Handle of the DLL
                    ULONG ulReason,         // Reason why NT called this DLL
                    LPVOID lpReserved)      // Reserved parameter for future use
{
    GetModuleFileName( hInst, file_name, sizeof(file_name));
    return TRUE;
}


#endif
