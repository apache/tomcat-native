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


package org.apache.coyote.tomcat3;

import java.io.*;
import java.net.*;
import java.util.*;
import java.text.*;
import org.apache.tomcat.core.*;
import org.apache.tomcat.util.res.StringManager;
import org.apache.tomcat.util.IntrospectionUtils;
import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.http.*;
import org.apache.tomcat.util.net.*;
import org.apache.tomcat.util.net.ServerSocketFactory;
import org.apache.tomcat.util.log.*;
import org.apache.tomcat.util.compat.*;
import org.apache.tomcat.modules.server.PoolTcpConnector;
import org.apache.coyote.Adapter;
import org.apache.coyote.Processor;
import org.apache.coyote.ActionHook;
import org.apache.coyote.ActionCode;

/** Standalone http.
 *
 *  Connector properties:
 *  <ul>
 *  <li> secure - will load a SSL socket factory and act as https server</li>
 *  </ul>
 *
 *  Properties passed to the net layer:
 *  <ul>
 *  <li>timeout</li>
 *  <li>backlog</li>
 *  <li>address</li>
 *  <li>port</li>
 *  </ul>
 * Thread pool properties:
 *  <ul>
 *  <li>minSpareThreads</li>
 *  <li>maxSpareThreads</li>
 *  <li>maxThreads</li>
 *  <li>poolOn</li>
 *  </ul>
 * Properties for HTTPS:
 *  <ul>
 *  <li>keystore - certificates - default to ~/.keystore</li>
 *  <li>keypass - password</li>
 *  <li>clientauth - true if the server should authenticate the client using certs</li>
 *  </ul>
 * Properties for HTTP:
 *  <ul>
 *  <li>reportedname - name of server sent back to browser (security purposes)</li>
 *  </ul>
 */
