/* * $Header$
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


import java.io.BufferedInputStream;
import java.io.EOFException;
import java.io.InterruptedIOException;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Locale;
import java.util.StringTokenizer;
import java.util.TreeMap;
import javax.servlet.ServletException;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.coyote.Adapter;
import org.apache.coyote.InputBuffer;
import org.apache.coyote.OutputBuffer;
import org.apache.coyote.Processor;
import org.apache.coyote.Request;
import org.apache.coyote.Response;

import org.apache.catalina.Connector;
import org.apache.catalina.Container;
import org.apache.catalina.Globals;
import org.apache.catalina.HttpRequest;
import org.apache.catalina.HttpResponse;
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleEvent;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.Logger;
import org.apache.catalina.util.LifecycleSupport;
import org.apache.catalina.util.RequestUtil;
import org.apache.catalina.util.StringManager;
import org.apache.catalina.util.StringParser;


/**
 * Implementation of a request processor which delegates the processing to a
 * Coyote processor.
 *
 * @author Craig R. McClanahan
 * @author Remy Maucherat
 * @version $Revision$ $Date$
 */

final class CoyoteProcessor
    implements Lifecycle, Runnable, Adapter {


    // ----------------------------------------------------------- Constructors


    /**
     * Construct a new CoyoteProcessor associated with the specified connector.
     *
     * @param connector CoyoteConnector that owns this processor
     * @param id Identifier of this CoyoteProcessor (unique per connector)
     */
    public CoyoteProcessor(CoyoteConnector connector, int id) {

        super();
        this.connector = connector;
        this.debug = connector.getDebug();
        this.id = id;
        this.proxyName = connector.getProxyName();
        this.proxyPort = connector.getProxyPort();
        this.request = (CoyoteRequest) connector.createRequest();
        this.response = (CoyoteResponse) connector.createResponse();
        this.serverPort = connector.getPort();
        this.threadName =
            "CoyoteProcessor[" + connector.getPort() + "][" + id + "]";

    }


    // ----------------------------------------------------- Instance Variables


    /**
     * Is there a new socket available?
     */
    private boolean available = false;


    /**
     * The CoyoteConnector with which this processor is associated.
     */
    private CoyoteConnector connector = null;


    /**
     * Coyote processor.
     */
    private Processor processor = null;


    /**
     * The debugging detail level for this component.
     */
    private int debug = 0;


    /**
     * The identifier of this processor, unique per connector.
     */
    private int id = 0;


    /**
     * The lifecycle event support for this component.
     */
    private LifecycleSupport lifecycle = new LifecycleSupport(this);


    /**
     * The match string for identifying a session ID parameter.
     */
    private static final String match =
        ";" + Globals.SESSION_PARAMETER_NAME + "=";


    /**
     * The match string for identifying a session ID parameter.
     */
    private static final char[] SESSION_ID = match.toCharArray();


    /**
     * The string parser we will use for parsing request lines.
     */
    private StringParser parser = new StringParser();


    /**
     * The proxy server name for our Connector.
     */
    private String proxyName = null;


    /**
     * The proxy server port for our Connector.
     */
    private int proxyPort = 0;


    /**
     * The Coyote request object we will pass to our associated container.
     */
    private CoyoteRequest request = null;


    /**
     * The Coyote response object we will pass to our associated container.
     */
    private CoyoteResponse response = null;


    /**
     * The actual server port for our Connector.
     */
    private int serverPort = 0;


    /**
     * The string manager for this package.
     */
    protected StringManager sm =
        StringManager.getManager(Constants.Package);


    /**
     * The socket we are currently processing a request for.  This object
     * is used for inter-thread communication only.
     */
    private Socket socket = null;


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
     * Processor state
     */
    private int status = Constants.PROCESSOR_IDLE;


    // --------------------------------------------------------- Public Methods


    /**
     * Return a String value representing this object.
     */
    public String toString() {

        return (this.threadName);

    }


    // -------------------------------------------------------- Package Methods


    /**
     * Process an incoming TCP/IP connection on the specified socket.  Any
     * exception that occurs during processing must be logged and swallowed.
     * <b>NOTE</b>:  This method is called from our Connector's thread.  We
     * must assign it to our own thread so that multiple simultaneous
     * requests can be handled.
     *
     * @param socket TCP socket to process
     */
    synchronized void assign(Socket socket) {

        // Wait for the Processor to get the previous Socket
        while (available) {
            try {
                wait();
            } catch (InterruptedException e) {
            }
        }

        // Store the newly available Socket and notify our thread
        this.socket = socket;
        available = true;
        notifyAll();

        if ((debug >= 1) && (socket != null))
            log(" An incoming request is being assigned");

    }


    // -------------------------------------------------------- Private Methods


    /**
     * Await a newly assigned Socket from our Connector, or <code>null</code>
     * if we are supposed to shut down.
     */
    private synchronized Socket await() {

        // Wait for the Connector to provide a new Socket
        while (!available) {
            try {
                wait();
            } catch (InterruptedException e) {
            }
        }

        // Notify the Connector that we have received this Socket
        Socket socket = this.socket;
        available = false;
        notifyAll();

        if ((debug >= 1) && (socket != null))
            log("  The incoming request has been awaited");

        return (socket);

    }



    /**
     * Log a message on the Logger associated with our Container (if any)
     *
     * @param message Message to be logged
     */
    private void log(String message) {

        Logger logger = connector.getContainer().getLogger();
        if (logger != null)
            logger.log(threadName + " " + message);

    }


    /**
     * Log a message on the Logger associated with our Container (if any)
     *
     * @param message Message to be logged
     * @param throwable Associated exception
     */
    private void log(String message, Throwable throwable) {

        Logger logger = connector.getContainer().getLogger();
        if (logger != null)
            logger.log(threadName + " " + message, throwable);

    }


    /**
     * Process an incoming HTTP request on the Socket that has been assigned
     * to this Processor.  Any exceptions that occur during processing must be
     * swallowed and dealt with.
     *
     * @param socket The socket on which we are connected to the client
     */
    private void process(Socket socket) {

        InputStream input = null;
        OutputStream output = null;

        try {
            input = socket.getInputStream();
            output = socket.getOutputStream();
            processor.process(input, output);
        } catch (Throwable t) {
            log(sm.getString("coyoteProcessor.process"), t);
        }

        try {
            int available = input.available();
            // skip any unread (bogus) bytes
            if (available > 0) {
                input.skip(available);
            }
        } catch (Exception e) {
            ;
        }
        try {
            socket.close();
        } catch (IOException e) {
            ;
        }
        socket = null;

    }


    /**
     * Service method.
     */
    public void service(Request req, Response res)
        throws Exception {

        // Wrapping the Coyote requests
        request.setCoyoteRequest(req);
        response.setCoyoteResponse(res);

        try {
            // Calling the container
            //connector.getContainer().invoke(request, response);
        } finally {
            // Recycle the wrapper request and response
            request.recycle();
            response.recycle();
        }

    }


    // ---------------------------------------------- Background Thread Methods


    /**
     * The background thread that listens for incoming TCP/IP connections and
     * hands them off to an appropriate processor.
     */
    public void run() {

        // Process requests until we receive a shutdown signal
        while (!stopped) {

            // Wait for the next socket to be assigned
            Socket socket = await();
            if (socket == null)
                continue;

            // Process the request from this socket
            process(socket);

            // Finish up this request
            connector.recycle(this);

        }

        // Tell threadStop() we have shut ourselves down successfully
        synchronized (threadSync) {
            threadSync.notifyAll();
        }

    }


    /**
     * Start the background processing thread.
     */
    private void threadStart() {

        log(sm.getString("httpProcessor.starting"));

        thread = new Thread(this, threadName);
        thread.setDaemon(true);
        thread.start();

        if (debug >= 1)
            log(" Background thread has been started");

    }


    /**
     * Stop the background processing thread.
     */
    private void threadStop() {

        log(sm.getString("httpProcessor.stopping"));

        stopped = true;
        assign(null);

        if (status != Constants.PROCESSOR_IDLE) {
            // Only wait if the processor is actually processing a command
            synchronized (threadSync) {
                try {
                    threadSync.wait(5000);
                } catch (InterruptedException e) {
                    ;
                }
            }
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
     * Start the background thread we will use for request processing.
     *
     * @exception LifecycleException if a fatal startup error occurs
     */
    public void start() throws LifecycleException {

        if (started)
            throw new LifecycleException
                (sm.getString("httpProcessor.alreadyStarted"));

        // Instantiate the Coyote processor
        String className = connector.getProcessorClassName();
        try {
            Class clazz = Class.forName(className);
            processor = (Processor)  clazz.newInstance();
        } catch (Exception e) {
            throw new LifecycleException
                (sm.getString("coyoteProcessor.processorInstantiationFailed", e));
        }
        processor.setAdapter(this);

        lifecycle.fireLifecycleEvent(START_EVENT, null);
        started = true;

        threadStart();

    }


    /**
     * Stop the background thread we will use for request processing.
     *
     * @exception LifecycleException if a fatal shutdown error occurs
     */
    public void stop() throws LifecycleException {

        if (!started)
            throw new LifecycleException
                (sm.getString("httpProcessor.notStarted"));
        lifecycle.fireLifecycleEvent(STOP_EVENT, null);
        started = false;

        threadStop();

    }


}
