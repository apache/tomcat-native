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

#include "jk_global.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_channel.h"


#ifdef HAVE_SIGNAL

#include "signal.h"

/** Deal with 'signals'. 
 */

static struct sigaction jkAction;

/* We use a jni channel to send the notification to java
 */
static jk_channel_t *jniChannel;

/* XXX we should sync or use multiple endpoints if multiple signals
   can be concurent
*/
static jk_endpoint_t *signalEndpoint;

static void jk2_SigAction(int sig) {
    jk_env_t *env;
    
    /* Make a callback using the jni channel */
    fprintf(stderr, "Signal %d\n", sig );

    if( jk_env_globalEnv == NULL ) {
        return;
    }

    env=jk_env_globalEnv->getEnv( jk_env_globalEnv );

    if( jniChannel==NULL ) {
        jniChannel=env->getByName( env, "channel.jni:jni" );
        fprintf(stderr, "Got jniChannel %p\n", jniChannel );
    }
    if( jniChannel==NULL ) {
        return;
    }
    if( signalEndpoint == NULL ) {
        jk_bean_t *component=env->createBean2( env, NULL, "endpoint", NULL );
        if( component == NULL ) {
            fprintf(stderr, "Can't create endpoint\n" );
            return;
        }
        component->init( env, component );
        fprintf(stderr, "Create endpoint %p\n", component->object );
        signalEndpoint=component->object;
    }

    /* Channel:jni should be initialized by the caller */

    /* XXX make the callback */
    
    jk_env_globalEnv->releaseEnv( jk_env_globalEnv, env );

}


/* XXX We need to: - preserve the old signal ( or get them ) - either
     implement "waitSignal" or use invocation in jk2_SigAction

     Probably waitSignal() is better ( we can have a thread that waits )
 */

static int jk2_signal_signal(jk_env_t *env, int signalNr )
{
    memset(& jkAction, 0, sizeof(jkAction));
    jkAction.sa_handler=jk2_SigAction;
    sigaction(signalNr, &jkAction, (void *) NULL);
    return 0;
}

static int jk2_signal_sendSignal(jk_env_t *env, int target , int signo)
{
    return kill( (pid_t)target, signo );
}



int JK_METHOD jk2_signal_factory( jk_env_t *env ,jk_pool_t *pool,
                                  jk_bean_t *result,
                                  const char *type, const char *name)
{
    result->setAttribute=NULL;
    result->object="signal_struct_placeholder";
    result->invoke=NULL;
    result->init=NULL;
    result->destroy=NULL;
    
    return JK_OK;
}



#else /* ! HAVE_SIGNALS */

int JK_METHOD jk2_signal_factory( jk_env_t *env ,jk_pool_t *pool,
                                  jk_bean_t *result,
                                  const char *type, const char *name)
{
    result->disabled=JK_TRUE;
    return JK_FALSE;
}

#endif
