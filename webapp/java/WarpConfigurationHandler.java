package org.apache.catalina.connector.warp;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;

public class WarpConfigurationHandler {

    public WarpConfigurationHandler() {
        super();
    }
    
    public boolean handle(WarpConnection connection)
    throws IOException {
        WarpPacket packet=new WarpPacket();
        
        // Prepare the Welcome packet
        packet.setType(Constants.TYPE_CONF_WELCOME);
        packet.writeUnsignedShort(Constants.VERS_MAJOR);
        packet.writeUnsignedShort(Constants.VERS_MINOR);
        packet.writeInteger(-1);
        connection.send(packet);
        
        return(true);
        //while (true) {
        //    connection.recv(packet);
    }
}        
