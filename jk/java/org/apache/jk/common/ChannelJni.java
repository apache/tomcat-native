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


/** Pass messages using unix domain sockets.
 *
 * @author Costin Manolache
 */
public class ChannelJni extends Channel {

    int receivedNote=1;
    public ChannelJni() {
        // we use static for now, it's easier on the C side.
        // Easy to change after we get everything working
        chJni=this;
    }

    public void init() throws IOException {
        // static field init, temp
        wEnv=we;
    }
    
    public int receive( Msg msg, Endpoint ep )
        throws IOException
    {
        Msg sentResponse=(Msg)ep.getNote( receivedNote );
        // same buffer is used, no need to copy
        if( msg==sentResponse ) {
            d("Returned previously received message ");
            return 0;
        }

        d("XXX Copy previously received message ");
        // send will alter the msg and insert the response.
        // copy...
        // XXX TODO
        
        return 0;
    }

    /** Send the packet. XXX This will modify msg !!!
     *  We could use 2 packets, or sendAndReceive().
     *    
     */
    public int send( Msg msg, Endpoint ep )
        throws IOException
    {
        byte buf[]=msg.getBuffer();
        EpData epData=(EpData)ep.getNote( epDataNote );

        // send and get the response
        d( "Sending packet ");
        msg.end();
        //         msg.dump("Outgoing: ");
        
        int status=sendPacket( epData.jkEnvP, epData.jkEndpointP,
                               epData.jkServiceP, buf, msg.getLen() );
        ep.setNote( receivedNote, msg );
        
        d( "Sending packet - done ");
        return 0;
    }

    /* ==================== ==================== */
    
    static WorkerEnv wEnv=null;
    static int epDataNote=-1;
    static ChannelJni chJni=new ChannelJni();

    static class EpData {
        public long jkEnvP;
        public long jkEndpointP;
        public long jkServiceP;
    }
    
    public static Endpoint createEndpointStatic(long env, long epP) {
        Endpoint ep=new Endpoint();
        if( epDataNote==-1) 
            epDataNote=wEnv.getNoteId(WorkerEnv.ENDPOINT_NOTE, "epData");

        d("createEndpointStatic() " + env + " " + epP);
        EpData epData=new EpData();
        epData.jkEnvP=env;
        epData.jkEndpointP=epP;
        ep.setNote( epDataNote, epData );
        return ep;
    }

    public static MsgAjp createMessage() {
        System.out.println("XXX CreateMessage");
        return new MsgAjp();
    }

    public static byte[] getBuffer(MsgAjp msg) {
        System.out.println("XXX getBuffer " + msg.getBuffer() + " "
                           + msg.getBuffer().length);
        return msg.getBuffer();
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
    public static int receiveRequest( long env, long rP, Endpoint ep,
                                      MsgAjp msg)
    {
        try {
            // first, we need to get an endpoint. It should be
            // per/thread - and probably stored by the C side.
            d("Received request " + rP);
            // The endpoint will store the message pt.
            msg.processHeader();
            // msg.dump("Incoming msg ");

            EpData epData=(EpData)ep.getNote( epDataNote );

            epData.jkServiceP=rP;
            
            int status=wEnv.processCallbacks( chJni, ep, msg );
            
            d("after processCallbacks ");

        } catch( Exception ex ) {
            ex.printStackTrace();
        }
        return 0;
    }

    /** Send the packet to the C side. On return it contains the response
     *  or indication there is no response. Asymetrical because we can't
     *  do things like continuations.
     */
    public static native int sendPacket(long env, long e, long s,
                                        byte data[], int len);
    
    private static final int dL=10;
    private static void d(String s ) {
        System.err.println( "ChannelJni: " + s );
    }


}
