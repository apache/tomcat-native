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
 * (via the internal <CODE>Ajp14Packet</CODE> class), and also about the
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
public class Ajp14
{
    // XXX The message id and name will be handled by a registry, and
    // will allow dynamic allocation, based on the intial negotiation.
    // For Ajp13 compatibility we'll also support "fixed id" messages.

    // XXX For now we'll just use the static final field, as before
    
    public static final int MAX_PACKET_SIZE=8192;
    public static final int H_SIZE=4;  // Size of basic packet header

    public static final int  MAX_READ_SIZE = MAX_PACKET_SIZE - H_SIZE - 2;
    public static final int  MAX_SEND_SIZE = MAX_PACKET_SIZE - H_SIZE - 4;
    
    // ============ Instance Properties ====================
    // Ajp13
    OutputStream out;
    InputStream in;

    // XXX public fields are temp. solutions until the API stabilizes
    // Buffer used of output body and headers
    public Ajp14Packet outBuf = new Ajp14Packet( MAX_PACKET_SIZE );
    // Buffer used for input body
    Ajp14Packet inBuf  = new Ajp14Packet( MAX_PACKET_SIZE );
    // Buffer used for request head ( and headers )
    Ajp14Packet hBuf=new Ajp14Packet( MAX_PACKET_SIZE );
    
    // Holds incoming reads of request body data (*not* header data)
    byte []bodyBuff = new byte[MAX_READ_SIZE];
    
    int blen;  // Length of current chunk of body data in buffer
    int pos;   // Current read position within that buffer
    
    // AJP14 
    // Required handler - detect protocol,
    // set communication parameters, login
    public NegociationHandler negHandler=new NegociationHandler();

    // Required handler - essential request processing
    public RequestHandler reqHandler=new RequestHandler();
    
    public Ajp14() 
    {
        super();
	setSeed("myveryrandomentropy");
	setPassword("myverysecretkey");
	negHandler.init( this );
	reqHandler.init( this );
    }

    public void initBuf()
    {	
	outBuf = new Ajp14Packet( MAX_PACKET_SIZE );
	inBuf  = new Ajp14Packet( MAX_PACKET_SIZE );
	hBuf   = new Ajp14Packet( MAX_PACKET_SIZE );
    }

    public void recycle() {
        if (debug > 0) {
            log("recycle()");
        }

        // This is a touch cargo-cultish, but I think wise.
        blen = 0; 
        pos = 0;
    }
    
    /**
     * Associate an open socket with this instance.
     */
    public void setSocket( Socket socket ) throws IOException {
        if (debug > 0) {
            log("setSocket()");
        }
        
	socket.setSoLinger( true, 100);
	out = socket.getOutputStream();
	in  = socket.getInputStream();
	pos = 0;
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

    // -------------------- Handlers registry --------------------

    static final int MAX_HANDLERS=32;
    static final int RESERVED=16;  // reserved names, backward compat

    // Note that we don't make distinction between in and out
    // messages ( i.e. one id is used only in one direction )
    AjpHandler handlers[]=new AjpHandler[MAX_HANDLERS];
    String handlerName[]=new String[MAX_HANDLERS];
    int currentId=RESERVED;

    public int registerMessageType( int id, String name, AjpHandler h,
				    String sig[] )
    {
	if( id < 0 ) {
	    // try to find it by name
	    for( int i=0; i< handlerName.length; i++ )
		if( name.equals( handlerName[i] ) ) return i;
	    handlerName[currentId]=name;
	    handlers[currentId]=h;
	    currentId++;
	    return currentId;
	}
	// fixed id
	handlerName[id]=name;
	handlers[id]=h;
	return id;
    }

    
    // -------------------- Main dispatch --------------------
    
    /**
     * Read a new packet from the web server and decode it.  If it's a
     * forwarded request, store its properties in the passed-in Request
     * object.
     *
     * @param req An empty (newly-recycled) request object.
     * 
     * @return 200 in case of a successful read of a forwarded request, 500
     * if there were errors in the reading of the request, and -2 if the
     * server is asking the container to shut itself down.  
     */
    public int receiveNextRequest(BaseRequest req) throws IOException 
    {
	// XXX The return values are awful.
	int err = receive(hBuf);
	if(err < 0) {
	    if(debug >0 ) log( "Error receiving message ");
	    return 500;
	}
	
	int type = (int)hBuf.getByte();
	if( type > handlers.length ) {
	    log( "Invalid handler " + type );
	    return 500;
	}
	if( debug > 0 ) log( "Received " + type + " " + handlerName[type]);

	if( ! negHandler.isLogged() ) {
	    if( type != NegociationHandler.JK_AJP14_LOGINIT_CMD &&
		type != NegociationHandler.JK_AJP14_LOGCOMP_CMD ) {
		return 300;
	    }
	    // else continue
	}

	// logged || loging message
	AjpHandler handler=handlers[type];
	if( handler==null ) {
	    log( "Unknown message " + type + handlerName[type] );
	    return 200;
	}

	return handler.handleAjpMessage( type, this, hBuf, req );
    }

    // ========= Internal Packet-Handling Methods =================

    /**
     * Read in a packet from the web server and store it in the passed-in
     * <CODE>Ajp14Packet</CODE> object.
     *
     * @param msg The object into which to store the incoming packet -- any
     * current contents will be overwritten.
     *
     * @return The number of bytes read on a successful read or -1 if there 
     * was an error.
     **/
    public int receive(Ajp14Packet msg) throws IOException {
	// XXX If the length in the packet header doesn't agree with the
	// actual number of bytes read, it should probably return an error
	// value.  Also, callers of this method never use the length
	// returned -- should probably return true/false instead.
	byte b[] = msg.getBuff();

	if( debug > 5 ) log( "Reading head " );
	int rd = in.read( b, 0, H_SIZE );
	if(rd <= 0) {
	    if( debug > 5 ) log( "Error reading " + rd );
	    return rd; 
	}
	
	int len = msg.checkIn();

	if( debug > 5 ) log( "Received " + rd + " " + len + " " + b[0] );
	
	// XXX check if enough space - it's assert()-ed !!!

 	int total_read = 0;
  	while (total_read < len) {
	    rd = in.read( b, 4 + total_read, len - total_read);
            if (rd == -1) {
 		log( "Incomplete read, deal with it " + len + " " + rd);
                break;
		// XXX log
		// XXX Return an error code?
	    }
     	    total_read += rd;
	}
	if( debug > 10 ) log( "Received total:  " + total_read );
	return total_read;
    }

    /**
     * Send a packet to the web server.  Works for any type of message.
     *
     * @param msg A packet with accumulated data to send to the server --
     * this method will write out the length in the header.  
     */
    public void send( Ajp14Packet msg ) throws IOException {
	msg.end(); // Write the packet header
	byte b[] = msg.getBuff();
	int len  = msg.getLen();
        if (debug > 5 ) log("send() " + len + " " + b[0] );
	out.write( b, 0, len );
    }
	
    /**
     * Close the socket connection to the web server.  In general, sockets
     * are maintained across many requests, so this will not be called
     * after finish().  
     *
     * @see Ajp14Interceptor#processConnection
     */
    public void close() throws IOException {
        if (debug > 0) log("close()");

	if(null != out) {        
	    out.close();
	}
	if(null !=in) {
	    in.close();
	}

	negHandler.setLogged( false );	// no more logged now 
    }

    // -------------------- Utils --------------------
    
    private static int debug=10;
    void log(String s) {
	System.out.println("Ajp14: " + s );
    }
 }
