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


package org.apache.coyote;

import java.io.IOException;
import java.util.Enumeration;
import java.util.Hashtable;

import org.apache.tomcat.util.buf.ByteChunk;
import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.buf.UDecoder;

import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.http.Parameters;
import org.apache.tomcat.util.http.ContentType;
import org.apache.tomcat.util.http.Cookies;

/**
 * This is a low-level, efficient representation of a server request. Most 
 * fields are GC-free, expensive operations are delayed until the  user code 
 * needs the information.
 *
 * Processing is delegated to modules, using a hook mechanism.
 * 
 * This class is not intended for user code - it is used internally by tomcat
 * for processing the request in the most efficient way. Users ( servlets ) can
 * access the information using a facade, which provides the high-level view
 * of the request.
 *
 * For lazy evaluation, the request uses the getInfo() hook. The following ids
 * are defined:
 * <ul>
 *  <li>req.encoding - returns the request encoding
 *  <li>req.attribute - returns a module-specific attribute ( like SSL keys, etc ).
 * </ul>
 *
 * Tomcat defines a number of attributes:
 * <ul>
 *   <li>"org.apache.tomcat.request" - allows access to the low-level
 *       request object in trusted applications 
 * </ul>
 *
 * @author James Duncan Davidson [duncan@eng.sun.com]
 * @author James Todd [gonzo@eng.sun.com]
 * @author Jason Hunter [jch@eng.sun.com]
 * @author Harish Prabandham
 * @author Alex Cruikshank [alex@epitonic.com]
 * @author Hans Bergsten [hans@gefionsoftware.com]
 * @author Costin Manolache
 * @author Remy Maucherat
 */
public final class Request {


    // ----------------------------------------------------------- Constructors


    public Request() {

	recycle();

        parameters.setQuery(queryMB);
        parameters.setURLDecoder(urlDecoder);
        parameters.setHeaders(headers);

    }


    // ----------------------------------------------------- Instance Variables


    private int serverPort = -1;
    private MessageBytes serverNameMB = new MessageBytes();

    private String localHost;

    private MessageBytes schemeMB = new MessageBytes();

    private MessageBytes methodMB = new MessageBytes();
    private MessageBytes unparsedURIMB = new MessageBytes();
    private MessageBytes uriMB = new MessageBytes();
    private MessageBytes decodedUriMB = new MessageBytes();
    private MessageBytes queryMB = new MessageBytes();
    private MessageBytes protoMB = new MessageBytes();

    // remote address/host
    private MessageBytes remoteAddrMB = new MessageBytes();
    private MessageBytes remoteHostMB = new MessageBytes();
    
    private MimeHeaders headers = new MimeHeaders();

    private MessageBytes instanceId = new MessageBytes();

    /**
     * Notes.
     */
    private Object notes[] = new Object[Constants.MAX_NOTES];


    /**
     * Associated input buffer.
     */
    private InputBuffer inputBuffer = null;


    /**
     * URL decoder.
     */
    private UDecoder urlDecoder = new UDecoder();


    /**
     * HTTP specific fields. (remove them ?)
     */
    private int contentLength = -1;
    // how much body we still have to read.
    private int available = -1; 
    private MessageBytes contentTypeMB = null;
    private String charEncoding = null;
    private Cookies cookies = new Cookies(headers);
    private Parameters parameters = new Parameters();

    private MessageBytes remoteUser=new MessageBytes();
    private MessageBytes authType=new MessageBytes();
    private Hashtable attributes=new Hashtable();

    private Response response;
    private ActionHook hook;
    // ------------------------------------------------------------- Properties


    /**
     * Get the instance id (or JVM route). Curently Ajp is sending it with each
     * request. In future this should be fixed, and sent only once ( or
     * 'negociated' at config time so both tomcat and apache share the same name.
     * 
     * @return the instance id
     */
    public MessageBytes instanceId() {
        return instanceId;
    }


    public MimeHeaders getMimeHeaders() {
        return headers;
    }


    public UDecoder getURLDecoder() {
        return urlDecoder;
    }

    // -------------------- Request data --------------------


    public MessageBytes scheme() {
        return schemeMB;
    }
    
    public MessageBytes method() {
        return methodMB;
    }
    
    public MessageBytes unparsedURI() {
        return unparsedURIMB;
    }

    public MessageBytes requestURI() {
        return uriMB;
    }

    public MessageBytes decodedURI() {
        return decodedUriMB;
    }

    public MessageBytes query() {
        return queryMB;
    }

    public MessageBytes queryString() {
        return queryMB;
    }

    public MessageBytes protocol() {
        return protoMB;
    }
    
    /** 
     * Return the buffer holding the server name, if
     * any. Use isNull() to check if there is no value
     * set.
     * This is the "virtual host", derived from the
     * Host: header.
     */
    public MessageBytes serverName() {
	return serverNameMB;
    }

