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


package org.apache.coyote.tomcat5;

import java.io.InputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.security.PrivilegedActionException;
import java.util.Enumeration;
import java.util.Map;
import java.util.Locale;
import java.net.Socket;
import javax.servlet.ServletException;
import javax.servlet.ServletInputStream;
import javax.servlet.ServletRequest;
import javax.servlet.RequestDispatcher;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpSession;

import org.apache.catalina.connector.RequestFacade;
import org.apache.catalina.session.StandardSessionFacade;

/**
 * Facade class that wraps a Coyote request object.  
 * All methods are delegated to the wrapped request.
 *
 * @author Craig R. McClanahan
 * @author Remy Maucherat
 * @author Jean-Francois Arcand
 * @version $Revision$ $Date$
 */


public class CoyoteRequestFacade 
    extends RequestFacade
    implements HttpServletRequest {
        
        
    // ----------------------------------------------------------- DoPrivileged
    
    private final class GetAttributePrivilegedAction implements PrivilegedAction{
        
        public Object run() {
            return request.getAttributeNames();
        }            
    }
     
    
    private final class GetParameterMapPrivilegedAction implements PrivilegedAction{
        
        public Object run() {
            return request.getParameterMap();
        }        
    }    
    
    
    private final class GetRequestDispatcherPrivilegedAction implements PrivilegedAction{
        private String path;
        public GetRequestDispatcherPrivilegedAction(String path){
            this.path = path;
        }
        
        public Object run() {   
            return request.getRequestDispatcher(path);
       }           
    }    
    
    
    private final class GetParameterPrivilegedAction implements PrivilegedAction{
        public String name;
        public GetParameterPrivilegedAction(String name){
            this.name = name;
        }
        public Object run() {       
            return request.getParameter(name);
        }           
    }    
    
     
    private final class GetParameterNamesPrivilegedAction implements PrivilegedAction{
        
        public Object run() {          
            return request.getParameterNames();
        }           
    } 
    
    
    private final class GetParameterValuePrivilegedAction implements PrivilegedAction{
        public String name;
        public GetParameterValuePrivilegedAction(String name){
            this.name = name;
        }
        public Object run() {       
            return request.getParameterValues(name);
        }           
    }    
  
    
    private final class GetCookiesPrivilegedAction implements PrivilegedAction{
        
       public Object run() {       
            return request.getCookies();
        }           
    }      
    
    
    private final class GetCharacterEncodingPrivilegedAction implements PrivilegedAction{
        
        public Object run() {       
            return request.getCharacterEncoding();
        }           
    }   
        
    
    private final class GetHeadersPrivilegedAction implements PrivilegedAction{
        private String name;
        public GetHeadersPrivilegedAction(String name){
            this.name = name;
        }
        
        public Object run() {       
            return request.getHeaders(name);
        }           
    }    
        
    
    private final class GetHeaderNamesPrivilegedAction implements PrivilegedAction{

        public Object run() {       
            return request.getHeaderNames();
        }           
    }  
            
    
    private final class GetLocalePrivilegedAction implements PrivilegedAction{

        public Object run() {       
            return request.getLocale();
        }           
    }    
            
    
    private final class GetLocalesPrivilegedAction implements PrivilegedAction{

        public Object run() {       
            return request.getLocales();
        }           
    }    
    
    
    // ----------------------------------------------------------- Constructors


    /**
     * Construct a wrapper for the specified request.
     *
     * @param request The request to be wrapped
     */
    public CoyoteRequestFacade(CoyoteRequest request) {

        super(request);
        this.request = request;

    }


    // ----------------------------------------------------- Instance Variables


    /**
     * The wrapped request.
     */
    protected CoyoteRequest request = null;


    // --------------------------------------------------------- Public Methods


    /**
     * Clear facade.
     */
    public void clear() {
        request = null;
    }


    // ------------------------------------------------- ServletRequest Methods


    public Object getAttribute(String name) {
        return request.getAttribute(name);
    }


    public Enumeration getAttributeNames() {
        if (System.getSecurityManager() != null){
            return (Enumeration)AccessController.doPrivileged(
                new GetAttributePrivilegedAction());        
        } else {
            return request.getAttributeNames();
        }
    }


    public String getCharacterEncoding() {
        if (System.getSecurityManager() != null){
            return (String)AccessController.doPrivileged(
                new GetCharacterEncodingPrivilegedAction());
        } else {
            return request.getCharacterEncoding();
        }         
    }


    public void setCharacterEncoding(String env)
        throws java.io.UnsupportedEncodingException {
        request.setCharacterEncoding(env);
    }


    public int getContentLength() {
        return request.getContentLength();
    }


    public String getContentType() {
        return request.getContentType();
    }


    public ServletInputStream getInputStream()
        throws IOException {
        return request.getInputStream();
    }


    public String getParameter(String name) {
        if (System.getSecurityManager() != null){
            return (String)AccessController.doPrivileged(
                new GetParameterPrivilegedAction(name));
        } else {
            return request.getParameter(name);
        }
    }


    public Enumeration getParameterNames() {
        if (System.getSecurityManager() != null){
            return (Enumeration)AccessController.doPrivileged(
                new GetParameterNamesPrivilegedAction());
        } else {
            return request.getParameterNames();
        }
    }


    public String[] getParameterValues(String name) {
        if (System.getSecurityManager() != null){
            return (String[]) AccessController.doPrivileged(
                new GetParameterValuePrivilegedAction(name));
        } else {
            return request.getParameterValues(name);
        }
    }


    public Map getParameterMap() {
        if (System.getSecurityManager() != null){
            return (Map)AccessController.doPrivileged(
                new GetParameterMapPrivilegedAction());        
        } else {
            return request.getParameterMap();
        }
    }


    public String getProtocol() {
        return request.getProtocol();
    }


    public String getScheme() {
        return request.getScheme();
    }


    public String getServerName() {
        return request.getServerName();
    }


    public int getServerPort() {
        return request.getServerPort();
    }


    public BufferedReader getReader()
        throws IOException {
        return request.getReader();
    }


    public String getRemoteAddr() {
        return request.getRemoteAddr();
    }


    public String getRemoteHost() {
        return request.getRemoteHost();
    }


    public void setAttribute(String name, Object o) {
        request.setAttribute(name, o);
    }


    public void removeAttribute(String name) {
        request.removeAttribute(name);
    }


    public Locale getLocale() {
        if (System.getSecurityManager() != null){
            return (Locale)AccessController.doPrivileged(
                new GetLocalePrivilegedAction());
        } else {
            return request.getLocale();
        }        
    }


    public Enumeration getLocales() {
        if (System.getSecurityManager() != null){
            return (Enumeration)AccessController.doPrivileged(
                new GetLocalesPrivilegedAction());
        } else {
            return request.getLocales();
        }        
    }


    public boolean isSecure() {
        return request.isSecure();
    }


    public RequestDispatcher getRequestDispatcher(String path) {
        if (System.getSecurityManager() != null){
            return (RequestDispatcher)AccessController.doPrivileged(
                new GetRequestDispatcherPrivilegedAction(path));
        } else {
            return request.getRequestDispatcher(path);
        }
    }


    public String getRealPath(String path) {
        return request.getRealPath(path);
    }


    public String getAuthType() {
        return request.getAuthType();
    }


    public Cookie[] getCookies() {
        if (System.getSecurityManager() != null){
            return (Cookie[])AccessController.doPrivileged(
                new GetCookiesPrivilegedAction());
        } else {
            return request.getCookies();
        }        
    }


    public long getDateHeader(String name) {
        return request.getDateHeader(name);
    }


    public String getHeader(String name) {
        return request.getHeader(name);
    }


    public Enumeration getHeaders(String name) {
        if (System.getSecurityManager() != null){
            return (Enumeration)AccessController.doPrivileged(
                new GetHeadersPrivilegedAction(name));
        } else {
            return request.getHeaders(name);
        }         
    }


    public Enumeration getHeaderNames() {
        if (System.getSecurityManager() != null){
            return (Enumeration)AccessController.doPrivileged(
                new GetHeaderNamesPrivilegedAction());
        } else {
            return request.getHeaderNames();
        }             
    }


    public int getIntHeader(String name) {
        return request.getIntHeader(name);
    }


    public String getMethod() {
        return request.getMethod();
    }


    public String getPathInfo() {
        return request.getPathInfo();
    }


    public String getPathTranslated() {
        return request.getPathTranslated();
    }


    public String getContextPath() {
        return request.getContextPath();
    }


    public String getQueryString() {
        return request.getQueryString();
    }


    public String getRemoteUser() {
        return request.getRemoteUser();
    }


    public boolean isUserInRole(String role) {
        return request.isUserInRole(role);
    }


    public java.security.Principal getUserPrincipal() {
        return request.getUserPrincipal();
    }


    public String getRequestedSessionId() {
        return request.getRequestedSessionId();
    }


    public String getRequestURI() {
        return request.getRequestURI();
    }


    public StringBuffer getRequestURL() {
        return request.getRequestURL();
    }


    public String getServletPath() {
        return request.getServletPath();
    }


    public HttpSession getSession(boolean create) {
        HttpSession session =
            request.getSession(create);
        if (session == null)
            return null;
        else
            return new StandardSessionFacade(session);
    }


    public HttpSession getSession() {
        return getSession(true);
    }


    public boolean isRequestedSessionIdValid() {
        return request.isRequestedSessionIdValid();
    }


    public boolean isRequestedSessionIdFromCookie() {
        return request.isRequestedSessionIdFromCookie();
    }


    public boolean isRequestedSessionIdFromURL() {
        return request.isRequestedSessionIdFromURL();
    }


    public boolean isRequestedSessionIdFromUrl() {
        return request.isRequestedSessionIdFromURL();
    }


}
