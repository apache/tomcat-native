/*
 * ====================================================================
 *
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution, if
 *    any, must include the following acknowlegement:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowlegement may appear in the software itself,
 *    if and wherever such third-party acknowlegements normally appear.
 *
 * 4. The names "The Jakarta Project", "Tomcat", and "Apache Software
 *    Foundation" must not be used to endorse or promote products derived
 *    from this software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * [Additional notices, if required by prior licensing conditions]
 *
 */
package org.apache.jk.core;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.util.Enumeration;
import java.security.*;

import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.HttpMessages;
import org.apache.tomcat.util.buf.HexUtils;


/**
 *
 * @author Henri Gomez [hgomez@slib.fr]
 * @author Dan Milstein [danmil@shore.net]
 * @author Keith Wannamaker [Keith@Wannamaker.org]
 * @author Kevin Seguin
 * @author Costin Manolache
 */
public class MsgContext {
    private int type;
    private Object notes[]=new Object[32];
    private JkHandler next;
    private JkHandler source;
    private Object req;
    private WorkerEnv wEnv;
    private Msg msgs[]=new Msg[10];
    private int status=0;

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
    
    public final JkHandler getSource() {
        return source;
    }
    
    public final void setSource(JkHandler ch) {
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
}
