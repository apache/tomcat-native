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


/* XXX Make the 'message type' pluggable
 */

/* A lot of the 'original' behavior is hardcoded - this uses Ajp13 wire protocol,
   TCP, Ajp14 API etc.
   As we add other protocols/transports/APIs this will change, the current goal
   is to get the same level of functionality as in the original jk connector.
*/

/**
 *  Jk2 can use multiple protocols/transports.
 *  Various container adapters should load this object ( as a bean ),
 *  set configurations and use it. Note that the connector will handle
 *  all incoming protocols - it's not specific to ajp1x. The protocol
 *  is abstracted by MsgContext/Message/Channel.
 */


/** Accept ( and send ) TCP messages.
 *
 * @author Costin Manolache
 */
public class ChannelSocket extends JkHandler {
    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( ChannelSocket.class );

    int startPort=8009;
    int maxPort=8019; // 0 for backward compat.
    int port=startPort;
    InetAddress inet;
    int serverTimeout;
    boolean tcpNoDelay;
    int linger=100;
    int socketTimeout;

    ThreadPool tp=new ThreadPool();

    /* ==================== Tcp socket options ==================== */

    public ThreadPool getThreadPool() {
        return tp;
    }

    /** Set the port for the ajp13 channel.
     *  To support seemless load balancing and jni, we treat this
     *  as the 'base' port - we'll try up until we find one that is not
     *  used. We'll also provide the 'difference' to the main coyote
     *  handler - that will be our 'sessionID' and the position in
     *  the scoreboard and the suffix for the unix domain socket.
     */
    public void setPort( int port ) {
        this.startPort=port;
        this.port=port;
        this.maxPort=port+10;
    }

    public void setAddress(InetAddress inet) {
        this.inet=inet;
    }

