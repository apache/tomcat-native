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

package org.apache.ajp.tomcat33;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.util.Enumeration;

import org.apache.tomcat.core.*;
import org.apache.tomcat.util.*;
import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.HttpMessages;

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
 * @see Ajp13Interceptor 
 *
 * @author Dan Milstein [danmil@shore.net]
 * @author Keith Wannamaker [Keith@Wannamaker.org]
 */
public class Ajp13
{
    public static final int MAX_PACKET_SIZE=8192;
    public static final int H_SIZE=4;  // Size of basic packet header

    public static final int  MAX_READ_SIZE = MAX_PACKET_SIZE - H_SIZE - 2;
    public static final int  MAX_SEND_SIZE = MAX_PACKET_SIZE - H_SIZE - 4;

    // Prefix codes for message types from server to container
    public static final byte JK_AJP13_FORWARD_REQUEST   = 2;
    public static final byte JK_AJP13_SHUTDOWN          = 7;
	
    // Prefix codes for message types from container to server
    public static final byte JK_AJP13_SEND_BODY_CHUNK   = 3;
    public static final byte JK_AJP13_SEND_HEADERS      = 4;
    public static final byte JK_AJP13_END_RESPONSE      = 5;
    public static final byte JK_AJP13_GET_BODY_CHUNK    = 6;
    
    // Integer codes for common response header strings
    public static final int SC_RESP_CONTENT_TYPE        = 0xA001;
    public static final int SC_RESP_CONTENT_LANGUAGE    = 0xA002;
    public static final int SC_RESP_CONTENT_LENGTH      = 0xA003;
    public static final int SC_RESP_DATE                = 0xA004;
    public static final int SC_RESP_LAST_MODIFIED       = 0xA005;
    public static final int SC_RESP_LOCATION            = 0xA006;
    public static final int SC_RESP_SET_COOKIE          = 0xA007;
    public static final int SC_RESP_SET_COOKIE2         = 0xA008;
    public static final int SC_RESP_SERVLET_ENGINE      = 0xA009;
    public static final int SC_RESP_STATUS              = 0xA00A;
    public static final int SC_RESP_WWW_AUTHENTICATE    = 0xA00B;
	
    // Integer codes for common (optional) request attribute names
    public static final byte SC_A_CONTEXT       = 1;  // XXX Unused
    public static final byte SC_A_SERVLET_PATH  = 2;  // XXX Unused
    public static final byte SC_A_REMOTE_USER   = 3;
    public static final byte SC_A_AUTH_TYPE     = 4;
    public static final byte SC_A_QUERY_STRING  = 5;
    public static final byte SC_A_JVM_ROUTE     = 6;
    public static final byte SC_A_SSL_CERT      = 7;
    public static final byte SC_A_SSL_CIPHER    = 8;
    public static final byte SC_A_SSL_SESSION   = 9;

    // Used for attributes which are not in the list above
    public static final byte SC_A_REQ_ATTRIBUTE = 10; 

    // Terminates list of attributes
    public static final byte SC_A_ARE_DONE      = (byte)0xFF;

    // Translates integer codes to names of HTTP methods
    public static final String []methodTransArray = {
        "OPTIONS",
        "GET",
        "HEAD",
        "POST",
        "PUT",
        "DELETE",
        "TRACE",
        "PROPFIND",
        "PROPPATCH",
        "MKCOL",
        "COPY",
        "MOVE",
        "LOCK",
        "UNLOCK",
        "ACL"
    };
    
    // Translates integer codes to request header names
    public static final String []headerTransArray = {
        "accept",
        "accept-charset",
        "accept-encoding",
        "accept-language",
        "authorization",
        "connection",
        "content-type",
        "content-length",
        "cookie",
        "cookie2",
        "host",
        "pragma",
        "referer",
        "user-agent"
    };


    // ============ Instance Properties ====================

    OutputStream out;
    InputStream in;

    // Buffer used of output body and headers
    OutputBuffer headersWriter=new OutputBuffer(MAX_PACKET_SIZE);
    Ajp13Packet outBuf = new Ajp13Packet( headersWriter );
    // Buffer used for input body
    Ajp13Packet inBuf  = new Ajp13Packet( MAX_PACKET_SIZE );
    // Boffer used for request head ( and headers )
    Ajp13Packet hBuf=new Ajp13Packet( MAX_PACKET_SIZE );
    
    // Holds incoming reads of request body data (*not* header data)
    byte []bodyBuff = new byte[MAX_READ_SIZE];
    
