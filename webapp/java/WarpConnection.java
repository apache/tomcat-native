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
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;

import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleEvent;
import org.apache.catalina.LifecycleListener;

public class WarpConnection implements LifecycleListener, Runnable {

    /* ==================================================================== */
    /* Instance variables                                                   */
    /* ==================================================================== */

    /* -------------------------------------------------------------------- */
    /* Local variables */

    /** Our socket input stream. */
    private InputStream input=null;
    /** Our socket output stream. */
    private OutputStream output=null;
    /** The started flag. */
    private boolean started=false;
    /** The local thread. */
    private Thread thread=null;

    /* -------------------------------------------------------------------- */
    /* Bean variables */

    /** The socket this connection is working on. */
    private Socket socket=null;
    /** The connector instance. */
    private WarpConnector connector=null;

    /* ==================================================================== */
    /* Constructor                                                          */
    /* ==================================================================== */

    /**
     * Construct a new instance of a <code>WarpConnection</code>.
     */
    public WarpConnection() {
        super();
    }

    /* ==================================================================== */
    /* Bean methods                                                         */
    /* ==================================================================== */

    /**
     * Set the socket this connection is working on.
     */
    public void setSocket(Socket socket) {
        this.socket=socket;
    }

    /**
     * Return the socket this connection is working on.
     */
    public Socket getSocket() {
        return(this.socket);
    }

    /**
     * Set the <code>WarpConnector</code> associated with this connection.
     */
    public void setConnector(WarpConnector connector) {
        this.connector=connector;
    }

    /**
     * Return the <code>WarpConnector</code> associated with this connection.
     */
    public WarpConnector getConnector() {
        return(this.connector);
    }

    /* ==================================================================== */
    /* Lifecycle methods                                                    */
    /* ==================================================================== */

    /**
     * Get notified of events in the connector.
     */
    public void lifecycleEvent(LifecycleEvent event) {
        if (Lifecycle.STOP_EVENT.equals(event.getType())) this.stop();
    }

    /**
     * Start working on this connection.
     */
    public void start() {
        synchronized(this) {
            this.started=true;
            this.thread=new Thread(this);
            this.thread.start();
        }
    }

    /**
     * Stop all we're doing on the connection.
     */
    public void stop() {
        synchronized(this) {
            try {
                this.started=false;
                this.socket.close();
                this.getConnector().removeLifecycleListener(this);
            } catch (IOException e) {
                this.getConnector().log(this,"Cannot close socket",e);
            }
        }
    }

    /**
     * Process data from the socket.
     */
    public void run() {
        if (Constants.DEBUG)
            this.getConnector().debug(this,"Connection starting");

        try {
            this.input=this.socket.getInputStream();
            this.output=this.socket.getOutputStream();
            boolean success=new WarpConfigurationHandler().handle(this);

            this.stop();
        } catch (IOException e) {
            this.getConnector().log(this,"Exception on socket",e);
        }

        if (Constants.DEBUG)
            this.getConnector().debug(this,"Connection terminated");
    }

    /* ==================================================================== */
    /* Public methods                                                       */
    /* ==================================================================== */

    /**
     * Send a WARP packet over this connection.
     */
    public void send(WarpPacket packet)
    throws IOException {
        if (Constants.DEBUG) {
            this.getConnector().debug(this,">> TYPE="+packet.getType()+
                                           " LENGTH="+packet.size);
            this.getConnector().debug(this,">> "+packet.dump());
        }

        this.output.write(packet.getType()&0x0ff);
        this.output.write((packet.size>>8)&0x0ff);
        this.output.write((packet.size>>0)&0x0ff);
        this.output.write(packet.buffer,0,packet.size);
        this.output.flush();
        packet.reset();
    }

    /**
     * Receive a WARP packet over this connection.
     */
    public void recv(WarpPacket packet)
    throws IOException {
        int t=this.input.read();
        int l1=this.input.read();
        int l2=this.input.read();

        if ((t|l1|l2)==-1)
            throw new IOException("Premature packet header end");

        packet.reset();
        packet.setType(t&0x0ff);
        packet.size=(( l1 & 0x0ff ) << 8) | ( l2 & 0x0ff );

        if (packet.size>0) {
            int off=0;
            int ret=0;
            while (true) {
                ret=this.input.read(packet.buffer,off,packet.size-off);
                if (ret==-1) 
                    throw new IOException("Premature packet payload end");
                off+=ret;
                if(off==packet.size) break;
            }
        }
            
        if (Constants.DEBUG) {
            this.getConnector().debug(this,"<< TYPE="+packet.getType()+
                                           " LENGTH="+packet.size);
            this.getConnector().debug(this,"<< "+packet.dump());
        }
    }
}
