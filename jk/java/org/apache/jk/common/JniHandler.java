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

package org.apache.jk.common;

import java.io.*;

import java.net.*;
import java.util.*;

import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.http.*;

import org.apache.tomcat.util.threads.*;

import org.apache.jk.core.*;
import org.apache.jk.apr.*;


/** 
 * Proxy for the Jk native component.
 *
 * @author Costin Manolache
 */
public class JniHandler extends JkHandler {
    protected AprImpl apr;
    
    // The native side handler
    protected long nativeJkHandlerP;

    protected String jkHome;

    // Dispatch table codes. Hardcoded for now, will change when we have more handlers.
    public static final int JK_HANDLE_JNI_DISPATCH=0x15; 
    public static final int JK_HANDLE_SHM_DISPATCH=0x16; 


    public static final int MSG_NOTE=0;
    public static final int C2B_NOTE=1;
    public static final int MB_NOTE=2;
    
    
    public JniHandler() {
    }

    /** 
     */
    public void setJkHome( String s ) {
        jkHome=s;
    }

    /** You must call initNative() inside the component init()
     */
    public void init() throws IOException {
        // static field init, temp
    }

    protected void initNative(String nativeComponentName) {
        apr=(AprImpl)wEnv.getHandler("apr");
        if( apr==null || ! apr.isLoaded() ) { 
            if( log.isDebugEnabled() )
                log.debug("No apr, disabling jni proxy ");
            apr=null;
            return;
        }
        
        long xEnv=apr.getJkEnv();
        nativeJkHandlerP=apr.getJkHandler(xEnv, nativeComponentName );
        
        if( nativeJkHandlerP==0 ) {
            log.info("Component not found, creating it " + nativeComponentName ); 
            nativeJkHandlerP=apr.createJkHandler(xEnv, nativeComponentName);
        }
        log.debug("Native proxy " + nativeJkHandlerP );
        
        apr.releaseJkEnv(xEnv); 
   }

    public void appendString( Msg msg, String s, C2BConverter charsetDecoder)
        throws IOException
    {
        ByteChunk bc=charsetDecoder.getByteChunk();
        charsetDecoder.recycle();
        charsetDecoder.convert( s );
        charsetDecoder.flushBuffer();
        msg.appendByteChunk( bc );
    }
    
    /** Create a msg context to be used with the shm channel
     */
    public MsgContext createMsgContext() {
        if( nativeJkHandlerP==0 || apr==null  )
            return null;

        try {
            MsgContext msgCtx=new MsgContext();
            MsgAjp msg=new MsgAjp();
            
            msgCtx.setNext( this );
            
            msgCtx.setMsg( MSG_NOTE, msg); // XXX Use noteId
            
            C2BConverter c2b=new C2BConverter(  "UTF8" );
            msgCtx.setNote( C2B_NOTE, c2b );
            
            MessageBytes tmpMB=new MessageBytes();
            msgCtx.setNote( MB_NOTE, tmpMB );
            return msgCtx;
        } catch( Exception ex ) {
            ex.printStackTrace();
            return null;
        }
    }

    protected void setNativeEndpoint(MsgContext msgCtx) {
        long xEnv=apr.getJkEnv();
        msgCtx.setJniEnv( xEnv );

        long epP=apr.createJkHandler(xEnv, "endpoint");
        msgCtx.setJniContext( epP );

    }

    protected void recycleNative(MsgContext ep) {
        apr.jkRecycle(ep.getJniEnv(), ep.getJniContext());
    }
    
    /** send and get the response in the same buffer.
     */
    protected int nativeDispatch( Msg msg, MsgContext ep, int code )
        throws IOException
    {
        if( log.isInfoEnabled() ) log.info( "Sending packet ");
        msg.end();
        if( log.isTraceEnabled() ) msg.dump("OUT:" );
        long xEnv=ep.getJniEnv();
        long nativeContext=ep.getJniContext();
        if( nativeContext==0 || xEnv==0 ) {
            setNativeEndpoint( ep );
            xEnv=ep.getJniEnv();
            nativeContext=ep.getJniContext();
        }

        if( xEnv==0 || nativeContext==0 || nativeJkHandlerP==0 ) {
            log.error("invokeNative: Null pointer ");
            return -1;
        }

        // Will process the message in the current thread.
        // No wait needed to receive the response
        int status=apr.jkInvoke( xEnv,
                                 nativeJkHandlerP,
                                 nativeContext,
                                 code, msg.getBuffer(), msg.getLen()); 
        
        if( log.isInfoEnabled() ) log.info( "Sending packet - done " + status);
        return status;
    }

    public  int invoke(Msg msg, MsgContext ep )
        throws IOException
    {
        long xEnv=ep.getJniEnv();
        int type=ep.getType();

        switch( type ) {
        case JkHandler.HANDLE_RECEIVE_PACKET:
            //            return receive( msg, ep );
        case JkHandler.HANDLE_SEND_PACKET:
            //            return send( msg, ep );
        }

        int status=nativeDispatch(msg, ep, type );
        
        apr.jkRecycle(xEnv, ep.getJniContext());

        if(log.isInfoEnabled() ) log.info("Shm invoke status " + status);

        return 0;
    }    

    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( JniHandler.class );

}
