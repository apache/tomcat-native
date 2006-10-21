/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

package org.apache.coyote.tomcat4;


import java.net.URLEncoder;
import java.util.HashMap;

import org.apache.tomcat.util.IntrospectionUtils;

import org.apache.coyote.Adapter;
import org.apache.coyote.ProtocolHandler;

import org.apache.catalina.Connector;
import org.apache.catalina.Container;
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.Logger;
import org.apache.catalina.Request;
import org.apache.catalina.Response;
import org.apache.catalina.Service;
import org.apache.catalina.core.StandardEngine;
import org.apache.catalina.net.DefaultServerSocketFactory;
import org.apache.catalina.net.ServerSocketFactory;
import org.apache.catalina.util.LifecycleSupport;
import org.apache.catalina.util.StringManager;
import org.apache.commons.modeler.Registry;

import javax.management.MBeanRegistration;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.MBeanServer;


/**
 * Implementation of a Coyote connector for Tomcat 4.x.
 *
 * @author Craig R. McClanahan
 * @author Remy Maucherat
 * @version $Revision$ $Date$
 */

public final class CoyoteConnector
    implements Connector, Lifecycle, MBeanRegistration {


    // ------------------------------------------------------------ Constructor
    
    public CoyoteConnector() throws Exception {
        this(null);
    }
    
    public CoyoteConnector(String protocolHandlerClassName) throws Exception {
        
        if (protocolHandlerClassName != null) {
            setProtocolHandlerClassName(protocolHandlerClassName);
        }
        
        // Instantiate protocol handler
        try {
            Class clazz = Class.forName(this.protocolHandlerClassName);
            protocolHandler = (ProtocolHandler) clazz.newInstance();
        } catch (Exception e) {
            e.printStackTrace();
            throw new LifecycleException(sm.getString(
                    "coyoteConnector.protocolHandlerInstantiationFailed", e));
        }

    }

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
     * Do we allow TRACE ?
     */
    private boolean allowTrace = false;


    /**
     * The input buffer size we should create on input streams.
     */
    private int bufferSize = 2048;


    /**
     * The Container used for processing requests received by this Connector.
     */
    protected Container container = null;


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
        "org.apache.coyote.tomcat4.CoyoteConnector2/1.0";


    /**
     * The lifecycle event support for this component.
     */
    protected LifecycleSupport lifecycle = new LifecycleSupport(this);


    /**
     * The minimum number of idle processors to have available. This is also
     * the number of processors that are started at initialization.
     */
    protected int minProcessors = 4;


    /**
     * The maximum amount of spare processors.
     */
    protected int maxSpareProcessors = 50;


    /**
     * The maximum number of processors allowed, or <0 for unlimited.
     */
    private int maxProcessors = 200;

    /**
     * The maximum permitted size of the request and response HTTP headers.
     */
    private int maxHttpHeaderSize = 4 * 1024;

    /**
     * Linger value on the incoming connection.
     * Note : a value inferior to 0 means no linger.
     */
    private int connectionLinger = Constants.DEFAULT_CONNECTION_LINGER;


    /**
     * Timeout value on the incoming connection.
     * Note : a value of 0 means no timeout.
     */
    private int connectionTimeout = Constants.DEFAULT_CONNECTION_TIMEOUT;


    /**
     * Timeout value on the incoming connection during request processing.
     * Note : a value of 0 means no timeout.
     */
    private int connectionUploadTimeout = 
        Constants.DEFAULT_CONNECTION_UPLOAD_TIMEOUT;


    /**
     * Timeout value on the server socket.
     * Note : a value of 0 means no timeout.
     */
    private int serverSocketTimeout = Constants.DEFAULT_SERVER_SOCKET_TIMEOUT;


    /**
     * The port number on which we listen for requests.
     */
    private int port = 8080;


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

    /** For jk, do tomcat authentication if true, trust server if false 
     */ 
    private boolean tomcatAuthentication = true;

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
     * Use TCP no delay ?
     */
    private boolean tcpNoDelay = true;


    /**
     * Flag to disable setting a seperate time-out for uploads.
     * If <code>true</code>, then the <code>timeout</code> parameter is
     * ignored.  If <code>false</code>, then the <code>timeout</code>
     * parameter is used to control uploads.
     */
    private boolean disableUploadTimeout = false;

    /**
     * Maximum number of Keep-Alive requests to honor per connection.
     */
    private int maxKeepAliveRequests = 100;


    /**
     * Compression value.
     */
    private String compressableMimeTypes = "text/html,text/xml,text/plain";
    private String compression = "off";


    /**
     * Coyote Protocol handler class name.
     * Defaults to the Coyote HTTP/1.1 protocolHandler.
     */
    private String protocolHandlerClassName =
        "org.apache.coyote.http11.Http11Protocol";


    /**
     * Use URI validation for Tomcat 4.0.x.
     */
    private boolean useURIValidationHack = true;


    /**
     * Coyote protocol handler.
     */
    private ProtocolHandler protocolHandler = null;


    /**
     * Coyote adapter.
     */
    private Adapter adapter = null;


     /**
      * URI encoding.
      */
     private String URIEncoding = null;


     /**
      * URI encoding as body.
      */
     private boolean useBodyEncodingForURI = true;


     protected static HashMap replacements = new HashMap();
     static {
         replacements.put("maxProcessors", "maxThreads");
         replacements.put("minProcessors", "minSpareThreads");
         replacements.put("maxSpareProcessors", "maxSpareThreads");
         replacements.put("acceptCount", "backlog");
         replacements.put("connectionLinger", "soLinger");
         replacements.put("connectionTimeout", "soTimeout");
         replacements.put("connectionUploadTimeout", "timeout");
         replacements.put("serverSocketTimeout", "serverSoTimeout");
         replacements.put("clientAuth", "clientauth");
         replacements.put("keystoreFile", "keystore");
         replacements.put("randomFile", "randomfile");
         replacements.put("rootFile", "rootfile");
         replacements.put("keystorePass", "keypass");
         replacements.put("keystoreType", "keytype");
         replacements.put("sslProtocol", "protocol");
         replacements.put("sslProtocols", "protocols");
     }

     
     // ------------------------------------------------------------- Properties


     /**
      * Return a configured property.
      */
     public Object getProperty(String name) {
         String repl = name;
         if (replacements.get(name) != null) {
             repl = (String) replacements.get(name);
         }
         return IntrospectionUtils.getProperty(protocolHandler, repl);
     }

     
     /**
      * Set a configured property.
      */
     public void setProperty(String name, String value) {
         String repl = name;
         if (replacements.get(name) != null) {
             repl = (String) replacements.get(name);
         }
         IntrospectionUtils.setProperty(protocolHandler, repl, value);
     }

     
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
     * Return the connection linger for this Connector.
     */
    public int getConnectionLinger() {

        return (connectionLinger);

    }


    /**
     * Set the connection linger for this Connector.
     *
     * @param connectionLinger The new connection linger
     */
    public void setConnectionLinger(int connectionLinger) {

        this.connectionLinger = connectionLinger;
        setProperty("connectionLinger", String.valueOf(connectionLinger));
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
     * @param connectionTimeout The new connection timeout
     */
    public void setConnectionTimeout(int connectionTimeout) {

        this.connectionTimeout = connectionTimeout;
        setProperty("connectionTimeout", String.valueOf(connectionTimeout));
    }


    /**
     * Return the connection upload timeout for this Connector.
     */
    public int getConnectionUploadTimeout() {

        return (connectionUploadTimeout);

    }


    /**
     * Set the connection upload timeout for this Connector.
     *
     * @param connectionUploadTimeout The new connection upload timeout
     */
    public void setConnectionUploadTimeout(int connectionUploadTimeout) {

        this.connectionUploadTimeout = connectionUploadTimeout;
        setProperty("connectionUploadTimeout",
                    String.valueOf(connectionUploadTimeout));

    }


    /**
     * Return the server socket timeout for this Connector.
     */
    public int getServerSocketTimeout() {

        return (serverSocketTimeout);

    }


    /**
     * Set the server socket timeout for this Connector.
     *
     * @param serverSocketTimeout The new server socket timeout
     */
    public void setServerSocketTimeout(int serverSocketTimeout) {

        this.serverSocketTimeout = serverSocketTimeout;
        setProperty("serverSocketTimeout", String.valueOf(serverSocketTimeout));

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
        setProperty("acceptCount", String.valueOf(acceptCount));
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
        setProperty("address", address);
        
    }


    /**
     * True if the TRACE method is allowed.  Default value is "false".
     */
    public boolean getAllowTrace() {

        return (this.allowTrace);

    }


    /**
     * Set the allowTrace flag, to disable or enable the TRACE HTTP method.
     *
     * @param allowTrace The new allowTrace flag
     */
    public void setAllowTrace(boolean allowTrace) {

        this.allowTrace = allowTrace;

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
     * Get the list of compressable MIME types.
     */
    public String getCompressableMimeType() {
        return compressableMimeTypes;
    }

    /**
     * Set the list of compressable MIME types.
     * 
     * @param compressableMimeTypes The new list of MIME types to enable for
     * compression.
     */
    public void setCompressableMimeType(String compressableMimeTypes) {
        this.compressableMimeTypes = compressableMimeTypes;
        setProperty("compressableMimeTypes", compressableMimeTypes);
    }

    /**
     * Get the value of compression.
     */
    public String getCompression() {

        return (compression);

    }


    /**
     * Set the value of compression.
     *
     * @param compression The new compression value, which can be "on", "off"
     * or "force"
     */
    public void setCompression(String compression) {

        this.compression = compression;
        setProperty("compression", compression);
        
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
        setProperty("minProcessors", String.valueOf(minProcessors));   
        
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
        setProperty("maxProcessors", String.valueOf(maxProcessors));
    }


    /**
     * Return the maximum number of spare processors allowed.
     */
    public int getMaxSpareProcessors() {

        return (maxSpareProcessors);

    }


    /**
     * Set the maximum number of spare processors allowed.
     *
     * @param maxSpareProcessors The new maximum of spare processors
     */
    public void setMaxSpareProcessors(int maxSpareProcessors) {

        this.maxSpareProcessors = maxSpareProcessors;
        setProperty("maxSpareProcessors", String.valueOf(maxSpareProcessors));

    }

    /**
     * Return the maximum permitted size of the HTTP request and response
     * headers.
     */
    public int getMaxHttpHeaderSize() {
        return maxHttpHeaderSize;
    }

    /**
     * Set the maximum permitted size of the HTTP request and response
     * headers.
     * 
     * @param maxHttpHeaderSize The new maximum header size in bytes.
     */
    public void setMaxHttpHeaderSize(int maxHttpHeaderSize) {
        this.maxHttpHeaderSize = maxHttpHeaderSize;
        setProperty("maxHttpHeaderSize", String.valueOf(maxHttpHeaderSize));
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
        setProperty("port", String.valueOf(port));
        
    }


    /**
     * Return the protocol handler associated with the connector.
     */
    public ProtocolHandler getProtocolHandler() {

        return (this.protocolHandler);

    }
    /**
     * Return the class name of the Coyote protocol handler in use.
     */
    public String getProtocolHandlerClassName() {

        return (this.protocolHandlerClassName);

    }


    /**
     * Set the class name of the Coyote protocol handler which will be used
     * by the connector.
     *
     * @param protocolHandlerClassName The new class name
     */
    public void setProtocolHandlerClassName(String protocolHandlerClassName) {

        this.protocolHandlerClassName = protocolHandlerClassName;

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

        if(! "".equals(proxyName) ) {
            this.proxyName = proxyName;
        } else {
            this.proxyName = null;
        }

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
     * Return the flag that specifies upload time-out behavior.
     */
    public boolean getDisableUploadTimeout() {
        return disableUploadTimeout;
    }

    /**
     * Set the flag to specify upload time-out behavior.
     *
     * @param isDisabled If <code>true</code>, then the <code>timeout</code>
     * parameter is ignored.  If <code>false</code>, then the
     * <code>timeout</code> parameter is used to control uploads.
     */
    public void setDisableUploadTimeout( boolean isDisabled ) {
        disableUploadTimeout = isDisabled;
        setProperty("disableUploadTimeout",
                    String.valueOf(disableUploadTimeout));
    }

    /**
     * Return the maximum number of Keep-Alive requests to honor per connection.
     */
    public int getMaxKeepAliveRequests() {
        return maxKeepAliveRequests;
    }

    /**
     * Set the maximum number of Keep-Alive requests to honor per connection.
     */
    public void setMaxKeepAliveRequests(int mkar) {
        maxKeepAliveRequests = mkar;
        setProperty("maxKeepAliveRequests",
                    String.valueOf(maxKeepAliveRequests));
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
        setProperty("secure",
                String.valueOf(secure));

    }

    public boolean getTomcatAuthentication() {
        return tomcatAuthentication;
    }

    public void setTomcatAuthentication(boolean tomcatAuthentication) {
        this.tomcatAuthentication = tomcatAuthentication;
        setProperty("tomcatAuthentication",
                    String.valueOf(tomcatAuthentication));
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
        setProperty("tcpNoDelay", String.valueOf(tcpNoDelay));
        
    }


     /**
      * Return the character encoding to be used for the URI.
      */
     public String getURIEncoding() {

         return (this.URIEncoding);

     }


     /**
      * Set the URI encoding to be used for the URI.
      *
      * @param URIEncoding The new URI character encoding.
      */
     public void setURIEncoding(String URIEncoding) {

         this.URIEncoding = URIEncoding;

     }


     /**
      * Return the true if the entity body encoding should be used for the URI.
      */
     public boolean getUseBodyEncodingForURI() {

         return (this.useBodyEncodingForURI);

     }


     /**
      * Set if the entity body encoding should be used for the URI.
      *
      * @param useBodyEncodingForURI The new value for the flag.
      */
     public void setUseBodyEncodingForURI(boolean useBodyEncodingForURI) {

         this.useBodyEncodingForURI = useBodyEncodingForURI;

     }


    /**
     * Return the value of the Uri validation flag.
     */
    public boolean getUseURIValidationHack() {

        return (this.useURIValidationHack);

    }


    /**
     * Set the value of the Uri validation flag.
     *
     * @param useURIValidationHack The new flag value
     */
    public void setUseURIValidationHack(boolean useURIValidationHack) {

        this.useURIValidationHack = useURIValidationHack;

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


    // -------------------------------------------------------- Private Methods


    /**
     * Log a message on the Logger associated with our Container (if any).
     *
     * @param message Message to be logged
     */
    private void log(String message) {

        Logger logger = container.getLogger();
        String localName = "CoyoteConnector";
        if (logger != null)
            logger.log(localName + " " + message);
        else
            System.out.println(localName + " " + message);

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

    protected ObjectName createObjectName(String domain, String type)
            throws MalformedObjectNameException {
        String encodedAddr = null;
        if (getProperty("address") != null) {
            encodedAddr = URLEncoder.encode(getProperty("address").toString());
        }
        String addSuffix = (getProperty("address") == null) ? "" : ",address="
                + encodedAddr;
        ObjectName _oname = new ObjectName(domain + ":type=" + type + ",port="
                + getPort() + addSuffix);
        return _oname;
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

        if( oname == null && (container instanceof StandardEngine)) {
            try {
                // we are loaded directly, via API - and no name was given to us
                StandardEngine cb=(StandardEngine)container;
                oname = createObjectName(cb.getName(), "Connector");
                Registry.getRegistry(null, null)
                    .registerComponent(this, oname, null);
            } catch (Exception e) {
                log ("Error registering connector " + e.toString());
            }
            if(debug > 0)
                log("Creating name for connector " + oname);
        }

        // Initialize adapter
        adapter = new CoyoteAdapter(this);

        protocolHandler.setAdapter(adapter);

        IntrospectionUtils.setProperty(protocolHandler, "jkHome",
                                       System.getProperty("catalina.base"));

        // Configure secure socket factory
        if (factory instanceof CoyoteServerSocketFactory) {
            IntrospectionUtils.setProperty(protocolHandler, "secure",
                                           "" + true);
            CoyoteServerSocketFactory ssf =
                (CoyoteServerSocketFactory) factory;
            IntrospectionUtils.setProperty(protocolHandler, "algorithm",
                                           ssf.getAlgorithm());
            IntrospectionUtils.setProperty(protocolHandler, "ciphers",
                                           ssf.getCiphers());
	        IntrospectionUtils.setProperty(protocolHandler, "clientauth",
                                           ssf.getClientAuth());
            IntrospectionUtils.setProperty(protocolHandler, "keystore",
                                           ssf.getKeystoreFile());
            IntrospectionUtils.setProperty(protocolHandler, "randomfile",
                                           ssf.getRandomFile());
            IntrospectionUtils.setProperty(protocolHandler, "rootfile",
                                           ssf.getRootFile());

            IntrospectionUtils.setProperty(protocolHandler, "keypass",
                                           ssf.getKeystorePass());
            IntrospectionUtils.setProperty(protocolHandler, "keytype",
                                           ssf.getKeystoreType());
            IntrospectionUtils.setProperty(protocolHandler, "protocol",
                                           ssf.getProtocol());
            IntrospectionUtils.setProperty(protocolHandler,
                                           "sSLImplementation",
                                           ssf.getSSLImplementation());
        }

        try {
            protocolHandler.init();
        } catch (Exception e) {
            throw new LifecycleException
                (sm.getString
                 ("coyoteConnector.protocolHandlerInitializationFailed", e));
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
        lifecycle.fireLifecycleEvent(START_EVENT, null);
        started = true;

        // We can't register earlier - the JMX registration of this happens
        // in Server.start callback
        if( this.oname != null ) {
            // We are registred - register the adapter as well.
            try {
                Registry.getRegistry(null, null).registerComponent(
                        protocolHandler,
                        this.domain + ":type=protocolHandler,className=" +
                        protocolHandlerClassName, null);
            } catch( Exception ex ) {
                ex.printStackTrace();
            }
        } else {
            log( "Coyote can't register jmx for protocol");
        }

        try {
            protocolHandler.start();
        } catch (Exception e) {
            throw new LifecycleException
                (sm.getString
                 ("coyoteConnector.protocolHandlerStartFailed", e));
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

        try {
            protocolHandler.destroy();
        } catch (Exception e) {
            throw new LifecycleException
                (sm.getString
                 ("coyoteConnector.protocolHandlerDestroyFailed", e));
        }

    }

    protected String domain;
    protected ObjectName oname;
    protected MBeanServer mserver;

    public ObjectName getObjectName() {
        return oname;
    }

    public String getDomain() {
        return domain;
    }

    public ObjectName preRegister(MBeanServer server,
                                  ObjectName name) throws Exception {
        oname=name;
        mserver=server;
        domain=name.getDomain();
        return name;
    }

    public void postRegister(Boolean registrationDone) {
    }

    public void preDeregister() throws Exception {
    }

    public void postDeregister() {
    }

}
