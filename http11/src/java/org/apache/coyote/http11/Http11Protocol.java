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
package org.apache.coyote.http11;

import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;

import org.apache.coyote.*;
import java.io.*;
import java.net.*;
import java.util.*;
import org.apache.tomcat.util.res.StringManager;
import org.apache.tomcat.util.threads.ThreadPool;
import org.apache.tomcat.util.net.*;
import org.apache.commons.modeler.Registry;
import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.MBeanRegistration;


/**
 * Abstract the protocol implementation, including threading, etc.
 * Processor is single threaded and specific to stream-based protocols,
 * will not fit Jk protocols like JNI.
 *
 * @author Remy Maucherat
 * @author Costin Manolache
 */
public class Http11Protocol implements ProtocolHandler, MBeanRegistration
{

    public Http11Protocol() {
    }

    /**
     * The string manager for this package.
     */
    protected static StringManager sm =
        StringManager.getManager(Constants.Package);

    Adapter adapter;
    Http11ConnectionHandler cHandler=new Http11ConnectionHandler( this );

    /** Pass config info
     */
    public void setAttribute( String name, Object value ) {
        if( log.isTraceEnabled())
            log.trace(sm.getString("http11protocol.setattribute", name, value));
        attributes.put(name, value);
/*
        if ("maxKeepAliveRequests".equals(name)) {
            maxKeepAliveRequests = Integer.parseInt((String) value.toString());
        } else if ("port".equals(name)) {
            setPort(Integer.parseInt((String) value.toString()));
        }
*/
    }

    public Object getAttribute( String key ) {
        return null;
    }

    /** The adapter, used to call the connector 
     */
    public void setAdapter(Adapter adapter) {
        this.adapter=adapter;
    }

    public Adapter getAdapter() {
        return adapter;
    }

    
    /** Start the protocol
     */
    public void init() throws Exception {
        ep.setConnectionHandler( cHandler );
	try {
            checkSocketFactory();
        } catch( Exception ex ) {
            log.error(sm.getString("http11protocol.socketfactory.initerror"),
                      ex);
            throw ex;
        }

        if( socketFactory!=null ) {
            Enumeration attE=attributes.keys();
            while( attE.hasMoreElements() ) {
                String key=(String)attE.nextElement();
                Object v=attributes.get( key );
                socketFactory.setAttribute( key, v );
            }
        }

        // XXX get domain from registration
        try {
            ep.initEndpoint();
        } catch (Exception ex) {
            log.error(sm.getString("http11protocol.endpoint.initerror"), ex);
            throw ex;
        }
        log.info(sm.getString("http11protocol.init", "" + ep.getPort()));

    }

    public void start() throws Exception {
        if( this.domain != null ) {
            try {
                Registry.getRegistry().registerComponent(tp, domain,"ThreadPool",
                        "type=ThreadPool,name=http" + ep.getPort());
            } catch (Exception e) {
                log.error("Can't register threadpool" );
            }
        }

        try {
            ep.startEndpoint();
        } catch (Exception ex) {
            log.error(sm.getString("http11protocol.endpoint.starterror"), ex);
            throw ex;
        }
        log.info(sm.getString("http11protocol.start", "" + ep.getPort()));
    }

    public void destroy() throws Exception {
        ep.stopEndpoint();
    }
    
    // -------------------- Properties--------------------
    protected ThreadPool tp=ThreadPool.createThreadPool(true);
    protected PoolTcpEndpoint ep=new PoolTcpEndpoint(tp);
    protected boolean secure;
    
    protected ServerSocketFactory socketFactory;
    protected SSLImplementation sslImplementation;
    // socket factory attriubtes ( XXX replace with normal setters ) 
    protected Hashtable attributes = new Hashtable();
    protected String socketFactoryName=null;
    protected String sslImplementationName=null;

    private int maxKeepAliveRequests=100; // as in Apache HTTPD server
    private int	timeout = 300000;	// 5 minutes as in Apache HTTPD server
    private String reportedname;
    private int socketCloseDelay=-1;
    private boolean disableUploadTimeout = false;
    private String compression = "off";

    // -------------------- Pool setup --------------------

    public void setPools( boolean t ) {
	ep.setPoolOn(t);
        setAttribute("pools", "" + t);
    }

    public void setMaxThreads( int maxThreads ) {
	ep.setMaxThreads(maxThreads);
        setAttribute("maxThreads", "" + maxThreads);
    }

    public void setMaxSpareThreads( int maxThreads ) {
	ep.setMaxSpareThreads(maxThreads);
        setAttribute("maxSpareThreads", "" + maxThreads);
    }

    public void setMinSpareThreads( int minSpareThreads ) {
	ep.setMinSpareThreads(minSpareThreads);
        setAttribute("minSpareThreads", "" + minSpareThreads);
    }

    // -------------------- Tcp setup --------------------

