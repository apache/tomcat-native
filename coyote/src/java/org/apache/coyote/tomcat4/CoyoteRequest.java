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


import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.BufferedReader;
import java.io.UnsupportedEncodingException;
import java.net.Socket;
import java.security.Principal;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Locale;
import java.util.Map;

import javax.servlet.RequestDispatcher;
import javax.servlet.ServletContext;
import javax.servlet.ServletException;
import javax.servlet.ServletInputStream;
import javax.servlet.ServletRequest;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpSession;

import org.apache.tomcat.util.http.Parameters;

import org.apache.coyote.Request;

import org.apache.catalina.Connector;
import org.apache.catalina.Context;
import org.apache.catalina.Globals;
import org.apache.catalina.HttpRequest;
import org.apache.catalina.Manager;
import org.apache.catalina.Realm;
import org.apache.catalina.Session;
import org.apache.catalina.Wrapper;

import org.apache.catalina.connector.HttpRequestFacade;

import org.apache.catalina.util.Enumerator;
import org.apache.catalina.util.ParameterMap;
import org.apache.catalina.util.RequestUtil;
import org.apache.catalina.util.StringManager;


/**
 * Wrapper object for the Coyote request.
 *
 * @author Remy Maucherat
 * @author Craig R. McClanahan
 * @version $Revision$ $Date$
 */

