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
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleEvent;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.util.LifecycleSupport;

/**
 *
 * @author <a href="mailto:pier.fumagalli@eng.sun.com">Pier Fumagalli</a>
 * @author Copyright &copy; 1999, 2000 <a href="http://www.apache.org">The
 *         Apache Software Foundation.
 * @version CVS $Id$
 */
public class WarpConnection implements Lifecycle, Runnable {

    // -------------------------------------------------------------- CONSTANTS

    /** Our debug flag status (Used to compile out debugging information). */
    private static final boolean DEBUG=WarpDebug.DEBUG;

    // -------------------------------------------------------- LOCAL VARIABLES

    /** The lifecycle event support for this component. */
    private LifecycleSupport lifecycle=null;
    /** The WarpHandlerTable contains the list of all current handlers. */
    private WarpHandlerTable table=null;
    /** The name of this connection. */
    private String name=null;
    /** Wether we started or not. */
    private boolean started=false;
    /** The number of active connections. */
    private static int num=0;

    // -------------------------------------------------------- BEAN PROPERTIES

    /** The socket used in this connection. */
    private Socket socket=null;
    /** The connector wich created this connection. */
    private WarpConnector connector=null;

    // ------------------------------------------------------------ CONSTRUCTOR

    /**
     * Create a new WarpConnection instance.
     */
    public WarpConnection() {
        super();
        this.lifecycle=new LifecycleSupport(this);
        this.table=new WarpHandlerTable();
        if (DEBUG) this.debug("New instance created");
    }

    // --------------------------------------------------------- PUBLIC METHODS

    /**
     * Run the thread waiting on the socket, reading packets from the client
     * and dispatching them to the appropriate handler.
     */
    public void run() {
        WarpHandler han=null;
        InputStream in=null;
        int rid=0;
        int typ=0;
        int len=0;
        int ret=0;
        int b1=0;
        int b2=0;
        byte buf[]=null;

        // Log the connection opening
        num++;
        if (DEBUG) this.debug("Connection started (num="+num+") "+this.name);

        try {
            // Open the socket InputStream
            in=this.socket.getInputStream();

            // Read packets
            while(this.started) {
                // RID number
                b1=in.read();
                b2=in.read();
                if ((b1 | b2)==-1) {
                    this.log("Premature RID end");
                    break;
                }
                rid=(((b1 & 0x0ff) << 8) | (b2 & 0x0ff));
                // Packet type
                b1=in.read();
                b2=in.read();
                if ((b1 | b2)==-1) {
                    this.log("Premature TYPE end");
                    break;
                }
                typ=(((b1 & 0x0ff) << 8) | (b2 & 0x0ff));
                // Packet payload length
                b1=in.read();
                b2=in.read();
                if ((b1 | b2)==-1) {
                    this.log("Premature LEN end");
                    break;
                }
                len=(((b1 & 0x0ff) << 8) | (b2 & 0x0ff));
                // Packet payload
                buf=new byte[len];
                if ((ret=in.read(buf,0,len))!=len) {
                    this.log("Premature packet end"+" ("+ret+" of "+len+")");
                    break;
                }

                if (DEBUG) this.debug("Received packet RID="+rid+" TYP="+typ);

                // Check if we have the special RID 0x0ffff (disconnect)
                if (rid==0x0ffff) {
                    this.log("Connection closing ("+new String(buf)+")");
                    break;
                }

                // Dispatch packet
                synchronized (this) { han=this.table.get(rid); }
                if (han==null) {
                    this.log("Handler for RID "+rid+" not found");
                    break;
                }
                han.processData(typ,buf);
            }
        } catch (IOException e) {
            if (this.started) e.printStackTrace(System.err);
        }

        // Close this connection before terminating the thread
        try {
            this.stop();
        } catch (LifecycleException e) {
            this.log(e);
        }
        num--;
        if (DEBUG) this.debug("Connection ended (num="+num+") "+this.name);
    }