    public void setBacklog( int i ) {
	ep.setBacklog(i);
        setAttribute("backlog", "" + i);
    }
    
    public void setPort( int port ) {
	ep.setPort(port);
        setAttribute("port", "" + port);
    	//this.port=port;
    }

    public void setAddress(InetAddress ia) {
	ep.setAddress( ia );
        setAttribute("address", "" + ia);
    }

    public void setHostName( String name ) {
	// ??? Doesn't seem to be used in existing or prev code
	// vhost=name;
    }

    public void setSocketFactory( String valueS ) {
	socketFactoryName = valueS;
        setAttribute("socketFactory", valueS);
    }

    public void setSSLImplementation( String valueS) {
 	sslImplementationName=valueS;
        setAttribute("sslImplementation", valueS);
    }
 	
    public void setTcpNoDelay( boolean b ) {
	ep.setTcpNoDelay( b );
        setAttribute("tcpNoDelay", "" + b);
    }

    public void setDisableUploadTimeout(boolean isDisabled) {
        disableUploadTimeout = isDisabled;
    }

    public void setCompression(String valueS) {
        compression = valueS;
    }

    public void setSoLinger( int i ) {
	ep.setSoLinger( i );
        setAttribute("soLinger", "" + i);
    }

    public void setSoTimeout( int i ) {
	ep.setSoTimeout(i);
        setAttribute("soTimeout", "" + i);
    }
    
    public void setServerSoTimeout( int i ) {
	ep.setServerSoTimeout(i);
        setAttribute("serverSoTimeout", "" + i);
    }
    
    public void setKeystore( String k ) {
        setAttribute("keystore", k);
    }

    public void setKeypass( String k ) {
        attributes.put("keypass", k);
        //setAttribute("keypass", k);
    }

    public void setKeytype( String k ) {
        setAttribute("keystoreType", k);
    }

    public void setClientauth( String k ) {
        setAttribute("clientauth", k);
    }

    public void setProtocol( String k ) {
        setAttribute("protocol", k);
    }

    public void setAlgorithm( String k ) {
        setAttribute("algorithm", k);
    }

    public void setSecure( boolean b ) {
    	secure=b;
        setAttribute("secure", "" + b);
    }

    /** Set the maximum number of Keep-Alive requests that we will honor.
     */
    public void setMaxKeepAliveRequests(int mkar) {
	maxKeepAliveRequests = mkar;
        setAttribute("maxKeepAliveRequests", "" + mkar);
    }

    public void setSocketCloseDelay( int d ) {
        socketCloseDelay=d;
        setAttribute("socketCloseDelay", "" + d);
    }

    private static ServerSocketFactory string2SocketFactory( String val)
	throws ClassNotFoundException, IllegalAccessException,
	InstantiationException
    {
	Class chC=Class.forName( val );
	return (ServerSocketFactory)chC.newInstance();
    }

    public void setTimeout( int timeouts ) {
	timeout = timeouts * 1000;
        setAttribute("timeout", "" + timeouts);
    }

    public void setReportedname( String reportedName) {
	reportedname = reportedName;
    }
    
    // --------------------  Connection handler --------------------

    static class Http11ConnectionHandler implements TcpConnectionHandler {
        Http11Protocol proto;
        static int count=0;
        RequestGroupInfo global=null;

        Http11ConnectionHandler( Http11Protocol proto ) {
            this.proto=proto;
        }
        
        public void setAttribute( String name, Object value ) {
        }
        
        public void setServer( Object o ) {
        }
    
        public Object[] init() {
            Object thData[]=new Object[3];
            //CoyoteProcessor adaptor = new CoyoteProcessor(cm);
            // XXX Should  be on request
            //             if( proto.secure )
            //                 proto.adapter.setSSLImplementation(proto.sslImplementation);

            Http11Processor  processor = new Http11Processor();
            processor.setAdapter( proto.adapter );
            processor.setThreadPool( proto.tp );
            processor.setMaxKeepAliveRequests( proto.maxKeepAliveRequests );
            processor.setTimeout( proto.timeout );
            processor.setDisableUploadTimeout( proto.disableUploadTimeout );
            processor.setCompression( proto.compression );

            //thData[0]=adapter;
            thData[1]=processor;
            thData[2]=null;

            if( proto.getDomain() != null ) {
                try {
                    if( global==null ) {
                        global=new RequestGroupInfo();
                        Registry.getRegistry().registerComponent( global,
                                proto.getDomain(), "GlobalRequestProcessor",
                                "type=GlobalRequestProcessor,name=http" +
                                proto.ep.getPort());
                    }
                    RequestInfo rp=processor.getRequest().getRequestProcessor();
                    rp.setGlobalProcessor(global);
                    Registry.getRegistry().registerComponent( rp,
                            proto.getDomain(), "RequestProcessor",
                            "type=RequestProcessor,worker=http" +
                            proto.ep.getPort() +",name=HttpRequest" + count++ );
                } catch( Exception ex ) {
                    log.warn("Error registering request");
                }
            }

            return  thData;
        }