    public int getServerPort() {
        return serverPort;
    }
    
    public void setServerPort(int serverPort ) {
	this.serverPort=serverPort;
    }

    public MessageBytes remoteAddr() {
	return remoteAddrMB;
    }

    public MessageBytes remoteHost() {
	return remoteHostMB;
    }

    public String getLocalHost() {
	return localHost;
    }

    public void setLocalHost(String host) {
	this.localHost = host;
    }


    // -------------------- encoding/type --------------------


    /**
     * Get the character encoding used for this request.
     */
    public String getCharacterEncoding() {

        if (charEncoding != null)
            return charEncoding;

        charEncoding = ContentType.getCharsetFromContentType(getContentType());
        return charEncoding;

    }


    public void setCharacterEncoding(String enc) {
	this.charEncoding = enc;
    }


    public void setContentLength(int len) {
	this.contentLength = len;
	available = len;
    }


    public int getContentLength() {
        if( contentLength > -1 ) return contentLength;

	MessageBytes clB = headers.getValue("content-length");
        contentLength = (clB == null || clB.isNull()) ? -1 : clB.getInt();
	available = contentLength;

	return contentLength;
    }


    public String getContentType() {
        contentType();
        if ((contentTypeMB == null) || contentTypeMB.isNull()) 
            return null;
        return contentTypeMB.toString();
    }


    public void setContentType(String type) {
        contentTypeMB.setString(type);
    }


    public MessageBytes contentType() {
        if (contentTypeMB == null)
            contentTypeMB = headers.getValue("content-type");
        return contentTypeMB;
    }


    public void setContentType(MessageBytes mb) {
        contentTypeMB=mb;
    }


    public String getHeader(String name) {
        return headers.getHeader(name);
    }

    // -------------------- Associated response --------------------

    public Response getResponse() {
        return response;
    }

    public void setResponse( Response response ) {
        this.response=response;
        response.setRequest( this );
    }
    
    public void action(ActionCode actionCode, Object param) {
        if( hook==null && response!=null )
            hook=response.getHook();
        
        if (hook != null) {
            if( param==null ) 
                hook.action(actionCode, this);
            else
                hook.action(actionCode, param);
        }
    }


    // -------------------- Cookies --------------------


    public Cookies getCookies() {
	return cookies;
    }


    // -------------------- Parameters --------------------


    public Parameters getParameters() {
	return parameters;
    }


    // -------------------- Other attributes --------------------
    // We can use notes for most - need to discuss what is of general interest
    
    public void setAttribute( String name, Object o ) {
        attributes.put( name, o );
    }

    public Hashtable getAttributes() {
        return attributes;
    }

    public Object getAttribute(String name ) {
        return attributes.get(name);
    }
    
    public MessageBytes getRemoteUser() {
        return remoteUser;
    }

    public MessageBytes getAuthType() {
        return authType;
    }

    // -------------------- Input Buffer --------------------


    public InputBuffer getInputBuffer() {
        return inputBuffer;
    }


    public void setInputBuffer(InputBuffer inputBuffer) {
        this.inputBuffer = inputBuffer;
    }


    /**
     * Read data from the input buffer and put it into a byte chunk.
     */
    public int doRead(ByteChunk chunk) 
        throws IOException {
        int n = inputBuffer.doRead(chunk);
        if (n > 0)
            available -= n;
        return n;
    }


    // -------------------- debug --------------------


    public String toString() {
	return "R( " + requestURI().toString() + ")";
    }


    // -------------------- Per-Request "notes" --------------------


    public final void setNote(int pos, Object value) {
	notes[pos] = value;
    }


    public final Object getNote(int pos) {
	return notes[pos];
    }


    // -------------------- Recycling -------------------- 


    public void recycle() {

	contentLength = -1;
        contentTypeMB = null;
        charEncoding = null;
        headers.recycle();
        serverNameMB.recycle();
        serverPort=-1;

	cookies.recycle();
        parameters.recycle();

        unparsedURIMB.recycle();
        uriMB.recycle();
        decodedUriMB.recycle();
	queryMB.recycle();
	methodMB.recycle();
	protoMB.recycle();
	//remoteAddrMB.recycle();
	//remoteHostMB.recycle();

	// XXX Do we need such defaults ?
        schemeMB.setString("http");
	methodMB.setString("GET");
        uriMB.setString("/");
        queryMB.setString("");
        protoMB.setString("HTTP/1.0");
        //remoteAddrMB.setString("127.0.0.1");
        //remoteHostMB.setString("localhost");

        instanceId.recycle();
        remoteUser.recycle();
        authType.recycle();
        attributes.clear();
    }


}
