/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
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

import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Random;
import java.util.Vector;

import org.apache.catalina.Connector;
import org.apache.catalina.Container;
import org.apache.catalina.Context;
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.Logger;
import org.apache.catalina.Request;
import org.apache.catalina.Response;
import org.apache.catalina.Service;
import org.apache.catalina.net.DefaultServerSocketFactory;
import org.apache.catalina.net.ServerSocketFactory;
import org.apache.catalina.util.LifecycleSupport;

public class WarpConnector implements Connector, Lifecycle, Runnable {

    /* ==================================================================== */
    /* Instance variables                                                   */
    /* ==================================================================== */

    /* -------------------------------------------------------------------- */
    /* Local variables */

    /** The running thread accepting connections */
    private Thread thread=null;
    /** The server socket. */
    private ServerSocket server=null;
    /** Our <code>WarpLogger</code>. */
    private WarpLogger logger=null;
    /** Our list of deployed web applications. */
    private Vector applications=new Vector();
    /** The unique ID of this connector instance. */
    protected int uniqueId=-1;

    /* -------------------------------------------------------------------- */
    /* Bean variables */

    /** The <code>Container</code> instance processing requests. */
    private Container container=null;
    /** The "enable DNS lookups" flag. */
    private boolean enableLookups=false;
    /** The <code>ServerSocketFactory</code> used by this connector. */
    private ServerSocketFactory factory=null;
    /** The redirect port value for SSL requests. */
    private int redirectPort=443;
    /** The request scheme value. */
    private String scheme="warp";
    /** The secure flag of this <code>Connector</code>. */
    private boolean secure=false;
    /** The <code>Service</code> we are associated with (if any). */
    private Service service=null;
    /** Descriptive information of this <code>Connector</code>. */
    private String info=null;
    /** The address we need to bind to. */
    private String address=null;
    /** The port we need to bind to. */
    private int port=8008;
    /** The server socket backlog length. */
    private int acceptCount=10;
    /** The server appBase for hosts created via WARP. */
    private String appBase="webapps";
    /** The debug level. */
    private int debug=0;

    /* -------------------------------------------------------------------- */
    /* Lifecycle variables */

    /** The lifecycle event support for this component. */
    private LifecycleSupport lifecycle=new LifecycleSupport(this);
    /** The "initialized" flag. */
    private boolean initialized=false;
    /** The "started" flag. */
    private boolean started=false;

    /* ==================================================================== */
    /* Constructor                                                          */
    /* ==================================================================== */

    /**
     * Construct a new instance of a <code>WarpConnector</code>.
     */
    public WarpConnector() {
        super();
        this.logger=new WarpLogger(this);
        this.uniqueId=new Random().nextInt();
        if (Constants.DEBUG)
            logger.debug("Instance created (ID="+this.uniqueId+")");
    }

    /* ==================================================================== */
    /* Bean methods                                                         */
    /* ==================================================================== */

    /**
     * Return the <code>Container</code> instance which will process all
     * requests received by this <code>Connector</code>.
     */
    public Container getContainer() {
        return(this.container);
    }

    /**
     * Set the <code>Container</code> instance which will process all requests
     * received by this <code>Connector</code>.
     *
     * @param container The new Container to use
     */
    public void setContainer(Container container) {
        this.container=container;
        this.logger.setContainer(container);

        if (Constants.DEBUG) {
            if (container==null) logger.debug("Setting null container");
            else logger.debug("Setting container "+container.getClass());
        }
    }

    /**
     * Return the "enable DNS lookups" flag.
     */
    public boolean getEnableLookups() {
        return(this.enableLookups);
    }

    /**
     * Set the "enable DNS lookups" flag.
     *
     * @param enableLookups The new "enable DNS lookups" flag value
     */
    public void setEnableLookups(boolean enableLookups) {
        this.enableLookups=enableLookups;

        if (Constants.DEBUG) logger.debug("Setting lookup to "+enableLookups);
    }

    /**
     * Return the <code>ServerSocketFactory</code> used by this
     * <code>Connector</code> to generate <code>ServerSocket</code> instances.
     */
    public ServerSocketFactory getFactory() {
        if (this.factory==null) {
            synchronized(this) {
                if (Constants.DEBUG) logger.debug("Creating factory");
                this.factory=new DefaultServerSocketFactory();
            }
        }
        return(this.factory);
    }

