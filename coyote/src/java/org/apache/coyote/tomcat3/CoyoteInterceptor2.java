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
import org.apache.tomcat.util.log.*;
import org.apache.tomcat.util.compat.*;
import org.apache.tomcat.util.net.*;
import org.apache.coyote.*;

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
public class CoyoteInterceptor2 extends BaseInterceptor
{
    private String processorClassName="org.apache.coyote.http11.Http11Protocol";
    Tomcat3Adapter adapter;
    ProtocolHandler proto;
    
    public CoyoteInterceptor2() {
	super();
	// defaults:
        this.setAttribute( "port", "8080" );
        this.setAttribute( "soLinger", "100" );
    }

    // -------------------- PoolTcpConnector --------------------

    /** Set the class of the processor to use.
     */
    public void setProcessorClassName(String pcn) {
	processorClassName = pcn;
    }

    // -------------------- Start/stop --------------------
    Hashtable attributes=new Hashtable();
    
    public void setAttribute( String prop, Object value) {
	attributes.put( prop, value );
    }


    public void setProperty( String prop, String value ) {
        setAttribute( prop, value );
    }

    /** Called when the ContextManger is started
     */
    public void engineInit(ContextManager cm) throws TomcatException {
	super.engineInit( cm );
        
        adapter=new Tomcat3Adapter(cm);
        try {
            Class c=Class.forName(processorClassName);
            proto=(ProtocolHandler)c.newInstance();
        } catch( Exception ex ) {
            ex.printStackTrace();
        }

        proto.setAdapter( adapter );
        try {
            Enumeration keys=attributes.keys();
            while( keys.hasMoreElements() ) {
                String k=(String)keys.nextElement();
                Object o=attributes.get(k);
                if( o instanceof String )
                    IntrospectionUtils.setProperty( proto, k, (String)o );
                else
                    IntrospectionUtils.setAttribute( proto, k, o );
            }
        } catch( Exception ex ) {
            throw new TomcatException( "Error setting protocol properties ", ex );
        }
    }

    /** Called when the ContextManger is started
     */
    public void engineStart(ContextManager cm) throws TomcatException {
	try {
            proto.init();
	} catch( Exception ex ) {
            ex.printStackTrace();
	    throw new TomcatException( ex );
	}
    }

    public void engineShutdown(ContextManager cm) throws TomcatException {
	try {
	    proto.destroy();	
        } catch( Exception ex ) {
	    throw new TomcatException( ex );
	}
    }

    // -------------------- Handler implementation --------------------

    /** Handle HTTP expectations.
     */
    public int preService(org.apache.tomcat.core.Request request,
                          org.apache.tomcat.core.Response response) {
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