    int blen;  // Length of current chunk of body data in buffer
    int pos;   // Current read position within that buffer

    public Ajp13() 
    {
        super();
    }

    public void recycle() 
    {
      // This is a touch cargo-cultish, but I think wise.
      blen = 0; 
      pos = 0;
      headersWriter.recycle();
    }
    
    /**
     * Associate an open socket with this instance.
     */
    public void setSocket( Socket socket ) throws IOException {
	socket.setSoLinger( true, 100);
	out = socket.getOutputStream();
	in  = socket.getInputStream();
	pos = 0;
    }

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
    public int receiveNextRequest(Request req) throws IOException 
    {
	// XXX The return values are awful.

	int err = receive(hBuf);
	if(err < 0) {
	    return 500;
	}
	
	int type = (int)hBuf.getByte();
	switch(type) {
	    
	case JK_AJP13_FORWARD_REQUEST:
	    return decodeRequest(req, hBuf);
	    
	case JK_AJP13_SHUTDOWN:
	    return -2;
	}
	return 200; // XXX This is actually an error condition 
    }

    /**
     * Parse a FORWARD_REQUEST packet from the web server and store its
     * properties in the passed-in request object.
     *
     * @param req An empty (newly-recycled) request object.
     * @param msg Holds the packet which has just been sent by the web
     * server, with its read position just past the packet header (which in
     * this case includes the prefix code for FORWARD_REQUEST).
     *
     * @return 200 in case of a successful decoduing, 500 in case of error.  
     */
    private int decodeRequest( Request req, Ajp13Packet msg ) throws IOException
    {
	// XXX Awful return values

        boolean isSSL = false;

        // Translate the HTTP method code to a String.
        byte methodCode = msg.getByte();
        req.method().setString( methodTransArray[(int)methodCode - 1] );

        msg.getMessageBytes( req.protocol()); 
        msg.getMessageBytes( req.requestURI());

        msg.getMessageBytes( req.remoteAddr());
        msg.getMessageBytes( req.remoteHost());
        msg.getMessageBytes( req.serverName());
        req.setServerPort( msg.getInt());

	isSSL = msg.getBool();

	// Decode headers
	MimeHeaders headers = req.getMimeHeaders();
	int hCount = msg.getInt();
        for(int i = 0 ; i < hCount ; i++) {
            String hName = null;

	    // Header names are encoded as either an integer code starting
	    // with 0xA0, or as a normal string (in which case the first
	    // two bytes are the length).
            int isc = msg.peekInt();
            int hId = isc & 0xFF;

	    MessageBytes vMB=null;
            isc &= 0xFF00;
            if(0xA000 == isc) {
                msg.getInt(); // To advance the read position
                hName = headerTransArray[hId - 1];
		vMB= headers.addValue( hName );
            } else {
		// XXX Not very elegant
		vMB=msg.addHeader( headers );
		if( vMB==null) return 500; // wrong packet
            }

            msg.getMessageBytes( vMB ); 
        }

	byte attributeCode;
        for(attributeCode = msg.getByte() ;
            attributeCode != SC_A_ARE_DONE ;
            attributeCode = msg.getByte()) {
            switch(attributeCode) {
	    case SC_A_CONTEXT      :
		//		contextPath = msg.getString();
                break;
		
	    case SC_A_SERVLET_PATH :
		//log("SC_A_SERVLET_PATH not in use " + msg.getString());
                break;
		
	    case SC_A_REMOTE_USER  :
		req.setRemoteUser( msg.getString());
                break;
		
	    case SC_A_AUTH_TYPE    :
		req.setAuthType( msg.getString());
                break;
		
	    case SC_A_QUERY_STRING :
		msg.getMessageBytes( req.queryString());
                break;
		
	    case SC_A_JVM_ROUTE    :
		req.setJvmRoute(msg.getString());
                break;
		
	    case SC_A_SSL_CERT     :
		isSSL = true;
		req.setAttribute("javax.servlet.request.X509Certificate",
				 msg.getString());
                break;
		
	    case SC_A_SSL_CIPHER   :
		isSSL = true;
		req.setAttribute("javax.servlet.request.cipher_suite",
				 msg.getString());
                break;
		
	    case SC_A_SSL_SESSION  :
		isSSL = true;
		req.setAttribute("javax.servlet.request.ssl_session",
				  msg.getString());
                break;
		
	    case SC_A_REQ_ATTRIBUTE :
		req.setAttribute(msg.getString(), 
				 msg.getString());
                break;

	    default:
		return 500; // Error
            }
        }

        if(isSSL) {
            req.scheme().setString("https");
        }

	// Check to see if there should be a body packet coming along
	// immediately after
	MessageBytes clB=headers.getValue("content-length");
        int contentLength = (clB==null) ? -1 : clB.getInt();
    	if(contentLength > 0) {
	    req.setContentLength( contentLength );
	    /* Read present data */
	    int err = receive(inBuf);
            if(err < 0) {
            	return 500;
	    }
	    
	    blen = inBuf.peekInt();
	    pos = 0;
	    inBuf.getBytes(bodyBuff);
    	}
    
        return 200; // Success
    }

