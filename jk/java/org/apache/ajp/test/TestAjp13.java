package org.apache.ajp.test;

import org.apache.ajp.*;
import org.apache.tomcat.util.http.*;

import java.io.*;
import java.net.*;
import java.util.*;

// junit
import junit.framework.*;

public class TestAjp13 extends TestCase {

    Ajp13Server server = null;

    public TestAjp13(String name) {
        super(name);
    }

    public static Test suite() {
        return new TestSuite(TestAjp13.class);
    }

    protected void setUp() {
        println("setup...");

        server = new Ajp13Server();
        server.start();
    }

    protected void tearDown() {
        println("tear down...");

        server.shutdown();
    }

    public void test1() throws Exception {
        println("running test1");

        Socket s = new Socket("localhost", 8009);

        Ajp13Packet p = new Ajp13Packet(Ajp13.MAX_PACKET_SIZE);
        p.appendInt(0x1234);
        p.appendInt(0);
        p.setByteOff(4);
        p.appendByte(Ajp13.JK_AJP13_FORWARD_REQUEST);
        p.appendByte((byte)2);
        p.appendString("http");
        p.appendString("/test_uri");
        p.appendString("remote_addr");
        p.appendString("remote_host");
        p.appendString("server_name");
        p.appendInt(80);
        p.appendBool(false);
        p.appendInt(3);
        p.appendString("my header");
        p.appendString("my header value");
        p.appendInt((0xA0 << 8) + Ajp13.SC_REQ_AUTHORIZATION);
        p.appendString("some auth string");
        p.appendInt((0xA0 << 8) + Ajp13.SC_REQ_USER_AGENT);
        p.appendString("TestAjp13 User Agent");
        p.appendByte(Ajp13.SC_A_ARE_DONE);

        int len = p.getByteOff() - 4;
        p.setByteOff(2);
        p.appendInt(len);

        OutputStream os = s.getOutputStream();
        os.write(p.getBuff(), 0, len + 4);

        InputStream is = s.getInputStream();

        println("decoding response...");
        
        boolean done = false;
        while (!done) {
            int b1, b2;
            // read a packet

            // first 2 bytes should be AB
            b1 = is.read();
            assertTrue("byte 1 was " + (char)b1, b1 == (int)'A');
            b2 = is.read();
            assertTrue("byte 2 was " + (char)b2, b2 == (int)'B');

            println("b1 = " + (char)b1 + "; b2 = " + (char)b2);
            
            // next is length
            b1 = is.read();
            b1 &= 0xFF;
            b2 = is.read();
            b2 &= 0xFF;
            
            int l = (b1 << 8) + b2;

            println("length = " + l);

            // now get data
            byte[] buf = new byte[l];
            int n = 0;
            int off = 0;
            int total = 0;
            while ((n = is.read(buf, off, l - off)) != -1 && (l - off != 0)) {
                total += n;
                off += n;
            }

            println("read " + total);

            assertTrue("total read was " + total +
                       ", should have been " + l,
                       total == l);

            

            int code = (int)buf[0];

            switch (code) {
            case 3:
                println("AJP13_SEND_BODY_CHUNK ");
                break;
            case 4:
                println("AJP13_SEND_HEADERS ");
                break;
            case 5:
                println("AJP13_END_RESPONSE ");
                done = true;
                break;
            case 6:
                println("AJP13_GET_BODY_CHUNK ");
                break;
            default:
                assertTrue("invalid prefix code:  " + code, false);
                break;
            }
        }

        println("shutting down socket...");
        s.shutdownOutput();
        s.shutdownInput();
        s.close();

        println("done test1...");
    }

    protected static void println(String msg) {
        System.out.println("[TestAjp13] " + msg);
    }

    public static void main(String[] args) throws Exception {
    }
}


class Ajp13Server extends Thread {

    boolean shutdown = false;

    void shutdown() {
        this.shutdown = true;

//          try {
            this.interrupt();
//          } catch (InterruptedException e) {
//              throw new RuntimeException(e.toString());
//          }
    }

    public void run() {
        try {
            ServerSocket server = new ServerSocket(8009);
            TestAjp13.println("Ajp13Server running...");
            Socket socket = server.accept();
            Ajp13 ajp13 = new Ajp13();
            MimeHeaders headers = new MimeHeaders();
            AjpRequest request = new AjpRequest();
            ajp13.setSocket(socket);

            boolean moreRequests = true;
            while (moreRequests && !shutdown) {
            
                int status = 0;
                try {
                    status = ajp13.receiveNextRequest(request);
                } catch (IOException e) {
                    if (shutdown) {
                        TestAjp13.println("Ajp13Server told to shutdown");
                        break;
                    }
                    TestAjp13.println("process: ajp13.receiveNextRequest -> " + e);
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

                TestAjp13.println(request.toString());

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
                TestAjp13.println("process: ajp13.close ->" + e);
            }

            try {
                socket.close();
            } catch (IOException e) {
                TestAjp13.println("process: socket.close ->" + e);
            }
            socket = null;

            TestAjp13.println("process:  done");

        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException(e.toString());
        }
    }
}

