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


/** Pass messages using jni 
 *
 * @author Costin Manolache
 */
public class ChannelJni extends JniHandler {
    int receivedNote=1;

    public ChannelJni() {
        // we use static for now, it's easier on the C side.
        // Easy to change after we get everything working
    }

    public void init() throws IOException {
        super.initNative("channel.jni:jni");

        if( apr==null ) return;
        
        // We'll be called from C. This deals with that.
        apr.addJkHandler( "channelJni", this );
        if( next==null ) {
            if( nextName!=null ) 
                setNext( wEnv.getHandler( nextName ) );
            if( next==null )
                next=wEnv.getHandler( "dispatch" );
            if( next==null )
                next=wEnv.getHandler( "request" );
            if( log.isDebugEnabled() )
                log.debug("Setting default next " + next.getClass().getName());
        }
    }

    /** Receives does nothing - send will put the response
     *  in the same buffer
     */
    public int receive( Msg msg, MsgContext ep )
        throws IOException
    {
        Msg sentResponse=(Msg)ep.getNote( receivedNote );
        return 0;
    }

    /** Send the packet. XXX This will modify msg !!!
     *  We could use 2 packets, or sendAndReceive().
     *    
     */
    public int send( Msg msg, MsgContext ep )
        throws IOException
    {
        int rc=super.nativeDispatch( msg, ep, JK_HANDLE_JNI_DISPATCH);
        ep.setNote( receivedNote, msg );
        return rc;
                

    }

    /** Receive a packet from the C side. This is called from the C
     *  code using invocation, but only for the first packet - to avoid
     *  recursivity and thread problems.
     *
     *  This may look strange, but seems the best solution for the
     *  problem ( the problem is that we don't have 'continuation' ).
     *
     *  sendPacket will move the thread execution on the C side, and
     *  return when another packet is available. For packets that
     *  are one way it'll return after it is processed too ( having
     *  2 threads is far more expensive ).
     *
     *  Again, the goal is to be efficient and behave like all other
     *  Channels ( so the rest of the code can be shared ). Playing with
     *  java objects on C is extremely difficult to optimize and do
     *  right ( IMHO ), so we'll try to keep it simple - byte[] passing,
     *  the conversion done in java ( after we know the encoding and
     *  if anyone asks for it - same lazy behavior as in 3.3 ).
     */
    public  int invoke(Msg msg, MsgContext ep )  throws IOException {
        if( log.isDebugEnabled() ) log.debug("ChannelJni.invoke: "  + ep );

        if( apr==null ) return -1;
        
        long xEnv=ep.getJniEnv();
        long cEndpointP=ep.getJniContext();

        int type=ep.getType();

        switch( type ) {
        case JkHandler.HANDLE_RECEIVE_PACKET:
            return receive( msg, ep );
        case JkHandler.HANDLE_SEND_PACKET:
            return send( msg, ep );
        }
        // Default is FORWARD - called from C 
        try {
            // first, we need to get an endpoint. It should be
            // per/thread - and probably stored by the C side.
            if( log.isDebugEnabled() ) log.debug("Received request " + xEnv);
            
            // The endpoint will store the message pt.
            msg.processHeader();
            //            if( log.isInfoEnabled() ) msg.dump("Incoming msg ");

            int status= next.invoke(  msg, ep );
            
            if( log.isDebugEnabled() ) log.debug("after processCallbacks " + status);
            
            return status;
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
        return 0;
    }    

    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( ChannelJni.class );

}
