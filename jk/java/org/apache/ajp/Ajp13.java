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
import java.io.ByteArrayInputStream;

import java.net.Socket;
import java.util.Enumeration;
import java.security.cert.X509Certificate;
import java.security.cert.CertificateFactory;

import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.buf.ByteChunk;
import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.http.HttpMessages;

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
 */
public class Ajp13 {

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
    
    // Error code for Ajp13
    public static final int  JK_AJP13_BAD_HEADER        = -100;
    public static final int  JK_AJP13_NO_HEADER         = -101;
    public static final int  JK_AJP13_COMM_CLOSED       = -102;
    public static final int  JK_AJP13_COMM_BROKEN       = -103;
    public static final int  JK_AJP13_BAD_BODY          = -104;
    public static final int  JK_AJP13_INCOMPLETE_BODY   = -105;

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
    public static final byte SC_A_SSL_KEYSIZE   = 11;

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
        "ACL",
        "REPORT",
        "VERSION-CONTROL",
        "CHECKIN",
        "CHECKOUT",
        "UNCHECKOUT",
        "SEARCH"
    };

    // id's for common request headers
    public static final int SC_REQ_ACCEPT          = 1;
    public static final int SC_REQ_ACCEPT_CHARSET  = 2;
    public static final int SC_REQ_ACCEPT_ENCODING = 3;
    public static final int SC_REQ_ACCEPT_LANGUAGE = 4;
    public static final int SC_REQ_AUTHORIZATION   = 5;
    public static final int SC_REQ_CONNECTION      = 6;
    public static final int SC_REQ_CONTENT_TYPE    = 7;
    public static final int SC_REQ_CONTENT_LENGTH  = 8;
    public static final int SC_REQ_COOKIE          = 9;
    public static final int SC_REQ_COOKIE2         = 10;
    public static final int SC_REQ_HOST            = 11;
    public static final int SC_REQ_PRAGMA          = 12;
    public static final int SC_REQ_REFERER         = 13;
    public static final int SC_REQ_USER_AGENT      = 14;
    
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
    Ajp13Packet outBuf = new Ajp13Packet( MAX_PACKET_SIZE );
    // Buffer used for input body
    Ajp13Packet inBuf  = new Ajp13Packet( MAX_PACKET_SIZE );
    // Buffer used for request head ( and headers )
    Ajp13Packet hBuf=new Ajp13Packet( MAX_PACKET_SIZE );
    
    // Holds incoming reads of request body data (*not* header data)
    byte []bodyBuff = new byte[MAX_READ_SIZE];
    
    int blen;  // Length of current chunk of body data in buffer
    int pos;   // Current read position within that buffer

    boolean end_of_stream;  // true if we've received an empty packet

    public Ajp13() {
	super();
	initBuf();
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
    public int receiveNextRequest(AjpRequest req) throws IOException {
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
            return -1;  // Indicate it's a disconnection from the remote end
        }
        
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
     * Try to decode Headers - AJP13 will do nothing but descendant will	
     * override this method to handle new headers (ie SSL_KEY_SIZE in AJP14)
     */
    int decodeMoreHeaders(AjpRequest req, byte attribute, Ajp13Packet msg)
    {
	return 500;
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
    protected int decodeRequest(AjpRequest req, Ajp13Packet msg)
        throws IOException
    {
        
        if (debug > 0) {
            logger.log("decodeRequest()");
        }

	// XXX Awful return values

        boolean isSSL = false;

        // Translate the HTTP method code to a String.
        byte methodCode = msg.getByte();
        req.method().setString(methodTransArray[(int)methodCode - 1]);

        msg.getMessageBytes(req.protocol()); 
        msg.getMessageBytes(req.requestURI());

        msg.getMessageBytes(req.remoteAddr());
        msg.getMessageBytes(req.remoteHost());
        msg.getMessageBytes(req.serverName());
        req.setServerPort(msg.getInt());

	isSSL = msg.getBool();

	// Decode headers
	MimeHeaders headers = req.headers();
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
		vMB= headers.addValue(hName);
            } else {
		// XXX Not very elegant
		vMB = msg.addHeader(headers);
		if (vMB == null) {
                    return 500; // wrong packet
                }
            }

            msg.getMessageBytes(vMB);

            // set content length, if this is it...
            if (hId == SC_REQ_CONTENT_LENGTH) {
                int contentLength = (vMB == null) ? -1 : vMB.getInt();
                req.setContentLength(contentLength);
            } else if (hId == SC_REQ_CONTENT_TYPE) {
                ByteChunk bchunk = vMB.getByteChunk();
                req.contentType().setBytes(bchunk.getBytes(),
                                           bchunk.getOffset(),
                                           bchunk.getLength());
            }
        }

	byte attributeCode;
        for(attributeCode = msg.getByte() ;
            attributeCode != SC_A_ARE_DONE ;
            attributeCode = msg.getByte()) {
            switch(attributeCode) {
	    case SC_A_CONTEXT      :
                break;
		
	    case SC_A_SERVLET_PATH :
                break;
		
	    case SC_A_REMOTE_USER  :
                msg.getMessageBytes(req.remoteUser());
                break;
		
	    case SC_A_AUTH_TYPE    :
                msg.getMessageBytes(req.authType());
                break;
		
	    case SC_A_QUERY_STRING :
		msg.getMessageBytes(req.queryString());
                break;
		
	    case SC_A_JVM_ROUTE    :
                msg.getMessageBytes(req.jvmRoute());
                break;
		
	    case SC_A_SSL_CERT     :
		isSSL = true;
                // Transform the string into certificate.
                String certString = msg.getString();
                byte[] certData = certString.getBytes();
                ByteArrayInputStream bais = new ByteArrayInputStream(certData);
 
                // Fill the first element.
                X509Certificate jsseCerts[] = null;
                try {
                    CertificateFactory cf =
                        CertificateFactory.getInstance("X.509");
                    X509Certificate cert = (X509Certificate)
                        cf.generateCertificate(bais);
                    jsseCerts =  new X509Certificate[1];
                    jsseCerts[0] = cert;
                } catch(java.security.cert.CertificateException e) {
                    logger.log("Certificate convertion failed" + e );
                }
 
                req.setAttribute("javax.servlet.request.X509Certificate",
                                 jsseCerts);
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

	    case SC_A_SSL_KEYSIZE :
                req.setAttribute("javax.servlet.request.key_size",
                                 new Integer (msg.getInt()));
                break;
	    default:
		if (decodeMoreHeaders(req, attributeCode, msg) != 500)
		    break;

		return 500;
            }
        }

        if(isSSL) {
            req.setScheme(req.SCHEME_HTTPS);
            req.setSecure(true);
        }

        // set cookies on request now that we have all headers
        req.cookies().setHeaders(req.headers());

	// Check to see if there should be a body packet coming along
	// immediately after
    	if(req.getContentLength() != 0) {

	    /* Read present data */
	    int err = receive(inBuf);
            if(err < 0) {
            	return 500;
	    }
	    
	    blen = inBuf.peekInt();
	    pos = 0;
	    inBuf.getBytes(bodyBuff);
    	}
    
        if (debug > 5) {
            logger.log(req.toString());
        }

        return 200; // Success
    }

    // ==================== Servlet Input Support =================

    public int available() throws IOException {
        if (debug > 0) {
            logger.log("available()");
        }

        if (pos >= blen) {
            if( ! refillReadBuffer()) {
		return 0;
	    }
        }
        return blen - pos;
    }

    /**
     * Return the next byte of request body data (to a servlet).
     *
     * @see Request#doRead
     */
    public int doRead() throws IOException 
    {
        if (debug > 0) {
            logger.log("doRead()");
        }

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
     * @see Request#doRead
     */
    public int doRead(byte[] b, int off, int len) throws IOException 
    {
        if (debug > 0) {
            logger.log("doRead(byte[], int, int)");
        }

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

	// Not enough data (blen < pos + len) or chunked encoded
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
        if (debug > 0) {
            logger.log("refillReadBuffer()");
        }

	// If the server returns an empty packet, assume that that end of
	// the stream has been reached (yuck -- fix protocol??).
        if (end_of_stream) {
            return false;
        }

	// Why not use outBuf??
	inBuf.reset();
	inBuf.appendByte(JK_AJP13_GET_BODY_CHUNK);
	inBuf.appendInt(MAX_READ_SIZE);
	send(inBuf);
	
	int err = receive(inBuf);
        if(err < 0) {
	    throw new IOException();
	}
	
        // No data received.
        if( inBuf.getLen() == 0 ) {
            pos=0;
            blen=0;
            end_of_stream = true;
            return false;
        }

    	blen = inBuf.peekInt();
    	pos = 0;
    	inBuf.getBytes(bodyBuff);

	return (blen > 0);
    }    

    // ==================== Servlet Output Support =================
    
    /**
     */
    public void beginSendHeaders(int status,
                                 String statusMessage,
                                 int numHeaders) throws IOException {

        if (debug > 0) {
            logger.log("sendHeaders()");
        }

	// XXX if more headers that MAX_SIZE, send 2 packets!

	outBuf.reset();
        outBuf.appendByte(JK_AJP13_SEND_HEADERS);

        if (debug > 0) {
            logger.log("status is:  " + status +
                       "(" + statusMessage + ")");
        }

        // set status code and message
        outBuf.appendInt(status);
        outBuf.appendString(statusMessage);

        // write the number of headers...
        outBuf.appendInt(numHeaders);
    }

    public void sendHeader(String name, String value) throws IOException {
        int sc = headerNameToSc(name);
        if(-1 != sc) {
            outBuf.appendInt(sc);
        } else {
            outBuf.appendString(name);
        }
        outBuf.appendString(value);
    }

    public void endSendHeaders() throws IOException {
        outBuf.end();
        send(outBuf);
    }

    /**
     * Send the HTTP headers back to the web server and on to the browser.
     *
     * @param status The HTTP status code to send.
     * @param headers The set of all headers.
     */
    public void sendHeaders(int status, MimeHeaders headers)
        throws IOException {
        sendHeaders(status, HttpMessages.getMessage(status), headers);
    }

    /**
     * Send the HTTP headers back to the web server and on to the browser.
     *
     * @param status The HTTP status code to send.
     * @param statusMessage the HTTP status message to send.
     * @param headers The set of all headers.
     */
    public void sendHeaders(int status, String statusMessage, MimeHeaders headers)
        throws IOException {
	// XXX if more headers that MAX_SIZE, send 2 packets!

	outBuf.reset();
        outBuf.appendByte(JK_AJP13_SEND_HEADERS);
        outBuf.appendInt(status);
	
	outBuf.appendString(statusMessage);
        
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
    public void finish() throws IOException {
        if (debug > 0) {
            logger.log("finish()");
        }

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
    public void doWrite(byte b[], int off, int len) throws IOException {
        if (debug > 0) {
            logger.log("doWrite(byte[], " + off + ", " + len + ")");
        }

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

            // connection just closed by remote
            if (got == 0)
                return JK_AJP13_COMM_CLOSED;

            // connection dropped by remote
            if (got < 0)
                return JK_AJP13_COMM_BROKEN;

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
        logger.log("bad read: " + rd);
         return rd;
    }

	int len = msg.checkIn();
    if (debug > 0) {
        logger.log("receive:  len = " + len);
	}

	// XXX check if enough space - it's assert()-ed !!!

 	int total_read = 0;

    total_read = readN(in, b, H_SIZE, len);

    if (total_read < 0) {
        logger.log("can't read body, waited #" + len);
        return JK_AJP13_BAD_BODY;
    }

    if (total_read != len) {
        logger.log( "incomplete read, waited #" + len + " got only " + total_read);
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
    private void send( Ajp13Packet msg ) throws IOException {
        if (debug > 0) {
            logger.log("send()");
        }

	msg.end(); // Write the packet header
	byte b[] = msg.getBuff();
	int len  = msg.getLen();

        if (debug > 0) {
            logger.log("sending msg, len = " + len);
        }

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
    }

    // -------------------- Debug --------------------
    private int debug = 0;

    public void setDebug(int debug) {
        this.debug = debug;
    }

    /**
     * XXX place holder...
     */
    Logger logger = new Logger();
    class Logger {
        void log(String msg) {
            System.out.println("[Ajp13] " + msg);
        }
        
        void log(String msg, Throwable t) {
            System.out.println("[Ajp13] " + msg);
            t.printStackTrace(System.out);
        }
    }
}
