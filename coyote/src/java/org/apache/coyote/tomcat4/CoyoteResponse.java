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


package org.apache.coyote.tomcat4;


import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Locale;
import java.util.Map;

import javax.servlet.ServletContext;
import javax.servlet.ServletException;
import javax.servlet.ServletOutputStream;
import javax.servlet.ServletResponse;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletResponse;

import org.apache.coyote.Response;

import org.apache.catalina.Connector;
import org.apache.catalina.Context;
import org.apache.catalina.Globals;
import org.apache.catalina.HttpResponse;
import org.apache.catalina.Manager;
import org.apache.catalina.Realm;
import org.apache.catalina.Session;
import org.apache.catalina.Wrapper;
import org.apache.catalina.connector.HttpResponseFacade;
import org.apache.catalina.util.CharsetMapper;
import org.apache.catalina.util.RequestUtil;
import org.apache.catalina.util.StringManager;


/**
 * Wrapper object for the Coyote response.
 *
 * @author Remy Maucherat
 * @version $Revision$ $Date$
 */

public class CoyoteResponse
    implements HttpResponse, HttpServletResponse {


    // ----------------------------------------------------- Instance Variables


    /**
     * The date format we will use for creating date headers.
     */
    protected final SimpleDateFormat format =
        new SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss zzz",Locale.US);


    /**
     * Descriptive information about this Response implementation.
     */
    protected static final String info =
        "org.apache.coyote.tomcat4.CoyoteResponse/1.0";


    /**
     * The string manager for this package.
     */
    protected static StringManager sm =
        StringManager.getManager(Constants.Package);


    // ------------------------------------------------------------- Properties


    /**
     * Associated Catalina connector.
     */
    protected Connector connector;

    /**
     * Return the Connector through which this Request was received.
     */
    public Connector getConnector() {
        return (this.connector);
    }

    /**
     * Set the Connector through which this Request was received.
     *
     * @param connector The new connector
     */
    public void setConnector(Connector connector) {
        this.connector = connector;
    }


    /**
     * Coyote response.
     */
    protected Response coyoteResponse;

    /**
     * Set the Coyote response.
     * 
     * @param response The Coyote response
     */
    public void setCoyoteResponse(Response response) {
        this.coyoteResponse = coyoteResponse;
    }

    /**
     * Get the Coyote response.
     */
    public Response getCoyoteResponse() {
        return (coyoteResponse);
    }


    /**
     * The Context within which this Request is being processed.
     */
    protected Context context = null;

    /**
     * Return the Context within which this Request is being processed.
     */
    public Context getContext() {
        return (this.context);
    }

    /**
     * Set the Context within which this Request is being processed.  This
     * must be called as soon as the appropriate Context is identified, because
     * it identifies the value to be returned by <code>getContextPath()</code>,
     * and thus enables parsing of the request URI.
     *
     * @param context The newly associated Context
     */
    public void setContext(Context context) {
        this.context = context;
    }


    // --------------------------------------------------------- Public Methods


    /**
     * Release all object references, and initialize instance variables, in
     * preparation for reuse of this object.
     */
    public void recycle() {
    }


    // ------------------------------------------------------- Response Methods


    /**
     * Return the number of bytes actually written to the output stream.
     */
    public int getContentCount() {
        return 0;
    }


    /**
     * Set the application commit flag.
     * 
     * @param appCommitted The new application committed flag value
     */
    public void setAppCommitted(boolean appCommitted) {
    }


    /**
     * Application commit flag accessor.
     */
    public boolean isAppCommitted() {
        return false;
    }


    /**
     * Return the "processing inside an include" flag.
     */
    public boolean getIncluded() {
        return false;
    }


    /**
     * Set the "processing inside an include" flag.
     *
     * @param included <code>true</code> if we are currently inside a
     *  RequestDispatcher.include(), else <code>false</code>
     */
    public void setIncluded(boolean included) {
    }


    /**
     * Return descriptive information about this Response implementation and
     * the corresponding version number, in the format
     * <code>&lt;description&gt;/&lt;version&gt;</code>.
     */
    public String getInfo() {
        return (info);
    }


    /**
     * The request with which this response is associated.
     */
    protected org.apache.catalina.Request request = null;

    /**
     * Return the Request with which this Response is associated.
     */
    public org.apache.catalina.Request getRequest() {
        return (this.request);
    }

    /**
     * Set the Request with which this Response is associated.
     *
     * @param request The new associated request
     */
    public void setRequest(org.apache.catalina.Request request) {
        this.request = request;
    }


    /**
     * The facade associated with this response.
     */
    protected HttpResponseFacade facade = new HttpResponseFacade(this);

    /**
     * Return the <code>ServletResponse</code> for which this object
     * is the facade.
     */
    public ServletResponse getResponse() {
        return (facade);
    }


    /**
     * Return the output stream associated with this Response.
     */
    public OutputStream getStream() {
        return null;
    }


    /**
     * Set the output stream associated with this Response.
     *
     * @param stream The new output stream
     */
    public void setStream(OutputStream stream) {
    }


    /**
     * Set the suspended flag.
     * 
     * @param suspended The new suspended flag value
     */
    public void setSuspended(boolean suspended) {
    }


    /**
     * Suspended flag accessor.
     */
    public boolean isSuspended() {
        return false;
    }


    /**
     * Set the error flag.
     */
    public void setError() {
    }


    /**
     * Error flag accessor.
     */
    public boolean isError() {
        return false;
    }


    /**
     * Create and return a ServletOutputStream to write the content
     * associated with this Response.
     *
     * @exception IOException if an input/output error occurs
     */
    public ServletOutputStream createOutputStream() 
        throws IOException {
        return null;
    }


    /**
     * Perform whatever actions are required to flush and close the output
     * stream or writer, in a single operation.
     *
     * @exception IOException if an input/output error occurs
     */
    public void finishResponse() 
        throws IOException {
    }


    /**
     * Return the content length that was set or calculated for this Response.
     */
    public int getContentLength() {
        return (coyoteResponse.getContentLength());
    }


    /**
     * Return the content type that was set or calculated for this response,
     * or <code>null</code> if no content type was set.
     */
    public String getContentType() {
        return (coyoteResponse.getContentType());
    }


    /**
     * Return a PrintWriter that can be used to render error messages,
     * regardless of whether a stream or writer has already been acquired.
     *
     * @return Writer which can be used for error reports. If the response is
     * not an error report returned using sendError or triggered by an
     * unexpected exception thrown during the servlet processing
     * (and only in that case), null will be returned if the response stream
     * has already been used.
     */
    public PrintWriter getReporter() {
        return null;
    }


    // ------------------------------------------------ ServletResponse Methods


    /**
     * Flush the buffer and commit this response.
     *
     * @exception IOException if an input/output error occurs
     */
    public void flushBuffer() 
        throws IOException {
    }


    /**
     * Return the actual buffer size used for this Response.
     */
    public int getBufferSize() {
        return 0;
    }


    /**
     * Return the character encoding used for this Response.
     */
    public String getCharacterEncoding() {
        return (coyoteResponse.getCharacterEncoding());
    }


    /**
     * Return the servlet output stream associated with this Response.
     *
     * @exception IllegalStateException if <code>getWriter</code> has
     *  already been called for this response
     * @exception IOException if an input/output error occurs
     */
    public ServletOutputStream getOutputStream() 
        throws IOException {
        return null;
    }


    /**
     * Return the Locale assigned to this response.
     */
    public Locale getLocale() {
        return (coyoteResponse.getLocale());
    }


    /**
     * Return the writer associated with this Response.
     *
     * @exception IllegalStateException if <code>getOutputStream</code> has
     *  already been called for this response
     * @exception IOException if an input/output error occurs
     */
    public PrintWriter getWriter() 
        throws IOException {
        return null;
    }


    /**
     * Has the output of this response already been committed?
     */
    public boolean isCommitted() {
        return false;
    }


    /**
     * Clear any content written to the buffer.
     *
     * @exception IllegalStateException if this response has already
     *  been committed
     */
    public void reset() {
    }


    /**
     * Reset the data buffer but not any status or header information.
     *
     * @exception IllegalStateException if the response has already
     *  been committed
     */
    public void resetBuffer() {
    }


    /**
     * Set the buffer size to be used for this Response.
     *
     * @param size The new buffer size
     *
     * @exception IllegalStateException if this method is called after
     *  output has been committed for this response
     */
    public void setBufferSize(int size) {
    }


    /**
     * Set the content length (in bytes) for this Response.
     *
     * @param length The new content length
     */
    public void setContentLength(int length) {
        
    }


    /**
     * Set the content type for this Response.
     *
     * @param type The new content type
     */
    public void setContentType(String type) {
    }


    /**
     * Set the Locale that is appropriate for this response, including
     * setting the appropriate character encoding.
     *
     * @param locale The new locale
     */
    public void setLocale(Locale locale) {
    }


    // --------------------------------------------------- HttpResponse Methods


    /**
     * Return an array of all cookies set for this response, or
     * a zero-length array if no cookies have been set.
     */
    public Cookie[] getCookies() {
        return null;
    }


    /**
     * Return the value for the specified header, or <code>null</code> if this
     * header has not been set.  If more than one value was added for this
     * name, only the first is returned; use getHeaderValues() to retrieve all
     * of them.
     *
     * @param name Header name to look up
     */
    public String getHeader(String name) {
        return null;
    }


    /**
     * Return an array of all the header names set for this response, or
     * a zero-length array if no headers have been set.
     */
    public String[] getHeaderNames() {
        return null;
    }


    /**
     * Return an array of all the header values associated with the
     * specified header name, or an zero-length array if there are no such
     * header values.
     *
     * @param name Header name to look up
     */
    public String[] getHeaderValues(String name) {
        return null;
    }


    /**
     * Return the error message that was set with <code>sendError()</code>
     * for this Response.
     */
    public String getMessage() {
        return null;
    }


    /**
     * Return the HTTP status code associated with this Response.
     */
    public int getStatus() {
        return 0;
    }


    /**
     * Reset this response, and specify the values for the HTTP status code
     * and corresponding message.
     *
     * @exception IllegalStateException if this response has already been
     *  committed
     */
    public void reset(int status, String message) {
    }


    // -------------------------------------------- HttpServletResponse Methods


    /**
     * Add the specified Cookie to those that will be included with
     * this Response.
     *
     * @param cookie Cookie to be added
     */
    public void addCookie(Cookie cookie) {
    }


    /**
     * Add the specified date header to the specified value.
     *
     * @param name Name of the header to set
     * @param value Date value to be set
     */
    public void addDateHeader(String name, long value) {
    }


    /**
     * Add the specified header to the specified value.
     *
     * @param name Name of the header to set
     * @param value Value to be set
     */
    public void addHeader(String name, String value) {
    }


    /**
     * Add the specified integer header to the specified value.
     *
     * @param name Name of the header to set
     * @param value Integer value to be set
     */
    public void addIntHeader(String name, int value) {
    }


    /**
     * Has the specified header been set already in this response?
     *
     * @param name Name of the header to check
     */
    public boolean containsHeader(String name) {
        return false;
    }


    /**
     * Encode the session identifier associated with this response
     * into the specified redirect URL, if necessary.
     *
     * @param url URL to be encoded
     */
    public String encodeRedirectURL(String url) {
        return null;
    }


    /**
     * Encode the session identifier associated with this response
     * into the specified redirect URL, if necessary.
     *
     * @param url URL to be encoded
     *
     * @deprecated As of Version 2.1 of the Java Servlet API, use
     *  <code>encodeRedirectURL()</code> instead.
     */
    public String encodeRedirectUrl(String url) {
        return (encodeRedirectURL(url));
    }


    /**
     * Encode the session identifier associated with this response
     * into the specified URL, if necessary.
     *
     * @param url URL to be encoded
     */
    public String encodeURL(String url) {
        return null;
    }


    /**
     * Encode the session identifier associated with this response
     * into the specified URL, if necessary.
     *
     * @param url URL to be encoded
     *
     * @deprecated As of Version 2.1 of the Java Servlet API, use
     *  <code>encodeURL()</code> instead.
     */
    public String encodeUrl(String url) {
        return (encodeURL(url));
    }


    /**
     * Send an error response with the specified status and a
     * default message.
     *
     * @param status HTTP status code to send
     *
     * @exception IllegalStateException if this response has
     *  already been committed
     * @exception IOException if an input/output error occurs
     */
    public void sendError(int status) 
        throws IOException {
    }


    /**
     * Send an error response with the specified status and message.
     *
     * @param status HTTP status code to send
     * @param message Corresponding message to send
     *
     * @exception IllegalStateException if this response has
     *  already been committed
     * @exception IOException if an input/output error occurs
     */
    public void sendError(int status, String message) 
        throws IOException {
    }


    /**
     * Send a temporary redirect to the specified redirect location URL.
     *
     * @param location Location URL to redirect to
     *
     * @exception IllegalStateException if this response has
     *  already been committed
     * @exception IOException if an input/output error occurs
     */
    public void sendRedirect(String location) 
        throws IOException {
    }


    /**
     * Set the specified date header to the specified value.
     *
     * @param name Name of the header to set
     * @param value Date value to be set
     */
    public void setDateHeader(String name, long value) {
    }


    /**
     * Set the specified header to the specified value.
     *
     * @param name Name of the header to set
     * @param value Value to be set
     */
    public void setHeader(String name, String value) {
    }


    /**
     * Set the specified integer header to the specified value.
     *
     * @param name Name of the header to set
     * @param value Integer value to be set
     */
    public void setIntHeader(String name, int value) {
    }


    /**
     * Set the HTTP status to be returned with this response.
     *
     * @param status The new HTTP status
     */
    public void setStatus(int status) {
    }


    /**
     * Set the HTTP status and message to be returned with this response.
     *
     * @param status The new HTTP status
     * @param message The associated text message
     *
     * @deprecated As of Version 2.1 of the Java Servlet API, this method
     *  has been deprecated due to the ambiguous meaning of the message
     *  parameter.
     */
    public void setStatus(int status, String message) {
    }


}
