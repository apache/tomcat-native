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
package org.apache.jk.core;

import java.io.*;
import java.util.*;
import java.security.*;

/**
 *
 * @author Costin Manolache
 */
public abstract class JkHandler {
    public static final int OK=0;
    public static final int LAST=1;
    public static final int ERROR=2;

    private Properties properties=new Properties();
    protected WorkerEnv wEnv;
    protected JkHandler next;
    protected String nextName=null;
    protected String name;
    protected int id;

    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( JkHandler.class );
    
    public void setWorkerEnv( WorkerEnv we ) {
        this.wEnv=we;
    }

    /** Set the name of the handler. Will allways be called by
     *  worker env after creating the worker.
     */
    public void setName(String s ) {
        name=s;
    }

    public String getName() {
        return name;
    }

    /** Set the id of the worker. We use an id for faster dispatch.
     *  Since we expect a decent number of handler in system, the
     *  id is unique - that means we may have to allocate bigger
     *  dispatch tables. ( easy to fix if needed )
     */
    public void setId( int id ) {
        this.id=id;
    }

    /** Catalina-style "recursive" invocation.
     *  A chain is used for Apache/3.3 style iterative invocation.
     */
    public void setNext( JkHandler h ) {
        if( log.isDebugEnabled() )
            log.debug("setNext " + h.getClass().getName());
        next=h;
    }

    public void setNext( String s ) {
        nextName=s;
    }

    /** Should register the request types it can handle,
     *   same style as apache2.
     */
    public void init() throws IOException {
    }

    /** Clean up and stop the handler
     */
    public void destroy() throws IOException {
    }

    public abstract int invoke(Msg msg, MsgContext mc )  throws IOException;

    
    public void setProperty( String name, String value ) {
        properties.put( name, value );
    }

    public String getProperty( String name ) {
        return properties.getProperty(name) ;
    }
}
