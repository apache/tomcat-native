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

/**
 *
 *
 * @author <a href="mailto:pier.fumagalli@eng.sun.com">Pier Fumagalli</a>
 * @author Copyright &copy; 1999, 2000 <a href="http://www.apache.org">The
 *         Apache Software Foundation.
 * @version CVS $Id$
 */
public class WarpConnection implements Runnable {

    /** The DEBUG flag, to compile out debugging informations. */
    private static final boolean DEBUG = WarpConstants.DEBUG;

    private WarpHandlerTable table;
    private Socket socket;
    private String name;
    
    public WarpConnection() {
        super();
        this.table=new WarpHandlerTable();
        this.socket=null;
        this.name=null;
    }

    /**
     *
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
        this.log("Connection opened");

        try {
            // Open the socket InputStream
            in=this.socket.getInputStream();
            
            // Read packets
            while(true) {
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
                    this.log("Premature LENGTH end");
                    break;
                }
                len=(((b1 & 0x0ff) << 8) | (b2 & 0x0ff));
                // Packet payload
                buf=new byte[len];
                if ((ret=in.read(buf,0,len))!=len) {
                    this.log("Premature PAYLOAD end ("+ret+" of "+len+")");
                    break;
                }
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
            this.log(e);
        }

        // Close this connection before terminating the thread
        this.close();
        if (DEBUG) this.debug("Thread exited");
    }

    /**
     * Initialize this connection.
     *
     * @param sock The socket used by this connection to transfer data.
     */
    public void init(Socket sock) {
        // Paranoia checks.
        if (sock==null) throw new NullPointerException("Null Socket");
        this.socket=sock;
        
        // Register the WarpConnectionHandler for RID=0 (connection)
        WarpHandler h=new WarpConnectionHandler();
        h.init(this,0);
        // Paranoia check
        if(this.registerHandler(h,0)!=true) {
            System.err.println("Something happened creating the connection");
            this.close();
            return;
        }
        // Set the thread and connection name and start the thread
        this.name=sock.getInetAddress().getHostAddress()+":"+sock.getPort();
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
    }

    /**
     * Close this connection.
     * <br>
     * The socket associated with this connection is closed, all handlers are
     * stopped and the thread reading from the socket is interrupted.
     */
    public void close() {
        // Stop all handlers
        WarpHandler handlers[]=this.table.handlers();
        for (int x=0; x<handlers.length; x++) handlers[x].stop();
        // Close the socket (this will make the thread exit)
        if (this.socket!=null) try {
            this.socket.close();
        } catch (IOException e) {
            this.log(e);
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

    /**
     * Log a message.
     *
     * @param msg The error message to log.
     */
    public void log(String msg) {
        System.out.println("[WarpConnection("+this.name+")] "+msg);
        System.out.flush();
    }

    /**
     * Log an exception.
     *
     * @param e The exception to log.
     */
    public void log(Exception e) {
        System.out.print("[WarpConnection("+this.name+")] ");
        e.printStackTrace(System.out);
        System.out.flush();
    }

    /**
     * Dump some debugging information.
     *
     * @param msg The error message to log.
     */
    public void debug(String msg) {
        if(DEBUG) this.log(msg);
    }
}
    