    // ==================== Servlet Input Support =================
    
    /**
     * Return the next byte of request body data (to a servlet).
     *
     * @see Ajp13Request#doRead
     */
    public int doRead() throws IOException 
    {
        if(pos >= blen) {
            if( ! refillReadBuffer()) {
		return -1;
	    }
        }
        return (char) bodyBuff[pos++];
    }
    
    /**
     * Store a chunk of request data into the passed-in byte buffer.
     *
     * @param b A buffer to fill with data from the request.
     * @param off The offset in the buffer at which to start filling.
     * @param len The number of bytes to copy into the buffer.
     *
     * @return The number of bytes actually copied into the buffer, or -1
     * if the end of the stream has been reached.
     *
     * @see Ajp13Request#doRead
     */
    public int doRead(byte[] b, int off, int len) throws IOException 
    {
	if(pos >= blen) {
	    if( ! refillReadBuffer()) {
		return -1;
	    }
	}

	if(pos + len <= blen) { // Fear the off by one error
	    // Sanity check b.length > off + len?
	    System.arraycopy(bodyBuff, pos, b, off, len);
	    pos += len;
	    return len;
	}

	// Not enough data (blen < pos + len)
	int toCopy = len;
	while(toCopy > 0) {
	    int bytesRemaining = blen - pos;
	    if(bytesRemaining < 0) 
		bytesRemaining = 0;
	    int c = bytesRemaining < toCopy ? bytesRemaining : toCopy;

	    System.arraycopy(bodyBuff, pos, b, off, c);

	    toCopy    -= c;

	    off       += c;
	    pos       += c; // In case we exactly consume the buffer

	    if(toCopy > 0) 
		if( ! refillReadBuffer()) { // Resets blen and pos
		    break;
		}
	}

	return len - toCopy;
    }
    
    /**
     * Get more request body data from the web server and store it in the 
     * internal buffer.
     *
     * @return true if there is more data, false if not.    
     */
    private boolean refillReadBuffer() throws IOException 
    {
	// If the server returns an empty packet, assume that that end of
	// the stream has been reached (yuck -- fix protocol??).

	// Why not use outBuf??
	inBuf.reset();
	inBuf.appendByte(JK_AJP13_GET_BODY_CHUNK);
	inBuf.appendInt(MAX_READ_SIZE);
	send(inBuf);
	
	int err = receive(inBuf);
        if(err < 0) {
	    throw new IOException();
	}
	
    	blen = inBuf.peekInt();
    	pos = 0;
    	inBuf.getBytes(bodyBuff);

	return (blen > 0);
    }    

    // ==================== Servlet Output Support =================
    
    /**
     * Send the HTTP headers back to the web server and on to the browser.
     *
     * @param status The HTTP status code to send.
     * @param headers The set of all headers.
     */
    public void sendHeaders(int status, MimeHeaders headers) throws IOException 
    {
	// XXX if more headers that MAX_SIZE, send 2 packets!

	outBuf.reset();
        outBuf.appendByte(JK_AJP13_SEND_HEADERS);
        outBuf.appendInt(status);
	
	outBuf.appendString(HttpMessages.getMessage( status ));
        
	int numHeaders = headers.size();
        outBuf.appendInt(numHeaders);
        
	for( int i=0 ; i < numHeaders ; i++ ) {
	    String headerName = headers.getName(i).toString();
	    int sc = headerNameToSc(headerName);
            if(-1 != sc) {
                outBuf.appendInt(sc);
            } else {
                outBuf.appendString(headerName);
            }
            outBuf.appendString(headers.getValue(i).toString() );
        }

        outBuf.end();
        send(outBuf);
    } 

