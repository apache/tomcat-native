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
