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
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;

import org.apache.tomcat.util.http.BaseRequest;
import org.apache.tomcat.util.http.HttpMessages;
import org.apache.tomcat.util.http.MimeHeaders;

/**
 * Represents a single, persistent connection between the web server and
 * the servlet container.  Uses the Apache JServ Protocol version 1.3 for
 * communication.  Because this protocal does not multiplex requests, this
 * connection can only be associated with a single request-handling cycle
 * at a time.<P>
 *
 * This class contains knowledge about how an individual packet is laid out
 * (via the <CODE>Ajp13Packet</CODE> class), and also about the
 * stages of communicaton between the server and the servlet container.  It
 * translates from Tomcat's internal servlet support methods
 * (e.g. doWrite) to the correct packets to send to the web server.
 *
 * @see Ajp13Interceptor 
 *
 * @author Dan Milstein [danmil@shore.net]
 * @author Keith Wannamaker [Keith@Wannamaker.org]
 * @author Kevin Seguin [seguin@apache.org]
 * @author Henri Gomez [hgomez@apache.org]
 * @author Costin Manolache
 */
public class Ajp13 {

    public static final int MAX_PACKET_SIZE=8192;
    public static final int H_SIZE=4;  // Size of basic packet header

    public static final int  MAX_READ_SIZE = MAX_PACKET_SIZE - H_SIZE - 2;
    public static final int  MAX_SEND_SIZE = MAX_PACKET_SIZE - H_SIZE - 4;

    // Error code for Ajp13
    public static final int  JK_AJP13_BAD_HEADER        = -100;
    public static final int  JK_AJP13_NO_HEADER         = -101;
    public static final int  JK_AJP13_COMM_CLOSED       = -102;
    public static final int  JK_AJP13_COMM_BROKEN       = -103;
    public static final int  JK_AJP13_BAD_BODY          = -104;
    public static final int  JK_AJP13_INCOMPLETE_BODY   = -105;

    // ============ Instance Properties ====================

    OutputStream out;
    InputStream in;

    // Buffer used of output body and headers
    public Ajp13Packet outBuf = new Ajp13Packet( MAX_PACKET_SIZE );
    // Buffer used for input body
    Ajp13Packet inBuf  = new Ajp13Packet( MAX_PACKET_SIZE );
    // Buffer used for request head ( and headers )
    Ajp13Packet hBuf=new Ajp13Packet( MAX_PACKET_SIZE );

    // Holds incoming reads of request body data (*not* header data)
    byte []bodyBuff = new byte[MAX_READ_SIZE];
    
    int blen;  // Length of current chunk of body data in buffer
    int pos;   // Current read position within that buffer

    boolean end_of_stream;  // true if we've received an empty packet
    
    // Required handler - essential request processing
    public RequestHandler reqHandler;
    // AJP14 - detect protocol,set communication parameters, login
    // If no password is set, use only Ajp13 messaging
    boolean backwardCompat=true;
    boolean logged=false;
    String secret=null;
    
    public Ajp13() {
	super();
	initBuf();
        reqHandler=new RequestHandler();
	reqHandler.init( this );
    }

    public Ajp13(RequestHandler reqHandler ) {
	super();
	initBuf();
        this.reqHandler=reqHandler;
	reqHandler.init( this );
    }

    /** Will be overriden
     */
    public void initBuf() {
	outBuf = new Ajp13Packet( MAX_PACKET_SIZE );
	inBuf  = new Ajp13Packet( MAX_PACKET_SIZE );
	hBuf=new Ajp13Packet( MAX_PACKET_SIZE );
    }
    
    public void recycle() {
        if (debug > 0) {
            logger.log("recycle()");
        }

        // This is a touch cargo-cultish, but I think wise.
        blen = 0; 
        pos = 0;
        end_of_stream = false;
        logged=false;
    }
    
    /**
     * Associate an open socket with this instance.
     */
    public void setSocket( Socket socket ) throws IOException {
        if (debug > 0) {
            logger.log("setSocket()");
        }
        
	socket.setSoLinger( true, 100);
	out = socket.getOutputStream();
	in  = socket.getInputStream();
	pos = 0;
    }

    /**
     * Backward compat mode, no login  needed
     */
    public void setBackward(boolean b) 
    {
        backwardCompat=b;
    }

    public boolean isLogged() {
	return logged;
    }

