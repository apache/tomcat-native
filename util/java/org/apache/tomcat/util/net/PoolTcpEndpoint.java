/*
 * $Header$
 * $Revision$
 * $Date$
 *
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


package org.apache.tomcat.util.net;

import org.apache.tomcat.util.res.*;
import org.apache.tomcat.util.collections.SimplePool;
import org.apache.tomcat.util.threads.*;
import org.apache.tomcat.util.log.*;
import java.io.*;
import java.net.*;
import java.util.*;

/* Similar with MPM module in Apache2.0. Handles all the details related with
   "tcp server" functionality - thread management, accept policy, etc.
   It should do nothing more - as soon as it get a socket ( and all socket options
   are set, etc), it just handle the stream to ConnectionHandler.processConnection. (costin)
*/



/**
 * Handle incoming TCP connections.
 *
 * This class implement a simple server model: one listener thread accepts on a socket and
 * creates a new worker thread for each incoming connection.
 *
 * More advanced Endpoints will reuse the threads, use queues, etc.
 *
 * @author James Duncan Davidson [duncan@eng.sun.com]
 * @author Jason Hunter [jch@eng.sun.com]
 * @author James Todd [gonzo@eng.sun.com]
 * @author Costin@eng.sun.com
 * @author Gal Shachor [shachor@il.ibm.com]
 */
public class PoolTcpEndpoint { // implements Endpoint {

    private StringManager sm = StringManager.getManager("org.apache.tomcat.resources");

    private static final int BACKLOG = 100;
    private static final int TIMEOUT = 1000;

    private boolean isPool = true;

    private int backlog = BACKLOG;
    private int serverTimeout = TIMEOUT;

    TcpConnectionHandler handler;

    private InetAddress inet;
    private int port;

    private ServerSocketFactory factory;
    private ServerSocket serverSocket;

    ThreadPoolRunnable listener;
    private boolean running = false;
    static final int debug=0;

    ThreadPool tp;
    protected Log _log=Log.getLog("tc/PoolTcpEndpoint", "PoolTcpEndpoint");

    protected boolean tcpNoDelay=false;
    protected int linger=100;
    protected int socketTimeout=-1;
    
    public PoolTcpEndpoint() {
	//	super("tc_log");	// initialize default logger
	tp = new ThreadPool();
    }
    
    // -------------------- Configuration --------------------

    public void setPoolOn(boolean isPool) {
        this.isPool = isPool;
    }

    public boolean isPoolOn() {
        return isPool;
    }

    public void setMaxThreads(int maxThreads) {
	if( maxThreads > 0)
	    tp.setMaxThreads(maxThreads);
    }

    public int getMaxThreads() {
        return tp.getMaxThreads();
    }

    public void setMaxSpareThreads(int maxThreads) {
	if(maxThreads > 0) 
	    tp.setMaxSpareThreads(maxThreads);
    }

    public int getMaxSpareThreads() {
        return tp.getMaxSpareThreads();
    }

    public void setMinSpareThreads(int minThreads) {
	if(minThreads > 0) 
	    tp.setMinSpareThreads(minThreads);
    }

    public int getMinSpareThreads() {
        return tp.getMinSpareThreads();
    }

    public int getPort() {
	    return port;
    }

    public void setPort(int port ) {
	    this.port=port;
    }

    public InetAddress getAddress() {
	    return inet;
    }

    public void setAddress(InetAddress inet) {
	    this.inet=inet;
    }

    public void setServerSocket(ServerSocket ss) {
	    serverSocket = ss;
    }

    public void setServerSocketFactory(  ServerSocketFactory factory ) {
	    this.factory=factory;
    }

   ServerSocketFactory getServerSocketFactory() {
 	    return factory;
   }

    public void setConnectionHandler( TcpConnectionHandler handler ) {
    	this.handler=handler;
    }

    public TcpConnectionHandler getConnectionHandler() {
	    return handler;
    }

    public boolean isRunning() {
	return running;
    }
    
    /**
     * Allows the server developer to specify the backlog that
     * should be used for server sockets. By default, this value
     * is 100.
     */
    public void setBacklog(int backlog) {
	if( backlog>0)
	    this.backlog = backlog;
    }