    /**
     * Initialize this connection.
     *
     * @param sock The socket used by this connection to transfer data.
     */
    public void start()
    throws LifecycleException {
        // Paranoia checks.
        if (this.socket==null)
            throw new LifecycleException("Null socket");
        if (this.connector==null)
            throw new LifecycleException("Null connector");

        // Register the WarpConnectionHandler for RID=0 (connection)
        this.started=true;
        WarpHandler h=new WarpConnectionHandler();
        h.setConnection(this);
        h.setRequestID(0);
        h.start();
        // Paranoia check
        if(this.registerHandler(h,0)!=true) {
            this.stop();
            throw new LifecycleException("Cannot register connection handler");
        }
        // Set the thread and connection name and start the thread
        this.name=this.socket.getInetAddress().getHostAddress();
        this.name=this.name+":"+this.socket.getPort();
        new Thread(this,name).start();
    }

    /**
     * Send a WARP packet.
     */
    public void send(int rid, int type, byte buffer[], int offset, int len)
    throws IOException {
        if (this.socket==null) throw new IOException("Connection closed "+type);
        OutputStream out=this.socket.getOutputStream();
        byte hdr[]=new byte[6];
        // Set the RID number
        hdr[0]=(byte)((rid>>8)&0x0ff);
        hdr[1]=(byte)(rid&0x0ff);
        // Set the TYPE
        hdr[2]=(byte)((type>>8)&0x0ff);
        hdr[3]=(byte)(type&0x0ff);
        // Set the payload length
        hdr[4]=(byte)((len>>8)&0x0ff);
        hdr[5]=(byte)(len&0x0ff);
        // Send the header and payload
        synchronized(this) {
            out.write(hdr,0,6);
            out.write(buffer,offset,len);
            out.flush();
        }
        if (DEBUG) this.debug("Sending packet RID="+rid+" TYP="+type);
    }

    /**
     * Close this connection.
     */
    public void stop()
    throws LifecycleException {
        this.started=false;
        // Stop all handlers
        WarpHandler handlers[]=this.table.handlers();
        for (int x=0; x<handlers.length; x++) handlers[x].stop();
        // Close the socket (this will make the thread exit)
        if (this.socket!=null) try {
            this.socket.close();
        } catch (IOException e) {
            this.log(e);
            throw new LifecycleException("Closing connection "+this.name,e);
        }

        this.socket=null;
        // Log this step
        this.log("Connection closed");
    }

    /**
     * Add a WarpHandler to this connection.
     *
     * @param han The WarpHandler add to this connection.
     * @param rid The RID number associated with the WarpHandler.
     * @return If another WarpHandler is associated with this RID return
     *         false, otherwise return true.
     */
    protected synchronized boolean registerHandler(WarpHandler han, int rid) {
        if (DEBUG) this.debug("Registering handler for RID "+rid);
        return(this.table.add(han, rid));
    }

    /**
     * Remove a WarpHandler from this connection.
     *
     * @param rid The RID number associated with the WarpHandler to remove.
     * @return The old WarpHandler associated with the specified RID or null.
     */
    protected synchronized WarpHandler removeHandler(int rid) {
        return(this.table.remove(rid));
    }

    // ----------------------------------------------------------- BEAN METHODS

    /**
     * Return the socket associated with this connection.
     */
    protected WarpConnector getConnector() {
        return(this.connector);
    }

    /**
     * Set the socket used by this connection.
     */
    protected void setConnector(WarpConnector connector) {
        if (DEBUG) this.debug("Setting connector");
        this.connector=connector;
    }

    /**
     * Return the socket associated with this connection.
     */
    protected Socket getSocket() {
        return(this.socket);
    }

    /**
     * Set the socket used by this connection.
     */
    protected void setSocket(Socket socket) {
        if (DEBUG) this.debug("Setting socket");
        this.socket=socket;
    }

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

    // ------------------------------------------ LOGGING AND DEBUGGING METHODS

    /**
     * Dump a log message.
     */
    public void log(String msg) {
        if (this.connector!=null) this.connector.log(msg);
        else WarpDebug.debug(this,msg);
    }

    /**
     * Dump information for an Exception.
     */
    public void log(Exception exc) {
        if (this.connector!=null) this.connector.log(exc);
        else WarpDebug.debug(this,exc);
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
