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
 *  is abstracted by Endpoint/Message/Channel.
 */


/** Accept ( and send ) TCP messages.
 *
 * @author Costin Manolache
 */
public class ChannelSocket extends JkChannel implements Channel {

    int port;
    InetAddress inet;
    int serverTimeout;
    boolean tcpNoDelay;
    int linger=100;
    int socketTimeout;

    Worker worker;

    ThreadPool tp=new ThreadPool();

    /* ==================== Tcp socket options ==================== */

    public ThreadPool getThreadPool() {
        return tp;
    }
    
    public void setPort( int port ) {
        this.port=port;
    }

    public void setWorker( Worker w ) {
        worker=w;
    }

    public Worker getWorker() {
        return worker;
    }

    public void setAddress(InetAddress inet) {
        this.inet=inet;
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

    /* ==================== ==================== */
    ServerSocket sSocket;
    int socketNote=1;
    int isNote=2;
    int osNote=3;

    public void accept( Endpoint ep ) throws IOException {
        Socket s=sSocket.accept();
        ep.setNote( socketNote, s );
        if(dL>0 )
            d("Accepted socket " + s );
        if( linger > 0 )
            s.setSoLinger( true, linger);

        InputStream is=new BufferedInputStream(s.getInputStream());
        OutputStream os= s.getOutputStream();
        ep.setNote( isNote, is );
        ep.setNote( osNote, os );
    }

    public void init() throws IOException {
        sSocket=new ServerSocket( port );
        if( serverTimeout > 0 )
            sSocket.setSoTimeout( serverTimeout );

        // Run a thread that will accept connections.
        tp.start();
        SocketAcceptor acceptAjp=new SocketAcceptor(  this );
        tp.runIt( acceptAjp);
    }

    public void open(Endpoint ep) throws IOException {
    }

    
    public void close(Endpoint ep) throws IOException {
        Socket s=(Socket)ep.getNote( socketNote );
        s.close();
    }

    public void destroy() throws IOException {
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

    public void write( Endpoint ep, byte[] b, int offset, int len) throws IOException {
        OutputStream os=(OutputStream)ep.getNote( osNote );

        os.write( b, offset, len );
    }
    
    /**
     * Read N bytes from the InputStream, and ensure we got them all
     * Under heavy load we could experience many fragmented packets
     * just read Unix Network Programming to recall that a call to
     * read didn't ensure you got all the data you want
     *
     * from read() Linux manual
     *
     * On success, the number of bytes read is returned (zero indicates end of file),
     * and the file position is advanced by this number.
     * It is not an error if this number is smaller than the number of bytes requested;
     * this may happen for example because fewer bytes
     * are actually available right now (maybe because we were close to end-of-file,
     * or because we are reading from a pipe, or  from  a
     * terminal),  or  because  read()  was interrupted by a signal.
     * On error, -1 is returned, and errno is set appropriately. In this
     * case it is left unspecified whether the file position (if any) changes.
     *
     **/
    public int read( Endpoint ep, byte[] b, int offset, int len) throws IOException {
        InputStream is=(InputStream)ep.getNote( isNote );
        int pos = 0;
        int got;

        if (dL > 5) {
            d("reading  # " + b + " " + (b==null ? 0: b.length) + " " + offset + " " + len);
        }
        while(pos < len) {
            got = is.read(b, pos + offset, len - pos);

            if (dL > 5) {
                d("read got # " + got);
            }

            // connection just closed by remote. 
            if (got <= 0) {
                // This happens periodically, as apache restarts
                // periodically.
                // It should be more gracefull ! - another feature for Ajp14 
                return -3;
            }

            pos += got;
        }
        return pos;
    }

    
    
    public Endpoint createEndpoint() {
        return new Endpoint();
    }

    boolean running=true;
    
    /** Accept incoming connections, dispatch to the thread pool
     */
    void acceptConnections() {
        if( dL>0 )
            d("Accepting ajp connections");
        while( running ) {
            try {
                Endpoint ep=this.createEndpoint();
                this.accept(ep);
                SocketConnection ajpConn=
                    new SocketConnection(this, ep);
                tp.runIt( ajpConn );
            } catch( Exception ex ) {
                ex.printStackTrace();
            }
        }
    }

    /** Process a single ajp connection.
     */
    void processConnection(Endpoint ep) {
        if( dL > 0 )
            d( "New ajp connection ");
        try {
            MsgAjp recv=new MsgAjp();
            while( running ) {
                recv.receive( this, ep );
                int status=we.processCallbacks( this, ep, recv );
            }
            this.close( ep );
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }
    
    private static final int dL=10;
    private static void d(String s ) {
        System.err.println( "ChannelSocket: " + s );
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
    Endpoint ep;

    SocketConnection(ChannelSocket wajp, Endpoint ep) {
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
