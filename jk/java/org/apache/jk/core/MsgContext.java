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

package org.apache.jk.core;

import java.io.IOException;


/**
 *
 * @author Henri Gomez [hgomez@apache.org]
 * @author Dan Milstein [danmil@shore.net]
 * @author Keith Wannamaker [Keith@Wannamaker.org]
 * @author Kevin Seguin
 * @author Costin Manolache
 */
public class MsgContext {
    private int type;
    private Object notes[]=new Object[32];
    private JkHandler next;
    private JkChannel source;
    private Object req;
    private WorkerEnv wEnv;
    private Msg msgs[]=new Msg[10];
    private int status=0;
    // Control object
    private Object control;

    // Application managed, like notes
    private long timers[]=new long[20];
    
    // The context can be used by JNI components as well
    private long jkEndpointP;
    private long xEnvP;

    // Temp: use notes and dynamic strings
    public static final int TIMER_RECEIVED=0;
    public static final int TIMER_PRE_REQUEST=1;
    public static final int TIMER_POST_REQUEST=2;
    
    public final Object getNote( int id ) {
        return notes[id];
    }

    public final void setNote( int id, Object o ) {
        notes[id]=o;
    }

    /** The id of the chain */
    public final int getType() {
        return type;
    }

    public final void setType(int i) {
        type=i;
    }

    public final void setLong( int i, long l) {
        timers[i]=l;
    }
    
    public final long getLong( int i) {
        return timers[i];
    }
    
    // Common attributes ( XXX should be notes for flexibility ? )

    public final WorkerEnv getWorkerEnv() {
        return wEnv;
    }

    public final void setWorkerEnv( WorkerEnv we ) {
        this.wEnv=we;
    }
    
    public final JkChannel getSource() {
        return source;
    }
    
    public final void setSource(JkChannel ch) {
        this.source=ch;
    }

    public final int getStatus() {
        return status;
    }

    public final void setStatus( int s ) {
        status=s;
    }
    
    public final JkHandler getNext() {
        return next;
    }
    
    public final void setNext(JkHandler ch) {
        this.next=ch;
    }

    /** The high level request object associated with this context
     */
    public final void setRequest( Object req ) {
        this.req=req;
    }

    public final  Object getRequest() {
        return req;
    }

    /** The context may store a number of messages ( buffers + marshalling )
     */
    public final Msg getMsg(int i) {
        return msgs[i];
    }

    public final void setMsg(int i, Msg msg) {
        this.msgs[i]=msg;
    }
    
    /** Each context contains a number of byte[] buffers used for communication.
     *  The C side will contain a char * equivalent - both buffers are long-lived
     *  and recycled.
     *
     *  This will be called at init time. A long-lived global reference to the byte[]
     *  will be stored in the C context.
     */
    public byte[] getBuffer( int id ) {
        // We use a single buffer right now. 
        if( msgs[id]==null ) {
            return null;
        }
        return msgs[id].getBuffer();
    }

    /** Invoke a java hook. The xEnv is the representation of the current execution
     *  environment ( the jni_env_t * )
     */
    public int execute() throws IOException {
        int status=next.invoke(msgs[0], this);
        return status;
    }

    // -------------------- Jni support --------------------
    
    /** Store native execution context data when this handler is called
     *  from JNI. This will change on each call, represent temproary
     *  call data.
     */
    public void setJniEnv( long xEnvP ) {
            this.xEnvP=xEnvP;
    }

    public long getJniEnv() {
        return xEnvP;
    }
    
    /** The long-lived JNI context associated with this java context.
     *  The 2 share pointers to buffers and cache data to avoid expensive
     *  jni calls.
     */
    public void setJniContext( long cContext ) {
        this.jkEndpointP=cContext;
    }

    public long getJniContext() {
        return jkEndpointP;
    }

    public Object getControl() {
        return control;
    }

    public void setControl(Object control) {
        this.control = control;
    }
}
