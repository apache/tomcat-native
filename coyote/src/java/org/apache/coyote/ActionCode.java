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
package org.apache.coyote;


/**
 * Enumerated class containing the adapter event codes.
 *
 * @author Remy Maucherat
 */
public final class ActionCode {


    // -------------------------------------------------------------- Constants


    public static final ActionCode ACTION_ACK = new ActionCode(1);


    public static final ActionCode ACTION_CLOSE = new ActionCode(2);


    public static final ActionCode ACTION_COMMIT = new ActionCode(3);


    /**
     * A flush() operation originated by the client ( i.e. a flush() on
     * the servlet output stream or writer, called by a servlet ).
     */
    public static final ActionCode ACTION_CLIENT_FLUSH = new ActionCode(4);

    
    public static final ActionCode ACTION_CUSTOM = new ActionCode(5);


    public static final ActionCode ACTION_RESET = new ActionCode(6);


    public static final ActionCode ACTION_START = new ActionCode(7);


    public static final ActionCode ACTION_STOP = new ActionCode(8);


    public static final ActionCode ACTION_WEBAPP = new ActionCode(9);

    /** Hook called after request, but before recycling. Can be used
        for logging, to update counters, custom cleanup - the request
        is still visible
    */
    public static final ActionCode ACTION_POST_REQUEST = new ActionCode(10);

    /**
     * Callback for lazy evaluation - extract the remote host name.
     */
    public static final ActionCode ACTION_REQ_HOST_ATTRIBUTE = 
        new ActionCode(11);


    /**
     * Callback for lazy evaluation - extract the SSL-related attributes.
     */
    public static final ActionCode ACTION_REQ_HOST_ADDR_ATTRIBUTE = new ActionCode(12);

    /**
     * Callback for lazy evaluation - extract the SSL-related attributes.
     */
    public static final ActionCode ACTION_REQ_SSL_ATTRIBUTE = new ActionCode(13);


    /** Chain for request creation. Called each time a new request is created
        ( requests are recycled ).
     */
    public static final ActionCode ACTION_NEW_REQUEST = new ActionCode(14);


    /**
     * Callback for lazy evaluation - extract the SSL-certificate 
     * (including forcing a re-handshake if necessary)
     */
    public static final ActionCode ACTION_REQ_SSL_CERTIFICATE = new ActionCode(15);


    // ----------------------------------------------------------- Constructors
    int code;

    /**
     * Private constructor.
     */
    private ActionCode(int code) {
        this.code=code;
    }

    /** Action id, useable in switches and table indexes
     */
    public int getCode() {
        return code;
    }


}
