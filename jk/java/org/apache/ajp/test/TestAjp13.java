package org.apache.ajp.test;

import org.apache.ajp.*;
import org.apache.tomcat.util.http.*;

import java.io.*;
import java.net.*;
import java.util.*;

public class TestAjp13 {

    public static void main(String[] args) throws Exception {
        ServerSocket server = new ServerSocket(8009);
        System.out.println("TestAjp13 running...");
        Socket socket = server.accept();
        Ajp13 ajp13 = new Ajp13();
        MimeHeaders headers = new MimeHeaders();
        AjpRequest request = new AjpRequest();
        ajp13.setSocket(socket);

        boolean moreRequests = true;
        while (moreRequests) {
            
            int status = 0;
            try {
                status = ajp13.receiveNextRequest(request);
            } catch (IOException e) {
                System.out.println("process: ajp13.receiveNextRequest -> " + e);
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

            System.out.println(request);

            String message =
                "<html><body><pre>" +
                "hello from ajp13:  " +
                System.getProperty("line.separator") +
                request.toString() +
                "</pre></body></html>";

            headers.addValue("content-type").setString( "text/html");
            headers.addValue("content-length").setInt(message.length());
            headers.addValue("my-header").setString( "my value");
            ajp13.sendHeaders(200, headers);

            byte[] b = message.getBytes();
            ajp13.doWrite(b, 0, b.length);

            ajp13.finish();

            request.recycle();
            headers.recycle();
        }

        try {
            ajp13.close();
	} catch (IOException e) {
	    System.out.println("process: ajp13.close ->" + e);
	}

	try {
            socket.close();
	} catch (IOException e) {
	    System.out.println("process: socket.close ->" + e);
	}
	socket = null;

        System.out.println("process:  done");

    }
}

