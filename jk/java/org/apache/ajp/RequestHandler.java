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

import java.io.*;
import java.net.Socket;
import java.util.Enumeration;
import java.security.*;
import java.security.cert.*;

import org.apache.tomcat.util.http.*;
import org.apache.tomcat.util.buf.*;


/**
 * Handle messages related with basic request information.
 *
 * This object can handle the following incoming messages:
 * - "FORWARD_REQUEST" input message ( sent when a request is passed from the web server )
 * - "RECEIVE_BODY_CHUNK" input ( sent by container to pass more body, in response to GET_BODY_CHUNK )
 *
 * It can handle the following outgoing messages:
 * - SEND_HEADERS. Pass the status code and headers.
 * - SEND_BODY_CHUNK. Send a chunk of body
 * - GET_BODY_CHUNK. Request a chunk of body data
 * - END_RESPONSE. Notify the end of a request processing.
 *
 * @author Henri Gomez [hgomez@slib.fr]
 * @author Dan Milstein [danmil@shore.net]
 * @author Keith Wannamaker [Keith@Wannamaker.org]
 * @author Costin Manolache
 */
public class RequestHandler extends AjpHandler
{
    // XXX Will move to a registry system.
    
    // Prefix codes for message types from server to container
    public static final byte JK_AJP13_FORWARD_REQUEST   = 2;

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
    // AJP14 new header
    public static final byte SC_A_SSL_KEY_SIZE  = 11; // XXX ??? 

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

    public RequestHandler() 
    {
    }

    public void init( Ajp13 ajp14 ) {
	// register incoming message handlers
	ajp14.registerMessageType( JK_AJP13_FORWARD_REQUEST,
				   "JK_AJP13_FORWARD_REQUEST",
				   this, null); // 2
	// register outgoing messages handler
	ajp14.registerMessageType( JK_AJP13_SEND_BODY_CHUNK, // 3
				   "JK_AJP13_SEND_BODY_CHUNK",
				   this,null );
	ajp14.registerMessageType( JK_AJP13_SEND_HEADERS,  // 4
				   "JK_AJP13_SEND_HEADERS",
				   this,null );
	ajp14.registerMessageType( JK_AJP13_END_RESPONSE, // 5
				   "JK_AJP13_END_RESPONSE",
				   this,null );
	ajp14.registerMessageType( JK_AJP13_GET_BODY_CHUNK, // 6
				   "JK_AJP13_GET_BODY_CHUNK",
				   this, null );
    }
    
