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
package org.apache.coyote;

import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.MimeHeaders;

/**
 * Simple adapter.
 *
 * @author Remy Maucherat
 */
public class Adapter {


    public static final String CRLF = "\r\n";


    /** 
     * Service method, which dumps the request to the console.
     */
    public void service(Request req, Response res)
	throws Exception {
        
        StringBuffer buf = new StringBuffer();
        buf.append(req.method());
        buf.append(" ");
        buf.append(req.unparsedURI());
        buf.append(" ");
        buf.append(req.protocol());
        buf.append(CRLF);
        
        
        
    }


}
