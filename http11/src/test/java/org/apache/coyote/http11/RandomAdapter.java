/*
 * ====================================================================
 *
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999 The Apache Software Foundation.  All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution, if
 *    any, must include the following acknowlegement:  
 *       "This product includes software developed by the 
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowlegement may appear in the software itself,
 *    if and wherever such third-party acknowlegements normally appear.
 *
 * 4. The names "The Jakarta Project", "Tomcat", and "Apache Software
 *    Foundation" must not be used to endorse or promote products derived
 *    from this software without prior written permission. For written 
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * [Additional notices, if required by prior licensing conditions]
 *
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
