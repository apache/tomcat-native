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

package org.apache.coyote.tomcat3;

import org.apache.coyote.Adapter;
import org.apache.tomcat.core.ContextManager;

/** Adapter between Coyote and Tomcat.
 *
 *  This class handles the task of passing of an individual request to
 *  Tomcat to handle.  Also some of the connection-specific methods are
 *  delegated to here.
 *
 *  @author Bill Barker
 */
public class Tomcat3Adapter implements Adapter {
    ContextManager cm;
    
    Tomcat3Adapter(ContextManager ctxman) {
	cm   = ctxman;
    }

    static int containerRequestNOTE=1; // XXX Implement a NoteManager, namespaces.
    
    /** Pass off an individual request to Tomcat.
     */
    public void service(org.apache.coyote.Request request, 
			org.apache.coyote.Response response) 
	    throws Exception
    {
        Tomcat3Request reqA;
        Tomcat3Response resA;

        reqA=(Tomcat3Request)request.getNote( containerRequestNOTE );
        if( reqA==null ) {
            reqA=new Tomcat3Request();
            resA=new Tomcat3Response();
            cm.initRequest( reqA, resA );

            reqA.setCoyoteRequest(request);
            resA.setCoyoteResponse(response);
            request.setNote( containerRequestNOTE, reqA );
        } else {
            resA=(Tomcat3Response)reqA.getResponse();
        }
        
        if( reqA.scheme().isNull() ) {
	    reqA.scheme().setString("http");
	}
	try {
	    cm.service( reqA, resA );
	} finally {
	    reqA.recycle();
	    resA.recycle();
	}
    }
}