    /**
     * Set the <code>ServerSocketFactory</code> used by this
     * <code>Connector</code> to generate <code>ServerSocket</code> instances.
     *
     * @param factory The new server socket factory
     */
    public void setFactory(ServerSocketFactory factory) {
        if (factory==null) throw new NullPointerException();
        this.factory=factory;

        if (Constants.DEBUG)
            logger.debug("Setting factory "+factory.getClass().getName());
    }

    /**
     * Return the port number to which a request should be redirected if
     * it comes in on a non-SSL port and is subject to a security constraint
     * with a transport guarantee that requires SSL.
     */
    public int getRedirectPort() {
        return(this.redirectPort);
    }

    /**
     * Set the redirect port number.
     *
     * @param redirectPort The redirect port number (non-SSL to SSL)
     */
    public void setRedirectPort(int redirectPort) {
        if ((redirectPort<1) || (redirectPort>65535))
            throw new IllegalArgumentException("Invalid port "+redirectPort);
        this.redirectPort=redirectPort;

        if (Constants.DEBUG)
            logger.debug("Setting redirection port to "+redirectPort);
    }

    /**
     * Return the scheme that will be assigned to requests received
     * through this connector.  Default value is "warp".
     */
    public String getScheme() {
        return(this.scheme);
    }

    /**
     * Set the scheme that will be assigned to requests received through
     * this connector.
     *
     * @param scheme The new scheme
     */
    public void setScheme(String scheme) {
        if (scheme==null) throw new NullPointerException();
        this.scheme=scheme;

        if (Constants.DEBUG) logger.debug("Setting scheme to "+scheme);
    }

    /**
     * Return the secure connection flag that will be assigned to requests
     * received through this connector.  Default value is "false".
     */
    public boolean getSecure() {
        return(this.secure);
    }