public class CoyoteInterceptor extends PoolTcpConnector
    implements  TcpConnectionHandler
{
    private int	timeout = 300000;	// 5 minutes as in Apache HTTPD server
    private String reportedname;
    private int socketCloseDelay=-1;
    private String processorClassName="org.apache.coyote.http11.Http11Processor";
    private int maxKeepAliveRequests=100; // as in Apache HTTPD server

    public CoyoteInterceptor() {
	super();
        super.setSoLinger( 100 );
	// defaults:
	this.setPort( 8080 );
    }

    // -------------------- PoolTcpConnector --------------------

    protected void localInit() throws Exception {
	ep.setConnectionHandler( this );
    }

    // -------------------- Attributes --------------------
    public void setTimeout( int timeouts ) {
	timeout = timeouts * 1000;
    }
    public void setReportedname( String reportedName) {
	reportedname = reportedName;
    }

    /** Set the maximum number of Keep-Alive requests that we will honor.
     */
    public void setMaxKeepAliveRequests(int mkar) {
	maxKeepAliveRequests = mkar;
    }

    /** Set the class of the processor to use.
     */
    public void setProcessorClassName(String pcn) {
	processorClassName = pcn;
    }

    public void setSocketCloseDelay( int d ) {
        socketCloseDelay=d;
    }

    public void setProperty( String prop, String value ) {
        setAttribute( prop, value );
    }

    // -------------------- Handler implementation --------------------
    public void setServer( Object o ) {
	this.cm=(ContextManager)o;
    }
    
    public Object[] init() {
	Object thData[]=new Object[3];
	CoyoteProcessor adaptor = new CoyoteProcessor(cm);
	if( secure )
	    adaptor.setSSLImplementation(sslImplementation);
	Processor processor = null;
	try {
	    Class processorClass = 
		getClass().getClassLoader().loadClass(processorClassName);
	    processor = (Processor)processorClass.newInstance();
	    processor.setAdapter(adaptor);
	    IntrospectionUtils.setProperty(processor,"maxKeepAliveRequests", 
					    String.valueOf(maxKeepAliveRequests));
	} catch(Exception ex) {
	    log("Can't load Processor", ex);
	}

	thData[0]=adaptor;
	thData[1]=processor;
	thData[2]=null;
	return  thData;
    }

    public void processConnection(TcpConnection connection, Object thData[]) {
	Socket socket=null;
	CoyoteProcessor adaptor=null;
	Processor  processor=null;
	try {
	    adaptor=(CoyoteProcessor)thData[0];
	    processor=(Processor)thData[1];

	    if (processor instanceof ActionHook) {
		((ActionHook) processor).action(ActionCode.ACTION_START, null);
	    }
	    socket=connection.getSocket();
 	    socket.setSoTimeout(timeout);

	    InputStream in = socket.getInputStream();
	    OutputStream out = socket.getOutputStream();
	    adaptor.setSocket(socket);
	    processor.process(in, out);

            // If unread input arrives after the shutdownInput() call
            // below and before or during the socket.close(), an error
            // may be reported to the client.  To help troubleshoot this
            // type of error, provide a configurable delay to give the
            // unread input time to arrive so it can be successfully read
            // and discarded by shutdownInput().
            if( socketCloseDelay >= 0 ) {
                try {
                    Thread.sleep(socketCloseDelay);
                } catch (InterruptedException ie) { /* ignore */ }
            }

	    TcpConnection.shutdownInput( socket );
	} catch(java.net.SocketException e) {
	    // SocketExceptions are normal
	    log( "SocketException reading request, ignored", null,
		 Log.INFORMATION);
	    log( "SocketException reading request:", e, Log.DEBUG);
	} catch (java.io.IOException e) {
	    // IOExceptions are normal 
	    log( "IOException reading request, ignored", null,
		 Log.INFORMATION);
	    log( "IOException reading request:", e, Log.DEBUG);
	}
	// Future developers: if you discover any other
	// rare-but-nonfatal exceptions, catch them here, and log as
	// above.
	catch (Throwable e) {
	    // any other exception or error is odd. Here we log it
	    // with "ERROR" level, so it will show up even on
	    // less-than-verbose logs.
	    e.printStackTrace();
	    log( "Error reading request, ignored", e, Log.ERROR);
	} finally {
	    if(adaptor != null) adaptor.recycle();
	    if (processor instanceof ActionHook) {
		((ActionHook) processor).action(ActionCode.ACTION_STOP, null);
	    }
	    // recycle kernel sockets ASAP
	    try { if (socket != null) socket.close (); }
	    catch (IOException e) { /* ignore */ }
        }
    }
 
     /**
       getInfo calls for SSL data
 
       @return the requested data
     */
     public Object getInfo( Context ctx, Request request,
 			   int id, String key ) {
       // The following code explicitly assumes that the only
       // attributes hand;ed here are HTTP. If you change that
       // you MUST change the test for sslSupport==null --EKR
 
       CoyoteRequest httpReq;

       
       try {
 	httpReq=(CoyoteRequest)request;
       } catch (ClassCastException e){
 	return null;
       }
 
       if(key!=null && httpReq!=null && httpReq.sslSupport!=null){
 	  try {
 	      if(key.equals("javax.servlet.request.cipher_suite"))
 		  return httpReq.sslSupport.getCipherSuite();
 	      if(key.equals("javax.servlet.request.X509Certificate"))
 		  return httpReq.sslSupport.getPeerCertificateChain();
 	  } catch (Exception e){
 	      log("Exception getting SSL attribute " + key,e,Log.WARNING);
 	      return null;
 	  }
       }
       return super.getInfo(ctx,request,id,key);
     }

    /** Handle HTTP expectations.
     */
    public int preService(Request request, Response response) {
	if(response instanceof CoyoteResponse) {
	    try {
		((CoyoteResponse)response).sendAcknowledgement();
	    } catch(Exception ex) {
		log("Can't send ACK", ex);
	    }
	}
	return 0;
    }
}

