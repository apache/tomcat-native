/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *         Copyright (c) 1999, 2000  The Apache Software Foundation.         *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Tomcat",  and  "Apache  Software *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */
package org.apache.catalina.connector.warp;

import java.io.*;
import java.net.*;
import org.apache.catalina.Container;
import org.apache.catalina.Connector;
import org.apache.catalina.Request;
import org.apache.catalina.Response;
import org.apache.catalina.net.DefaultServerSocketFactory;
import org.apache.catalina.net.ServerSocketFactory;
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleEvent;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.util.LifecycleSupport;
import org.apache.catalina.connector.HttpRequestBase;
import org.apache.catalina.connector.HttpResponseBase;

/**
 *
 *
 * @author <a href="mailto:pier.fumagalli@eng.sun.com">Pier Fumagalli</a>
 * @author Copyright &copy; 1999, 2000 <a href="http://www.apache.org">The
 *         Apache Software Foundation.
 * @version CVS $Id$
 */
public class WarpConnector implements Connector, Lifecycle, Runnable {

    // -------------------------------------------------------------- CONSTANTS

    /** Our debug flag status (Used to compile out debugging information). */
    private static final boolean DEBUG=WarpDebug.DEBUG;
    /** The descriptive information related to this implementation. */
    private static final String info="WarpConnector/"+WarpConstants.VERSION;

    // -------------------------------------------------------- LOCAL VARIABLES

    /** The lifecycle event support for this component. */
    private LifecycleSupport lifecycle=null;
    /** The server socket factory for this connector. */
    private ServerSocket socket=null;
    /** Wether the start() method was called or not. */
    private boolean started=false;
    /** The accept count for the server socket. */
    private int count = 10;

    // -------------------------------------------------------- BEAN PROPERTIES

    /** Wether requests received through this connector are secure. */
    private boolean secure=false;
    /** The scheme to be set on requests received through this connector. */
    private String scheme="http";
    /** The Container used to process requests received by this Connector. */
    private Container container=null;
    /** The server socket factory for this connector. */
    private ServerSocketFactory factory=null;
    /** The server socket port. */
    private int port=8008;
    /** The number of concurrent connections we can handle. */
    private int acceptcount=10;
    /** The root path for web applications. */
    private String appbase="";

    // ------------------------------------------------------------ CONSTRUCTOR
    
    public WarpConnector() {
        super();
        this.lifecycle=new LifecycleSupport(this);
        if (DEBUG) this.debug("New instance created");
    }

    // --------------------------------------------------------- PUBLIC METHODS

    /** 
     * Run the acceptor thread, the thread that will wait on the server socket
     * and create new connections.
     */
    public void run() {
        if (DEBUG) this.debug("Acceptor thread started");
        try {
            while (this.started) {
                Socket s=this.socket.accept();
                WarpConnection c=new WarpConnection();
                c.setSocket(s);
                c.setConnector(this);
                try {
                    c.start();
                } catch (LifecycleException e) {
                    this.log(e);
                    continue;
                }
            }
            this.socket.close();
        } catch (IOException e) {
            if (this.started) e.printStackTrace(System.err);
        }
        if (DEBUG) this.debug("Acceptor thread exited");
    }

    /**
     * Create and return a Request object suitable for receiving the contents
     * of a Request from the responsible Container.
     */
    public Request createRequest() {
        HttpRequestBase request = new HttpRequestBase();
        request.setConnector(this);
        return (request);
    }

    /**
     * Create and return a Response object suitable for receiving the contents
     * of a Response from the responsible Container.
     */
    public Response createResponse() {
        HttpResponseBase response = new HttpResponseBase();
        response.setConnector(this);
        return (response);
    }

    /**
     * Begin processing requests via this Connector.
     */
    public void start() throws LifecycleException {
        if (DEBUG) this.debug("Starting");
        // Validate and update our current state
        if (started)
            throw new LifecycleException("Connector already started");
    
        // We're starting
        lifecycle.fireLifecycleEvent(START_EVENT, null);
        this.started=true;

        // Establish a server socket on the specified port
        try {
            this.socket=getFactory().createSocket(this.port,this.count);
        } catch (Exception e) {
            throw new LifecycleException("Error creating server socket", e);
        }

        // Start our background thread
        new Thread(this).start();
    }

    /**
     * Terminate processing requests via this Connector.
     */
    public void stop() throws LifecycleException {
        // Validate and update our current state
        if (!started)
            throw new LifecycleException("Cannot stop (never started)");

        // We're stopping
        lifecycle.fireLifecycleEvent(STOP_EVENT, null);
        started = false;
        
        // Close the server socket
        try {
            this.socket.close();
        } catch (IOException e) {
            throw new LifecycleException("Error closing server socket", e);
        }
    }

    // ---------------------------------------- CONNECTOR AND LIFECYCLE METHODS

    /**
     * Add a lifecycle event listener to this component.
     */
    public void addLifecycleListener(LifecycleListener listener) {
        this.lifecycle.addLifecycleListener(listener);
    }

