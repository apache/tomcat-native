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

package org.apache.ajp.tomcat33;

import java.io.IOException;
import org.apache.tomcat.core.*;
import org.apache.tomcat.util.*;
import org.apache.tomcat.util.http.*;
import java.util.Vector;
import java.io.File;

// we'll use the system.out/err until the code is stable, then
// try to logger. Since this is a normal interceptor everything should
// work

/**
 * Connector for a JNI connections using the API in tomcat.service.
 * You need to set a "connection.handler" property with the class name of
 * the JNI connection handler
 * <br>
 * Based on <code>TcpEndpointConnector</code>
 *
 * @author Gal Shachor <shachor@il.ibm.com>
 */
public class JNIConnectionHandler extends BaseInterceptor {

    public JNIConnectionHandler() {
    }

    // -------------------- Config -------------------- 
    boolean nativeLibLoaded=false;
    
    /** Location of the jni library
     */
    public void setNativeLibrary(String lib) {
        // First try to load from the library path
        try {
            System.loadLibrary(lib);
	    nativeLibLoaded=true;
            System.out.println("Library " + lib +
			       " was loaded from the lib path");
            return;
        } catch(UnsatisfiedLinkError usl) {
            //usl.printStackTrace();
            System.err.println("Failed to loadLibrary() " + lib);
        }
        
        // Loading from the library path failed
        // Try to load assuming lib is a complete pathname.
        try {
	    System.load(lib);
	    nativeLibLoaded=true;
	    System.out.println("Library " + lib + " loaded");
            return;
        } catch(UnsatisfiedLinkError usl) {
            System.err.println("Failed to load() " + lib);
            //usl.printStackTrace();
        }
        
        // OK, try to load from the default libexec 
        // directory. 
        // libexec directory = tomcat.home + / + libexec
        File f = new File(System.getProperties().getProperty("tomcat.home"),
			  "libexec");

	String os=System.getProperty( "os.name" ).toLowerCase();
        if( os.indexOf("windows")>= 0) {
            f = new File(f, "jni_connect.dll");
        } else {
            f = new File(f, "jni_connect.so");
        }
        System.load(f.toString());
	nativeLibLoaded=true;
        System.out.println("Library " + f.toString() + " loaded");
    }

    // ==================== hack for server startup  ====================

    // JNIEndpoint was called to start tomcat
    // Hack used to set the handler in JNIEndpoint.
    // This works - if we have problems we may take the time
    // and implement a better mechanism
    static JNIEndpoint ep;
    boolean running = true;

    public static void setEndpoint(JNIEndpoint jniep)
    {
        ep = jniep;
    }

    /** Called when the ContextManger is started
     */
    public void engineInit(ContextManager cm) throws TomcatException {
	super.engineInit( cm );
	if(! nativeLibLoaded ) {
	    throw new TomcatException("Missing connector native library name");
	}
	try {
	    // notify the jni side that jni is set up corectly
	    ep.setConnectionHandler(this);
	} catch( Exception ex ) {
	    throw new TomcatException( ex );
	}
    }

    public void engineShutdown(ContextManager cm) throws TomcatException {
	try {
	    // notify the jni side that the jni handler is no longer
	    // in use ( we shut down )
	    ep.setConnectionHandler(null);
	} catch( Exception ex ) {
	    throw new TomcatException( ex );
	}
    }

    // ==================== callbacks from web server ====================
    
    static Vector pool=new Vector();
    static boolean reuse=true;
    /** Called from the web server for each request
     */
    public void processConnection(long s, long l) {
	JNIRequestAdapter reqA=null;
	JNIResponseAdapter resA=null;

        try {
	    
	    if( reuse ) {
		synchronized( this ) {
		    if( pool.size()==0 ) {
			reqA=new JNIRequestAdapter( cm, this);
			resA=new JNIResponseAdapter( this );
			cm.initRequest( reqA, resA );
		    } else {
			reqA = (JNIRequestAdapter)pool.lastElement();
			resA=(JNIResponseAdapter)reqA.getResponse();
			pool.removeElement( reqA );
		    }
		}
		reqA.recycle();
		resA.recycle();
	    } else  {
		reqA = new JNIRequestAdapter(cm, this);
		resA =new JNIResponseAdapter(this);
		cm.initRequest( reqA , resA );
	    }
	    
            resA.setRequestAttr(s, l);
    	    reqA.readNextRequest(s, l);

	    //     	    if(reqA.shutdown )
	    //         		return;
    	    if(resA.getStatus() >= 400) {
        		resA.finish();
    		    return;
    	    }

    	    cm.service( reqA, resA );
    	} catch(Exception ex) {
    	    ex.printStackTrace();
    	}
	if( reuse ) {
	    synchronized( this ) {
		pool.addElement( reqA );
	    }
	}
    }

    // -------------------- Native methods --------------------
    // Calls from tomcat to the web server
    
    native int readEnvironment(long s, long l, String []env);

    native int getNumberOfHeaders(long s, long l);