    public int getBacklog() {
        return backlog;
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
    
    public void setServerSoTimeout( int i ) {
	serverTimeout=i;
    }

    // -------------------- Public methods --------------------

    public void startEndpoint() throws IOException, InstantiationException {
	try {
	    if(factory==null)
		factory=ServerSocketFactory.getDefault();
	    if(serverSocket==null) {
                try {
                    if (inet == null) {
                        serverSocket = factory.createSocket(port, backlog);
                    } else {
                        serverSocket = factory.createSocket(port, backlog, inet);
                    }
                } catch ( BindException be ) {
                    throw new BindException(be.getMessage() + ":" + port);
                }
	    }
	    if( serverTimeout >= 0 )
		serverSocket.setSoTimeout( serverTimeout );
	} catch( IOException ex ) {
	    //	    log("couldn't start endpoint", ex, Logger.DEBUG);
            throw ex;
	} catch( InstantiationException ex1 ) {
	    //	    log("couldn't start endpoint", ex1, Logger.DEBUG);
            throw ex1;
	}
	if(isPool) {
	    tp.start();
	}
	running = true;
        if(isPool) {
    	    listener = new TcpWorkerThread(this);
            tp.runIt(listener);
        } else {
	    log("XXX Error - need pool !", null, Log.ERROR);
	}
    }

    public void stopEndpoint() {
	if (running) {
	    tp.shutdown();
	    running = false;
	    try {
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
		//		System.out.println("Closing socket " + port );
		serverSocket.close(); // XXX?
	    } catch(Exception e) {
                log("Caught exception trying to unlock accept.", e);
	    }
	    serverSocket = null;
	}
    }

    // -------------------- Private methods

    Socket acceptSocket() {
        Socket accepted = null;
    	try {
    	    if (running) {
		if(null!= serverSocket) {
                     if(factory==null){
 		        accepted = serverSocket.accept();
 		    }
 		    else {
 		        accepted = factory.acceptSocket(serverSocket);
 		    }
		    if(!running) {
			if(null != accepted) {
			    accepted.close();  // rude, but unlikely!
			    accepted = null;
			}
		    }
		    if( factory != null && accepted != null)
			factory.initSocket( accepted );
    	        }
    	    }	    
    	}
	catch(InterruptedIOException iioe) {
    	    // normal part -- should happen regularly so
    	    // that the endpoint can release if the server
    	    // is shutdown.
    	}
	catch (SocketException e) {

	    // TCP stacks can throw SocketExceptions when the client
	    // disconnects.  We don't want this to shut down the
	    // endpoint, so ignore it. Is there a more robust
	    // solution?  Should we compare the message string to
	    // "Connection reset by peer"?

	    // socket exceptions just after closing endpoint (when
	    // running=false) aren't even logged
    	    if (running != false) {
		String msg = sm.getString("endpoint.err.nonfatal",
					  serverSocket, e);
		log(msg, e, Log.INFORMATION);
    	    }

    	} 
	
	// Future developers: if you identify any other nonfatal
	// exceptions, catch them here and log as above

	catch(Throwable e) {
            // If we are running with a SecurityManager, don't shutdown Socket        
            // on an AccessControlException.
	    if( e.getClass().getName().equals("java.security.AccessControlException") ) {
		String msg = "Socket: "+ serverSocket + " AccessControlException: " + e.toString();
		log(msg, Log.ERROR);
	    } else {
		String msg = sm.getString("endpoint.err.fatal",
					serverSocket, e);    
		log(msg, e, Log.ERROR);
		stopEndpoint();	// safe to call this from inside thread pool?
	    }
    	}

    	return accepted;
    }

    public void log(String msg) 
    {
	_log.log(msg, null, Log.INFORMATION);
    }
    
    public void log(String msg, Throwable t) 
    {
	_log.log(msg, t, Log.ERROR);
    }

    public void log(String msg, int level) 
    {
	_log.log(msg, null, level);
    }

    public void log(String msg, Throwable t, int level) {
	_log.log( msg, t, level );
    }

    void setSocketOptions(Socket socket)
    {
	try {
	    if(linger >= 0 ) 
		socket.setSoLinger( true, linger);
	    if( tcpNoDelay )
		socket.setTcpNoDelay(tcpNoDelay);
	    if( socketTimeout > 0 )
		socket.setSoTimeout( socketTimeout );
	} catch(  SocketException se ) {
	    se.printStackTrace();
	}
    }

}

// -------------------- Threads --------------------

/*
 * I switched the threading model here.
 *
 * We used to have a "listener" thread and a "connection"
 * thread, this results in code simplicity but also a needless
 * thread switch.
 *
 * Instead I am now using a pool of threads, all the threads are
 * simmetric in their execution and no thread switch is needed.
 */
class TcpWorkerThread implements ThreadPoolRunnable {
    /* This is not a normal Runnable - it gets attached to an existing
       thread, runs and when run() ends - the thread keeps running.

       It's better to keep the name ThreadPoolRunnable - avoid confusion.
       We also want to use per/thread data and avoid sync wherever possible.
    */
    PoolTcpEndpoint endpoint;
    SimplePool connectionCache;
    static final boolean usePool=true;
    
    public TcpWorkerThread(PoolTcpEndpoint endpoint) {
	this.endpoint = endpoint;
	if( usePool ) {
	    connectionCache = new SimplePool(endpoint.getMaxThreads());
	    for(int i = 0 ; i < endpoint.getMaxThreads()/2 ; i++) {
		connectionCache.put(new TcpConnection());
	    }
	}
    }

    public Object[] getInitData() {
	if( usePool ) {
	    return endpoint.getConnectionHandler().init();
	} else {
	    // no synchronization overhead, but 2 array access 
	    Object obj[]=new Object[2];
	    obj[1]= endpoint.getConnectionHandler().init();
	    obj[0]=new TcpConnection();
	    return obj;
	}
    }
    
    public void runIt(Object perThrData[]) {
	TcpConnection con=null;
	if( ! usePool ) {
	    // extract the original.
	    con=(TcpConnection) perThrData[0];
	    perThrData = (Object []) perThrData[1];
	}
	
	// Create per-thread cache
	while(endpoint.isRunning()) {
	    Socket s = endpoint.acceptSocket();
	    if(null != s) {
		// Continue accepting on another thread...
		endpoint.tp.runIt(this);
		
		try {
 		    if(endpoint.getServerSocketFactory()!=null) {
 			endpoint.getServerSocketFactory().handshake(s);
 		    }
 
		    if( usePool ) {
			con=(TcpConnection)connectionCache.get();
			if( con == null ) 
			    con = new TcpConnection();
		    } else {
			if( con==null )
			    continue;
		    }
		    
		    con.setEndpoint(endpoint);
		    con.setSocket(s);
		    endpoint.setSocketOptions( s );
		    endpoint.getConnectionHandler().processConnection(con, perThrData);
 		} catch (IOException e){
 		    endpoint.log("Handshake failed",e,Log.ERROR);
                } finally {
                    con.recycle();
                    if( usePool && con != null ) connectionCache.put(con);
                }
                break;
	    }
	}
    }

}