    /**
     * Remove a lifecycle event listener from this component.
     */
    public void removeLifecycleListener(LifecycleListener listener) {
        lifecycle.removeLifecycleListener(listener);
    }

    /**
     * Return descriptive information about this implementation.
     */
    public String getInfo() {
        return (info);
    }

    // ----------------------------------------------------------- BEAN METHODS

    /**
     * Return the secure connection flag that will be assigned to requests
     * received through this connector. Default value is "false".
     * <br>
     * NOTE: For protocols such as WARP this is pointless, as we will only know
     * at request time wether the received request is secure or not. Security
     * is handled by an HTTP stack outside this JVM, and this can be (like with
     * Apache) handling both HTTP and HTTPS requests.
     */
    public boolean getSecure() {
        return(this.secure);
    }

    /**
     * Set the secure connection flag that will be assigned to requests
     * received through this connector.
     * <br>
     * NOTE: For protocols such as WARP this is pointless, as we will only know
     * at request time wether the received request is secure or not. Security
     * is handled by an HTTP stack outside this JVM, and this can be (like with
     * Apache) handling both HTTP and HTTPS requests.
     */
    public void setSecure(boolean secure) {
        if (DEBUG) this.debug("Setting secure to "+secure);
        this.secure=secure;
    }

    /**
     * Return the scheme that will be assigned to requests received through
     * this connector.  Default value is "http".
     * <br>
     * NOTE: As noted in the getSecure() and setSecure() methods, for WARP
     * we don't know the scheme of the request until the request is received.
     */
    public String getScheme() {
        return(this.scheme);
    }

    /**
     * Set the scheme that will be assigned to requests received through
     * this connector.
     * <br>
     * NOTE: As noted in the getSecure() and setSecure() methods, for WARP
     * we don't know the scheme of the request until the request is received.
     */
    public void setScheme(String scheme) {
        if (DEBUG) this.debug("Setting scheme to "+scheme);
        this.scheme=scheme;
    }

    /**
     * Return the Container used for processing requests received by this
     * Connector.
     */
    public Container getContainer() {
        return(this.container);
    }

    /**
     * Set the Container used for processing requests received by this
     * Connector.
     */
    public void setContainer(Container container) {
        if (DEBUG) {
            if (container==null) this.debug("Setting null container");
            else this.debug("Setting container "+container.getInfo());
        }
        this.container=container;
    }

    /**
     * Return the server socket factory used by this Connector.
     */
    public ServerSocketFactory getFactory() {
        synchronized (this) {
            if (this.factory==null) {
                if (DEBUG) this.debug("Creating ServerSocketFactory");

                this.factory=new DefaultServerSocketFactory();
            }
        }
        return (this.factory);
    }

    /**
     * Set the server socket factory used by this Container.
     */
    public void setFactory(ServerSocketFactory factory) {
        if (factory==null) return;
        synchronized (this) {
            if (DEBUG) this.debug("Setting ServerSocketFactory");
            this.factory=factory;
        }
    }

    /**
     * Return the port number on which we listen for HTTP requests.
     */
    public int getPort() {
    	return(this.port);
    }


    /**
     * Set the port number on which we listen for HTTP requests.
     */
    public void setPort(int port) {
        if (DEBUG) this.debug("Setting port to "+port);
    	this.port=port;
    }

    /**
     * Return the accept count for this Connector.
     */
    public int getAcceptCount() {
    	return (this.acceptcount);
    }

    /**
     * Set the accept count for this Connector.
     *
     * @param acceptcount The new accept count
     */
    public void setAcceptCount(int acceptcount) {
        if (DEBUG) this.debug("Setting accept count to "+acceptcount);
    	this.acceptcount=acceptcount;
    }

    /**
     * Return the application root for this Connector. This can be an absolute
     * pathname, a relative pathname, or a URL.
     */
    public String getAppBase() {
    	return (this.appbase);
    }

    /**
     * Set the application root for this Connector. This can be an absolute
     * pathname, a relative pathname, or a URL.
     */
    public void setAppBase(String appbase) {
        if (appbase==null) return;
        if (DEBUG) this.debug("Setting application root to "+appbase);
    	String old=this.appbase;
    	this.appbase=appbase;
    }

    // ------------------------------------------ LOGGING AND DEBUGGING METHODS

    /**
     * Dump a log message.
     */
    public void log(String msg) {
        // FIXME: Log thru catalina
        WarpDebug.debug(this,msg);
    }

    /**
     * Dump information for an Exception.
     */
    public void log(Exception exc) {
        // FIXME: Log thru catalina
        WarpDebug.debug(this,exc);
    }

    /**
     * Dump a debug message.
     */
    private void debug(String msg) {
        if (DEBUG) WarpDebug.debug(this,msg);
    }

    /**
     * Dump information for an Exception.
     */
    private void debug(Exception exc) {
        if (DEBUG) WarpDebug.debug(this,exc);
    }
}
