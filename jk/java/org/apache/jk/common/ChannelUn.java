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
public class ChannelUn extends JniHandler {
    static final int CH_OPEN=4;
    static final int CH_CLOSE=5;
    static final int CH_READ=6;
    static final int CH_WRITE=7;

    String file;
    ThreadPool tp;

    /* ==================== Tcp socket options ==================== */

    public ThreadPool getThreadPool() {
        return tp;
    }
    
    public void setFile( String f ) {
        file=f;
    }

    /* ==================== ==================== */
    int socketNote=1;
    int isNote=2;
    int osNote=3;
    
    public void init() throws IOException {
        if( file==null ) {
            log.error("No file, disabling unix channel");
            throw new IOException( "No file for the unix socket channel");
        }
        if( wEnv.getLocalId() != 0 ) {
            file=file+ wEnv.getLocalId();
        }

        super.initNative( "channel.un:" + file );

        if( apr==null || ! apr.isLoaded() ) {
            log.debug("Apr is not available, disabling unix channel ");
            apr=null;
            return;
        }
        
        // Set properties and call init.
        setNativeAttribute( "file", file );
        // unixListenSocket=apr.unSocketListen( file, 10 );

        setNativeAttribute( "listen", "10" );
        setNativeAttribute( "debug", "10" );

        // Initialize the thread pool and execution chain
        if( next==null ) {
            if( nextName!=null ) 
                setNext( wEnv.getHandler( nextName ) );
            if( next==null )
                next=wEnv.getHandler( "dispatch" );
            if( next==null )
                next=wEnv.getHandler( "request" );
        }

        tp=new ThreadPool();

        File socketFile=new File( file );
        if( socketFile.exists() ) {
            // The socket file cannot be removed ...
            if (!socketFile.delete())
                  throw(new IOException("Cannot remove " + file));
        }

        super.initJkComponent();

        log.info("JK: listening on unix socket: " + file );
        
        // Run a thread that will accept connections.
        tp.start();
        AprAcceptor acceptAjp=new AprAcceptor(  this );
        tp.runIt( acceptAjp);
    }

    public void destroy() throws IOException {
        if( apr==null ) return;
        try {
            if( tp != null )
                tp.shutdown();
            
            //apr.unSocketClose( unixListenSocket,3);
            super.destroyJkComponent();
            
        } catch(Exception e) {
            e.printStackTrace();
        }
    }

    /** Open a connection - since we're listening that will block in
        accept
    */
    public void open(MsgContext ep) throws IOException {
        // Will associate a jk_endpoint with ep and call open() on it.
        // jk_channel_un will accept a connection and set the socket info
        // in the endpoint. MsgContext will represent an active connection.
        super.nativeDispatch( ep.getMsg(0), ep, CH_OPEN, 1 );
    }
    
    public void close(MsgContext ep) throws IOException {
        super.nativeDispatch( ep.getMsg(0), ep, CH_CLOSE, 1 );
    }

    public int send( Msg msg, MsgContext ep)
        throws IOException
    {
        return super.nativeDispatch( msg, ep, CH_WRITE, 0 );
    }

    public int receive( Msg msg, MsgContext ep )
        throws IOException
    {
        int rc=super.nativeDispatch( msg, ep, CH_READ, 1 );

        if( rc!=0 ) {
            log.error("receive error:   " + rc);
            return -1;
        }
        
        msg.processHeader();
        
        if (log.isDebugEnabled())
             log.debug("receive:  total read = " + msg.getLen());

	return msg.getLen();
    }
    
    boolean running=true;
    
    /** Accept incoming connections, dispatch to the thread pool
     */
    void acceptConnections() {
        if( apr==null ) return;

        if( log.isDebugEnabled() )
            log.debug("Accepting ajp connections on " + file);
        
        while( running ) {
            try {
                MsgContext ep=this.createMsgContext();

                // blocking - opening a server connection.
                this.open(ep);

                //    if( log.isDebugEnabled() )
                //     log.debug("Accepted ajp connections ");
        
                AprConnection ajpConn= new AprConnection(this, ep);
                tp.runIt( ajpConn );
            } catch( Exception ex ) {
                ex.printStackTrace();
            }
        }
    }

    /** Process a single ajp connection.
     */
    void processConnection(MsgContext ep) {
        if( log.isDebugEnabled() )
            log.debug( "New ajp connection ");
        try {
            MsgAjp recv=new MsgAjp();
            while( running ) {
                int res=this.receive( recv, ep );
                if( res<0 ) {
                    // EOS
                    break;
                }
                ep.setType(0);
                log.debug( "Process msg ");
                int status=next.invoke( recv, ep );
            }
            if( log.isDebugEnabled() )
                log.debug( "Closing un channel");
            this.close( ep );
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }

    public int invoke( Msg msg, MsgContext ep ) throws IOException {
        int type=ep.getType();

        switch( type ) {
        case JkHandler.HANDLE_RECEIVE_PACKET:
            return receive( msg, ep );
        case JkHandler.HANDLE_SEND_PACKET:
            return send( msg, ep );
        case JkHandler.HANDLE_FLUSH:
            return OK;
        }

        // return next.invoke( msg, ep );
        return OK;
    }

    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( ChannelUn.class );
}

class AprAcceptor implements ThreadPoolRunnable {
    ChannelUn wajp;
    
    AprAcceptor(ChannelUn wajp ) {
        this.wajp=wajp;
    }

    public Object[] getInitData() {
        return null;
    }

    public void runIt(Object thD[]) {
        wajp.acceptConnections();
    }
}

class AprConnection implements ThreadPoolRunnable {
    ChannelUn wajp;
    MsgContext ep;

    AprConnection(ChannelUn wajp, MsgContext ep) {
        this.wajp=wajp;
        this.ep=ep;
    }


    public Object[] getInitData() {
        return null;
    }
    
    public void runIt(Object perTh[]) {
        wajp.processConnection(ep);
    }
}
