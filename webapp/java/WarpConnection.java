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
        if (Constants.DEBUG)
            this.getConnector().debug(this,"Sending packet TYPE="+
                                      packet.getType()+" LENGTH="+
                                      packet.size);

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

}