    public void setAddress(String inet) {
        try {
            this.inet= InetAddress.getByName( inet );
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }

    /**
     * Sets the timeout in ms of the server sockets created by this
     * server. This method allows the developer to make servers
     * more or less responsive to having their server sockets
     * shut down.
     *
     * <p>By default this value is 1000ms.
     */
    public void setServerTimeout(int timeout) {
	this.serverTimeout = timeout;
    }

    public void setTcpNoDelay( boolean b ) {
	tcpNoDelay=b;
    }

    public void setSoLinger( int i ) {
	linger=i;
    }

    public void setSoTimeout( int i ) {
	socketTimeout=i;
    }

    public void setMaxPort( int i ) {
        maxPort=i;
    }

    public int getInstanceId() {
        return port-startPort;
    }
    
    /* ==================== ==================== */
    ServerSocket sSocket;
    int socketNote=1;
    int isNote=2;
    int osNote=3;

    public void accept( MsgContext ep ) throws IOException {
        if( sSocket==null ) return;
        Socket s=sSocket.accept();
        ep.setNote( socketNote, s );
        if(log.isDebugEnabled() )
            log.debug("Accepted socket " + s );
        if( linger > 0 )
            s.setSoLinger( true, linger);
        if( socketTimeout > 0 ) 
            s.setSoTimeout( socketTimeout );

        InputStream is=new BufferedInputStream(s.getInputStream());
        OutputStream os= s.getOutputStream();
        ep.setNote( isNote, is );
        ep.setNote( osNote, os );
    }

    public void init() throws IOException {
        // Find a port.
        if( maxPort<startPort) maxPort=startPort;

        for( int i=startPort; i<=maxPort; i++ ) {
            try {
                sSocket=new ServerSocket( i );
                port=i;
                break;
            } catch( IOException ex ) {
                log.info("Port busy " + i + " " + ex.toString());
                continue;
            }
        }
        
        if( sSocket==null ) {
            log.error("Can't find free port " + startPort + " " + maxPort );
            return;
        }
        log.info("JK: listening on tcp port " + port );
        
        // If this is not the base port and we are the 'main' channleSocket and
        // SHM didn't already set the localId - we'll set the instance id
        if( "channelSocket".equals( name ) &&
            port != startPort &&
            (wEnv.getLocalId()==0) ) {
            wEnv.setLocalId(  port - startPort );
        }
        if( serverTimeout > 0 )
            sSocket.setSoTimeout( serverTimeout );

        if( next==null ) {
            if( nextName!=null ) 
                setNext( wEnv.getHandler( nextName ) );
            if( next==null )
                next=wEnv.getHandler( "dispatch" );
            if( next==null )
                next=wEnv.getHandler( "request" );
        }

        running = true;

        // Run a thread that will accept connections.
        tp.start();
        SocketAcceptor acceptAjp=new SocketAcceptor(  this );
        tp.runIt( acceptAjp);
    }

    public void open(MsgContext ep) throws IOException {
    }

    
    public void close(MsgContext ep) throws IOException {
        Socket s=(Socket)ep.getNote( socketNote );
        s.close();
    }

    public void destroy() throws IOException {
        running = false;
        try {
            tp.shutdown();

            // Need to create a connection to unlock the accept();
            Socket s;
            if (inet == null) {
                s=new Socket("127.0.0.1", port );
            }else{
                s=new Socket(inet, port );
                // setting soLinger to a small value will help shutdown the
                // connection quicker
                s.setSoLinger(true, 0);
            }
            s.close();
            sSocket.close(); // XXX?
        } catch(Exception e) {
            e.printStackTrace();
        }
    }

    public int send( Msg msg, MsgContext ep)
        throws IOException
    {
        msg.end(); // Write the packet header
        byte buf[]=msg.getBuffer();
        int len=msg.getLen();
        
        if(log.isTraceEnabled() )
            log.trace("send() " + len + " " + buf[4] );

        OutputStream os=(OutputStream)ep.getNote( osNote );
        os.write( buf, 0, len );
        return len;
    }

    public int receive( Msg msg, MsgContext ep )
        throws IOException
    {
        if (log.isDebugEnabled()) {
            log.debug("receive() ");
        }

        byte buf[]=msg.getBuffer();
        int hlen=msg.getHeaderLength();
        
	// XXX If the length in the packet header doesn't agree with the
	// actual number of bytes read, it should probably return an error
	// value.  Also, callers of this method never use the length
	// returned -- should probably return true/false instead.

        int rd = this.read(ep, buf, 0, hlen );
        
        if(rd < 0) {
            // Most likely normal apache restart.
            // log.warn("Wrong message " + rd );
            return rd;
        }

        msg.processHeader();

        /* After processing the header we know the body
           length
        */
        int blen=msg.getLen();
        
	// XXX check if enough space - it's assert()-ed !!!
        
 	int total_read = 0;
        
        total_read = this.read(ep, buf, hlen, blen);
        
        if (total_read <= 0) {
            log.warn("can't read body, waited #" + blen);
            return  -1;
        }
        
        if (total_read != blen) {
             log.warn( "incomplete read, waited #" + blen +
                        " got only " + total_read);
            return -2;
        }
        
	return total_read;
    }
    
    /**
     * Read N bytes from the InputStream, and ensure we got them all
     * Under heavy load we could experience many fragmented packets
     * just read Unix Network Programming to recall that a call to
     * read didn't ensure you got all the data you want
     *
     * from read() Linux manual
     *
     * On success, the number of bytes read is returned (zero indicates end
     * of file),and the file position is advanced by this number.
     * It is not an error if this number is smaller than the number of bytes
     * requested; this may happen for example because fewer bytes
     * are actually available right now (maybe because we were close to
     * end-of-file, or because we are reading from a pipe, or  from  a
     * terminal),  or  because  read()  was interrupted by a signal.
     * On error, -1 is returned, and errno is set appropriately. In this
     * case it is left unspecified whether the file position (if any) changes.
     *
     **/
    public int read( MsgContext ep, byte[] b, int offset, int len)
        throws IOException
    {
        InputStream is=(InputStream)ep.getNote( isNote );
        int pos = 0;
        int got;

        while(pos < len) {
            got = is.read(b, pos + offset, len - pos);

            if (log.isTraceEnabled()) {
                log.trace("read() " + b + " " + (b==null ? 0: b.length) + " " +
                          offset + " " + len + " = " + got );
            }

            // connection just closed by remote. 
            if (got <= 0) {
                // This happens periodically, as apache restarts
                // periodically.
                // It should be more gracefull ! - another feature for Ajp14
                // log.warn( "server has closed the current connection (-1)" );
                return -3;
            }

            pos += got;
        }
        return pos;
    }
    
    protected boolean running=true;
    
    /** Accept incoming connections, dispatch to the thread pool
     */
    void acceptConnections() {
        if( log.isDebugEnabled() )
            log.debug("Accepting ajp connections on " + port);
        while( running ) {
            try {
                MsgContext ep=new MsgContext();
                ep.setSource(this);
                ep.setWorkerEnv( wEnv );
                this.accept(ep);
                SocketConnection ajpConn=
                    new SocketConnection(this, ep);
                tp.runIt( ajpConn );
            } catch( Exception ex ) {
                if (running)
                    ex.printStackTrace();
            }
        }
    }

    /** Process a single ajp connection.
     */
    void processConnection(MsgContext ep) {
        try {
            MsgAjp recv=new MsgAjp();
            while( running ) {
                int status= this.receive( recv, ep );
                if( status <= 0 ) {
                    if( status==-3)
                        log.warn( "server has closed the current connection (-1)" );
                    else 
                        log.warn("Closing ajp connection " + status );
                    break;
                }

                ep.setType( 0 );
                status= this.invoke( recv, ep );
                if( status!= JkHandler.OK ) {
                    log.warn("processCallbacks status " + status );
                    break;
                }
            }
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
        }

        return next.invoke( msg, ep );
    }
    
    public boolean isSameAddress(MsgContext ep) {
        Socket s=(Socket)ep.getNote( socketNote );
        return isSameAddress( s.getLocalAddress(), s.getInetAddress());
    }
    
    
    /**
     * Return <code>true</code> if the specified client and server addresses
     * are the same.  This method works around a bug in the IBM 1.1.8 JVM on
     * Linux, where the address bytes are returned reversed in some
     * circumstances.
     *
     * @param server The server's InetAddress
     * @param client The client's InetAddress
     */
    public static boolean isSameAddress(InetAddress server, InetAddress client)
    {
	// Compare the byte array versions of the two addresses
	byte serverAddr[] = server.getAddress();
	byte clientAddr[] = client.getAddress();
	if (serverAddr.length != clientAddr.length)
	    return (false);
	boolean match = true;
	for (int i = 0; i < serverAddr.length; i++) {
	    if (serverAddr[i] != clientAddr[i]) {
		match = false;
		break;
	    }
	}
	if (match)
	    return (true);

	// Compare the reversed form of the two addresses
	for (int i = 0; i < serverAddr.length; i++) {
	    if (serverAddr[i] != clientAddr[(serverAddr.length-1)-i])
		return (false);
	}
	return (true);
    }

    
}

class SocketAcceptor implements ThreadPoolRunnable {
    ChannelSocket wajp;
    
    SocketAcceptor(ChannelSocket wajp ) {
        this.wajp=wajp;
    }

    public Object[] getInitData() {
        return null;
    }

    public void runIt(Object thD[]) {
        wajp.acceptConnections();
    }
}

class SocketConnection implements ThreadPoolRunnable {
    ChannelSocket wajp;
    MsgContext ep;

    SocketConnection(ChannelSocket wajp, MsgContext ep) {
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
