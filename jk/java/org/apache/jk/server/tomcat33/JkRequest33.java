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

package org.apache.jk.server.tomcat33;

import java.io.*;
import java.net.*;
import java.util.*;

import org.apache.jk.core.*;
import org.apache.jk.common.*;
import org.apache.tomcat.modules.server.PoolTcpConnector;

import org.apache.tomcat.core.*;

import org.apache.tomcat.util.net.*;
import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.log.*;
import org.apache.tomcat.util.http.*;

class JkRequest33 extends Request 
{
    BaseRequest ajpReq;
    MsgContext ep;

    JkInputStream jkIS;
    
    public JkRequest33(BaseRequest ajpReq) 
    {
	headers = ajpReq.headers();
	methodMB=ajpReq.method();
	protoMB=ajpReq.protocol();
	uriMB = ajpReq.requestURI();
	queryMB = ajpReq.queryString();
	remoteAddrMB = ajpReq.remoteAddr();
	remoteHostMB = ajpReq.remoteHost();
	serverNameMB = ajpReq.serverName();
        
	// XXX sync cookies 
	scookies = new Cookies( headers );
	urlDecoder=new UDecoder();

	// XXX sync headers
	
	params.setQuery( queryMB );
	params.setURLDecoder( urlDecoder );
	params.setHeaders( headers );
	initRequest(); 	

	this.ajpReq=ajpReq;
    }

    public void setEndpoint( MsgContext ep ) {
        this.ep=ep;
        int bodyNote= ep.getWorkerEnv().getNoteId( WorkerEnv.ENDPOINT_NOTE,
                                                   "jkInputStream");
        jkIS=(JkInputStream)ep.getNote( bodyNote );
    }
    
    // -------------------- Wrappers for changed method names

    public int getServerPort() {
        return ajpReq.getServerPort();
    }

    public void setServerPort(int i ) {
	ajpReq.setServerPort( i );
    }

    public  void setRemoteUser( String s ) {
	super.setRemoteUser(s);
	ajpReq.remoteUser().setString(s);
    }

    public String getRemoteUser() {
	String s=ajpReq.remoteUser().toString();
	if( s == null )
	    s=super.getRemoteUser();
	return s;
    }

    public String getAuthType() {
	return ajpReq.authType().toString();
    }
    
    public void setAuthType(String s ) {
	ajpReq.authType().setString(s);
    }

    public String getJvmRoute() {
	return ajpReq.jvmRoute().toString();
    }
    
    public void setJvmRoute(String s ) {
	ajpReq.jvmRoute().setString(s);
    }

    // XXX scheme
    
    public boolean isSecure() {
	return ajpReq.getSecure();
    }
    
    // -------------------- Attributes --------------------
    
    public void setAttribute(String name, Object value) {
	ajpReq.setAttribute( name, value );
    }

    public Object getAttribute(String name) {
        if (name == null) {
            return null;
        }

        return ajpReq.getAttribute( name );
    }

    // XXX broken
//     public Iterator getAttributeNames() {
//         return attributes.keySet().iterator();
//     }


    // --------------------

    public void recycle() {
	super.recycle();
	ajpReq.recycle();
        jkIS.recycle();
    }

    public String dumpRequest() {
	return ajpReq.toString();
    }
    
    // -------------------- 
    
    public int doRead() throws IOException 
    {
        // we need to manager the request's available
        // this should go away, we support this in JkInputStream,
        // which is similar with OutputBuffer - it should
        // take away the body processing
        if( contentLength == -1 ) {
            return jkIS.read();
	}
	if( available <= 0 ) {
            return -1;
        }
	available--;

        return jkIS.read();
    }
    
    public int doRead(byte[] b, int off, int len) throws IOException 
    {
        int rd=-1;
        if( contentLength == -1 ) {
	    rd=jkIS.read(b,off,len);
	    return rd;
	}
	if( available <= 0 ) {
	    return -1;
        }
	rd=jkIS.read( b,off, len );
	available -= rd;
	return rd;
    }
    
}
