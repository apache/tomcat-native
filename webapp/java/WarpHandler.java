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

import java.io.IOException;
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleEvent;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.util.LifecycleSupport;

/**
 *
 *
 * @author <a href="mailto:pier.fumagalli@eng.sun.com">Pier Fumagalli</a>
 * @author Copyright &copy; 1999, 2000 <a href="http://www.apache.org">The
 *         Apache Software Foundation.
 * @version CVS $Id$
 */
public abstract class WarpHandler implements Lifecycle, Runnable {

    // -------------------------------------------------------------- CONSTANTS

    /** Our debug flag status (Used to compile out debugging information). */
    protected static final boolean DEBUG=true; //WarpDebug.DEBUG;

    // -------------------------------------------------------- LOCAL VARIABLES

    /** The lifecycle event support for this component. */
    private LifecycleSupport lifecycle=null;
    /** The WARP packet type. */
    private int type=-1;
    /** The WARP packet payload. */
    private byte buffer[]=null;
    /** Wether type and buffer have already been processed by notifyData(). */
    private boolean processed=false;
    /** Wether the stop() method was called. */
    private boolean stopped=false;
    /** The name of this handler. */
    private String name=null;

    // -------------------------------------------------------- BEAN PROPERTIES

    /** The WarpConnection under which this handler is registered. */
    private WarpConnection connection=null;
    /** The WARP RID associated with this WarpHandler. */
    private int rid=-1;

    // ------------------------------------------------------------ CONSTRUCTOR

    /**
     * Construct a new instance of a WarpHandler.
     */
    public WarpHandler() {
        super();
        this.lifecycle=new LifecycleSupport(this);
        if (DEBUG) this.debug("New instance created");
    }

    // --------------------------------------------------------- PUBLIC METHODS

    /**
     * Process WARP packets.
     * <br>
     * This method will wait for data to be notified by a WarpConnection with
     * the processData() method, and will handle it to the abstract process()
     * method.
     * <br>
     * When process() returns false (meaning that there is no more
     * data to process) this method will unregister this handler from the
     * WarpConnection, and the thread created in init() will terminate.
     */
    public final synchronized void run() {
        while(true) {
            try {
                // Wait for this thread to be notified by processData().
                while (this.processed==true) {
                    // Check if we were notified by stop()
                    this.wait();
                    if (this.stopped) {
                        this.processed=true;
                        this.notifyAll();
                        this.connection.removeHandler(this.rid);
                        if (DEBUG) this.debug("Thread exited");
                    }
                }
            } catch (InterruptedException e) {
                // Uh-oh.. Something bad happened. Drop this handler.
                this.connection.removeHandler(this.rid);
                return;
            }

            // If processed is set to true, it means that somehow we were
            // not notified by the processData() method. So we just ignore
            // and go back to sleep.
            if (this.processed) continue;

            // The processed flag is set to false, so we need to handle the
            // type and buffer to the process() method. If this method
            // returns true, we have some more packets to process before
            // terminating this method.
            if (process(this.type, this.buffer)) {
                // The process method returned true, meaning that we still have
                // some other packets to wait for this WARP RID. Set the
                // processed flag to true and notify the thread that might be
                // waiting in the processData() method, then get back to sleep.
                this.processed=true;
                this.notifyAll();
                continue;
            } else {
                // The process() method returned false, meaning that our work
                // is done. Let's get out.
                break;
            }
        }
        // We got out of the loop (the process() method returned false). Let's
        // notify all other threads (just in case after a packet is processed
        // another one for the same handler has been received), unregister
        // this handler and terminate.
        this.processed=true;
        this.notifyAll();
        this.connection.removeHandler(this.rid);
        if (DEBUG) this.debug("Thread exited");
    }

    /**
     * Initialize this handler instance.
     * <br>
     * This method will initialize this handler and starts the despooling
     * thread that will handle the data to the processData() method.
     * <br>
     * NOTE: To receive data notification this WarpConnection must be
     * registered in the WarpConnection. Only un-registration (when the thread
     * exits) is performed.
     */
    public final void start()
    throws LifecycleException {
        if (this.connection==null)
            throw new LifecycleException("Null connection");
        if (this.rid<0)
            throw new LifecycleException("Invalid handler request ID");

        this.processed=true;
        this.stopped=false;
        String n=this.getClass().getName();
        this.name=n.substring(n.lastIndexOf('.')+1)+"[RID="+rid+"]";
        new Thread(this,this.name).start();
    }

    /**
     * Stop this handler.
     * <br>
     * The thread associated with this handler is stopped and this instance is
     * unregistered from the WarpConnection.
     */
    public final synchronized void stop() {
        // Set the stopped flag and notify all threads
        if (DEBUG) this.debug("Stopping");
        this.stopped=true;
        this.notifyAll();
        this.connection.removeHandler(this.rid);
        if (DEBUG) this.debug("Stopped");
    }