    // -------------------- Incoming message --------------------
    public int handleAjpMessage( int type, Ajp13 channel,
				 Ajp13Packet ajp, BaseRequest req )
	throws IOException
    {
	switch( type ) {
	case RequestHandler.JK_AJP13_FORWARD_REQUEST:
	    return decodeRequest(channel, channel.hBuf, req );
	default:
	    return UNKNOWN;
	}
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
    protected int decodeRequest(Ajp13 ch, Ajp13Packet msg, BaseRequest req)
        throws IOException
    {
        
        if (debug > 0) {
            log("decodeRequest()");
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
                //
                // header name is encoded as an int
                //
                msg.getInt(); // To advance the read position
                hName = headerTransArray[hId - 1];
		vMB= headers.addValue(hName);
                msg.getMessageBytes(vMB);

                if (hId == SC_REQ_CONTENT_LENGTH) {
                    // just read content-length header
                    int contentLength = (vMB == null) ? -1 : vMB.getInt();
                    req.setContentLength(contentLength);
                } else if (hId == SC_REQ_CONTENT_TYPE) {
                    // just read content-type header
                    ByteChunk bchunk = vMB.getByteChunk();
                    req.contentType().setBytes(bchunk.getBytes(),
                                               bchunk.getOffset(),
                                               bchunk.getLength());
                }
            } else {
                //
                // header name is a string
                //
		// XXX Not very elegant
		vMB = msg.addHeader(headers);
		if (vMB == null) {
                    return 500; // wrong packet
                }
                msg.getMessageBytes(vMB);
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
                    log("Certificate convertion failed" + e );
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

	    case SC_A_SSL_KEY_SIZE: // Ajp13 !
		req.setAttribute("javax.servlet.request.key_size",
				 Integer.toString(msg.getInt()));
		return 200;
	    default:
		return 500; // Error
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
    	if(req.getContentLength() > 0) {

	    /* Read present data */
	    int err = ch.receive(ch.inBuf);
            if(err < 0) {
            	return 500;
	    }
	    
	    ch.blen = ch.inBuf.peekInt();
	    ch.pos = 0;
	    ch.inBuf.getBytes(ch.bodyBuff);
    	}
    
        if (debug > 5) {
            log(req.toString());
        }

        return 200; // Success
    }
    

    // -------------------- Messages from container to server ------------------
    
    /**
     * Send the HTTP headers back to the web server and on to the browser.
     *
     * @param status The HTTP status code to send.
     * @param statusMessage the HTTP status message to send.
     * @param headers The set of all headers.
     */
    public void sendHeaders(Ajp13 ch, Ajp13Packet outBuf,
			    int status, String statusMessage,
                            MimeHeaders headers)
        throws IOException
    {
	// XXX if more headers that MAX_SIZE, send 2 packets!
	if( statusMessage==null ) statusMessage=HttpMessages.getMessage(status);
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
        ch.send(outBuf);
    } 


    /**
     * Signal the web server that the servlet has finished handling this
     * request, and that the connection can be reused.
     */
    public void finish(Ajp13 ch, Ajp13Packet outBuf) throws IOException {
        if (debug > 0)  log("finish()");

	outBuf.reset();
        outBuf.appendByte(JK_AJP13_END_RESPONSE);
        outBuf.appendBool(true); // Reuse this connection
        outBuf.end();
        ch.send(outBuf);
    }

    /**
     * Send a chunk of response body data to the web server and on to the
     * browser.
     *
     * @param b A huffer of bytes to send.
     * @param off The offset into the buffer from which to start sending.
     * @param len The number of bytes to send.
     */    
    public void doWrite(Ajp13 ch, Ajp13Packet outBuf,
			byte b[], int off, int len)
	throws IOException
    {
        if (debug > 0) log("doWrite(byte[], " + off + ", " + len + ")");

	int sent = 0;
	while(sent < len) {
	    int to_send = len - sent;
	    to_send = to_send > Ajp13.MAX_SEND_SIZE ? Ajp13.MAX_SEND_SIZE : to_send;

	    outBuf.reset();
	    outBuf.appendByte(JK_AJP13_SEND_BODY_CHUNK);	        	
	    outBuf.appendBytes(b, off + sent, to_send);	        
	    ch.send(outBuf);
	    sent += to_send;
	}
    }

    // -------------------- Utils -------------------- 

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
   
    private static int debug=0;
    void log(String s) {
	System.out.println("RequestHandler: " + s );
    }

    // ==================== Servlet Input Support =================
    // XXX DEPRECATED
    
    public int available(Ajp13 ch) throws IOException {
        if (debug > 0) {
            log("available()");
        }

        if (ch.pos >= ch.blen) {
            if( ! refillReadBuffer(ch)) {
		return 0;
	    }
        }
        return ch.blen - ch.pos;
    }

    /**
     * Return the next byte of request body data (to a servlet).
     *
     * @see Request#doRead
     */
    public int doRead(Ajp13 ch) throws IOException 
    {
        if (debug > 0) {
            log("doRead()");
        }

        if(ch.pos >= ch.blen) {
            if( ! refillReadBuffer(ch)) {
		return -1;
	    }
        }
        return (char) ch.bodyBuff[ch.pos++];
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
    public int doRead(Ajp13 ch, byte[] b, int off, int len) throws IOException 
    {
        if (debug > 0) {
            log("doRead(byte[], int, int)");
        }

	if(ch.pos >= ch.blen) {
	    if( ! refillReadBuffer(ch)) {
		return -1;
	    }
	}

	if(ch.pos + len <= ch.blen) { // Fear the off by one error
	    // Sanity check b.length > off + len?
	    System.arraycopy(ch.bodyBuff, ch.pos, b, off, len);
	    ch.pos += len;
	    return len;
	}

	// Not enough data (blen < pos + len)
	int toCopy = len;
	while(toCopy > 0) {
	    int bytesRemaining = ch.blen - ch.pos;
	    if(bytesRemaining < 0) 
		bytesRemaining = 0;
	    int c = bytesRemaining < toCopy ? bytesRemaining : toCopy;
            
	    System.arraycopy(ch.bodyBuff, ch.pos, b, off, c);

	    toCopy    -= c;

	    off       += c;
	    ch.pos       += c; // In case we exactly consume the buffer

	    if(toCopy > 0) 
		if( ! refillReadBuffer(ch)) { // Resets blen and pos
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
    public boolean refillReadBuffer(Ajp13 ch) throws IOException 
    {
        if (debug > 0) {
            log("refillReadBuffer()");
        }

	// If the server returns an empty packet, assume that that end of
	// the stream has been reached (yuck -- fix protocol??).

	// Why not use outBuf??
	ch.inBuf.reset();
	ch.inBuf.appendByte(JK_AJP13_GET_BODY_CHUNK);
	ch.inBuf.appendInt(Ajp13.MAX_READ_SIZE);
	ch.send(ch.inBuf);
	
	int err = ch.receive(ch.inBuf);
        if(err < 0) {
	    throw new IOException();
	}
	
        // check for empty packet, which means end of stream
        if (ch.inBuf.getLen() == 0) {
            if (debug > 0) {
                log("refillReadBuffer():  "
                    + "received empty packet -> end of stream");
            }
            ch.blen = 0;
            ch.pos = 0;
            return false;
        }

    	ch.blen = ch.inBuf.peekInt();
    	ch.pos = 0;
    	ch.inBuf.getBytes(ch.bodyBuff);

	return (ch.blen > 0);
    }    

    // ==================== Servlet Output Support =================
    
    /**
     */
    public void beginSendHeaders(Ajp13 ch, Ajp13Packet outBuf,
				 int status,
                                 String statusMessage,
                                 int numHeaders) throws IOException {

        if (debug > 0) {
            log("sendHeaders()");
        }

	// XXX if more headers that MAX_SIZE, send 2 packets!

	outBuf.reset();
        outBuf.appendByte(JK_AJP13_SEND_HEADERS);

        if (debug > 0) {
            log("status is:  " + status +
                       "(" + statusMessage + ")");
        }

        // set status code and message
        outBuf.appendInt(status);
        outBuf.appendString(statusMessage);

        // write the number of headers...
        outBuf.appendInt(numHeaders);
    }

    public void sendHeader(Ajp13Packet outBuf,
			   String name, String value)
	throws IOException
    {
        int sc = headerNameToSc(name);
        if(-1 != sc) {
            outBuf.appendInt(sc);
        } else {
            outBuf.appendString(name);
        }
        outBuf.appendString(value);
    }

    public void endSendHeaders(Ajp13 ch, Ajp13Packet outBuf)
	throws IOException
    {
        outBuf.end();
        ch.send(outBuf);
    }

    /**
     * Send the HTTP headers back to the web server and on to the browser.
     *
     * @param status The HTTP status code to send.
     * @param headers The set of all headers.
     */
    public void sendHeaders(Ajp13 ch, Ajp13Packet outBuf,
			    int status, MimeHeaders headers)
        throws IOException
    {
        sendHeaders(ch, outBuf, status, HttpMessages.getMessage(status),
                    headers);
    }
    

 }