    /**
     * Translate an HTTP response header name to an integer code if
     * possible.  Case is ignored.
     * 
     * @param name The name of the response header to translate.
     *
     * @return The code for that header name, or -1 if no code exists.
     */
    protected int headerNameToSc(String name)
    {       
        switch(name.charAt(0)) {
	case 'c':
	case 'C':
	    if(name.equalsIgnoreCase("Content-Type")) {
		return SC_RESP_CONTENT_TYPE;
	    } else if(name.equalsIgnoreCase("Content-Language")) {
		return SC_RESP_CONTENT_LANGUAGE;
	    } else if(name.equalsIgnoreCase("Content-Length")) {
		return SC_RESP_CONTENT_LENGTH;
	    }
            break;
            
	case 'd':
	case 'D':
	    if(name.equalsIgnoreCase("Date")) {
                    return SC_RESP_DATE;
	    }
            break;
            
	case 'l':
	case 'L':
	    if(name.equalsIgnoreCase("Last-Modified")) {
		return SC_RESP_LAST_MODIFIED;
	    } else if(name.equalsIgnoreCase("Location")) {
		return SC_RESP_LOCATION;
	    }
            break;

	case 's':
	case 'S':
	    if(name.equalsIgnoreCase("Set-Cookie")) {
		return SC_RESP_SET_COOKIE;
	    } else if(name.equalsIgnoreCase("Set-Cookie2")) {
		return SC_RESP_SET_COOKIE2;
	    }
            break;
            
	case 'w':
	case 'W':
	    if(name.equalsIgnoreCase("WWW-Authenticate")) {
		return SC_RESP_WWW_AUTHENTICATE;
	    }
            break;          
        }
        
        return -1;
    }

    /**
     * Signal the web server that the servlet has finished handling this
     * request, and that the connection can be reused.
     */
    public void finish() throws IOException 
    {
	outBuf.reset();
        outBuf.appendByte(JK_AJP13_END_RESPONSE);
        outBuf.appendBool(true); // Reuse this connection
        outBuf.end();
        send(outBuf);
    }

    /**
     * Send a chunk of response body data to the web server and on to the
     * browser.
     *
     * @param b A huffer of bytes to send.
     * @param off The offset into the buffer from which to start sending.
     * @param len The number of bytes to send.
     */    
    public void doWrite(  byte b[], int off, int len) throws IOException 
    {
	int sent = 0;
	while(sent < len) {
	    int to_send = len - sent;
	    to_send = to_send > MAX_SEND_SIZE ? MAX_SEND_SIZE : to_send;

	    outBuf.reset();
	    outBuf.appendByte(JK_AJP13_SEND_BODY_CHUNK);	        	
	    outBuf.appendBytes(b, off + sent, to_send);	        
	    send(outBuf);
	    sent += to_send;
	}
    }
    

    // ========= Internal Packet-Handling Methods =================

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
    private int receive(Ajp13Packet msg) throws IOException {
	// XXX If the length in the packet header doesn't agree with the
	// actual number of bytes read, it should probably return an error
	// value.  Also, callers of this method never use the length
	// returned -- should probably return true/false instead.
	byte b[] = msg.getBuff();
	
	int rd = in.read( b, 0, H_SIZE );
	if(rd <= 0) {
	    return rd;
	}
	
	int len = msg.checkIn();
	
	// XXX check if enough space - it's assert()-ed !!!

 	int total_read = 0;
  	while (total_read < len) {
	    rd = in.read( b, 4 + total_read, len - total_read);
            if (rd == -1) {
 		System.out.println( "Incomplete read, deal with it " + len + " " + rd);
                break;
		// XXX log
		// XXX Return an error code?
	    }
     	    total_read += rd;
	}
	return total_read;
    }

    /**
     * Send a packet to the web server.  Works for any type of message.
     *
     * @param msg A packet with accumulated data to send to the server --
     * this method will write out the length in the header.  
     */
    private void send( Ajp13Packet msg ) throws IOException {
	msg.end(); // Write the packet header
	byte b[] = msg.getBuff();
	int len  = msg.getLen();
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
	if(null != out) {        
	    out.close();
	}
	if(null !=in) {
	    in.close();
	}
    }

    /**
     * A single packet for communication between the web server and the
     * container.  Designed to be reused many times with no creation of
     * garbage.  Understands the format of data types for these packets.
     * Can be used (somewhat confusingly) for both incoming and outgoing
     * packets.  
     */
    public static class Ajp13Packet {
	byte buff[]; // Holds the bytes of the packet
	int pos;     // The current read or write position in the buffer
	OutputBuffer ob;

