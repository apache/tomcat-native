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
 * Adapter which will generate content.
 *
 * @author Remy Maucherat
 */
public class TestAdapter
    implements Adapter {


    public static final String CRLF = "\r\n";


    /** 
     * Service method, which dumps the request to the console.
     */
    public void service(Request req, Response res)
	throws Exception {
        
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

    }


}