        public void processConnection(TcpConnection connection, Object thData[]) {
            Socket socket=null;
            Http11Processor  processor=null;
            try {
                processor=(Http11Processor)thData[1];
                
                if (processor instanceof ActionHook) {
                    ((ActionHook) processor).action(ActionCode.ACTION_START, null);
                }
                socket=connection.getSocket();
                
                InputStream in = socket.getInputStream();
                OutputStream out = socket.getOutputStream();

                if( proto.secure ) {
                    SSLSupport sslSupport=null;
                    if(proto.sslImplementation != null)
                        sslSupport = proto.sslImplementation.getSSLSupport(socket);
                    processor.setSSLSupport(sslSupport);
                } else {
                    processor.setSSLSupport( null );
                }
                processor.setSocket( socket );
                
                processor.process(in, out);
                
                // If unread input arrives after the shutdownInput() call
                // below and before or during the socket.close(), an error
                // may be reported to the client.  To help troubleshoot this
                // type of error, provide a configurable delay to give the
                // unread input time to arrive so it can be successfully read
                // and discarded by shutdownInput().
                if( proto.socketCloseDelay >= 0 ) {
                    try {
                        Thread.sleep(proto.socketCloseDelay);
                    } catch (InterruptedException ie) { /* ignore */ }
                }
                
                TcpConnection.shutdownInput( socket );
            } catch(java.net.SocketException e) {
                // SocketExceptions are normal
                proto.log.debug
                    (sm.getString
                     ("http11protocol.proto.socketexception.debug"), e);
            } catch (java.io.IOException e) {
                // IOExceptions are normal 
                proto.log.debug
                    (sm.getString
                     ("http11protocol.proto.ioexception.debug"), e);
            }
            // Future developers: if you discover any other
            // rare-but-nonfatal exceptions, catch them here, and log as
            // above.
            catch (Throwable e) {
                // any other exception or error is odd. Here we log it
                // with "ERROR" level, so it will show up even on
                // less-than-verbose logs.
                proto.log.error(sm.getString("http11protocol.proto.error"), e);
            } finally {
                //       if(proto.adapter != null) proto.adapter.recycle();
                //                processor.recycle();
                
                if (processor instanceof ActionHook) {
                    ((ActionHook) processor).action(ActionCode.ACTION_STOP, null);
                }
                // recycle kernel sockets ASAP
                try { if (socket != null) socket.close (); }
                catch (IOException e) { /* ignore */ }
            }
        }
    }

    protected static org.apache.commons.logging.Log log 
        = org.apache.commons.logging.LogFactory.getLog(Http11Protocol.class);

    // -------------------- Various implementation classes --------------------

    /** Sanity check and socketFactory setup.
     *  IMHO it is better to stop the show on a broken connector,
     *  then leave Tomcat running and broken.
     *  @exception TomcatException Unable to resolve classes
     */
    private void checkSocketFactory() throws Exception {
	if(secure) {
 	    try {
 		// The SSL setup code has been moved into
 		// SSLImplementation since SocketFactory doesn't
 		// provide a wide enough interface
 		sslImplementation=SSLImplementation.getInstance
 		    (sslImplementationName);
                socketFactory = 
                        sslImplementation.getServerSocketFactory();
		ep.setServerSocketFactory(socketFactory);
 	    } catch (ClassNotFoundException e){
 		throw e;
  	    }
  	}
 	else {
 	    if (socketFactoryName != null) {
 		try {
 		    socketFactory = string2SocketFactory(socketFactoryName);
 		    ep.setServerSocketFactory(socketFactory);
 		} catch(Exception sfex) {
 		    throw sfex;
 		}
	    }
	}
    }

    /*
      public int getPort() {
    	return ep.getPort();
    }

    public InetAddress getAddress() {
	return ep.getAddress();
    }

    public boolean isKeystoreSet() {
        return (attributes.get("keystore") != null);
    }

    public boolean isKeypassSet() {
        return (attributes.get("keypass") != null);
    }

    public boolean isClientauthSet() {
        return (attributes.get("clientauth") != null);
    }

    public boolean isAttributeSet( String attr ) {
        return (attributes.get(attr) != null);
    }

    public boolean isSecure() {
        return secure;
    }

   
    public PoolTcpEndpoint getEndpoint() {
	return ep;
    }
    
    */

    protected String domain;
    protected ObjectName oname;
    protected MBeanServer mserver;

    public ObjectName getObjectName() {
        return oname;
    }

    public String getDomain() {
        return domain;
    }

    public ObjectName preRegister(MBeanServer server,
                                  ObjectName name) throws Exception {
        oname=name;
        mserver=server;
        domain=name.getDomain();
        return name;
    }

    public void postRegister(Boolean registrationDone) {
    }

    public void preDeregister() throws Exception {
    }

    public void postDeregister() {
    }
}
