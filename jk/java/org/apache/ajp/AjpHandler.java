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

package org.apache.ajp;

import java.io.IOException;

import org.apache.tomcat.util.http.BaseRequest;

/**
 * Base class for handlers of Ajp messages. Jk provide a simple bidirectional 
 * communication mechanism between the web server and a servlet container. It is
 * based on messages that are passed between the 2 server, using a single
 * thread on each side.
 *
 * The container side must be able to deal with at least the "REQUEST FORWARD",
 * the server side must be able to deal with at least "HEADERS", "BODY",
 * "END" messages.
 *
 * @author Henri Gomez
 * @author Costin Manolache
 */
public class AjpHandler
{
    public static final int UNKNOWN=-1;
    Ajp13 channel;
    
    public void init( Ajp13 channel ) {
        this.channel=channel;
    }
    
    /** Execute the callback 
     */
    public int handleAjpMessage( int type, Ajp13 channel,
				 Ajp13Packet ajp, BaseRequest req )
	throws IOException
    {
	return UNKNOWN;
    }
}    
