/*
 * Copyright 1999,2004 The Apache Software Foundation.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */ 
package org.apache.coyote.http11;

import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.buf.ByteChunk;
import org.apache.tomcat.util.http.MimeHeaders;

import org.apache.coyote.ActionCode;
import org.apache.coyote.Adapter;
import org.apache.coyote.Request;
import org.apache.coyote.Response;

/**
 * Adapter which will return random responses using pipelining, which means it
 * will never try to generate bogus responses.
 *
 * @author Remy Maucherat
 */
public class RandomAdapter
    implements Adapter {


    public static final String CRLF = "\r\n";
    public static final byte[] b = "0123456789\r\n".getBytes();
    public static final ByteChunk bc = new ByteChunk();


    /** 
     * Service method, which dumps the request to the console.
     */
    public void service(Request req, Response res)
	throws Exception {

        double rand = Math.random();
        int n = (int) Math.round(10 * rand);

        // Temp variables
        byte[] buf = null;
        int nRead = 0;
        StringBuffer sbuf = new StringBuffer();
        MimeHeaders headers = req.getMimeHeaders();
        int size = headers.size();

        switch (n) {

        case 0:

            // 0) Do nothing
            System.out.println("Response 0");
            break;

        case 1:

            // 1) Set content length, and write the appropriate content
            System.out.println("Response 1");
            res.setContentLength(b.length);
            bc.setBytes(b, 0, b.length);
            res.doWrite(bc);
            break;

        case 2:

            // 2) Read the request data, and print out the number of bytes
            // read
            System.out.println("Response 2");
            while (nRead >= 0) {
                nRead = req.doRead(bc);
                buf = ("Read " + nRead + " bytes\r\n").getBytes();
                bc.setBytes(buf, 0, buf.length);
                res.doWrite(bc);
            }
            break;

        case 3:

            // 3) Return 204 (no content), while reading once on input
            System.out.println("Response 3");
            res.setStatus(204);
            nRead = req.doRead(bc);
            res.setHeader("Info", "Read " + nRead + " bytes");
            break;

        case 4:

            // 4) Do a request dump
            System.out.println("Response 4");
            sbuf.append("Request dump:");
            sbuf.append(CRLF);
            sbuf.append(req.method());
            sbuf.append(" ");
            sbuf.append(req.unparsedURI());
            sbuf.append(" ");
            sbuf.append(req.protocol());
            sbuf.append(CRLF);
            for (int i = 0; i < size; i++) {
                sbuf.append(headers.getName(i) + ": ");
                sbuf.append(headers.getValue(i).toString());
                sbuf.append(CRLF);
            }
            sbuf.append("Request body:");
            sbuf.append(CRLF);
            res.action(ActionCode.ACTION_ACK, null);
            ByteChunk bc2 = new ByteChunk();
            byte[] b2 = sbuf.toString().getBytes();
            bc2.setBytes(b2, 0, b2.length);
            res.doWrite(bc2);
            while (nRead >= 0) {
                nRead = req.doRead(bc2);
                if (nRead > 0)
                    res.doWrite(bc2);
            }
            break;

        default:

            // Response not implemented yet
            System.out.println("Response " + n + " is not implemented yet");

        }

/*
        StringBuffer buf = new StringBuffer();
        buf.append("Request dump:");
        buf.append(CRLF);
        buf.append(req.method());
        buf.append(" ");
        buf.append(req.unparsedURI());
        buf.append(" ");
        buf.append(req.protocol());
        buf.append(CRLF);
        
        MimeHeaders headers = req.getMimeHeaders();
        int size = headers.size();
        for (int i = 0; i < size; i++) {
            buf.append(headers.getName(i) + ": ");
            buf.append(headers.getValue(i).toString());
            buf.append(CRLF);
        }

        buf.append("Request body:");
        buf.append(CRLF);

        res.action(ActionCode.ACTION_ACK, null);

        ByteChunk bc = new ByteChunk();
        byte[] b = buf.toString().getBytes();
        bc.setBytes(b, 0, b.length);
        res.doWrite(bc);

        int nRead = 0;

        while (nRead >= 0) {
            nRead = req.doRead(bc);
            if (nRead > 0)
                res.doWrite(bc);
        }
*/

    }


}
