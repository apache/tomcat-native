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


package org.apache.ajp.tomcat4;


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
import org.apache.catalina.util.RequestUtil;
import org.apache.catalina.util.LifecycleSupport;
import org.apache.catalina.util.StringManager;
import org.apache.catalina.util.StringParser;

import org.apache.ajp.Ajp13;
import org.apache.tomcat.util.http.BaseRequest;

/**
 * @author Kevin Seguin
 * @version $Revision$ $Date$
 */

final class Ajp13Processor
    implements Lifecycle, Runnable {


    // ----------------------------------------------------------- Constructors


    /**
     * Construct a new Ajp13Processor associated with the specified connector.
     *
     * @param connector Ajp13Connector that owns this processor
     * @param id Identifier of this Ajp13Processor (unique per connector)
     */
    public Ajp13Processor(Ajp13Connector connector, int id) {

	super();
	this.connector = connector;
	this.debug = connector.getDebug();
	this.id = id;
	this.request = (Ajp13Request) connector.createRequest();
        this.request.setConnector(connector);
        this.request.setConnector(connector);
	this.response = (Ajp13Response) connector.createResponse();
        this.response.setConnector(connector);
	this.threadName =
	  "Ajp13Processor[" + connector.getPort() + "][" + id + "]";

        this.logger.setConnector(connector);
        this.logger.setName(this.threadName);
    }


    // ----------------------------------------------------- Instance Variables

    private Ajp13Logger logger = new Ajp13Logger();
    private BaseRequest ajpRequest = new BaseRequest();

    /**
     * Is there a new socket available?
     */
    private boolean available = false;


    /**
     * The Ajp13Connector with which this processor is associated.
     */
    private Ajp13Connector connector = null;


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
     * The AJP13 request object we will pass to our associated container.
     */
    private Ajp13Request request = null;


    /**
     * The AJP13 response object we will pass to our associated container.
     */
    private Ajp13Response response = null;


    /**
     * The string manager for this package.
     */
    protected StringManager sm =
	StringManager.getManager(Constants.PACKAGE);


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
	    logger.log(" An incoming request is being assigned");

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
	    logger.log("  The incoming request has been awaited");

	return (socket);

    }

    /**
     * Parse and record the connection parameters related to this request.
     *
     * @param socket The socket on which we are connected
     *
     * @exception IOException if an input/output error occurs
     * @exception ServletException if a parsing error occurs
     */
    private void parseConnection(Socket socket)
        throws IOException, ServletException {

	if (debug >= 2)
	    logger.log("  parseConnection: address=" + socket.getInetAddress() +
		", port=" + connector.getPort());
	request.setServerPort(connector.getPort());
        request.setSocket(socket);

    }

    /**
     * Process an incoming AJP13 request on the Socket that has been assigned
     * to this Processor.  Any exceptions that occur during processing must be
     * swallowed and dealt with.
     *
     * @param socket The socket on which we are connected to the client
     */
    private void process(Socket socket) {

        Ajp13 ajp13 = new Ajp13();
        ajp13.setDebug(debug);
        Ajp13InputStream input = new Ajp13InputStream(ajp13);
        Ajp13OutputStream output = new Ajp13OutputStream(ajp13);
        response.setAjp13(ajp13);

        try {
            ajp13.setSocket(socket);
        } catch (IOException e) {
            logger.log("process: ajp13.setSocket", e);
        }

        boolean moreRequests = true;
        while (moreRequests) {
            
            int status = 0;
            try {
                status = ajp13.receiveNextRequest(ajpRequest);                
            } catch (IOException e) {
                logger.log("process: ajp13.receiveNextRequest", e);
            }
            
            if( status==-2) {
                // special case - shutdown
                // XXX need better communication, refactor it
//                  if( !doShutdown(socket.getLocalAddress(),
//                                  socket.getInetAddress())) {
//                      moreRequests = false;
//                      continue;
//                  }
                break;
            }
            
            if( status != 200 )
                break;

            try {
                // set up request
                request.setAjpRequest(ajpRequest);
                request.setResponse(response);
                request.setStream(input);
                
                // setup response
                response.setRequest(request);
                response.setStream(output);
                
                if (this.debug > 0) {
                    logger.log("invoking...");
                }
                connector.getContainer().invoke(request, response);

                response.finishResponse();
                request.finishRequest();

            } catch (Exception e) {
                logger.log("process: invoke", e);
            }

            // Recycling the request and the response objects
            ajpRequest.recycle();
            request.recycle();
            response.recycle();

            // recycle ajp13 object
            ajp13.recycle();
        }
        
	try {
            ajp13.close();
	} catch (IOException e) {
	    logger.log("process: ajp13.close", e);
	}

	try {
            socket.close();
	} catch (IOException e) {
	    logger.log("process: socket.close", e);
	}
	socket = null;

        logger.log("process:  done");
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

	logger.log(sm.getString("ajp13Processor.starting"));

	thread = new Thread(this, threadName);
	thread.setDaemon(true);
	thread.start();

	if (debug >= 1)
	    logger.log(" Background thread has been started");

    }


    /**
     * Stop the background processing thread.
     */
    private void threadStop() {

	logger.log(sm.getString("ajp13Processor.stopping"));

	stopped = true;
        assign(null);
	synchronized (threadSync) {
	    try {
		threadSync.wait(5000);
	    } catch (InterruptedException e) {
		;
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
        return null; // FIXME: lifecycle.findLifecycleListeners();
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
		(sm.getString("ajp13Processor.alreadyStarted"));
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
		(sm.getString("ajp13Processor.notStarted"));
	lifecycle.fireLifecycleEvent(STOP_EVENT, null);
	started = false;

	threadStop();

    }


}
