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

package org.apache.ajp.tomcat4;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.util.Enumeration;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.Cookie;

import org.apache.catalina.Globals;
import org.apache.catalina.util.RequestUtil;

import org.apache.ajp.Ajp13Packet;

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
 * @author Kevin Seguin [seguin@motive.com]
 */
public class Ajp13
{
    private static final String match =
	";" + Globals.SESSION_PARAMETER_NAME + "=";

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

    private Ajp13Connector connector;
    private int id;
    private String name;
    private int debug;
    private Ajp13Logger logger = new Ajp13Logger();

    public Ajp13(Ajp13Connector connector, int id) {
        super();
        this.connector = connector;
        this.id = id;
        this.name = "Ajp13[" + id + "]";
        this.debug = connector.getDebug();
        this.logger.setConnector(connector);
        this.logger.setName(name);
    }

    public void recycle() 
    {
        if (debug > 0) {
            logger.log("recycle()");
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
            logger.log("setSocket()");
        }
        
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
    public int receiveNextRequest(Ajp13Request req) throws IOException 
    {
        if (debug > 0) {
            logger.log("receiveNextRequest()");
        }
        
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
    private int decodeRequest(Ajp13Request req, Ajp13Packet msg)
        throws IOException {
        
        if (debug > 0) {
            logger.log("decodeRequest()");
        }

	// XXX Awful return values

        boolean isSSL = false;
        int contentLength = -1;

        // Translate the HTTP method code to a String.
        byte methodCode = msg.getByte();
        req.setMethod(methodTransArray[(int)methodCode - 1]);
        if (debug > 5) {
            logger.log("method = " + req.getMethod());
        }

        req.setProtocol(msg.getString());
        if (debug > 5) {
            logger.log("protocol = " + req.getProtocol());
        }
        
        // get uri and Parse any requested session ID out of the request URI
        String uri = msg.getString();
	int semicolon = uri.indexOf(match);
	if (semicolon >= 0) {
	    String rest = uri.substring(semicolon + match.length());
	    int semicolon2 = rest.indexOf(";");
	    if (semicolon2 >= 0) {
		req.setRequestedSessionId(rest.substring(0, semicolon2));
		rest = rest.substring(semicolon2);
	    } else {
		req.setRequestedSessionId(rest);
		rest = "";
	    }
	    req.setRequestedSessionURL(true);
	    uri = uri.substring(0, semicolon) + rest;
	    if (debug >= 1)
	        logger.log(" Requested URL session id is " +
                           ((HttpServletRequest) req.getRequest())
                           .getRequestedSessionId());
	} else {
	    req.setRequestedSessionId(null);
	    req.setRequestedSessionURL(false);
	}

        req.setRequestURI(uri);
        if (debug > 5) {
            logger.log("requestURI = " + req.getRequestURI());
        }

        req.setRemoteAddr(msg.getString());
        if (debug > 5) {
            logger.log("remoteAddr = " + req.getRemoteAddr());
        }
        
        req.setRemoteHost(msg.getString());
        if (debug > 5) {
            logger.log("remoteHost = " + req.getRemoteHost());
        }
        
        req.setServerName(msg.getString());
        if (debug > 5) {
            logger.log("serverName = " + req.getServerName());
        }
        
        req.setServerPort(msg.getInt());
        if (debug > 5) {
            logger.log("port = " + req.getServerPort());
        }

	isSSL = msg.getBool();

	// Decode headers
	int hCount = msg.getInt();
        for(int i = 0 ; i < hCount ; i++) {
            String hName = null;
            String hValue = null;

	    // Header names are encoded as either an integer code starting
	    // with 0xA0, or as a normal string (in which case the first
	    // two bytes are the length).
            int isc = msg.peekInt();
            int hId = isc & 0xFF;

            isc &= 0xFF00;
            if(0xA000 == isc) {
                msg.getInt(); // To advance the read position
                hName = headerTransArray[hId - 1];
            } else {
                hName = msg.getString();
                hId = -1;
            }

            hValue = msg.getString();
            if (debug > 5) {
                logger.log("header:  " + hName + " = " + hValue);
            }

            switch (hId) {
            case SC_REQ_CONTENT_TYPE:
                {
                    req.setContentType(hValue);
                    if (debug > 0) {
                        logger.log("setting content type to " + hValue);
                        logger.log(req.getContentType());
                    }
                    break;
                }
            case SC_REQ_CONTENT_LENGTH:
                {
                    if (hValue != null) {
                        try {
                            contentLength = Integer.parseInt(hValue);
                        } catch (Exception e) {
                            logger.log("parse content-length", e);
                        }
                    }
                    break;
                }
            case SC_REQ_COOKIE:
            case SC_REQ_COOKIE2:
                {
                    Cookie cookies[] = RequestUtil.parseCookieHeader(hValue);
                    for (int j = 0; j < cookies.length; j++) {
                        Cookie cookie = cookies[j];
                        if (cookie.getName().equals(Globals.SESSION_COOKIE_NAME)) {
                            // Override anything requested in the URL
                            if (!req.isRequestedSessionIdFromCookie()) {
                                // Accept only the first session id cookie
                                req.setRequestedSessionId(cookie.getValue());
                                req.setRequestedSessionCookie(true);
                                req.setRequestedSessionURL(false);
                                if (debug > 0) {
                                    logger.log(" Requested cookie session id is " +
                                               ((HttpServletRequest) req.getRequest())
                                               .getRequestedSessionId());
                                }
                            }
                        }
                        if (debug > 0) {
                            logger.log(" Adding cookie " + cookie.getName() + "=" +
                                       cookie.getValue());
                        }
                        req.addCookie(cookie);                    
                    }
                }
            }

            req.addHeader(hName, hValue);
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
                Ajp13Principal p = new Ajp13Principal(msg.getString());
                req.setUserPrincipal(p);
                if (debug > 5) {
                    logger.log("remoteUser = " + req.getRemoteUser());
                }
                break;
		
	    case SC_A_AUTH_TYPE    :
		req.setAuthType( msg.getString());
                if (debug > 5) {
                    logger.log("authType = " + req.getAuthType());
                }
                break;
		
	    case SC_A_QUERY_STRING :
                req.setQueryString(msg.getString());
                if (debug > 5) {
                    logger.log("queryString = " + req.getQueryString());
                }
                break;
		
	    case SC_A_JVM_ROUTE    :
                // FIXME
		//req.setJvmRoute(msg.getString());
                break;
		
	    case SC_A_SSL_CERT     :
		isSSL = true;
		req.setAttribute("javax.servlet.request.X509Certificate",
				 msg.getString());
                if (debug > 5) {
                    logger.log("setting javax.servlet.request.X509Certificate");
                }
                break;
		
	    case SC_A_SSL_CIPHER   :
		isSSL = true;
		req.setAttribute("javax.servlet.request.cipher_suite",
				 msg.getString());
                if (debug > 5) {
                    logger.log("javax.servlet.request.cipher_suite = " +
                               req.getAttribute("javax.servlet.request.cipher_suite"));
                }
                break;
		
	    case SC_A_SSL_SESSION  :
		isSSL = true;
		req.setAttribute("javax.servlet.request.ssl_session",
                                 msg.getString());
                if (debug > 5) {
                    logger.log("javax.servlet.request.ssl_session = " +
                               req.getAttribute("javax.servlet.request.ssl_session"));
                }
                break;
		
	    case SC_A_REQ_ATTRIBUTE :
                {
                    String aname = msg.getString();
                    String avalue = msg.getString();
                    req.setAttribute(aname, avalue);

                    if (debug > 5) {
                        logger.log("setting attr:  " + aname + " = " + avalue);
                    }
                }
                break;

	    default:
		return 500; // Error
            }
        }

        req.setSecure(isSSL);
        if(isSSL) {
            req.setScheme("https");
        } else {
            req.setScheme("http");
        }

        if (debug > 5) {
            logger.log("secure = " + req.isSecure());
            logger.log("scheme = " + req.getScheme());
        }

	// Check to see if there should be a body packet coming along
	// immediately after
    	if(contentLength > 0) {
            if (debug > 0) {
                logger.log("contentLength = " + contentLength +
                           ", reading data ...");
            }
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
     * @see Ajp13Request#doRead
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
     * @see Ajp13Request#doRead
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
        if (debug > 0) {
            logger.log("refillReadBuffer()");
        }

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
    public void sendHeaders(Ajp13Response resp)
        throws IOException {

        if (debug > 0) {
            logger.log("sendHeaders()");
        }

	// XXX if more headers that MAX_SIZE, send 2 packets!

	outBuf.reset();
        outBuf.appendByte(JK_AJP13_SEND_HEADERS);

        if (debug > 0) {
            logger.log("status is:  " + resp.getStatus() +
                       "(" + resp.getStatusMessage() + ")");
        }

        // set status code and message
        outBuf.appendInt(resp.getStatus());
        outBuf.appendString(resp.getStatusMessage());

        // can have multiple headers of the same name...
        // need to loop through headers once to get total
        // count, once to add header to outBuf
        String[] hnames = resp.getHeaderNames();
        Object[] hvalues = new Object[hnames.length];
        int numHeaders = 0;

        int i;
        for (i = 0; i < hnames.length; ++i) {
            String[] tmp = resp.getHeaderValues(hnames[i]);
            numHeaders += tmp.length;
            hvalues[i] = tmp;
        }

        if (debug > 0) {
            logger.log("num headers = " + numHeaders);
        }

        // write the number of headers...
        outBuf.appendInt(numHeaders);

        // now write headers/values
        for (i = 0; i < hnames.length; ++i) {
	    String name = hnames[i];
            String[] values = (String[])hvalues[i];

            for (int j = 0; j < values.length; ++j) {
                logger.log("adding header " + name + " : " + values[j]);
                int sc = headerNameToSc(name);
                if(-1 != sc) {
                    outBuf.appendInt(sc);
                } else {
                    outBuf.appendString(name);
                }
                outBuf.appendString(values[j]);
            }
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
        if (debug > 0) {
            logger.log("receive()");
        }

	// XXX If the length in the packet header doesn't agree with the
	// actual number of bytes read, it should probably return an error
	// value.  Also, callers of this method never use the length
	// returned -- should probably return true/false instead.
	byte b[] = msg.getBuff();
	
	int rd = in.read( b, 0, H_SIZE );
	if(rd <= 0) {
            logger.log("bad read: " + rd);
	    return rd;
	}
	
	int len = msg.checkIn();
        logger.log("receive:  len = " + len);
	
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

}

class Ajp13Principal implements java.security.Principal {
    String user;
    Ajp13Principal(String user) {
        this.user = user;
    }
    public boolean equals(Object o) {
        if (o == null) {
            return false;
        } else if (!(o instanceof Ajp13Principal)) {
            return false;
        } else if (o == this) {
            return true;
        } else if (this.user == null && ((Ajp13Principal)o).user == null) {
            return true;
        } else if (user != null) {
            return user.equals( ((Ajp13Principal)o).user);
        } else {
            return false;
        }
    }
    
    public String getName() {
        return user;
    }
    
    public int hashCode() {
        if (user == null) return 0;
        else return user.hashCode();
    }
    
    public String toString() {
        return getName();
    }
}
