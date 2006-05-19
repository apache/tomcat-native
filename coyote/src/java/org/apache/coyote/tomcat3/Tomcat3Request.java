/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

package org.apache.coyote.tomcat3;

import java.io.IOException;

import org.apache.coyote.ActionCode;
import org.apache.tomcat.util.buf.ByteChunk;
import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.core.BaseInterceptor;

/** The Request to connect with Coyote.
 *  This class handles the I/O requirements and transferring the request
 *  line and Mime headers between Coyote and Tomcat.
 * 
 *  @author Bill Barker
 *  @author Costin Manolache
 */
public class Tomcat3Request extends org.apache.tomcat.core.Request {

    org.apache.coyote.Request coyoteRequest=null;
    BaseInterceptor   connector = null;

    // For SSL attributes we need to call an ActionHook to get
    // info from the protocol handler.
    //    SSLSupport sslSupport=null;

    ByteChunk  readChunk = new ByteChunk(8096);
    int  pos=-1;
    int  end=-1;
    byte [] readBuffer = null;


    public Tomcat3Request() {
        super();
        remoteAddrMB.recycle();
        remoteHostMB.recycle();
    }

    public void recycle() {
	super.recycle();
	if( coyoteRequest != null) coyoteRequest.recycle();

        remoteAddrMB.recycle();
        remoteHostMB.recycle();
	readChunk.recycle();

	readBuffer=null;
	pos=-1;
	end=-1;
    }

    public org.apache.coyote.Request getCoyoteRequest() {
        return coyoteRequest;
    }
    
    /** Attach the Coyote Request to this Request.
     *  This is currently set pre-request to allow copying the request
     *  attributes to the Tomcat attributes.
     */
    public void setCoyoteRequest(org.apache.coyote.Request cReq) {
        coyoteRequest=cReq;

        // The CoyoteRequest/Tomcat3Request are bound togheter, they
        // don't change. That means we can use the same field ( which
        // doesn't change as well.
        schemeMB = coyoteRequest.scheme();
        methodMB = coyoteRequest.method();
        uriMB = coyoteRequest.requestURI();
        queryMB = coyoteRequest.query();
        protoMB = coyoteRequest.protocol();

	headers  = coyoteRequest.getMimeHeaders();
	scookies.setHeaders(headers);
	params.setHeaders(headers);
        params.setQuery( queryMB );
        
        remoteAddrMB = coyoteRequest.remoteAddr();
	remoteHostMB = coyoteRequest.remoteHost();
	serverNameMB = coyoteRequest.serverName();
    }

    /**
     * Set the Connector that this request services
     */
    void setConnector(BaseInterceptor conn) {
	connector = conn;
    }

    /**
     * Get the Connector that this request services
     */
    BaseInterceptor getConnector() {
	return connector;
    }
    
    /** Read a single character from the request body.
     */
    public int doRead() throws IOException {
	if( available == 0 ) 
	    return -1;
	// #3745
	// if available == -1: unknown length, we'll read until end of stream.
	if( available!= -1 )
	    available--;
	if(pos >= end) {
	    if(readBytes() < 0)
		return -1;
	}
	return readBuffer[pos++] & 0xFF;
    }

    /** Read a chunk from the request body.
     */
    public int doRead(byte[] b, int off, int len) throws IOException {
	if( available == 0 )
	    return -1;
	// if available == -1: unknown length, we'll read until end of stream.
	if(pos >= end) {
	    if(readBytes() <= 0) 
		return -1;
	}
	int rd = -1;
	if((end - pos) > len) {
	    rd = len;
	} else {
	    rd = end - pos;
	}

        System.arraycopy(readBuffer, pos, b, off, rd);
	pos += rd;
	if( available!= -1 )
	    available -= rd;

	return rd;
    }
    
    /**
     * Read bytes to the read chunk buffer.
     */
    protected int readBytes()
        throws IOException {

        int result = coyoteRequest.doRead(readChunk);
        if (result > 0) {
            readBuffer = readChunk.getBytes();
            end = readChunk.getEnd();
            pos = readChunk.getStart();
        } else if( result < 0 ) {
            throw new IOException( "Read bytes failed " + result );
        }
        return result;

    }

    // -------------------- override special methods

    public MessageBytes remoteAddr() {
	if( remoteAddrMB.isNull() ) {
	    coyoteRequest.action( ActionCode.ACTION_REQ_HOST_ADDR_ATTRIBUTE, coyoteRequest );
	}
	return remoteAddrMB;
    }

    public MessageBytes remoteHost() {
	if( remoteHostMB.isNull() ) {
	    coyoteRequest.action( ActionCode.ACTION_REQ_HOST_ATTRIBUTE, coyoteRequest );
	}
	return remoteHostMB;
    }

    public String getLocalHost() {
	return localHost;
    }

    public MessageBytes serverName(){
        // That's set by protocol in advance, it's needed for mapping anyway,
        // no need to do lazy eval.
        return coyoteRequest.serverName();
    }

    public int getServerPort(){
        return coyoteRequest.getServerPort();
    }
    
    public void setServerPort(int i ) {
	coyoteRequest.setServerPort( i );
    }


    public  void setRemoteUser( String s ) {
	super.setRemoteUser(s);
	coyoteRequest.getRemoteUser().setString(s);
    }

    public String getRemoteUser() {
	String s=coyoteRequest.getRemoteUser().toString();
	if( s == null )
	    s=super.getRemoteUser();
	return s;
    }

    public String getAuthType() {
	return coyoteRequest.getAuthType().toString();
    }
    
    public void setAuthType(String s ) {
	coyoteRequest.getAuthType().setString(s);
    }

    public String getJvmRoute() {
	return coyoteRequest.instanceId().toString();
    }
    
    public void setJvmRoute(String s ) {
	coyoteRequest.instanceId().setString(s);
    }

    public boolean isSecure() {
	return "https".equalsIgnoreCase( coyoteRequest.scheme().toString());
    }
}