public class CoyoteRequest
    implements HttpRequest, HttpServletRequest {


    // ------------------------------------------------------------- Properties


    /**
     * Coyote request.
     */
    protected Request coyoteRequest;

    /**
     * Set the Coyote request.
     * 
     * @param coyoteRequest The Coyote request
     */
    public void setCoyoteRequest(Request coyoteRequest) {
        this.coyoteRequest = coyoteRequest;
        inputStream.setRequest(coyoteRequest);
    }

    /**
     * Get the Coyote request.
     */
    public Request getCoyoteRequest() {
        return (this.coyoteRequest);
    }


    // ----------------------------------------------------- Instance Variables


    /**
     * The string manager for this package.
     */
    protected static StringManager sm =
        StringManager.getManager(Constants.Package);


    /**
     * The set of cookies associated with this Request.
     */
    protected ArrayList cookies = new ArrayList();


    /**
     * The set of SimpleDateFormat formats to use in getDateHeader().
     */
    protected SimpleDateFormat formats[] = {
        new SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss zzz", Locale.US),
        new SimpleDateFormat("EEEEEE, dd-MMM-yy HH:mm:ss zzz", Locale.US),
        new SimpleDateFormat("EEE MMMM d HH:mm:ss yyyy", Locale.US)
    };


    /**
     * The default Locale if none are specified.
     */
    protected static Locale defaultLocale = Locale.getDefault();


    /**
     * The attributes associated with this Request, keyed by attribute name.
     */
    protected HashMap attributes = new HashMap();


    /**
     * The preferred Locales assocaited with this Request.
     */
    protected ArrayList locales = new ArrayList();


    /**
     * Internal notes associated with this request by Catalina components
     * and event listeners.
     */
    private transient HashMap notes = new HashMap();


    /**
     * Authentication type.
     */
    protected String authType = null;


    /**
     * Reader.
     * Note: At the moment, no attempt is being made at recycling the reader,
     * but this could be implemented in the future, using a design like the one
     * used for the output buffer.
     */
    protected BufferedReader reader = null;


    /**
     * ServletInputStream.
     */
    protected CoyoteInputStream inputStream = new CoyoteInputStream();


    /**
     * Using stream flag.
     */
    protected boolean usingInputStream = false;


    /**
     * Using writer flag.
     */
    protected boolean usingReader = false;


    /**
     * Context path.
     */
    protected String contextPath = "";


    /**
     * Path info.
     */
    protected String pathInfo = null;


    /**
     * Servlet path.
     */
    protected String servletPath = null;


    /**
     * Query string.
     */
    protected String queryString = null;


    /**
     * User principal.
     */
    protected Principal userPrincipal = null;


    /**
     * Session parsed flag.
     */
    protected boolean sessionParsed = false;


    /**
     * Request parameters parsed flag.
     */
    protected boolean requestParametersParsed = false;


    /**
     * Authorization parsed flag.
     */
    protected boolean authorizationParsed = false;


    /**
     * Secure flag.
     */
    protected boolean secure = false;


    /**
     * Post data buffer.
     */
    protected static int CACHED_POST_LEN = 8192;
    protected byte[] postData = null;


    /**
     * Hash map used in the getParametersMap method.
     */
    protected ParameterMap parameterMap = new ParameterMap();


    // --------------------------------------------------------- Public Methods


    /**
     * Release all object references, and initialize instance variables, in
     * preparation for reuse of this object.
     */
    public void recycle() {

        context = null;
        wrapper = null;

        authType = null;
        usingInputStream = false;
        usingReader = false;
        contextPath = "";
        pathInfo = null;
        servletPath = null;
        queryString = null;
        reader = null;
        userPrincipal = null;
        sessionParsed = false;
        requestParametersParsed = false;
        authorizationParsed = false;
        secure = false;

        attributes.clear();
        notes.clear();
        cookies.clear();

        parameterMap.setLocked(false);
        parameterMap.clear();

    }


    // -------------------------------------------------------- Request Methods


    /**
     * The authorization credentials sent with this Request.
     */
    protected String authorization = null;

    /**
     * Return the authorization credentials sent with this request.
     */
    public String getAuthorization() {

        if (!authorizationParsed) {
            setAuthorization(coyoteRequest.getHeader
                             (Constants.AUTHORIZATION_HEADER));
            authorizationParsed = true;
        }

        return (this.authorization);

    }

    /**
     * Set the authorization credentials sent with this request.
     *
     * @param authorization The new authorization credentials
     */
    public void setAuthorization(String authorization) {
        this.authorization = authorization;
    }


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


    /**
     * Descriptive information about this Request implementation.
     */
    protected static final String info =
        "org.apache.coyote.catalina.CoyoteRequest/1.0";

    /**
     * Return descriptive information about this Request implementation and
     * the corresponding version number, in the format
     * <code>&lt;description&gt;/&lt;version&gt;</code>.
     */
    public String getInfo() {
        return (info);
    }


    /**
     * The facade associated with this request.
     */
    protected HttpRequestFacade facade = new HttpRequestFacade(this);

    /**
     * Return the <code>ServletRequest</code> for which this object
     * is the facade.  This method must be implemented by a subclass.
     */
    public ServletRequest getRequest() {
        return (facade);
    }


    /**
     * The response with which this request is associated.
     */
    protected org.apache.catalina.Response response = null;

    /**
     * Return the Response with which this Request is associated.
     */
    public org.apache.catalina.Response getResponse() {
        return (this.response);
    }

    /**
     * Set the Response with which this Request is associated.
     *
     * @param response The new associated response
     */
    public void setResponse(org.apache.catalina.Response response) {
        this.response = response;
    }


    /**
     * Return the Socket (if any) through which this Request was received.
     * This should <strong>only</strong> be used to access underlying state
     * information about this Socket, such as the SSLSession associated with
     * an SSLSocket.
     */
    public Socket getSocket() {
        return (null); // FIXME: Return a note
    }

    /**
     * Set the Socket (if any) through which this Request was received.
     *
     * @param socket The socket through which this request was received
     */
    public void setSocket(Socket socket) {
    }


    /**
     * Return the input stream associated with this Request.
     */
    public InputStream getStream() {
        return inputStream;
    }

    /**
     * Set the input stream associated with this Request.
     *
     * @param stream The new input stream
     */
    public void setStream(InputStream stream) {
        // Ignore
    }


    /**
     * The Wrapper within which this Request is being processed.
     */
    protected Wrapper wrapper = null;

    /**
     * Return the Wrapper within which this Request is being processed.
     */
    public Wrapper getWrapper() {
        return (this.wrapper);
    }

    /**
     * Set the Wrapper within which this Request is being processed.  This
     * must be called as soon as the appropriate Wrapper is identified, and
     * before the Request is ultimately passed to an application servlet.
     *
     * @param wrapper The newly associated Wrapper
     */
    public void setWrapper(Wrapper wrapper) {
        this.wrapper = wrapper;
    }


    // ------------------------------------------------- Request Public Methods


    /**
     * Create and return a ServletInputStream to read the content
     * associated with this Request.
     *
     * @exception IOException if an input/output error occurs
     */
    public ServletInputStream createInputStream() 
        throws IOException {
        return inputStream;
    }


    /**
     * Perform whatever actions are required to flush and close the input
     * stream or reader, in a single operation.
     *
     * @exception IOException if an input/output error occurs
     */
    public void finishRequest() throws IOException {
        // The reader and input stream don't need to be closed
    }


    /**
     * Return the object bound with the specified name to the internal notes
     * for this request, or <code>null</code> if no such binding exists.
     *
     * @param name Name of the note to be returned
     */
    public Object getNote(String name) {
        return (notes.get(name));
    }


    /**
     * Return an Iterator containing the String names of all notes bindings
     * that exist for this request.
     */
    public Iterator getNoteNames() {
        return (notes.keySet().iterator());
    }


    /**
     * Remove any object bound to the specified name in the internal notes
     * for this request.
     *
     * @param name Name of the note to be removed
     */
    public void removeNote(String name) {
        notes.remove(name);
    }


    /**
     * Bind an object to a specified name in the internal notes associated
     * with this request, replacing any existing binding for this name.
     *
     * @param name Name to which the object should be bound
     * @param value Object to be bound to the specified name
     */
    public void setNote(String name, Object value) {
        notes.put(name, value);
    }


    /**
     * Set the content length associated with this Request.
     *
     * @param length The new content length
     */
    public void setContentLength(int length) {
        // Not used
    }


    /**
     * Set the content type (and optionally the character encoding)
     * associated with this Request.  For example,
     * <code>text/html; charset=ISO-8859-4</code>.
     *
     * @param type The new content type
     */
    public void setContentType(String type) {
        // Not used
    }


    /**
     * Set the protocol name and version associated with this Request.
     *
     * @param protocol Protocol name and version
     */
    public void setProtocol(String protocol) {
        // Not used
    }


    /**
     * Set the IP address of the remote client associated with this Request.
     *
     * @param remoteAddr The remote IP address
     */
    public void setRemoteAddr(String remoteAddr) {
        // Not used
    }


    /**
     * Set the fully qualified name of the remote client associated with this
     * Request.
     *
     * @param remoteHost The remote host name
     */
    public void setRemoteHost(String remoteHost) {
        // Not used
    }


    /**
     * Set the name of the scheme associated with this request.  Typical values
     * are <code>http</code>, <code>https</code>, and <code>ftp</code>.
     *
     * @param scheme The scheme
     */
    public void setScheme(String scheme) {
        // Not used
    }


    /**
     * Set the value to be returned by <code>isSecure()</code>
     * for this Request.
     *
     * @param secure The new isSecure value
     */
    public void setSecure(boolean secure) {
        this.secure = secure;
    }


    /**
     * Set the name of the server (virtual host) to process this request.
     *
     * @param name The server name
     */
    public void setServerName(String name) {
        coyoteRequest.serverName().setString(name);
    }


    /**
     * Set the port number of the server to process this request.
     *
     * @param port The server port
     */
    public void setServerPort(int port) {
        coyoteRequest.setServerPort(port);
    }


    // ------------------------------------------------- ServletRequest Methods


    /**
     * Return the specified request attribute if it exists; otherwise, return
     * <code>null</code>.
     *
     * @param name Name of the request attribute to return
     */
    public Object getAttribute(String name) {
        return (attributes.get(name));
    }


    /**
     * Return the names of all request attributes for this Request, or an
     * empty <code>Enumeration</code> if there are none.
     */
    public Enumeration getAttributeNames() {
        return (new Enumerator(attributes.keySet()));
    }


    /**
     * Return the character encoding for this Request.
     */
    public String getCharacterEncoding() {
      return (coyoteRequest.getCharacterEncoding());
    }


    /**
     * Return the content length for this Request.
     */
    public int getContentLength() {
        return (coyoteRequest.getContentLength());
    }


    /**
     * Return the content type for this Request.
     */
    public String getContentType() {
        return (coyoteRequest.getContentType());
    }


    /**
     * Return the servlet input stream for this Request.  The default
     * implementation returns a servlet input stream created by
     * <code>createInputStream()</code>.
     *
     * @exception IllegalStateException if <code>getReader()</code> has
     *  already been called for this request
     * @exception IOException if an input/output error occurs
     */
    public ServletInputStream getInputStream() throws IOException {

        if (usingReader)
            throw new IllegalStateException
                (sm.getString("requestBase.getInputStream.ise"));

        usingInputStream = true;
        return inputStream;

    }


    /**
     * Return the preferred Locale that the client will accept content in,
     * based on the value for the first <code>Accept-Language</code> header
     * that was encountered.  If the request did not specify a preferred
     * language, the server's default Locale is returned.
     */
    public Locale getLocale() {

        if (locales.size() > 0) {
            return ((Locale) locales.get(0));
        } else {
            return (defaultLocale);
        }

    }


    /**
     * Return the set of preferred Locales that the client will accept
     * content in, based on the values for any <code>Accept-Language</code>
     * headers that were encountered.  If the request did not specify a
     * preferred language, the server's default Locale is returned.
     */
    public Enumeration getLocales() {

        if (locales.size() > 0)
            return (new Enumerator(locales));
        ArrayList results = new ArrayList();
        results.add(defaultLocale);
        return (new Enumerator(results));

    }


    /**
     * Return the value of the specified request parameter, if any; otherwise,
     * return <code>null</code>.  If there is more than one value defined,
     * return only the first one.
     *
     * @param name Name of the desired request parameter
     */
    public String getParameter(String name) {

        if (!requestParametersParsed)
            parseRequestParameters();

        return coyoteRequest.getParameters().getParameter(name);

    }



    /**
     * Returns a <code>Map</code> of the parameters of this request.
     * Request parameters are extra information sent with the request.
     * For HTTP servlets, parameters are contained in the query string
     * or posted form data.
     *
     * @return A <code>Map</code> containing parameter names as keys
     *  and parameter values as map values.
     */
    public Map getParameterMap() {

        if (parameterMap.isLocked())
            return parameterMap;

        Enumeration enum = getParameterNames();
        while (enum.hasMoreElements()) {
            String name = enum.nextElement().toString();
            String[] values = getParameterValues(name);
            parameterMap.put(name, values);
        }

        parameterMap.setLocked(true);

        return parameterMap;

    }


    /**
     * Return the names of all defined request parameters for this request.
     */
    public Enumeration getParameterNames() {

        if (!requestParametersParsed)
            parseRequestParameters();

        return coyoteRequest.getParameters().getParameterNames();

    }


    /**
     * Return the defined values for the specified request parameter, if any;
     * otherwise, return <code>null</code>.
     *
     * @param name Name of the desired request parameter
     */
    public String[] getParameterValues(String name) {

        if (!requestParametersParsed)
            parseRequestParameters();

        return coyoteRequest.getParameters().getParameterValues(name);

    }


    /**
     * Return the protocol and version used to make this Request.
     */
    public String getProtocol() {
        return coyoteRequest.protocol().toString();
    }


    /**
     * Read the Reader wrapping the input stream for this Request.  The
     * default implementation wraps a <code>BufferedReader</code> around the
     * servlet input stream returned by <code>createInputStream()</code>.
     *
     * @exception IllegalStateException if <code>getInputStream()</code>
     *  has already been called for this request
     * @exception IOException if an input/output error occurs
     */
    public BufferedReader getReader() throws IOException {

        if (usingInputStream)
            throw new IllegalStateException
                (sm.getString("requestBase.getReader.ise"));

        usingReader = true;
        if (reader == null) {
            String encoding = getCharacterEncoding();
            InputStreamReader r = new InputStreamReader(inputStream, encoding);
            reader = new BufferedReader(r);
        }
        return (reader);

    }


    /**
     * Return the real path of the specified virtual path.
     *
     * @param path Path to be translated
     *
     * @deprecated As of version 2.1 of the Java Servlet API, use
     *  <code>ServletContext.getRealPath()</code>.
     */
    public String getRealPath(String path) {

        if (context == null)
            return (null);
        ServletContext servletContext = context.getServletContext();
        if (servletContext == null)
            return (null);
        else {
            try {
                return (servletContext.getRealPath(path));
            } catch (IllegalArgumentException e) {
                return (null);
            }
        }

    }


    /**
     * Return the remote IP address making this Request.
     */
    public String getRemoteAddr() {
        return coyoteRequest.remoteAddr().toString();
    }


    /**
     * Return the remote host name making this Request.
     */
    public String getRemoteHost() {
        return coyoteRequest.remoteHost().toString();
    }


    /**
     * Return a RequestDispatcher that wraps the resource at the specified
     * path, which may be interpreted as relative to the current request path.
     *
     * @param path Path of the resource to be wrapped
     */
    public RequestDispatcher getRequestDispatcher(String path) {

        if (context == null)
            return (null);

        // If the path is already context-relative, just pass it through
        if (path == null)
            return (null);
        else if (path.startsWith("/"))
            return (context.getServletContext().getRequestDispatcher(path));

        // Convert a request-relative path to a context-relative one
        String servletPath = (String) getAttribute(Globals.SERVLET_PATH_ATTR);
        if (servletPath == null)
            servletPath = getServletPath();
        String relative = RequestUtil.normalize(servletPath + "/../" + path);
        return (context.getServletContext().getRequestDispatcher(relative));

    }


    /**
     * Return the scheme used to make this Request.
     */
    public String getScheme() {
        return (coyoteRequest.scheme().toString());
    }


    /**
     * Return the server name responding to this Request.
     */
    public String getServerName() {
        return (coyoteRequest.serverName().toString());
    }


    /**
     * Return the server port responding to this Request.
     */
    public int getServerPort() {
        return (coyoteRequest.getServerPort());
    }


    /**
     * Was this request received on a secure connection?
     */
    public boolean isSecure() {
        return (secure);
    }


    /**
     * Remove the specified request attribute if it exists.
     *
     * @param name Name of the request attribute to remove
     */
    public void removeAttribute(String name) {
        attributes.remove(name);
    }


    /**
     * Set the specified request attribute to the specified value.
     *
     * @param name Name of the request attribute to set
     * @param value The associated value
     */
    public void setAttribute(String name, Object value) {

        // Name cannot be null
        if (name == null)
            throw new IllegalArgumentException
                (sm.getString("requestBase.setAttribute.namenull"));

        // Null value is the same as removeAttribute()
        if (value == null) {
            removeAttribute(name);
            return;
        }

        attributes.put(name, value);

    }


    /**
     * Overrides the name of the character encoding used in the body of
     * this request.  This method must be called prior to reading request
     * parameters or reading input using <code>getReader()</code>.
     *
     * @param enc The character encoding to be used
     *
     * @exception UnsupportedEncodingException if the specified encoding
     *  is not supported
     *
     * @since Servlet 2.3
     */
    public void setCharacterEncoding(String enc)
        throws UnsupportedEncodingException {

        // Ensure that the specified encoding is valid
        byte buffer[] = new byte[1];
        buffer[0] = (byte) 'a';
        String dummy = new String(buffer, enc);

        // Save the validated encoding
        coyoteRequest.setCharacterEncoding(enc);

    }


    // ---------------------------------------------------- HttpRequest Methods


    /**
     * Add a Cookie to the set of Cookies associated with this Request.
     *
     * @param cookie The new cookie
     */
    public void addCookie(Cookie cookie) {
        cookies.add(cookie);
    }


    /**
     * Add a Header to the set of Headers associated with this Request.
     *
     * @param name The new header name
     * @param value The new header value
     */
    public void addHeader(String name, String value) {
        // Not used ?
        System.out.println("coyoteRequest.addHeader(name, value)");
    }


    /**
     * Add a Locale to the set of preferred Locales for this Request.  The
     * first added Locale will be the first one returned by getLocales().
     *
     * @param locale The new preferred Locale
     */
    public void addLocale(Locale locale) {
        locales.add(locale);
    }


    /**
     * Add a parameter name and corresponding set of values to this Request.
     * (This is used when restoring the original request on a form based
     * login).
     *
     * @param name Name of this request parameter
     * @param values Corresponding values for this request parameter
     */
    public void addParameter(String name, String values[]) {
        // Not used
    }


    /**
     * Clear the collection of Cookies associated with this Request.
     */
    public void clearCookies() {
        cookies.clear();
    }


    /**
     * Clear the collection of Headers associated with this Request.
     */
    public void clearHeaders() {
        // Not used
    }


    /**
     * Clear the collection of Locales associated with this Request.
     */
    public void clearLocales() {
        locales.clear();
    }


    /**
     * Clear the collection of parameters associated with this Request.
     */
    public void clearParameters() {
        // Not used
    }


    /**
     * Set the authentication type used for this request, if any; otherwise
     * set the type to <code>null</code>.  Typical values are "BASIC",
     * "DIGEST", or "SSL".
     *
     * @param type The authentication type used
     */
    public void setAuthType(String type) {
        this.authType = type;
    }


    /**
     * Set the context path for this Request.  This will normally be called
     * when the associated Context is mapping the Request to a particular
     * Wrapper.
     *
     * @param path The context path
     */
    public void setContextPath(String path) {

        if (path == null) {
            this.contextPath = "";
        } else {
            this.contextPath = path;
        }

    }


    /**
     * Set the HTTP request method used for this Request.
     *
     * @param method The request method
     */
    public void setMethod(String method) {
        // Not used
    }


    /**
     * Set the query string for this Request.  This will normally be called
     * by the HTTP Connector, when it parses the request headers.
     *
     * @param query The query string
     */
    public void setQueryString(String query) {
        // Not used
    }


    /**
     * Set the path information for this Request.  This will normally be called
     * when the associated Context is mapping the Request to a particular
     * Wrapper.
     *
     * @param path The path information
     */
    public void setPathInfo(String path) {
        this.pathInfo = path;
    }


    /**
     * Set a flag indicating whether or not the requested session ID for this
     * request came in through a cookie.  This is normally called by the
     * HTTP Connector, when it parses the request headers.
     *
     * @param flag The new flag
     */
    public void setRequestedSessionCookie(boolean flag) {
        // Not used
    }


    /**
     * Set the requested session ID for this request.  This is normally called
     * by the HTTP Connector, when it parses the request headers.
     *
     * @param id The new session id
     */
    public void setRequestedSessionId(String id) {
        // Not used
    }


    /**
     * Set a flag indicating whether or not the requested session ID for this
     * request came in through a URL.  This is normally called by the
     * HTTP Connector, when it parses the request headers.
     *
     * @param flag The new flag
     */
    public void setRequestedSessionURL(boolean flag) {
        // Not used
    }


    /**
     * Set the unparsed request URI for this Request.  This will normally be
     * called by the HTTP Connector, when it parses the request headers.
     *
     * @param uri The request URI
     */
    public void setRequestURI(String uri) {
        // Not used
    }


    /**
     * Set the servlet path for this Request.  This will normally be called
     * when the associated Context is mapping the Request to a particular
     * Wrapper.
     *
     * @param path The servlet path
     */
    public void setServletPath(String path) {
        this.servletPath = path;
    }


    /**
     * Set the Principal who has been authenticated for this Request.  This
     * value is also used to calculate the value to be returned by the
     * <code>getRemoteUser()</code> method.
     *
     * @param principal The user Principal
     */
    public void setUserPrincipal(Principal principal) {
        this.userPrincipal = principal;
    }


    // --------------------------------------------- HttpServletRequest Methods


    /**
     * Return the authentication type used for this Request.
     */
    public String getAuthType() {
        return (authType);
    }


    /**
     * Return the portion of the request URI used to select the Context
     * of the Request.
     */
    public String getContextPath() {
        return (contextPath);
    }


    /**
     * Return the set of Cookies received with this Request.
     */
    public Cookie[] getCookies() {

        if (cookies.size() < 1)
            return (null);
        Cookie results[] = new Cookie[cookies.size()];
        return ((Cookie[]) cookies.toArray(results));

    }


    /**
     * Return the value of the specified date header, if any; otherwise
     * return -1.
     *
     * @param name Name of the requested date header
     *
     * @exception IllegalArgumentException if the specified header value
     *  cannot be converted to a date
     */
    public long getDateHeader(String name) {

        String value = getHeader(name);
        if (value == null)
            return (-1L);

        // Work around a bug in SimpleDateFormat in pre-JDK1.2b4
        // (Bug Parade bug #4106807)
        value += " ";

        // Attempt to convert the date header in a variety of formats
        for (int i = 0; i < formats.length; i++) {
            try {
                Date date = formats[i].parse(value);
                return (date.getTime());
            } catch (ParseException e) {
                ;
            }
        }
        throw new IllegalArgumentException(value);

    }


    /**
     * Return the first value of the specified header, if any; otherwise,
     * return <code>null</code>
     *
     * @param name Name of the requested header
     */
    public String getHeader(String name) {
        return coyoteRequest.getHeader(name);
    }


    /**
     * Return all of the values of the specified header, if any; otherwise,
     * return an empty enumeration.
     *
     * @param name Name of the requested header
     */
    public Enumeration getHeaders(String name) {
        return coyoteRequest.getMimeHeaders().values(name);
    }


    /**
     * Return the names of all headers received with this request.
     */
    public Enumeration getHeaderNames() {
        return coyoteRequest.getMimeHeaders().names();
    }


    /**
     * Return the value of the specified header as an integer, or -1 if there
     * is no such header for this request.
     *
     * @param name Name of the requested header
     *
     * @exception IllegalArgumentException if the specified header value
     *  cannot be converted to an integer
     */
    public int getIntHeader(String name) {

        String value = getHeader(name);
        if (value == null) {
            return (-1);
        } else {
            return (Integer.parseInt(value));
        }

    }


    /**
     * Return the HTTP request method used in this Request.
     */
    public String getMethod() {
        return coyoteRequest.method().toString();
    }


    /**
     * Return the path information associated with this Request.
     */
    public String getPathInfo() {
        return (pathInfo);
    }


    /**
     * Return the extra path information for this request, translated
     * to a real path.
     */
    public String getPathTranslated() {

        if (context == null)
            return (null);

        if (pathInfo == null) {
            return (null);
        } else {
            return (context.getServletContext().getRealPath(pathInfo));
        }

    }


    /**
     * Return the query string associated with this request.
     */
    public String getQueryString() {
        return (queryString);
    }


    /**
     * Return the name of the remote user that has been authenticated
     * for this Request.
     */
    public String getRemoteUser() {

        if (userPrincipal != null) {
            return (userPrincipal.getName());
        } else {
            return (null);
        }

    }


    /**
     * Return the session identifier included in this request, if any.
     */
    public String getRequestedSessionId() {
        return null;
    }


    /**
     * Return the request URI for this request.
     */
    public String getRequestURI() {
        return coyoteRequest.requestURI().toString();
    }


    /**
     * Reconstructs the URL the client used to make the request.
     * The returned URL contains a protocol, server name, port
     * number, and server path, but it does not include query
     * string parameters.
     * <p>
     * Because this method returns a <code>StringBuffer</code>,
     * not a <code>String</code>, you can modify the URL easily,
     * for example, to append query parameters.
     * <p>
     * This method is useful for creating redirect messages and
     * for reporting errors.
     *
     * @return A <code>StringBuffer</code> object containing the
     *  reconstructed URL
     */
    public StringBuffer getRequestURL() {

        StringBuffer url = new StringBuffer();
        String scheme = getScheme();
        int port = getServerPort();
        if (port < 0)
            port = 80; // Work around java.net.URL bug

        url.append(scheme);
        url.append("://");
        url.append(getServerName());
        if ((scheme.equals("http") && (port != 80))
            || (scheme.equals("https") && (port != 443))) {
            url.append(':');
            url.append(port);
        }
        url.append(getRequestURI());

        return (url);

    }


    /**
     * Return the portion of the request URI used to select the servlet
     * that will process this request.
     */
    public String getServletPath() {
        return (servletPath);
    }


    /**
     * Return the session associated with this Request, creating one
     * if necessary.
     */
    public HttpSession getSession() {
        return (getSession(true));
    }


    /**
     * Return the session associated with this Request, creating one
     * if necessary and requested.
     *
     * @param create Create a new session if one does not exist
     */
    public HttpSession getSession(boolean create) {
        return null;
    }


    /**
     * Return <code>true</code> if the session identifier included in this
     * request came from a cookie.
     */
    public boolean isRequestedSessionIdFromCookie() {
        return false;
    }


    /**
     * Return <code>true</code> if the session identifier included in this
     * request came from the request URI.
     */
    public boolean isRequestedSessionIdFromURL() {
        return false;
    }


    /**
     * Return <code>true</code> if the session identifier included in this
     * request came from the request URI.
     *
     * @deprecated As of Version 2.1 of the Java Servlet API, use
     *  <code>isRequestedSessionIdFromURL()</code> instead.
     */
    public boolean isRequestedSessionIdFromUrl() {
        return (isRequestedSessionIdFromURL());
    }


    /**
     * Return <code>true</code> if the session identifier included in this
     * request identifies a valid session.
     */
    public boolean isRequestedSessionIdValid() {
        return false;
    }


    /**
     * Return <code>true</code> if the authenticated user principal
     * possesses the specified role name.
     *
     * @param role Role name to be validated
     */
    public boolean isUserInRole(String role) {

        // Have we got an authenticated principal at all?
        if (userPrincipal == null)
            return (false);

        // Identify the Realm we will use for checking role assignmenets
        if (context == null)
            return (false);
        Realm realm = context.getRealm();
        if (realm == null)
            return (false);

        // Check for a role alias defined in a <security-role-ref> element
        if (wrapper != null) {
            String realRole = wrapper.findSecurityReference(role);
            if ((realRole != null) &&
                realm.hasRole(userPrincipal, realRole))
                return (true);
        }

        // Check for a role defined directly as a <security-role>
        return (realm.hasRole(userPrincipal, role));

    }


    /**
     * Return the principal that has been authenticated for this Request.
     */
    public Principal getUserPrincipal() {
        return (userPrincipal);
    }


    // ------------------------------------------------------ Protected Methods


    /**
     * Parse request parameters.
     */
    protected void parseRequestParameters() {

        requestParametersParsed = true;

        Parameters parameters = coyoteRequest.getParameters();

        String enc = coyoteRequest.getCharacterEncoding();
        if (enc != null) {
            parameters.setEncoding(enc);
        } else {
            parameters.setEncoding
                (org.apache.coyote.Constants.DEFAULT_CHARACTER_ENCODING);
        }

        parameters.handleQueryParameters();

        if (usingInputStream || usingReader)
            return;

        if (!getMethod().equalsIgnoreCase("POST"))
            return;

        int len = getContentLength();

        if (len > 0) {
            try {
                byte[] formData = null;
                if (len < CACHED_POST_LEN) {
                    if (postData == null)
                        postData = new byte[CACHED_POST_LEN];
                    formData = postData;
                } else {
                    formData = new byte[len];
                }
                readPostBody(formData, len);
                parameters.processParameters(formData, 0, len);
            } catch (Throwable t) {
                t.printStackTrace(); // TEMP
                ; // Ignore
            }
        }

    }


    /**
     * Read post body in an array.
     */
    protected int readPostBody(byte body[], int len)
        throws IOException {

        int offset = 0;
        do {
            int inputLen = getStream().read(body, offset, len - offset);
            if (inputLen <= 0) {
                return offset;
            }
            offset += inputLen;
        } while ((len - offset) > 0);
        return len;

    }


}
