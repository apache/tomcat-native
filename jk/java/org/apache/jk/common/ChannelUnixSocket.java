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


/** Accept ( and send ) messages via Unix Socket (AF_UNIX).
 * The AF_UNIX is not supported by JAVA so we need a piece of native code.
 *
 * @author Costin Manolache
 * @author Jean-Frederic Clere (Well, I have copied Costin's code and ideas).
 */
public class ChannelUnixSocket extends Channel {

    /* XXX do not have port/Address */
    // int port;
    // InetAddress inet;
    int serverTimeout;
    boolean tcpNoDelay;
    int linger=100;
    int socketTimeout;

    Worker worker;

    ThreadPool tp=new ThreadPool();

    /* ==================== socket options ==================== */

    public ThreadPool getThreadPool() {
        return tp;
    }
    
    public void setPort( int port ) {
        // this.port=port;
    }

    public void setWorker( Worker w ) {
        worker=w;
    }

    public Worker getWorker() {
        return worker;
    }

    public void setAddress(InetAddress inet) {
        // this.inet=inet;
        // XXX we have to set the filename for the AF_UNIX socket.
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
    UnixSocketServer sSocket; // File descriptor of the AF_UNIX server socket.
    

    /* ==================== ==================== */
    int socketNote=1; // File descriptor.
    int isNote=2; // Input Stream.
    int osNote=3; // Output Stream.

    public void accept( Endpoint ep ) throws IOException {
        UnixSocket s =sSocket.accept();
        ep.setNote( socketNote, s );
        if(dL>0 )
            d("Accepted socket " + s );
        if( linger > 0 )
            s.setSoLinger( true, linger);

        InputStream is=new UnixSocketIn(s.fd);
        OutputStream os=new UnixSocketOut(s.fd);
        ep.setNote( isNote, is );
        ep.setNote( osNote, os );
    }

    public void init() throws IOException {
        // I have put it here...
        // It load libunixsocket.so in jre/lib/i386 in my Linux using a Sun JVM.
        System.loadLibrary("unixsocket");
        sSocket=new UnixSocketServer(); // port );
        if( serverTimeout > 0 )
            sSocket.setSoTimeout( serverTimeout );

        // Run a thread that will accept connections.
        tp.start();
        USocketAcceptor acceptAjp=new USocketAcceptor(  this );
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
            // I do not if this is needed but the file should be removed.
/*
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
 */
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
                USocketConnection ajpConn=
                    new USocketConnection(this, ep);
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
        System.err.println( "ChannelUnixSocket: " + s );
    }

}

class USocketAcceptor implements ThreadPoolRunnable {
    ChannelUnixSocket wajp;
    
    USocketAcceptor(ChannelUnixSocket wajp ) {
        this.wajp=wajp;
    }

    public Object[] getInitData() {
        return null;
    }

    public void runIt(Object thD[]) {
        wajp.acceptConnections();
    }
}

class USocketConnection implements ThreadPoolRunnable {
    ChannelUnixSocket wajp;
    Endpoint ep;

    USocketConnection(ChannelUnixSocket wajp, Endpoint ep) {
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

/**
 * Native AF_UNIX socket server
 */
class UnixSocketServer {
    private int fd; // From socket + bind.

    private static native int createSocketNative (String filename);

    private static native int acceptNative (int fd);
    private static native int closeNative (int fd);
    private static native int setSoTimeoutNative (int fd,int value);

    public UnixSocketServer() throws IOException {
        fd = createSocketNative("/usr/tmp/apache_socket");
        if (fd<0) 
            throw new IOException();	
    }
    public UnixSocket accept() throws IOException {
        UnixSocket socket = new UnixSocket();
        socket.fd = acceptNative(this.fd);
        if (socket.fd<0)
            throw new IOException();
        return socket;
    }
    public void setSoTimeout(int value) throws SocketException {
        if (setSoTimeoutNative(this.fd, value)<0)
            throw new SocketException();
    }
    public void close() {
        closeNative(this.fd);
    }
}
/**
 * Native AF_UNIX socket
 */
class UnixSocket {
    public int fd; // From accept.

    private static native int setSoLingerNative (int fd, int l_onoff,
                                                 int l_linger);

    public void setSoLinger  (boolean bool ,int l_linger) throws IOException {
        int l_onoff=0;
        if (bool)
            l_onoff = 1;
        if (setSoLingerNative(fd,l_onoff,l_linger)<0)
            throw new IOException();
    }
}
/**
 * Provide an InputStream for the native socket.
 */
class UnixSocketIn extends InputStream {
    private int fd; // From openNative.

    private static native int readNative (int fd,  byte buf [], int len);

    public int read() throws IOException {
        byte buf [] = new byte[1];
        if (readNative(fd,buf,1) < 0)
            throw new IOException();
        return 0xff & buf[0];
    }
    public UnixSocketIn(int fd) {
        this.fd = fd;
    }
}

/**
 * Provide an OutputStream for the native socket.
 */
class UnixSocketOut extends OutputStream {
    private int fd; // From openNative.

    private static native int writeNative (int fd,  byte buf [], int len);

    public void write(int value) throws IOException {
        byte buf [] = new byte[] { (byte) value };
        if (writeNative(fd,buf,1) < 0)
            throw new IOException();
    }

    public UnixSocketOut(int fd) {
        this.fd = fd;
    }
}
