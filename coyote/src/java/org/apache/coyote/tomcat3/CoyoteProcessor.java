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
import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.http.*;
import org.apache.tomcat.util.net.*;
import org.apache.tomcat.util.net.ServerSocketFactory;
import org.apache.tomcat.util.log.*;
import org.apache.tomcat.util.compat.*;
import org.apache.coyote.Adapter;
import org.apache.coyote.Processor;

class CoyoteProcessor implements Adapter {
    private CoyoteRequest reqA;
    private CoyoteResponse resA;
    private ContextManager cm;
    private boolean secure=false;
    private int serverPort=-1;
    private String localHost = null;
    private SSLImplementation sslImplementation=null;
    private SSLSupport sslSupport=null;

    CoyoteProcessor(ContextManager ctxman) {
	cm   = ctxman;
	reqA=new CoyoteRequest();
	resA=new CoyoteResponse();
	cm.initRequest( reqA, resA );
    }

    public void recycle() {
	secure = false;
	sslImplementation=null;
	sslSupport=null;
	serverPort=-1;
	localHost=null;
    }

    public void setSSLImplementation(SSLImplementation ssl) {
	sslImplementation = ssl;
    }
    public void setSecure(boolean isSecure) {
	secure = isSecure;
    }
    public void setSocket(Socket socket) {
	serverPort = socket.getLocalPort();
	InetAddress localAddress = socket.getLocalAddress();
	localHost = localAddress.getHostName();
	if( secure ) {
	    // Load up the SSLSupport class
	    if(sslImplementation != null)
		sslSupport = sslImplementation.getSSLSupport(socket);
	}
    }
    public void service(org.apache.coyote.Request request, 
			org.apache.coyote.Response response) 
	    throws Exception{
	reqA.setCoyoteRequest(request);
	reqA.setServerPort(serverPort);
	request.setLocalHost(localHost);
	resA.setCoyoteResponse(response);
	if( secure ) {
	    reqA.scheme().setString( "https" );
	    
 		// Load up the SSLSupport class
	    if(sslImplementation != null)
		reqA.setSSLSupport(sslSupport);
	}
	    
	cm.service( reqA, resA );
    }
}
