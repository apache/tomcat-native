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
package org.apache.tomcat.util.handler;

import java.io.*;
import java.util.*;
import java.security.*;


/**
 * The lowest level component of Jk ( and hopefully Coyote ). 
 *
 * Try to keep it minimal and flexible - add only if you _have_ to add.
 *
 * It is similar in concept and can implement/wrap tomcat3.3 Interceptor, tomcat4.0 Valve,
 * axis Handler, tomcat3.3 Handler, apache2 Hooks etc.
 *
 * Both iterative (Interceptor, Hook ) and recursive ( Valve ) behavior are supported.
 * Named TcHandler because Handler name is too overloaded.
 *
 * The interface allows both stateless and statefull implementations ( like Servlet ).
 *
 * @author Costin Manolache
 */
public abstract class TcHandler {
    public static final int OK=0;
    public static final int LAST=1;
    public static final int ERROR=2;

    protected Hashtable attributes=new Hashtable();
    protected TcHandler next;
    protected String name;
    protected int id;

    // -------------------- Configuration --------------------
    
    /** Set the name of the handler. Will allways be called by
     *  worker env after creating the worker.
     */
    public void setName(String s ) {
        name=s;
    }

    public String getName() {
        return name;
    }

    /** Set the id of the worker. It can be used for faster dispatch.
     *  Must be unique, managed by whoever creates the handlers.
     */
    public void setId( int id ) {
        this.id=id;
    }

    public int getId() {
        return id;
    }
    
    /** Catalina-style "recursive" invocation. A handler is required to call
     *  the next handler if set.
     */
    public void setNext( TcHandler h ) {
        next=h;
    }


    /** Base implementation will just save all attributes. 
     *  It is higly desirable to override this and allow runtime reconfiguration.
     *  XXX Should I make it abstract and force everyone to override ?
     */
    public void setAttribute( String name, Object value ) {
        attributes.put( name, value );
    }

    /** Get an attribute. Override to allow runtime query ( attribute can be
     *  anything, including statistics, etc )
     */
    public Object getAttribute( String name ) {
        return attributes.get(name) ;
    }

    //-------------------- Lifecycle --------------------
    
    /** Should register the request types it can handle,
     *   same style as apache2.
     */
    public void init() throws IOException {
    }

    /** Clean up and stop the handler. Override if needed.
     */
    public void destroy() throws IOException {
    }

    public void start() throws IOException {
    }

    public void stop() throws IOException {
    }

    // -------------------- Action --------------------
    
    /** The 'hook' method. If a 'next' was set, invoke should call it ( recursive behavior,
     *  similar with valve ).
     *
     * The application using the handler can also iterate, using the same semantics with
     * Interceptor or APR hooks.
     *
     * @returns OK, LAST, ERROR Status of the execution, semantic similar with apache
     */
    public abstract int invoke(TcHandlerCtx tcCtx)  throws IOException;



}