    native int readHeaders(long s,
                           long l,
                           String []names,
                           String []values);

    native int read(long s,
                    long l,
                    byte []buf,
                    int from,
                    int cnt);

    native int startReasponse(long s,
                              long l,
                              int sc,
                              String msg,
                              String []headerNames,
                              String []headerValues,
                              int headerCnt);

    native int write(long s,
                     long l,
                     byte []buf,
                     int from,
                     int cnt);
}

// ==================== Request/Response adapters ====================

class JNIRequestAdapter extends Request {
    JNIConnectionHandler h;
    long s;
    long l;

    public JNIRequestAdapter(ContextManager cm,
                             JNIConnectionHandler h) {
    	this.contextM = cm;
    	this.h = h;
    }

    public  int doRead(byte b[], int off, int len) throws IOException {
	if( available <= 0 )
	    return 0;
        int rc = 0;

        while(0 == rc) {
	        rc = h.read(s, l, b, off, len);
	        if(0 == rc) {
	            Thread.currentThread().yield();
	        }
	    }
	available -= rc;
	return rc;
    }

    protected void readNextRequest(long s, long l) throws IOException {
        String []env = new String[15];
        int i = 0;

    	this.s = s;
    	this.l = l;

        for(i = 0 ; i < 12 ; i++) {
            env[i] = null;
        }

        /*
         * Read the environment
         */
        if(h.readEnvironment(s, l, env) > 0) {
    		methodMB.setString( env[0] );
    		uriMB.setString( env[1] );
    		queryMB.setString( env[2] );
    		remoteAddrMB.setString( env[3] );
    		remoteHostMB.setString( env[4] );
    		serverNameMB.setString( env[5] );
            serverPort  = Integer.parseInt(env[6]);
            authType    = env[7];
            remoteUser  = env[8];
            schemeMB.setString(env[9]);
            protoMB.setString( env[10]);
            // response.setServerHeader(env[11]);
            
            if(schemeMB.equalsIgnoreCase("https")) {
                if(null != env[12]) {
		            attributes.put("javax.servlet.request.X509Certificate",
	                               env[12]);
	            }
	            
                if(null != env[13]) {
		            attributes.put("javax.servlet.request.cipher_suite",
	                               env[13]);
	            }
	            
                if(null != env[14]) {
		            attributes.put("javax.servlet.request.ssl_session",
	                               env[14]);
	            }
            }
            
            
        } else {
            throw new IOException("Error: JNI implementation error");
        }

        /*
         * Read the headers
         */
        int nheaders = h.getNumberOfHeaders(s, l);
        if(nheaders > 0) {
            String []names = new String[nheaders];
            String []values = new String[nheaders];
            if(h.readHeaders(s, l, names, values) > 0) {
                for(i = 0 ; i < nheaders ; i++) {
                    headers.addValue(names[i]).setString(values[i]);
                }
            } else {
                throw new IOException("Error: JNI implementation error");
            }
        }

	// REQUEST_URI may include a query string
	String requestURI=uriMB.toString();
	int indexQ=requestURI.indexOf("?");
	int rLen=requestURI.length();
	if ( (indexQ >-1) && ( indexQ  < rLen) ) {
	    queryMB.setString( requestURI.substring(indexQ + 1, requestURI.length()));
	    uriMB.setString( requestURI.substring(0, indexQ));
	} 
    }
}


// Ajp use Status: instead of Status
class JNIResponseAdapter extends Response {

    JNIConnectionHandler h;
    long s;
    long l;

    public JNIResponseAdapter(JNIConnectionHandler h) {
    	this.h = h;
    }

    protected void setRequestAttr(long s, long l) throws IOException {
    	this.s = s;
    	this.l = l;
    }

    public void endHeaders() throws IOException {

    	if(request.protocol().isNull()) // HTTP/0.9 
	        return;

        super.endHeaders();
        
	// Servlet Engine header will be set per/adapter - smarter adapters
	// will not send it every time ( have it in C side ), and we may also
	// want to add informations about the adapter used 
	// 	if( request.getContext() != null)
	// 	    setHeader("Servlet-Engine",
	// 		      request.getContext().getEngineHeader());

        int    hcnt = 0;
        String []headerNames = null;
        String []headerValues = null;
	// Shouldn't be set - it's a bug if it is
        // headers.removeHeader("Status");
        hcnt = headers.size();
        headerNames = new String[hcnt];
        headerValues = new String[hcnt];

        for(int i = 0; i < hcnt; i++) {
            headerNames[i] = headers.getName(i).toString();
            headerValues[i] = headers.getValue(i).toString();
        }

        if(h.startReasponse(s, l, status,
			    HttpMessages.getMessage(status),
			    headerNames, headerValues, hcnt) <= 0) {
            throw new IOException("JNI startReasponse implementation error");
        }
    }

    public void doWrite(byte buf[], int pos, int count) throws IOException {
        if(h.write(s, l, buf, pos, count) <= 0) {
            throw new IOException("JNI implementation error");
        }
    }
}

