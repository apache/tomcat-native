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
#include "jni_nstcl.h"
#include <jni.h>

typedef struct {
    char* serverName;
    jk_uriEnv_t* uriEnv;
} uriContext;
  

#ifdef WIN32
static char  file_name[_MAX_PATH];
#endif


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
static Ns_Thread initThread = 0; /* thread which initialized JVM */
static Ns_Tls jkTls; /* TLS destructor detaches request thread from JVM */
static unsigned jkInitCount =0; /* number of running virtual servers using this module */

/* argument to pass into JK2 init. thread */
struct init_thread_arg {
    /* Tomcat home */
    char* server_root;

    /* current working directory - temporary storage */
    char cwd_buf[PATH_MAX];

    /* synchronization for init phase */
    Ns_Mutex init_lock;
    Ns_Cond init_cond;
    int     init_flag;
    int     init_rc;

    /* synchronization for shutdown phase */
    Ns_Mutex shut_lock;
    Ns_Cond shut_cond;
    int     shut_flag;
} initThreadArg;


/** Basic initialization for jk2.
 */
static int jk2_create_workerEnv(apr_pool_t *p, char* serverRoot) {
    jk_env_t *env;
    jk_logger_t *l;
    jk_pool_t *globalPool;
    jk_bean_t *jkb;
    
    if (jk2_pool_apr_create( NULL, &globalPool, NULL, p) != JK_OK) {
        Ns_Log (Error, "jk2_create_workerEnv: Cannot create apr pool");
	return JK_ERR;
    }

    /** Create the global environment. This will register the default
        factories
    */
    env=jk2_env_getEnv( NULL, globalPool );
    if (env == NULL) {
        Ns_Log (Error, "jk2_create_workerEnv: Cannot get environment");
	return JK_ERR;
    }
        
    /* Optional. Register more factories ( or replace existing ones ) */
    /* Init the environment. */
    
    /* Create the logger */
#ifdef NO_NS_LOGGER
    jkb=env->createBean2( env, env->globalPool, "logger.file", "");
    if (jkb == NULL) {
        Ns_Log (Error, "jk2_create_workerEnv: Cannot create logger bean");
	return JK_ERR;
    }

    env->alias( env, "logger.file:", "logger");
    env->alias( env, "logger.file:", "logger:");
    l = jkb->object;
#else
    env->registerFactory( env, "logger.ns",  jk2_logger_ns_factory );

    jkb=env->createBean2( env, env->globalPool, "logger.ns", "");
    if (jkb == NULL) {
        Ns_Log (Error, "jk2_create_workerEnv: Cannot create logger bean (with factory)");
	return JK_ERR;
    }

    env->alias( env, "logger.ns:", "logger");
    l = jkb->object;
#endif
    
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
        Ns_Log (Error, "jk2_create_workerEnv: Cannot create worker environment bean");
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
    void* thread_rc;

    if (--jkInitCount==0 && initThread!=0) {

        Ns_MutexLock (&initThreadArg.shut_lock);
	initThreadArg.shut_flag=1;
	Ns_CondSignal (&initThreadArg.shut_cond);
	Ns_MutexUnlock (&initThreadArg.shut_lock);

	Ns_ThreadJoin (&initThread, &thread_rc);
	initThread = 0;
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

	if( workerEnv->vm != NULL  && ! workerEnv->vm->mbean->disabled )
	    workerEnv->vm->destroy( env, workerEnv->vm );

        workerEnv = NULL;
    }
    return APR_SUCCESS;
}


