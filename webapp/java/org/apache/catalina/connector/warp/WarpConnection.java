/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

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
    /** Our logger. */
    private WarpLogger logger=null;

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
        this.logger=new WarpLogger(this);
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
        this.logger.setContainer(connector.getContainer());
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
                logger.log("Cannot close socket",e);
            }
        }
    }

    /**
     * Process data from the socket.
     */
    public void run() {
        WarpPacket packet=new WarpPacket();

        if (Constants.DEBUG) logger.debug("Connection starting");

        try {
            this.input=this.socket.getInputStream();
            this.output=this.socket.getOutputStream();
            if (!new WarpConfigurationHandler().handle(this,packet)) {
                logger.log("Configuration handler returned false");
                this.stop();
            }
            WarpRequestHandler requestHandler=new WarpRequestHandler();
            while (requestHandler.handle(this,packet));
        } catch (IOException e) {
            logger.log("Exception on socket",e);
        } finally {
            this.stop();
        }

        if (Constants.DEBUG) logger.debug("Connection terminated");
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
            String typ=Integer.toHexString(packet.getType());
            logger.debug(">> TYPE="+typ+" LENGTH="+packet.size);
            //logger.debug(">> "+packet.dump());
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
            String typ=Integer.toHexString(packet.getType());
            logger.debug("<< TYPE="+typ+" LENGTH="+packet.size);
            // logger.debug("<< "+packet.dump());
        }
    }
}
