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
public class ChannelUn extends JkChannel implements Channel {

    String file;
    Worker worker;
    ThreadPool tp=new ThreadPool();
    String jkHome;

    /* ==================== Tcp socket options ==================== */

    public ThreadPool getThreadPool() {
        return tp;
    }
    
    public void setWorker( Worker w ) {
        worker=w;
    }

    public Worker getWorker() {
        return worker;
    }

    public void setFile( String f ) {
        file=f;
    }

    /** Set the base dir of the jk webapp. This is used to locate
     *  the (fixed) path to the native lib.
     */
    public void setJkHome( String s ) {
        jkHome=s;
    }

    /* ==================== ==================== */
    long unixListenSocket;
    int socketNote=1;
    int isNote=2;
    int osNote=3;
    AprImpl apr;
    long gPool;
    
    public void accept( Endpoint ep ) throws IOException {
        long l= apr.unAccept(gPool, unixListenSocket);
        /* We could create a real java.net.Socket, or a UnixSocket, etc
         */
        ep.setNote( socketNote, new Long( l ) );

        if(dL>0 )
            d("Accepted socket " + l );
    }

    public void init() throws IOException {
        apr=new AprImpl();
        File f=new File( jkHome );
        File aprBase=new File( jkHome, "/WEB-INF/jk2/jni" );
        apr.setBaseDir( aprBase.getAbsolutePath() );
        apr.loadNative();

        apr.initialize();
        gPool=apr.poolCreate( 0 );
        if( dL > 0 ) d( "Create pool " + gPool );

        File socketFile=new File( file );
        if( socketFile.exists() ) {
            // The socket file cannot be removed ...
            if (!socketFile.delete())
                  throw(new IOException("Cannot remove " + file));
        }
        unixListenSocket=apr.unSocketListen( gPool, file, 10 );
        if (unixListenSocket<0)
            throw(new IOException("Cannot create listening socket"));

        // Run a thread that will accept connections.
        tp.start();
        AprAcceptor acceptAjp=new AprAcceptor(  this );
        tp.runIt( acceptAjp);
    }

    public void open(Endpoint ep) throws IOException {
    }

    
    public void close(Endpoint ep) throws IOException {
        Long s=(Long)ep.getNote( socketNote );
        apr.unSocketClose(gPool, s.longValue(),3);
    }

    public void destroy() throws IOException {
        try {
            tp.shutdown();

            apr.unSocketClose(gPool, unixListenSocket,3);
        } catch(Exception e) {
            e.printStackTrace();
        }
    }

    public void write( Endpoint ep, byte[] b, int offset, int len) throws IOException {
        Long s=(Long)ep.getNote( socketNote );

        apr.unWrite( gPool, s.longValue(), b, offset, len );
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
        Long s=(Long)ep.getNote( socketNote );
        int pos = 0;
        int got;

        while(pos < len) {
            got=apr.unRead( gPool, s.longValue(),
                            b, pos + offset, len - pos);

            if (dL > 5) {
                d("reading  # " + b + " " + (b==null ? 0: b.length) + " " +
                  offset + " " + len + " got # " + got);
            }
            // connection just closed by remote. 
            if (got <= 0) {
                return got;
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
            d("Accepting ajp connections on " + file);
        while( running ) {
            try {
                Endpoint ep=this.createEndpoint();
                this.accept(ep);
                AprConnection ajpConn=
                    new AprConnection(this, ep);
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
                int res=recv.receive( this, ep );
                if( res<0 ) {
                    // EOS
                    break;
                }
                int status=we.processCallbacks( this, ep, recv );
            }
            this.close( ep );
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }
    
    private static final int dL=10;
    private static void d(String s ) {
        System.err.println( "ChannelUn: " + s );
    }

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
    Endpoint ep;

    AprConnection(ChannelUn wajp, Endpoint ep) {
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