/** Initialize jk, using workers2.properties. 
*/
static int jk2_init(jk_env_t *env, apr_pool_t *pconf,
		     jk_workerEnv_t *workerEnv)
{
    if (workerEnv->init(env, workerEnv ) != JK_OK) {
        Ns_Log (Error, "jk2_init: Cannot initialize worker environment");
	return JK_ERR;
    }

    workerEnv->server_name   = Ns_InfoServerVersion ();
    apr_pool_cleanup_register(pconf, NULL, jk2_shutdown, apr_pool_cleanup_null);

    return JK_OK;
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
                      "nsjk2.handler() finding worker for %#lx %#lx %s\n",
                      worker, uriEnv, workerName);
        uriEnv->worker=worker;
    }

    if(worker==NULL || worker->mbean==NULL || worker->mbean->localName==NULL ) {
        env->l->jkLog(env, env->l, JK_LOG_ERROR, 
                      "nsjk2.handle() No worker for %s\n", conn->request->url); 
        workerEnv->globalEnv->releaseEnv( workerEnv->globalEnv, env );
        return NS_FILTER_RETURN;
    }

    if( uriEnv->mbean->debug > 0 )
        env->l->jkLog(env, env->l, JK_LOG_DEBUG, 
                      "nsjk2.handler() serving %s with %#lx %#lx %s\n",
                      uriEnv->mbean->localName, worker, worker->mbean, worker->mbean->localName );

    /* Get a pool for the request */
    rPool= worker->rPoolCache->get( env, worker->rPoolCache );
    if( rPool == NULL ) {
        rPool=worker->mbean->pool->create( env, worker->mbean->pool, HUGE_POOL_SIZE );
        if( uriEnv->mbean->debug > 0 )
            env->l->jkLog(env, env->l, JK_LOG_DEBUG,
                          "nsjk2.handler(): new rpool %#lx\n", rPool );
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

    env->l->jkLog(env, env->l, JK_LOG_ERROR,
                  "nsjk2.handler() Error connecting to tomcat %d\n", rc);
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


/* APR, JNI and JK2 initialization. Call this from a thread separate from AOLserver main thread
 * because of the Java garbage collection using signals that AOLserver tends to override
 */
static int InitJK2Module (char* server_root) {
    jk_env_t *env;
    apr_status_t aprrc;
    char errbuf[512];
    apr_pool_t *jk_globalPool = NULL;
        
    if ((aprrc=apr_initialize()) != APR_SUCCESS) {
        Ns_Log (Error, "InitJK2Module: Cannot initialize APR:%s", apr_strerror (aprrc, errbuf, sizeof(errbuf)));
	return JK_ERR;
    }

    if ((aprrc=apr_pool_create(&jk_globalPool, NULL)) != APR_SUCCESS) {
        Ns_Log (Error, "InitJK2Module: Cannot create global APR pool:%s", apr_strerror (aprrc, errbuf, sizeof(errbuf)));
	return JK_ERR;
    }

    if (workerEnv==NULL && jk2_create_workerEnv(jk_globalPool, server_root)==JK_ERR)
        return JK_ERR;

    env=workerEnv->globalEnv;
    env->setAprPool(env, jk_globalPool);
    
    if (jk2_init( env, jk_globalPool, workerEnv) != JK_OK) {
        Ns_Log (Error, "InitJK2Module: Cannot initialize module");
	return JK_ERR;
    }

    workerEnv->was_initialized = JK_TRUE; 
    
    if ((aprrc=apr_pool_userdata_set( "INITOK", "InitJK2Module", NULL, jk_globalPool )) != APR_SUCCESS) {
        Ns_Log (Error, "InitJK2Module: Cannot set APR pool user data:%s", apr_strerror (aprrc, errbuf, sizeof(errbuf)));
	return JK_ERR;
    }

    if (workerEnv->parentInit( env, workerEnv) != JK_OK) {
        Ns_Log (Error, "InitJK2Module: Cannot initialize global environment");
	return JK_ERR;
    }

    /* Obtain TLS slot - it's destructor will detach threads from JVM */
    jvmGlobal = workerEnv->vm->jvm;
    Ns_TlsAlloc (&jkTls, jkTlsDtor);

    /* TLS slot for non-server Tcl interpreter */
    Ns_TlsAlloc (&tclInterpData, tclInterpDataDtor);
    
    return JK_OK;
}

/*
 * Must initialize JVM in a separate thread due to signal conflicts (failed to confirm existance, but strongly
 * recommended by nsjava code).
 * AOLserver main thread allegedly masks signals that JVM uses for garbage collection
 *
 */
static void InitJK2ModuleThread (void* arg) {
   int rc;

   Ns_ThreadSetName ("nsjk2");

   rc =InitJK2Module (initThreadArg.server_root);

   /* prepare to report initialization results */
   Ns_MutexLock(&initThreadArg.init_lock);
   initThreadArg.init_rc = rc;
   initThreadArg.init_flag =1;
   Ns_CondSignal (&initThreadArg.init_cond);
   Ns_MutexUnlock(&initThreadArg.init_lock);
   
   if (rc==JK_OK) {
       /* wait for shutdown */

       Ns_MutexLock (&initThreadArg.shut_lock);

       while (!initThreadArg.shut_flag)
	   Ns_CondWait (&initThreadArg.shut_cond, &initThreadArg.shut_lock);
    
       Ns_MutexUnlock (&initThreadArg.shut_lock);

       apr_pool_terminate ();
       apr_terminate ();
   }
     
   Ns_ThreadExit ((void*)rc);
}


/**
 * Initialize JK2 in a new thread
 */
int CreateJK2InitThread (char* module)
{
    int init_rc;
    char* modPath;
    char cwdBuf[PATH_MAX];

    modPath = Ns_ConfigGetPath (NULL, module, NULL);
    initThreadArg.server_root = (modPath? Ns_ConfigGetValue (modPath, "serverRoot") : NULL);

    if (initThreadArg.server_root == NULL)
       initThreadArg.server_root  = getenv ("TOMCAT_HOME");

    if (initThreadArg.server_root == NULL)
        initThreadArg.server_root = getcwd (initThreadArg.cwd_buf, sizeof(cwdBuf));

    initThreadArg.init_flag = 0;
    Ns_MutexInit (&initThreadArg.init_lock);
    Ns_CondInit (&initThreadArg.init_cond);

    initThreadArg.shut_flag = 0;
    Ns_MutexInit (&initThreadArg.shut_lock);
    Ns_CondInit (&initThreadArg.shut_cond);

    /* create thread with 2M of stack */
    Ns_ThreadCreate (InitJK2ModuleThread, &initThreadArg, 2*1024*1024, &initThread);
    if (initThread == 0) {
        Ns_Log (Error, "Ns_ModuleInit: Cannot create JK2 initialization thread");
	return NS_ERROR;
    }
    
    /* wait till JVM is initialized */
    Ns_MutexLock (&initThreadArg.init_lock);

    while (initThreadArg.init_flag==0)
        Ns_CondWait (&initThreadArg.init_cond, &initThreadArg.init_lock);

    init_rc=initThreadArg.init_rc;

    Ns_MutexUnlock (&initThreadArg.init_lock);

    return init_rc;
}


/** Create context to pass to URI handler
 */
uriContext* createURIContext (jk_uriEnv_t* uriEnv, char* serverName) {
    uriContext* ctx = Ns_Malloc (sizeof (uriContext));

    if (ctx) {
        ctx->serverName = serverName;
	ctx->uriEnv = uriEnv;
    }

    return ctx;
}


/** Callback to get rid of URI context in registered procs
 */
static void deleteURIContext (void* context) {
    Ns_Free (context);
}
 
/** Standard AOLserver callback.
 * Initialize jk2, unless already done in another server. Register URI mappping for Tomcat
 * If multiple virtual servers use this module, calls to Ns_ModuleInit will be made serially
 * in order of appearance of those servers in nsd.tcl
 */
int Ns_ModuleInit(char *server, char *module)
{
    int init_rc = JK_OK;
    jk_map_t* uriMap;
    int i, urimapsz;
    char *uriname, *vhost;
    jk_env_t *env;
    jk_uriEnv_t* uriEnv;
    uriContext* ctx;
    char* serverName=NULL;
    char* confPath;

    if (jkInitCount++ == 0)
        init_rc = CreateJK2InitThread (module);

    Ns_RegisterShutdown (jk2_shutdown_system, NULL);

    if (init_rc != JK_OK) {
        Ns_Log (Error, "Ns_ModuleInit: Cannot initialize JK2 module");
	return NS_ERROR;
    }

    /* get Tomcat serverName override if specified */
    confPath = Ns_ConfigGetPath (server, module, NULL);
    if (confPath != NULL)
        serverName = Ns_ConfigGetValue (confPath, "serverName");
    
    /* create context with proper worker info for each URI registered in workers2.properties */
    uriMap = workerEnv->uriMap->maps;
    env=workerEnv->globalEnv;

    for (i=0, urimapsz=uriMap->size(env, uriMap); i<urimapsz; i++) {
        uriEnv = uriMap->valueAt(env, uriMap, i);
        uriname=uriEnv->mbean->getAttribute(env, uriEnv->mbean, "uri");
        vhost=uriEnv->mbean->getAttribute(env, uriEnv->mbean, "host");

	if (uriname!=NULL && strcmp(uriname, "/") && /* i.e., not global root */ 
	    (vhost==NULL || strcmp(vhost, "*")==0 || strcmp(vhost, server)==0)) /* match virtual server */ {

	    ctx = createURIContext (uriEnv, serverName);
	    Ns_RegisterRequest(server, "GET", uriname, jk2_handler, NULL, ctx, 0);
	    Ns_RegisterRequest(server, "HEAD", uriname, jk2_handler, NULL, ctx, 0);
	    Ns_RegisterRequest(server, "POST", uriname, jk2_handler, NULL, ctx, 0);
	}
    }

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