	int len; 
	// This actually means different things depending on whether the
	// packet is read or write.  For read, it's the length of the
	// payload (excluding the header).  For write, it's the length of
	// the packet as a whole (counting the header).  Oh, well.

	/**
	 * Create a new packet with an internal buffer of given size.
	 */
	public Ajp13Packet( int size ) {
	    buff = new byte[size];
	}

	public Ajp13Packet( byte b[] ) {
	    buff = b;
	}

	public Ajp13Packet( OutputBuffer ob ) {
	    this.ob=ob;
	    buff=ob.getBuffer();
	}
	
	public byte[] getBuff() {
	    return buff;
	}
	
	public int getLen() {
	    return len;
	}
	
	public int getByteOff() {
	    return pos;
	}

	public void setByteOff(int c) {
	    pos=c;
	}

	/** 
	 * Parse the packet header for a packet sent from the web server to
	 * the container.  Set the read position to immediately after
	 * the header.
	 *
	 * @return The length of the packet payload, as encoded in the
	 * header, or -1 if the packet doesn't have a valid header.  
	 */
	public int checkIn() {
	    pos = 0;
	    int mark = getInt();
	    len      = getInt();
	    
	    if( mark != 0x1234 ) {
		// XXX Logging
		System.out.println("BAD packet " + mark);
		dump( "In: " );
		return -1;
	    }
	    return len;
	}
	
	/**
	 * Prepare this packet for accumulating a message from the container to
	 * the web server.  Set the write position to just after the header
	 * (but leave the length unwritten, because it is as yet unknown).  
	 */
	public void reset() {
	    len = 4;
	    pos = 4;
	    buff[0] = (byte)'A';
	    buff[1] = (byte)'B';
	}
	
	/**
	 * For a packet to be sent to the web server, finish the process of
	 * accumulating data and write the length of the data payload into
	 * the header.  
	 */
	public void end() {
	    len = pos;
	    setInt( 2, len-4 );
	}
	
	// ============ Data Writing Methods ===================

	/**
	 * Write an integer at an arbitrary position in the packet, but don't
	 * change the write position.
	 *
	 * @param bpos The 0-indexed position within the buffer at which to
	 * write the integer (where 0 is the beginning of the header).
	 * @param val The integer to write.
	 */
	private void setInt( int bPos, int val ) {
	    buff[bPos]   = (byte) ((val >>>  8) & 0xFF);
	    buff[bPos+1] = (byte) (val & 0xFF);
	}

	public void appendInt( int val ) {
	    setInt( pos, val );
	    pos += 2;
	}
	
	public void appendByte( byte val ) {
	    buff[pos++] = val;
	}
	
	public void appendBool( boolean val) {
	    buff[pos++] = (byte) (val ? 1 : 0);
	}

	/**
	 * Write a String out at the current write position.  Strings are
	 * encoded with the length in two bytes first, then the string, and
	 * then a terminating \0 (which is <B>not</B> included in the
	 * encoded length).  The terminator is for the convenience of the C
	 * code, where it saves a round of copying.  A null string is
	 * encoded as a string with length 0.  
	 */
	public void appendString( String str ) {
	    // Dual use of the buffer - as Ajp13Packet and as OutputBuffer
	    // The idea is simple - fewer buffers, smaller footprint and less
	    // memcpy. The code is a bit tricky, but only local to this
	    // function.
	    if(str == null) {
		setInt( pos, 0);
		buff[pos + 2] = 0;
		pos += 3;
		return;
	    }

	    int strStart=pos;

	    // This replaces the old ( buggy and slow ) str.length()
	    // and str.getBytes(). str.length() is chars, may be != bytes
	    // and getBytes is _very_ slow.
	    // XXX setEncoding !!!
	    ob.setByteOff( pos+2 ); 
	    try {
		ob.write( str );
		ob.flushChars();
	    } catch( IOException ex ) {
		ex.printStackTrace();
	    }
	    int strEnd=ob.getByteOff();
		
	    buff[strEnd]=0; // The \0 terminator
	    int strLen=strEnd-strStart;
	    setInt( pos, strEnd - strStart );
	    pos += strLen + 3; 
	}