    /**
     * Receive notification of data from a WarpConnection.
     *
     * @param type The WARP packet type.
     * @param buffer The WARP packet payload.
     */
    protected final synchronized void processData(int type, byte buffer[]) {
        // Check if the run() method already handled the previous packet to
        // the process() method. If not so, wait until that happens.
        while(this.processed==false) try {
            this.wait();
        } catch (InterruptedException e) {
            return;
        }
        // We were notified and the processed flag is set to true, meanging
        // that the previous packet was successfully despooled by the run()
        // method, so we can safely set the type and buffer in our local
        // variables.
        this.type=type;
        this.buffer=buffer;
        // Set the processed flag to false, and notify the thread waiting in
        // the run() method that another packet is ready to process.
        this.processed=false;
        this.notifyAll();
    }

    // -------------------------------------------- PACKET TRANSMISSION METHODS

    /**
     * Send a WARP packet.
     * <br>
     * This method will send a WARP packet back to the client, invoking
     * the send() method in the WarpConnection associated with this
     * WarpHandler with the appropriate WARP RID.
     *
     * @param type The WARP packet type.
     * @param buffer The buffer containing the data to send.
     * @param off The offset in the buffer of the data to send.
     * @param len The number of bytes to send.
     * @exception IOException If something happened sending the data.
     */
    public final void send(int type, byte buffer[], int offset, int len)
    throws IOException {
        this.connection.send(this.rid, type, buffer, offset, len);
    }

    /**
     * Send a WARP packet.
     * <br>
     * This method will send a WARP packet back to the client, invoking
     * the send() method in the WarpConnection associated with this
     * WarpHandler with the appropriate WARP RID.
     *
     * @param type The WARP packet type.
     * @param packet The packet to write.
     * @exception IOException If something happened sending the data.
     */
    public final void send(int type, WarpPacket packet)
    throws IOException {
        this.send(type,packet.getBuffer(),0,packet.getLength());
    }

    /**
     * Send a WARP packet.
     * <br>
     * This method is equivalent to send(type, buffer, 0, buffer.length).
     *
     * @param type The WARP packet type.
     * @param buffer The buffer containing the data to send.
     * @exception IOException If something happened sending the data.
     */
    public final void send(int type, byte buffer[])
    throws IOException {
        this.send(type, buffer, 0, buffer.length);
    }

    // ------------------------------------------------------- ABSTRACT METHODS

    /**
     * Process a WARP packet.
     * <br>
     * This method is the one which will actually perform the operation of
     * analyzing the packet and doing whatever needs to be done.
     * <br>
     * This method will return true if another packet is expected for this
     * RID, or it will return false if this was the last packet for this RID.
     * When we return false this handler is unregistered, and the Thread
     * started in the init() method is terminated.
     *
     * @param type The WARP packet type.
     * @param buffer The WARP packet payload.
     * @return If more packets are expected for this RID, true is returned,
     *         false if this was the last packet.
     */
    public abstract boolean process(int type, byte buffer[]);

    // ------------------------------------------------------ LIFECYCLE METHODS

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

    // ----------------------------------------------------------- BEAN METHODS

    /**
     * Return the WarpConnection associated with this handler.
     */
    protected WarpConnection getConnection() {
        return(this.connection);
    }

    /**
     * Return the WarpConnector associated with this handler.
     */
    protected WarpConnector getConnector() {
        return(this.connection.getConnector());
    }

    /**
     * Set the WarpConnection associated with this handler.
     */
    protected void setConnection(WarpConnection connection) {
        if (DEBUG) this.debug("Setting connection");
        this.connection=connection;
    }

    /**
     * Return the Request ID associated with this handler.
     */
    protected int getRequestID() {
        return(this.rid);
    }

    /**
     * Set the Request ID associated with this handler.
     */
    protected void setRequestID(int rid) {
        if (DEBUG) this.debug("Setting Request ID "+rid);
        this.rid=rid;
    }

    // ------------------------------------------ LOGGING AND DEBUGGING METHODS

    /**
     * Log a message.
     *
     * @param msg The error message to log.
     */
    protected void log(String msg) {
        WarpConnection connection=this.getConnection();
        if (connection!=null) connection.log(msg);
        else WarpDebug.debug(this,msg);
    }

    /**
     * Log an exception.
     *
     * @param e The exception to log.
     */
    protected void log(Exception exc) {
        WarpConnection connection=this.getConnection();
        if (connection!=null) connection.log(exc);
        else WarpDebug.debug(this,exc);
    }

    /**
     * Dump a debug message.
     */
    protected void debug(String msg) {
        if (DEBUG) WarpDebug.debug(this,msg);
    }

    /**
     * Dump information for an Exception.
     */
    protected void debug(Exception exc) {
        if (DEBUG) WarpDebug.debug(this,exc);
    }
}
