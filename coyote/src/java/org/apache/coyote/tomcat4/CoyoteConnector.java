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
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.UnknownHostException;
import java.security.AccessControlException;
import java.util.Stack;
import java.util.Vector;
import java.util.Enumeration;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateException;
import java.security.UnrecoverableKeyException;
import java.security.KeyManagementException;

import org.apache.catalina.Connector;
import org.apache.catalina.Container;
import org.apache.catalina.HttpRequest;
import org.apache.catalina.HttpResponse;
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleEvent;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.Logger;
import org.apache.catalina.Request;
import org.apache.catalina.Response;
import org.apache.catalina.Service;
import org.apache.catalina.net.DefaultServerSocketFactory;
import org.apache.catalina.net.ServerSocketFactory;
import org.apache.catalina.util.LifecycleSupport;
import org.apache.catalina.util.StringManager;


/**
 * Implementation of a Coyote connector for Tomcat 4.x.
 *
 * @author Craig R. McClanahan
 * @author Remy Maucherat
 * @version $Revision$ $Date$
 */


public final class CoyoteConnector
    implements Connector, Lifecycle, Runnable {


    // ----------------------------------------------------- Instance Variables


    /**
     * The <code>Service</code> we are associated with (if any).
     */
    private Service service = null;


    /**
     * The accept count for this Connector.
     */
    private int acceptCount = 10;


    /**
     * The IP address on which to bind, if any.  If <code>null</code>, all
     * addresses on the server will be bound.
     */
    private String address = null;


    /**
     * The input buffer size we should create on input streams.
     */
    private int bufferSize = 2048;


    /**
     * The Container used for processing requests received by this Connector.
     */
    protected Container container = null;


    /**
     * The set of processors that have ever been created.
     */
    private Vector created = new Vector();


    /**
     * The current number of processors that have been created.
     */
    private int curProcessors = 0;


    /**
     * The debugging detail level for this component.
     */
    private int debug = 0;


    /**
     * The "enable DNS lookups" flag for this Connector.
     */
    private boolean enableLookups = false;


    /**
     * The server socket factory for this component.
     */
    private ServerSocketFactory factory = null;


    /**
     * Descriptive information about this Connector implementation.
     */
    private static final String info =
        "org.apache.coyote.tomcat4.CoyoteConnector/1.0";


    /**
     * The lifecycle event support for this component.
     */
    protected LifecycleSupport lifecycle = new LifecycleSupport(this);


    /**
     * The minimum number of processors to start at initialization time.
     */
    protected int minProcessors = 5;


    /**
     * The maximum number of processors allowed, or <0 for unlimited.
     */
    private int maxProcessors = 20;


    /**
     * Timeout value on the incoming connection.
     * Note : a value of 0 means no timeout.
     */
    private int connectionTimeout = Constants.DEFAULT_CONNECTION_TIMEOUT;


    /**
     * The port number on which we listen for requests.
     */
    private int port = 8080;


    /**
     * The set of processors that have been created but are not currently
     * being used to process a request.
     */
    private Stack processors = new Stack();


    /**
     * The server name to which we should pretend requests to this Connector
     * were directed.  This is useful when operating Tomcat behind a proxy
     * server, so that redirects get constructed accurately.  If not specified,
     * the server name included in the <code>Host</code> header is used.
     */
    private String proxyName = null;


    /**
     * The server port to which we should pretent requests to this Connector
     * were directed.  This is useful when operating Tomcat behind a proxy
     * server, so that redirects get constructed accurately.  If not specified,
     * the port number specified by the <code>port</code> property is used.
     */
    private int proxyPort = 0;


    /**
     * The redirect port for non-SSL to SSL redirects.
     */
    private int redirectPort = 443;


    /**
     * The request scheme that will be set on all requests received
     * through this connector.
     */
    private String scheme = "http";


    /**
     * The secure connection flag that will be set on all requests received
     * through this connector.
     */
    private boolean secure = false;


    /**
     * The server socket through which we listen for incoming TCP connections.
     */
    private ServerSocket serverSocket = null;


    /**
     * The string manager for this package.
     */
    private StringManager sm =
        StringManager.getManager(Constants.Package);


    /**
     * Has this component been initialized yet?
     */
    private boolean initialized = false;


    /**
     * Has this component been started yet?
     */
    private boolean started = false;


    /**
     * The shutdown signal to our background thread
     */
    private boolean stopped = false;


    /**
     * The background thread.
     */
    private Thread thread = null;


    /**
     * The name to register for the background thread.
     */
    private String threadName = null;


    /**
     * The thread synchronization object.
     */
    private Object threadSync = new Object();


    /**
     * Is chunking allowed ?
     */
    private boolean allowChunking = true;


    /**
     * Use TCP no delay ?
     */
    private boolean tcpNoDelay = true;


    /**
     * Coyote Processor class name. Defaults to the Coyote HTTP/1.1 processor.
     */
    private String processorClassName = 
        "org.apache.coyote.http11.Http11Processor";


    // ------------------------------------------------------------- Properties


    /**
     * Return the <code>Service</code> with which we are associated (if any).
     */
    public Service getService() {

        return (this.service);

    }


    /**
     * Set the <code>Service</code> with which we are associated (if any).
     *
     * @param service The service that owns this Engine
     */
    public void setService(Service service) {

        this.service = service;

    }


    /**
     * Return the connection timeout for this Connector.
     */
    public int getConnectionTimeout() {

        return (connectionTimeout);

    }


    /**
     * Set the connection timeout for this Connector.
     *
     * @param count The new connection timeout
     */
    public void setConnectionTimeout(int connectionTimeout) {

        this.connectionTimeout = connectionTimeout;

    }


    /**
     * Return the accept count for this Connector.
     */
    public int getAcceptCount() {

        return (acceptCount);

    }


    /**
     * Set the accept count for this Connector.
     *
     * @param count The new accept count
     */
    public void setAcceptCount(int count) {

        this.acceptCount = count;

    }


    /**
     * Get the allow chunking flag.
     */
    public boolean isChunkingAllowed() {

        return (allowChunking);

    }


    /**
     * Get the allow chunking flag.
     */
    public boolean getAllowChunking() {

        return isChunkingAllowed();

    }


    /**
     * Set the allow chunking flag.
     *
     * @param allowChunking Allow chunking flag
     */
    public void setAllowChunking(boolean allowChunking) {

        this.allowChunking = allowChunking;

    }


    /**
     * Return the bind IP address for this Connector.
     */
    public String getAddress() {

        return (this.address);

    }


    /**
     * Set the bind IP address for this Connector.
     *
     * @param address The bind IP address
     */
    public void setAddress(String address) {

        this.address = address;

    }


    /**
     * Is this connector available for processing requests?
     */
    public boolean isAvailable() {

        return (started);

    }


    /**
     * Return the input buffer size for this Connector.
     */
    public int getBufferSize() {

        return (this.bufferSize);

    }


    /**
     * Set the input buffer size for this Connector.
     *
     * @param bufferSize The new input buffer size.
     */
    public void setBufferSize(int bufferSize) {

        this.bufferSize = bufferSize;

    }


    /**
     * Return the Container used for processing requests received by this
     * Connector.
     */
    public Container getContainer() {

        return (container);

    }


    /**
     * Set the Container used for processing requests received by this
     * Connector.
     *
     * @param container The new Container to use
     */
    public void setContainer(Container container) {

        this.container = container;

    }


    /**
     * Return the current number of processors that have been created.
     */
    public int getCurProcessors() {

        return (curProcessors);

    }


    /**
     * Return the debugging detail level for this component.
     */
    public int getDebug() {

        return (debug);

    }


    /**
     * Set the debugging detail level for this component.
     *
     * @param debug The new debugging detail level
     */
    public void setDebug(int debug) {

        this.debug = debug;

    }


    /**
     * Return the "enable DNS lookups" flag.
     */
    public boolean getEnableLookups() {

        return (this.enableLookups);

    }


    /**
     * Set the "enable DNS lookups" flag.
     *
     * @param enableLookups The new "enable DNS lookups" flag value
     */
    public void setEnableLookups(boolean enableLookups) {

        this.enableLookups = enableLookups;

    }


    /**
     * Return the server socket factory used by this Container.
     */
    public ServerSocketFactory getFactory() {

        if (this.factory == null) {
            synchronized (this) {
                this.factory = new DefaultServerSocketFactory();
            }
        }
        return (this.factory);

    }


    /**
     * Set the server socket factory used by this Container.
     *
     * @param factory The new server socket factory
     */
    public void setFactory(ServerSocketFactory factory) {

        this.factory = factory;

    }


    /**
     * Return descriptive information about this Connector implementation.
     */
    public String getInfo() {

        return (info);

    }


    /**
     * Return the minimum number of processors to start at initialization.
     */
    public int getMinProcessors() {

        return (minProcessors);

    }


    /**
     * Set the minimum number of processors to start at initialization.
     *
     * @param minProcessors The new minimum processors
     */
    public void setMinProcessors(int minProcessors) {

        this.minProcessors = minProcessors;

    }


    /**
     * Return the maximum number of processors allowed, or <0 for unlimited.
     */
    public int getMaxProcessors() {

        return (maxProcessors);

    }


    /**
     * Set the maximum number of processors allowed, or <0 for unlimited.
     *
     * @param maxProcessors The new maximum processors
     */
    public void setMaxProcessors(int maxProcessors) {

        this.maxProcessors = maxProcessors;

    }


    /**
     * Return the port number on which we listen for requests.
     */
    public int getPort() {

        return (this.port);

    }


    /**
     * Set the port number on which we listen for requests.
     *
     * @param port The new port number
     */
    public void setPort(int port) {

        this.port = port;

    }


    /**
     * Return the class name of the Coyote processor in use.
     */
    public String getProcessorClassName() {

        return (this.processorClassName);

    }


    /**
     * Set the class name of the Coyote processor which will be used 
     * by the connector.
     * 
     * @param processorClassName The new class name
     */
    public void setProcessorClassName(String processorClassName) {

        this.processorClassName = processorClassName;

    }


    /**
     * Return the proxy server name for this Connector.
     */
    public String getProxyName() {

        return (this.proxyName);

    }


    /**
     * Set the proxy server name for this Connector.
     *
     * @param proxyName The new proxy server name
     */
    public void setProxyName(String proxyName) {

        this.proxyName = proxyName;

    }


    /**
     * Return the proxy server port for this Connector.
     */
    public int getProxyPort() {

        return (this.proxyPort);

    }


    /**
     * Set the proxy server port for this Connector.
     *
     * @param proxyPort The new proxy server port
     */
    public void setProxyPort(int proxyPort) {

        this.proxyPort = proxyPort;

    }


    /**
     * Return the port number to which a request should be redirected if
     * it comes in on a non-SSL port and is subject to a security constraint
     * with a transport guarantee that requires SSL.
     */
    public int getRedirectPort() {

        return (this.redirectPort);

    }


    /**
     * Set the redirect port number.
     *
     * @param redirectPort The redirect port number (non-SSL to SSL)
     */
    public void setRedirectPort(int redirectPort) {

        this.redirectPort = redirectPort;

    }


    /**
     * Return the scheme that will be assigned to requests received
     * through this connector.  Default value is "http".
     */
    public String getScheme() {

        return (this.scheme);

    }


    /**
     * Set the scheme that will be assigned to requests received through
     * this connector.
     *
     * @param scheme The new scheme
     */
    public void setScheme(String scheme) {

        this.scheme = scheme;

    }


    /**
     * Return the secure connection flag that will be assigned to requests
     * received through this connector.  Default value is "false".
     */
    public boolean getSecure() {

        return (this.secure);

    }


    /**
     * Set the secure connection flag that will be assigned to requests
     * received through this connector.
     *
     * @param secure The new secure connection flag
     */
    public void setSecure(boolean secure) {

        this.secure = secure;

    }


    /**
     * Return the TCP no delay flag value.
     */
    public boolean getTcpNoDelay() {

        return (this.tcpNoDelay);

    }


    /**
     * Set the TCP no delay flag which will be set on the socket after
     * accepting a connection.
     *
     * @param tcpNoDelay The new TCP no delay flag
     */
    public void setTcpNoDelay(boolean tcpNoDelay) {

        this.tcpNoDelay = tcpNoDelay;

    }


    // --------------------------------------------------------- Public Methods


    /**
     * Create (or allocate) and return a Request object suitable for
     * specifying the contents of a Request to the responsible Container.
     */
    public Request createRequest() {

        CoyoteRequest request = new CoyoteRequest();
        request.setConnector(this);
        return (request);

    }


    /**
     * Create (or allocate) and return a Response object suitable for
     * receiving the contents of a Response from the responsible Container.
     */
    public Response createResponse() {

        CoyoteResponse response = new CoyoteResponse();
        response.setConnector(this);
        return (response);

    }


    // -------------------------------------------------------- Package Methods


    /**
     * Recycle the specified Processor so that it can be used again.
     *
     * @param processor The processor to be recycled
     */
    void recycle(CoyoteProcessor processor) {

        processors.push(processor);

    }


    // -------------------------------------------------------- Private Methods


    /**
     * Create (or allocate) and return an available processor for use in
     * processing a specific request, if possible.  If the maximum
     * allowed processors have already been created and are in use, return
     * <code>null</code> instead.
     */
    private CoyoteProcessor createProcessor() {

        synchronized (processors) {
            if (processors.size() > 0) {
                return ((CoyoteProcessor) processors.pop());
            }
            if ((maxProcessors > 0) && (curProcessors < maxProcessors)) {
                return (newProcessor());
            } else {
                if (maxProcessors < 0) {
                    return (newProcessor());
                } else {
                    return (null);
                }
            }
        }

    }


    /**
     * Log a message on the Logger associated with our Container (if any).
     *
     * @param message Message to be logged
     */
    private void log(String message) {

        Logger logger = container.getLogger();
        String localName = threadName;
        if (localName == null)
            localName = "CoyoteConnector";
        if (logger != null)
            logger.log(localName + " " + message);
        else
            System.out.println(localName + " " + message);

    }


    /**
     * Log a message on the Logger associated with our Container (if any).
     *
     * @param message Message to be logged
     * @param throwable Associated exception
     */
    private void log(String message, Throwable throwable) {

        Logger logger = container.getLogger();
        String localName = threadName;
        if (localName == null)
            localName = "CoyoteConnector";
        if (logger != null)
            logger.log(localName + " " + message, throwable);
        else {
            System.out.println(localName + " " + message);
            throwable.printStackTrace(System.out);
        }

    }


    /**
     * Create and return a new processor suitable for processing
     * requests and returning the corresponding responses.
     */
    private CoyoteProcessor newProcessor() {

        CoyoteProcessor processor = new CoyoteProcessor(this, curProcessors++);
        if (processor instanceof Lifecycle) {
            try {
                ((Lifecycle) processor).start();
            } catch (LifecycleException e) {
                log(sm.getString("coyoteConnector.newProcessor"), e);
                return (null);
            }
        }
        created.addElement(processor);
        return (processor);

    }


    /**
     * Open and return the server socket for this Connector.  If an IP
     * address has been specified, the socket will be opened only on that
     * address; otherwise it will be opened on all addresses.
     *
     * @exception IOException                input/output or network error
     * @exception KeyStoreException          error instantiating the
     *                                       KeyStore from file (SSL only)
     * @exception NoSuchAlgorithmException   KeyStore algorithm unsupported
     *                                       by current provider (SSL only)
     * @exception CertificateException       general certificate error 
     *                                       (SSL only)
     * @exception UnrecoverableKeyException  internal KeyStore problem with
     *                                       the certificate (SSL only)
     * @exception KeyManagementException     problem in the key management
     *                                       layer (SSL only)
     */
    private ServerSocket open()
        throws IOException, KeyStoreException, NoSuchAlgorithmException,
        CertificateException, UnrecoverableKeyException,
        KeyManagementException {

        // Acquire the server socket factory for this Connector
        ServerSocketFactory factory = getFactory();

        // If no address is specified, open a connection on all addresses
        if (address == null) {
            log(sm.getString("coyoteConnector.allAddresses"));
            return (factory.createSocket(port, acceptCount));
        }

        // Open a server socket on the specified address
        try {
            InetAddress is = InetAddress.getByName(address);
            log(sm.getString("coyoteConnector.anAddress", address));
            return (factory.createSocket(port, acceptCount, is));
        } catch (Exception e) {
            log(sm.getString("coyoteConnector.noAddress", address));
            return (factory.createSocket(port, acceptCount));
        }

    }


    /**
     * Open server socket.
     */
    private Exception openServerSocket() {

        Exception result = null;

        // Establish a server socket on the specified port
        try {
            serverSocket = open();
        } catch (IOException e) {
            log(sm.getString("coyoteConnector.IOProblem"), e);
            result = e;
        } catch (KeyStoreException e) {
            log(sm.getString("coyoteConnector.keystoreProblem"), e);
            result = e;
        } catch (NoSuchAlgorithmException e) {
            log(sm.getString("coyoteConnector.keystoreAlgorithmProblem"), e);
            result = e;
        } catch (CertificateException e) {
            log(sm.getString("coyoteConnector.certificateProblem"), e);
            result = e;
        } catch (UnrecoverableKeyException e) {
            log(sm.getString("coyoteConnector.unrecoverableKey"), e);
            result = e;
        } catch (KeyManagementException e) {
            log(sm.getString("coyoteConnector.keyManagementProblem"), e);
            result = e;
        }

        return result;

    }


    // ---------------------------------------------- Background Thread Methods


    /**
     * The background thread that listens for incoming TCP/IP connections and
     * hands them off to an appropriate processor.
     */
    public void run() {

        // Loop until we receive a shutdown command
        while (!stopped) {

            // Accept the next incoming connection from the server socket
            Socket socket = null;
            try {
                socket = serverSocket.accept();
                if (connectionTimeout > 0)
                    socket.setSoTimeout(connectionTimeout);
                socket.setTcpNoDelay(tcpNoDelay);
            } catch (AccessControlException e) {
                log(sm.getString("coyoteConnector.securityException"), e);
                continue;
            } catch (IOException e) {
                try {
                    // If reopening fails, exit
                    synchronized (threadSync) {
                        if (started && !stopped)
                            log("accept: ", e);
                        if (!stopped) {
                            serverSocket.close();
                            openServerSocket();
                        }
                    }
                } catch (IOException ioe) {
                    log(sm.getString("coyoteConnector.serverSocketReopenFail"),
                        ioe);
                    break;
                }
                continue;
            }

            // Hand this socket off to an appropriate processor
            CoyoteProcessor processor = createProcessor();
            if (processor == null) {
                try {
                    log(sm.getString("coyoteConnector.noProcessor"));
                    socket.close();
                } catch (IOException e) {
                    ;
                }
                continue;
            }
            processor.assign(socket);

            // The processor will recycle itself when it finishes

        }

        // Notify the threadStop() method that we have shut ourselves down
        synchronized (threadSync) {
            threadSync.notifyAll();
        }

    }


    /**
     * Start the background processing thread.
     */
    private void threadStart() {

        log(sm.getString("coyoteConnector.starting"));

        thread = new Thread(this, threadName);
        thread.setDaemon(true);
        thread.start();

    }


    /**
     * Stop the background processing thread.
     */
    private void threadStop() {

        log(sm.getString("coyoteConnector.stopping"));

        stopped = true;
        try {
            threadSync.wait(5000);
        } catch (InterruptedException e) {
            ;
        }
        thread = null;

    }


    // ------------------------------------------------------ Lifecycle Methods


    /**
     * Add a lifecycle event listener to this component.
     *
     * @param listener The listener to add
     */
    public void addLifecycleListener(LifecycleListener listener) {

        lifecycle.addLifecycleListener(listener);

    }


    /**
     * Get the lifecycle listeners associated with this lifecycle. If this 
     * Lifecycle has no listeners registered, a zero-length array is returned.
     */
    public LifecycleListener[] findLifecycleListeners() {

        return null;//lifecycle.findLifecycleListeners();

    }


    /**
     * Remove a lifecycle event listener from this component.
     *
     * @param listener The listener to add
     */
    public void removeLifecycleListener(LifecycleListener listener) {

        lifecycle.removeLifecycleListener(listener);

    }


    /**
     * Initialize this connector (create ServerSocket here!)
     */
    public void initialize()
        throws LifecycleException {

        if (initialized)
            throw new LifecycleException 
                (sm.getString("coyoteConnector.alreadyInitialized"));

        this.initialized = true;

        Exception eRethrow = openServerSocket();

        if (eRethrow != null) {
            throw new LifecycleException
                (sm.getString("coyoteConnector.initException", threadName), 
                 eRethrow);
        }

    }


    /**
     * Begin processing requests via this Connector.
     *
     * @exception LifecycleException if a fatal startup error occurs
     */
    public void start() throws LifecycleException {

        // Validate and update our current state
        if (started)
            throw new LifecycleException
                (sm.getString("coyoteConnector.alreadyStarted"));
        threadName = "CoyoteConnector[" + port + "]";
        lifecycle.fireLifecycleEvent(START_EVENT, null);
        started = true;

        // Start our background thread
        threadStart();

        // Create the specified minimum number of processors
        while (curProcessors < minProcessors) {
            if ((maxProcessors > 0) && (curProcessors >= maxProcessors))
                break;
            CoyoteProcessor processor = newProcessor();
            recycle(processor);
        }

    }


    /**
     * Terminate processing requests via this Connector.
     *
     * @exception LifecycleException if a fatal shutdown error occurs
     */
    public void stop() throws LifecycleException {

        // Validate and update our current state
        if (!started)
            throw new LifecycleException
                (sm.getString("coyoteConnector.notStarted"));
        lifecycle.fireLifecycleEvent(STOP_EVENT, null);
        started = false;

        // Gracefully shut down all processors we have created
        for (int i = created.size() - 1; i >= 0; i--) {
            CoyoteProcessor processor = (CoyoteProcessor) created.elementAt(i);
            if (processor instanceof Lifecycle) {
                try {
                    ((Lifecycle) processor).stop();
                } catch (LifecycleException e) {
                    log(sm.getString("coyoteConnector.stopException"), e);
                }
            }
        }

        synchronized (threadSync) {
            // Close the server socket we were using
            if (serverSocket != null) {
                try {
                    serverSocket.close();
                } catch (IOException e) {
                    ;
                }
            }
            // Stop our background thread
            threadStop();
        }
        serverSocket = null;

    }


}
