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
package org.apache.ajp;

import java.io.PrintWriter;
import java.io.StringWriter;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.Iterator;

import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.http.Cookies;

/**
 * Encapsulates an ajp request.
 *
 * When an ajp request is handled, it is decoded into an
 * <code>AjpRequest</code> that can then be used by an adaptor
 * and/or connector to process the request.
 */
public class AjpRequest {

    // scheme constants
    public static final String SCHEME_HTTP = "http";
    public static final String SCHEME_HTTPS = "https";

    // an empty iterator
    private final static Iterator emptyItr = new LinkedList().iterator();

    // request attributes
    MessageBytes method = new MessageBytes();
    MessageBytes protocol = new MessageBytes();
    MessageBytes requestURI = new MessageBytes();
    MessageBytes remoteAddr = new MessageBytes();
    MessageBytes remoteHost = new MessageBytes();
    MessageBytes serverName = new MessageBytes();
    int serverPort = 80;
    MessageBytes remoteUser = new MessageBytes();
    MessageBytes authType = new MessageBytes();
    MessageBytes queryString = new MessageBytes();
    MessageBytes jvmRoute = new MessageBytes();
    String scheme = SCHEME_HTTP;
    boolean secure = false;
    int contentLength = 0;
    MessageBytes contentType = new MessageBytes();
    MimeHeaders headers = new MimeHeaders();
    Cookies cookies = new Cookies();
    HashMap attributes = new HashMap();
    
    /**
     * Recycles this object and readies it further use.
     */
    public void recycle() {
        method.recycle();
        protocol.recycle();
        requestURI.recycle();
        remoteAddr.recycle();
        remoteHost.recycle();
        serverName.recycle();
        serverPort = 80;
        remoteUser.recycle();
        authType.recycle();
        queryString.recycle();
        jvmRoute.recycle();
        scheme = SCHEME_HTTP;
        secure = false;
        contentLength = 0;
        contentType.recycle();
        headers.recycle();
        cookies.recycle();
        attributes.clear();
    }

    /**
     * Get the method.
     * @return the method
     */
    public MessageBytes getMethod() {
        return method;
    }

    /**
     * Get the protocol
     * @return the protocol
     */
    public MessageBytes getProtocol() {
        return protocol;
    }

    /**
     * Get the request uri
     * @return the request uri
     */
    public MessageBytes getRequestURI() {
        return requestURI;
    }

    /**
     * Get the remote address
     * @return the remote address
     */
    public MessageBytes getRemoteAddr() {
        return remoteAddr;
    }

    /**
     * Get the remote host
     * @return the remote host
     */
    public MessageBytes getRemoteHost() {
        return remoteHost;
    }

    /**
     * Get the server name
     * @return the server name
     */
    public MessageBytes getServerName() {
        return serverName;
    }

    /**
     * Get the server port
     * @return the server port
     */
    public int getServerPort() {
        return serverPort;
    }

    /**
     * Get the remote user
     * @return the remote user
     */
    public MessageBytes getRemoteUser() {
        return remoteUser;
    }

    /**
     * Get the auth type
     * @return the auth type
     */
    public MessageBytes getAuthType() {
        return authType;
    }

    /**
     * Get the query string
     * @return the query string
     */
    public MessageBytes getQueryString() {
        return queryString;
    }

    /**
     * Get the jvm route
     * @return the jvm route
     */
    public MessageBytes getJvmRoute() {
        return jvmRoute;
    }

    /**
     * Get the scheme
     * @return the scheme
     */
    public String getScheme() {
        return scheme;
    }

    /**
     * Get whether the request is secure or not.
     * @return <code>true</code> if the request is secure.
     */
    public boolean getSecure() {
        return secure;
    }

    /**
     * Get the content length
     * @return the content length
     */
    public int getContentLength() {
        return contentLength;
    }

    /**
     * Get the content type
     * @return the content type
     */
    public MessageBytes getContentType() {
        return contentType;
    }

    /**
     * Get this request's headers
     * @return request headers
     */
    public MimeHeaders getHeaders() {
        return headers;
    }

    /**
     * Get cookies.
     * @return request cookies.
     */
    public Cookies getCookies() {
        return cookies;
    }

    /**
     * Set an attribute on the request
     * @param name attribute name
     * @param value attribute value
     */
    public void setAttribute(String name, Object value) {
        if (name == null || value == null) {
            return;
        }
        attributes.put(name, value);
    }

    /**
     * Get an attribute on the request
     * @param name attribute name
     * @return attribute value
     */
    public Object getAttribute(String name) {
        if (name == null) {
            return null;
        }

        return attributes.get(name);
    }

    /**
     * Get iterator over attribute names
     * @return iterator over attribute names
     */
    public Iterator getAttributeNames() {
        return attributes.keySet().iterator();
    }

    /**
     * ** SLOW ** for debugging only!
     */
    public String toString() {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);

        pw.println("=== AjpRequest ===");
        pw.println("method          = " + method.toString());
        pw.println("protocol        = " + protocol.toString());
        pw.println("requestURI      = " + requestURI.toString());
        pw.println("remoteAddr      = " + remoteAddr.toString());
        pw.println("remoteHost      = " + remoteHost.toString());
        pw.println("serverName      = " + serverName.toString());
        pw.println("serverPort      = " + serverPort);
        pw.println("remoteUser      = " + remoteUser.toString());
        pw.println("authType        = " + authType.toString());
        pw.println("queryString     = " + queryString.toString());
        pw.println("jvmRoute        = " + jvmRoute.toString());
        pw.println("scheme          = " + scheme.toString());
        pw.println("secure          = " + secure);
        pw.println("contentLength   = " + contentLength);
        pw.println("contentType     = " + contentType);
        pw.println("attributes      = " + attributes.toString());
        pw.println("headers         = " + headers.toString());
        pw.println("cookies         = " + cookies.toString());
        return sw.toString();
    }
}
