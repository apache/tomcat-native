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

import java.io.*;
import java.net.*;
import java.util.*;
import org.apache.tomcat.util.*;

/**
 * This interface will be implemented by any object that
 * uses TcpConnections. It is supported by the pool tcp
 * connection manager and should be supported by future
 * managers.
 * The goal is to decouple the connection handler from
 * the thread, socket and pooling complexity.
 */
public interface TcpConnectionHandler {
    
    /** Add informations about the a "controler" object
     *  specific to the server. In tomcat it will be a
     *  ContextManager.
     *  @deprecated This has nothing to do with TcpHandling,
     *  was used as a workaround
     */
    public void setServer(Object manager);

    
    /** Used to pass config informations to the handler
     *  @deprecated. This has nothing to do with Tcp,
     *  was used as a workaround 
     */
    public void setAttribute(String name, Object value );
    
    /** Called before the call to processConnection.
     *  If the thread is reused, init() should be called once per thread.
     *
     *  It may look strange, but it's a _very_ good way to avoid synchronized
     *  methods and keep per thread data.
     *
     *  Assert: the object returned from init() will be passed to
     *  all processConnection() methods happening in the same thread.
     * 
     */
    public Object[] init( );

    /**
     *  Assert: connection!=null
     *  Assert: connection.getSocket() != null
     *  Assert: thData != null and is the result of calling init()
     *  Assert: thData is preserved per Thread.
     */
    public void processConnection(TcpConnection connection, Object thData[]);    
}