    /**
     * Set the secure connection flag that will be assigned to requests
     * received through this connector.
     *
     * @param secure The new secure connection flag
     */
    public void setSecure(boolean secure) {
        this.secure=secure;

        if (Constants.DEBUG) logger.debug("Setting secure to "+secure);
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
     * Return descriptive information about this <code>Connector</code>.
     */
    public String getInfo() {
        if (this.info==null) {
            synchronized(this) {
                this.info=this.getClass().getName()+"/"+
                          Constants.VERS_MINOR+Constants.VERS_MAJOR;
            }
        }
        return(this.info);
    }

    /**
     * Set descriptive information about this <code>Connector</code>.
     */
    public void setInfo(String info) {
        if (info==null) throw new NullPointerException();
        this.info=info;

        if (Constants.DEBUG) logger.debug("Setting info to "+info);
    }

    /**
     * Return the IP address to which this <code>Connector</code> will bind to.
     */
    public String getAddress() {
        return(this.address);
    }

    /**
     * Set the IP address to which this <code>Connector</code> will bind to.
     *
     * @param address The bind IP address
     */
    public void setAddress(String address) {
        this.address=address;

        if (Constants.DEBUG) logger.debug("Setting address to "+address);
    }

    /**
     * Return the port to which this <code>Connector</code> will bind to.
     */
    public int getPort() {
        return(this.port);
    }

    /**
     * Set the port to which this <code>Connector</code> will bind to.
     * 
     * @param port The bind port
     */
    public void setPort(int port) {
        this.port=port;
    }

    /**
     * Set the IP address to which this <code>Connector</code> will bind to.
     *
     * @param address The bind IP address
     */
    public void setAddress(int port) {
        if ((port<1) || (port>65535))
            throw new IllegalArgumentException("Invalid port "+port);
        this.port=port;

        if (Constants.DEBUG) logger.debug("Setting port to "+port);
    }

    /**
     * Return the accept count for this Connector.
     */
    public int getAcceptCount() {
        return (this.acceptCount);
    }


    /**
     * Set the accept count for this Connector.
     *
     * @param count The new accept count
     */
    public void setAcceptCount(int count) {
        this.acceptCount = count;

        if (Constants.DEBUG) logger.debug("Setting acceptCount to "+count);
    }

    /**
     * Get the applications base directory for hosts created via WARP.
     */
    public String getAppBase() {
        return (this.appBase);
    }


    /**
     * Set the applications base directory for hosts created via WARP.
     *
     * @param appBase The appbase property.
     */
    public void setAppBase(String appBase) {
        this.appBase = appBase;

        if (Constants.DEBUG) logger.debug("Setting appBase to "+appBase);
    }

    /**
     * Return the debug level.
     */
    public int getDebug() {
        return(this.debug);
    }

    /**
     * Set the debug level.
     */
    public void setDebug(int debug) {
        this.debug=debug;
    }

    /* ==================================================================== */
    /* Lifecycle methods                                                    */
    /* ==================================================================== */

    /**
     * Add a <code>LifecycleEvent</code> listener to this
     * <code>Connector</code>.
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
        return lifecycle.findLifecycleListeners();
    }

    /**
     * Remove a <code>LifecycleEvent</code> listener from this
     * <code>Connector</code>.
     *
     * @param listener The listener to remove
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
            throw new LifecycleException("Already initialized");
        this.initialized=true;

        // Get a hold on a server socket
        try {
            ServerSocketFactory fact=this.getFactory();
            int port=this.getPort();
            int accc=this.getAcceptCount();

            if (this.getAddress()==null) {
                this.server=fact.createSocket(port,accc);
            } else {
                InetAddress addr=InetAddress.getByName(this.getAddress());
                this.server=fact.createSocket(port,accc,addr);
            }
        } catch (Exception e) {
            throw new LifecycleException("Error creating server socket ("+
                e.getClass().getName()+")",e);
        }
    }

    /**
     * Start accepting connections by this <code>Connector</code>.
     */
    public void start() throws LifecycleException {
        if (!initialized) this.initialize();
        if (started) throw new LifecycleException("Already started");

        // Can't get a hold of a server socket
        if (this.server==null)
            throw new LifecycleException("Server socket not created");

        lifecycle.fireLifecycleEvent(START_EVENT, null);

        this.started = true;

        this.thread=new Thread(this);
        this.thread.start();
    }

    /**
     * Stop accepting connections by this <code>Connector</code>.
     */
    public void stop() throws LifecycleException {
        if (!started) throw new LifecycleException("Not started");

        lifecycle.fireLifecycleEvent(STOP_EVENT, null);

        this.started = false;

        if (this.server!=null) try {
            this.server.close();
        } catch (IOException e) {
            logger.log("Cannot close ServerSocket",e);
        }
    }

    /**
     * Check whether this service was started or not.
     */
    public boolean isStarted() {
        return(this.started);
    }

    /* ==================================================================== */
    /* Public methods                                                       */
    /* ==================================================================== */

    /**
     * Return the application ID for a given <code>Context</code>.
     */
    protected int applicationId(Context context) {
        int id=this.applications.indexOf(context);
        if (id==-1) {
            this.applications.add(context);
            id=this.applications.indexOf(context);
        }
        return(id);
    }

    /**
     * Return the application for a given ID.
     */
    protected Context applicationContext(int id) {
        try {
            return((Context)this.applications.elementAt(id));
        } catch (ArrayIndexOutOfBoundsException e) {
            return(null);
        }
    }

    /**
     * Create (or allocate) and return a Request object suitable for
     * specifying the contents of a Request to the responsible Container.
     */
    public Request createRequest() {
        return(null);
    }

    /**
     * Create (or allocate) and return a Response object suitable for
     * receiving the contents of a Response from the responsible Container.
     */
    public Response createResponse() {
        return(null);
    }

    /**
     * Start accepting WARP requests from the network.
     */
    public void run() {
        // Start accepting connections
        try {
            while (this.isStarted()) {
                Socket sock=this.server.accept();
                InetAddress raddr=sock.getInetAddress();
                InetAddress laddr=sock.getLocalAddress();
                int rport=sock.getPort();
                int lport=sock.getLocalPort();
                logger.log("Connection from "+raddr+":"+rport+" to "+laddr+
                           ":"+lport);
                WarpConnection conn=new WarpConnection();
                conn.setConnector(this);
                conn.setSocket(sock);
                this.addLifecycleListener(conn);
                conn.start();
            }
        } catch (IOException e) {
            logger.log("Error accepting requests",e);
        }
    }
}