    void setLogged( boolean b ) {
        logged=b;
    }

    public void setSecret( String s ) {
        secret=s;
    }

    public String getSecret() {
        return secret;
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
     * forwarded request, store its properties in the passed-in AjpRequest
     * object.
     *
     * @param req An empty (newly-recycled) request object.
     * 
     * @return 200 in case of a successful read of a forwarded request, 500
     * if there were errors in the reading of the request, and -2 if the
     * server is asking the container to shut itself down.  
     */
    public int receiveNextRequest(BaseRequest req) throws IOException {
        if (debug > 0) {
            logger.log("receiveNextRequest()");
        }
        
        // XXX The return values are awful.

        int err = 0;

        // if we receive an IOException here, it must be because
        // the remote just closed the ajp13 connection, and it's not
        // an error, we just need to close the AJP13 connection
        try {
            err = receive(hBuf);
        } catch (IOException ioe) {
            if(debug >0 ) logger.log( "IOException receiving message ");
            return -1;  // Indicate it's a disconnection from the remote end
        }
        
	if(err < 0) {
	    if(debug >0 ) logger.log( "Error receiving message ");
	    return 500;
	}
	
	int type = (int)hBuf.getByte();
        //        System.out.println("XXX " + this );
        return handleMessage( type, hBuf, req );
    }

    /** Override for ajp14, temporary
     */
    public int handleMessage( int type, Ajp13Packet hBuf, BaseRequest req )
        throws IOException
    {
        if( type > handlers.length ) {
	    logger.log( "Invalid handler " + type );
	    return 500;
	}

        if( debug > 0 )
            logger.log( "Received " + type + " " + handlerName[type]);
        
        // Ajp14, unlogged
	if( ! backwardCompat && ! isLogged() ) {
	    if( type != NegociationHandler.JK_AJP14_LOGINIT_CMD &&
		type != NegociationHandler.JK_AJP14_LOGCOMP_CMD ) {

                logger.log( "Ajp14 error: not logged " +
                            type + " " + handlerName[type]);

		return 300;
	    }
	    // else continue
	}

        // Ajp13 messages
	switch(type) {
	case RequestHandler.JK_AJP13_FORWARD_REQUEST:
	    return reqHandler.decodeRequest(this, hBuf, req);
	    
	case RequestHandler.JK_AJP13_PING_REQUEST:
		return reqHandler.sendPong(this, outBuf);
		
	case RequestHandler.JK_AJP13_SHUTDOWN:
	    return -2;
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

    // ==================== Servlet Input Support =================

    /** @deprecated -- Will use reqHandler, make sure nobody else
     calls this */

    
    public int available() throws IOException {
        return reqHandler.available(this);
    }

    public int doRead() throws IOException 
    {
        return reqHandler.doRead( this );
    }
    
    public int doRead(byte[] b, int off, int len) throws IOException 
    {
        return reqHandler.doRead( this, b, off, len );
    }
    
    private boolean refillReadBuffer() throws IOException 
    {
        return reqHandler.refillReadBuffer(this);
    }    

    public void beginSendHeaders(int status,
                                 String statusMessage,
                                 int numHeaders) throws IOException {
        reqHandler.beginSendHeaders( this, outBuf,
                                         status, statusMessage,
                                         numHeaders);
    }        

	public void sendHeader(String name, String value) throws IOException {
		reqHandler.sendHeader(  outBuf, name, value );
	}


    public void endSendHeaders() throws IOException {
        reqHandler.endSendHeaders(this, outBuf);
    }

    public void sendHeaders(int status, MimeHeaders headers)
        throws IOException
    {
        reqHandler.sendHeaders(this, outBuf, status,
                                   HttpMessages.getMessage(status),
                                   headers);
    }

    public void sendHeaders(int status, String statusMessage,
                            MimeHeaders headers)
        throws IOException
    {
        reqHandler.sendHeaders( this, outBuf, status,
                                    statusMessage, headers );
    }

    public void finish() throws IOException {
        reqHandler.finish(this, outBuf );
    }

    public void doWrite(byte b[], int off, int len) throws IOException {
        reqHandler.doWrite( this, outBuf, b, off, len );
    }
    

    // ========= Internal Packet-Handling Methods =================

    /**
     * Read N bytes from the InputStream, and ensure we got them all
     * Under heavy load we could experience many fragmented packets
     * just read Unix Network Programming to recall that a call to
     * read didn't ensure you got all the data you want
     *
     * from read() Linux manual
     *
     * On success, the number of bytes read is returned (zero indicates end of file),
     * and the file position is advanced by this number.
     * It is not an error if this number is smaller than the number of bytes requested;
     * this may happen for example because fewer bytes
     * are actually available right now (maybe because we were close to end-of-file,
     * or because we are reading from a pipe, or  from  a
     * terminal),  or  because  read()  was interrupted by a signal.
     * On error, -1 is returned, and errno is set appropriately. In this
     * case it is left unspecified whether the file position (if any) changes.
     *
     **/
    private int readN(InputStream in, byte[] b, int offset, int len) throws IOException {
        int pos = 0;
        int got;

        while(pos < len) {
            got = in.read(b, pos + offset, len - pos);

            if (debug > 10) {
                logger.log("read got # " + got);
            }

            // connection just closed by remote. 
            if (got <= 0) {
                // This happens periodically, as apache restarts
                // periodically.
                // It should be more gracefull ! - another feature for Ajp14 
                return JK_AJP13_COMM_BROKEN;
            }

            pos += got;
        }
        return pos;
     }

    /**
     * Read in a packet from the web server and store it in the passed-in
     * <CODE>Ajp13Packet</CODE> object.
     *
     * @param msg The object into which to store the incoming packet -- any
     * current contents will be overwritten.
     *
     * @return The number of bytes read on a successful read or -1 if there 
     * was an error.
     **/
    public int receive(Ajp13Packet msg) throws IOException {
        if (debug > 0) {
            logger.log("receive()");
        }

	// XXX If the length in the packet header doesn't agree with the
	// actual number of bytes read, it should probably return an error
	// value.  Also, callers of this method never use the length
	// returned -- should probably return true/false instead.
	byte b[] = msg.getBuff();
	
        int rd = readN(in, b, 0, H_SIZE );
        
        // XXX - connection closed (JK_AJP13_COMM_CLOSED)
        //     - connection broken (JK_AJP13_COMM_BROKEN)
        //
        if(rd < 0) {
            // Most likely normal apache restart.
            return rd;
        }
        
	int len = msg.checkIn();
	if( debug > 5 )
            logger.log( "Received " + rd + " " + len + " " + b[0] );
        
	// XXX check if enough space - it's assert()-ed !!!
        
 	int total_read = 0;
        
        total_read = readN(in, b, H_SIZE, len);

        // it's ok to have read 0 bytes when len=0 -- this means
        // the end of the stream has been reached.
        if (total_read < 0) {
            logger.log("can't read body, waited #" + len);
            return JK_AJP13_BAD_BODY;
        }
        
        if (total_read != len) {
            logger.log( "incomplete read, waited #" + len +
                        " got only " + total_read);
            return JK_AJP13_INCOMPLETE_BODY;
        }
        
        if (debug > 0)
            logger.log("receive:  total read = " + total_read);
	return total_read;
    }
    
    /**
     * Send a packet to the web server.  Works for any type of message.
     *
     * @param msg A packet with accumulated data to send to the server --
     * this method will write out the length in the header.  
     */
    public void send( Ajp13Packet msg ) throws IOException {
        if (debug > 0) {
            logger.log("send()");
        }

	msg.end(); // Write the packet header
	byte b[] = msg.getBuff();
	int len  = msg.getLen();

        if (debug > 5 )
            logger.log("send() " + len + " " + b[0] );

	out.write( b, 0, len );
    }
	
    /**
     * Close the socket connection to the web server.  In general, sockets
     * are maintained across many requests, so this will not be called
     * after finish().  
     *
     * @see Ajp13Interceptor#processConnection
     */
    public void close() throws IOException {
        if (debug > 0) {
            logger.log("close()");
        }

	if(null != out) {        
	    out.close();
	}
	if(null !=in) {
	    in.close();
	}
        setLogged( false );	// no more logged now 
    }

    // -------------------- Debug --------------------
    protected int debug = 0;
    
    public void setDebug(int debug) {
        this.debug = debug;
        this.reqHandler.setDebug(debug);
    }

    public void setLogger(Logger l) {
        this.logger = l;
        this.reqHandler.setLogger(l);
    }

    /**
     * XXX place holder...
     */
    Logger logger = new Logger();
}
