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

package org.apache.ajp;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.util.Enumeration;
import java.security.*;

import org.apache.tomcat.util.http.*;
import org.apache.tomcat.util.buf.*;


/**
 * Represents a single, persistent connection between the web server and
 * the servlet container.  Uses the Apache JServ Protocol version 1.3 for
 * communication.  Because this protocal does not multiplex requests, this
 * connection can only be associated with a single request-handling cycle
 * at a time.<P>
 *
 * This class contains knowledge about how an individual packet is laid out
 * (via the internal <CODE>Ajp13Packet</CODE> class), and also about the
 * stages of communicaton between the server and the servlet container.  It
 * translates from Tomcat's internal servlet support methods
 * (e.g. doWrite) to the correct packets to send to the web server.
 *
 * @see Ajp14Interceptor 
 *
 * @author Henri Gomez [hgomez@slib.fr]
 * @author Dan Milstein [danmil@shore.net]
 * @author Keith Wannamaker [Keith@Wannamaker.org]
 */
public class Ajp14 extends Ajp13
{
    // XXX The message id and name will be handled by a registry, and
    // will allow dynamic allocation, based on the intial negotiation.
    // For Ajp13 compatibility we'll also support "fixed id" messages.

    // XXX For now we'll just use the static final field, as before
    
    // ============ Instance Properties ====================
    
    // AJP14 
    // Required handler - detect protocol,
    // set communication parameters, login
    public NegociationHandler negHandler=new NegociationHandler();
    
    public Ajp14() 
    {
        super();
	setSeed("myveryrandomentropy");
	setPassword("myverysecretkey");
	negHandler.init( this );
    }

    // -------------------- Parameters --------------------

    /** Set the server signature, to be sent in Servlet-Container header
     */
    public void setContainerSignature( String s ) {
	negHandler.setContainerSignature( s );
    }
    
    /**
     * Set the original entropy seed
     */
    public void setSeed(String pseed) 
    {
	negHandler.setSeed( pseed );
    }

    /**
     * Set the secret password
     */
    public void setPassword(String ppwd) 
    {
	negHandler.setPassword( ppwd );
    }


    public int handleMessage( int type, Ajp13Packet hBuf, AjpRequest req )
        throws IOException
    {
        logger.log("ajp14 handle message " + type );
        if( type > handlers.length ) {
	    logger.log( "Invalid handler " + type );
	    return 500;
	}
	if( debug > 0 )
            logger.log( "Received " + type + " " + handlerName[type]);

	if( ! negHandler.isLogged() ) {
	    if( type != NegociationHandler.JK_AJP14_LOGINIT_CMD &&
		type != NegociationHandler.JK_AJP14_LOGCOMP_CMD ) {

                logger.log( "Ajp14 error: not logged " +
                            type + " " + handlerName[type]);

		return 300;
	    }
	    // else continue
	}
        
	// logged || loging message
	AjpHandler handler=handlers[type];
	if( handler==null ) {
	    logger.log( "Unknown message " + type + handlerName[type] );
	    return 200;
	}

        if( debug > 0 )
            logger.log( "Ajp14 handler " + handler );
	return handler.handleAjpMessage( type, this, hBuf, req );
    }

    // ========= Internal Packet-Handling Methods =================
    public void close() throws IOException {
        super.close();
	negHandler.setLogged( false );	// no more logged now 
    }
 }