	/** 
	 * Copy a chunk of bytes into the packet, starting at the current
	 * write position.  The chunk of bytes is encoded with the length
	 * in two bytes first, then the data itself, and finally a
	 * terminating \0 (which is <B>not</B> included in the encoded
	 * length).
	 *
	 * @param b The array from which to copy bytes.
	 * @param off The offset into the array at which to start copying
	 * @param len The number of bytes to copy.  
	 */
	public void appendBytes( byte b[], int off, int numBytes ) {
	    appendInt( numBytes );
	    if( pos + numBytes > buff.length ) {
		System.out.println("Buffer overflow " + buff.length + " " + pos + " " + numBytes );
		// XXX Log
	    }
	    System.arraycopy( b, off, buff, pos, numBytes);
	    buff[pos + numBytes] = 0; // Terminating \0
	    pos += numBytes + 1;
	}

	
	// ============ Data Reading Methods ===================

	/**
	 * Read an integer from packet, and advance the read position past
	 * it.  Integers are encoded as two unsigned bytes with the
	 * high-order byte first, and, as far as I can tell, in
	 * little-endian order within each byte.  
	 */
	public int getInt() {
	    int result = peekInt();
	    pos += 2;
	    return result;
	}

	/**
	 * Read an integer from the packet, but don't advance the read
	 * position past it.  
	 */
	public int peekInt() {
	    int b1 = buff[pos] & 0xFF;  // No swap, Java order
	    int b2 = buff[pos + 1] & 0xFF;

	    return  (b1<<8) + b2;
	}

	public byte getByte() {
	    byte res = buff[pos];
	    pos++;
	    return res;
	}

	public byte peekByte() {
	    return buff[pos];
	}

	public boolean getBool() {
	    return (getByte() == (byte) 1);
	}

	public static final String DEFAULT_CHAR_ENCODING = "8859_1";

	public void getMessageBytes( MessageBytes mb ) {
	    int length = getInt();
	    if( (length == 0xFFFF) || (length == -1) ) {
		mb.setString( null );
		return;
	    }
	    mb.setBytes( buff, pos, length );
	    pos += length;
	    pos++; // Skip the terminating \0
	}

	public MessageBytes addHeader( MimeHeaders headers ) {
	    int length = getInt();
	    if( (length == 0xFFFF) || (length == -1) ) {
		return null;
	    }
	    MessageBytes vMB=headers.addValue( buff, pos, length );
	    pos += length;
	    pos++; // Skip the terminating \0
	    
	    return vMB;
	}
	
	/**
	 * Read a String from the packet, and advance the read position
	 * past it.  See appendString for details on string encoding.
	 **/
	public String getString() throws java.io.UnsupportedEncodingException {
	    int length = getInt();
	    if( (length == 0xFFFF) || (length == -1) ) {
		//	    System.out.println("null string " + length);
		return null;
	    }
	    String s = new String( buff, pos, length, DEFAULT_CHAR_ENCODING );

	    pos += length;
	    pos++; // Skip the terminating \0
	    return s;
	}

	/**
	 * Copy a chunk of bytes from the packet into an array and advance
	 * the read position past the chunk.  See appendBytes() for details
	 * on the encoding.
	 *
	 * @return The number of bytes copied.
	 */
	public int getBytes(byte dest[]) {
	    int length = getInt();
	    if( length > buff.length ) {
		// XXX Should be if(pos + length > buff.legth)?
		System.out.println("XXX Assert failed, buff too small ");
	    }
	
	    if( (length == 0xFFFF) || (length == -1) ) {
		System.out.println("null string " + length);
		return 0;
	    }

	    System.arraycopy( buff, pos,  dest, 0, length );
	    pos += length;
	    pos++; // Skip terminating \0  XXX I believe this is wrong but harmless
	    return length;
	}

	// ============== Debugging code =========================
	private String hex( int x ) {
	    //	    if( x < 0) x=256 + x;
	    String h=Integer.toHexString( x );
	    if( h.length() == 1 ) h = "0" + h;
	    return h.substring( h.length() - 2 );
	}

	private void hexLine( int start ) {
	    for( int i=start; i< start+16 ; i++ ) {
	      if( i < len + 4)
		System.out.print( hex( buff[i] ) + " ");
	      else 
		System.out.print( "   " );
	    }
	    System.out.print(" | ");
	    for( int i=start; i < start+16 && i < len + 4; i++ ) {
		if( Character.isLetterOrDigit( (char)buff[i] ))
		    System.out.print( new Character((char)buff[i]) );
		else
		    System.out.print( "." );
	    }
	    System.out.println();
	}
    
	public void dump(String msg) {
	    System.out.println( msg + ": " + buff + " " + pos +"/" + (len + 4));

	    for( int j=0; j < len + 4; j+=16 )
		hexLine( j );
	
	    System.out.println();
	}
    }
}
