/*
 * $Header$
 * $Revision$
 * $Date$
 *
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

package org.apache.tomcat.util.net;

import java.io.IOException;
import java.io.InputStream;
import java.net.Socket;

/**
 *
 */
public class TcpConnection  { // implements Endpoint {
    /**
     * Maxium number of times to clear the socket input buffer.
     */
    static  int MAX_SHUTDOWN_TRIES=20;

    public TcpConnection() {
    }

    // -------------------- Properties --------------------

    PoolTcpEndpoint endpoint;
    Socket socket;

    public static void setMaxShutdownTries(int mst) {
	MAX_SHUTDOWN_TRIES = mst;
    }
    public void setEndpoint(PoolTcpEndpoint endpoint) {
	this.endpoint = endpoint;
    }

    public PoolTcpEndpoint getEndpoint() {
	return endpoint;
    }

    public void setSocket(Socket socket) {
	this.socket=socket;
    }

    public Socket getSocket() {
	return socket;
    }

    public void recycle() {
        endpoint = null;
        socket = null;
    }

    // Another frequent repetition
    public static int readLine(InputStream in, byte[] b, int off, int len)
	throws IOException
    {
	if (len <= 0) {
	    return 0;
	}
	int count = 0, c;

	while ((c = in.read()) != -1) {
	    b[off++] = (byte)c;
	    count++;
	    if (c == '\n' || count == len) {
		break;
	    }
	}
	return count > 0 ? count : -1;
    }

    
    // Usefull stuff - avoid having it replicated everywhere
    public static void shutdownInput(Socket socket)
	throws IOException
    {
	try {
	    InputStream is = socket.getInputStream();
	    int available = is.available ();
	    int count=0;
	    
	    // XXX on JDK 1.3 just socket.shutdownInput () which
	    // was added just to deal with such issues.
	    
	    // skip any unread (bogus) bytes
	    while (available > 0 && count++ < MAX_SHUTDOWN_TRIES) {
		is.skip (available);
		available = is.available();
	    }
	}catch(NullPointerException npe) {
	    // do nothing - we are just cleaning up, this is
	    // a workaround for Netscape \n\r in POST - it is supposed
	    // to be ignored
	}
    }
}


