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
    // Original AJP13 commands    
    // Prefix codes for message types from server to container
    public static final byte JK_AJP13_FORWARD_REQUEST   = 2;
    public static final byte JK_AJP13_SHUTDOWN          = 7;
	
    // Prefix codes for message types from container to server
    public static final byte JK_AJP13_SEND_BODY_CHUNK   = 3;
    public static final byte JK_AJP13_SEND_HEADERS      = 4;
    public static final byte JK_AJP13_END_RESPONSE      = 5;
    public static final byte JK_AJP13_GET_BODY_CHUNK    = 6;
    


    // New AJP14 commands
    
    // Initial Login Phase (web server -> servlet engine)
    public static final byte JK_AJP14_LOGINIT_CMD			= 0x10;
    
    // Second Login Phase (servlet engine -> web server), md5 seed is received
    public static final byte JK_AJP14_LOGSEED_CMD			= 0x11;
    
    // Third Login Phase (web server -> servlet engine), md5 of seed + secret is sent
    public static final byte JK_AJP14_LOGCOMP_CMD			= 0x12;
    
    // Login Accepted (servlet engine -> web server)
    public static final byte JK_AJP14_LOGOK_CMD         	= 0x13;

    // Login Rejected (servlet engine -> web server), will be logged
	public static final byte JK_AJP14_LOGNOK_CMD        	= 0x14;
    
    // Context Query (web server -> servlet engine), which URI are handled by servlet engine ?
    public static final byte JK_AJP14_CONTEXT_QRY_CMD		= 0x15;
    
    // Context Info (servlet engine -> web server), URI handled response
    public static final byte JK_AJP14_CONTEXT_INFO_CMD		= 0x16;
    
    // Context Update (servlet engine -> web server), status of context changed
    public static final byte JK_AJP14_CONTEXT_UPDATE_CMD	= 0x17;
    
    // Servlet Engine Status (web server -> servlet engine), what's the status of the servlet engine ?
    public static final byte JK_AJP14_STATUS_CMD			= 0x18;
    
    // Secure Shutdown command (web server -> servlet engine), please servlet stop yourself.
    public static final byte JK_AJP14_SHUTDOWN_CMD			= 0x19;
    
    // Secure Shutdown command Accepted (servlet engine -> web server)
    public static final byte JK_AJP14_SHUTOK_CMD			= 0x1A;
    
    // Secure Shutdown Rejected (servlet engine -> web server)
    public static final byte JK_AJP14_SHUTNOK_CMD           = 0x1B;
    
    // Context Status (web server -> servlet engine), what's the status of the context ?
    public static final byte JK_AJP14_CONTEXT_STATE_CMD     = 0x1C;
    
    // Context Status Reply (servlet engine -> web server), status of context
    public static final byte JK_AJP14_CONTEXT_STATE_REP_CMD = 0x1D;
    
    // Unknown Packet Reply (web server <-> servlet engine), when a packet couldn't be decoded
    public static final byte JK_AJP14_UNKNOW_PACKET_CMD     = 0x1E;


    // -------------------- Other constants -------------------- 

    // Entropy Packet Size
    public static final int	 AJP14_ENTROPY_SEED_LEN 		= 32;
    public static final int	 AJP14_COMPUTED_KEY_LEN 		= 32;
    
    // web-server want context info after login
    public static final int	 AJP14_CONTEXT_INFO_NEG			= 0x80000000;
    
    // web-server want context updates
    public static final int	 AJP14_CONTEXT_UPDATE_NEG		= 0x40000000;
    
    // web-server want compressed stream
    public static final int  AJP14_GZIP_STREAM_NEG			= 0x20000000;
    
    // web-server want crypted DES56 stream with secret key
    public static final int  AJP14_DES56_STREAM_NEG      	= 0x10000000;
    
    // Extended info on server SSL vars
    public static final int  AJP14_SSL_VSERVER_NEG			= 0x08000000;
    
    // Extended info on client SSL vars
    public static final int  AJP14_SSL_VCLIENT_NEG			= 0x04000000;
    
    // Extended info on crypto SSL vars
    public static final int	 AJP14_SSL_VCRYPTO_NEG			= 0x02000000;
    
    // Extended info on misc SSL vars
    public static final int  AJP14_SSL_VMISC_NEG         	= 0x01000000;
    
    // mask of protocol supported
    public static final int  AJP14_PROTO_SUPPORT_AJPXX_NEG  = 0x00FF0000;
    
    // communication could use AJP14
    public static final int  AJP14_PROTO_SUPPORT_AJP14_NEG  = 0x00010000;
    
    // communication could use AJP15
    public static final int  AJP14_PROTO_SUPPORT_AJP15_NEG  = 0x00020000;
    
    // communication could use AJP16
    public static final int  AJP14_PROTO_SUPPORT_AJP16_NEG  = 0x00040000;
    
    // Some failure codes
    public static final int  AJP14_BAD_KEY_ERR				 = 0xFFFFFFFF;
    public static final int  AJP14_ENGINE_DOWN_ERR           = 0xFFFFFFFE;
    public static final int  AJP14_RETRY_LATER_ERR           = 0xFFFFFFFD;
    public static final int  AJP14_SHUT_AUTHOR_FAILED_ERR    = 0xFFFFFFFC;
    
    // Some status codes
    public static final byte AJP14_CONTEXT_DOWN       		 = 0x01;
    public static final byte AJP14_CONTEXT_UP         		 = 0x02;
    public static final byte AJP14_CONTEXT_OK         		 = 0x03;

    
    public static final int MAX_PACKET_SIZE=8192;
    public static final int H_SIZE=4;  // Size of basic packet header

    public static final int  MAX_READ_SIZE = MAX_PACKET_SIZE - H_SIZE - 2;
    public static final int  MAX_SEND_SIZE = MAX_PACKET_SIZE - H_SIZE - 4;

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

    
    // ============ Instance Properties ====================
    // Ajp13
    OutputStream out;
    InputStream in;

    // Buffer used of output body and headers
    Ajp14Packet outBuf = new Ajp14Packet( MAX_PACKET_SIZE );
    // Buffer used for input body
    Ajp14Packet inBuf  = new Ajp14Packet( MAX_PACKET_SIZE );
    // Buffer used for request head ( and headers )
    Ajp14Packet hBuf=new Ajp14Packet( MAX_PACKET_SIZE );
    
    // Holds incoming reads of request body data (*not* header data)
    byte []bodyBuff = new byte[MAX_READ_SIZE];
    
    int blen;  // Length of current chunk of body data in buffer
    int pos;   // Current read position within that buffer
    
    // AJP14 
    boolean logged = false;
    int	    webserverNegociation	= 0;
    String  webserverName;
    String  seed="seed";// will use random
    String  password;
    String  containerSignature="Ajp14-based container";

    public Ajp14() 
    {
        super();
	setSeed("myveryrandomentropy");
	setPassword("myverysecretkey");
    }

    public void setContainerSignature( String s ) {
	containerSignature=s;
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
    
    /**
     * Set the original entropy seed
     */
    public void setSeed(String pseed) 
    {
	String[] credentials = new String[1];
	credentials[0] = pseed;
	seed = digest(credentials, "md5");
    }

    /**
     * Get the original entropy seed
     */
    public String getSeed()
    {
	return seed;
    }

    /**
     * Set the secret password
     */
    public void setPassword(String ppwd) 
    {
	password = ppwd;
    }
    
    /**
     * Get the secret password
     */
    public String getPassword()
    {
	return password;
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
	if( debug > 0 ) log( "Received " + type + " " + MSG_NAME[type]);
	
	switch(type) {
	    
	case JK_AJP13_FORWARD_REQUEST:
		if (logged)
	    	return decodeRequest(req, hBuf);
	    else {
			log("can't handle forward request until logged");
			return 200;
		}

	case JK_AJP13_SHUTDOWN:
		if (logged)
	    	return -2;
		else {
			log("can't handle AJP13 shutdown command until logged");
			return 200;
		}

	case JK_AJP14_LOGINIT_CMD :
		if (! logged)
			return handleLogInit(hBuf);
		else {
			log("shouldn't handle logint command when logged");
			return 200;
		}

	case JK_AJP14_LOGCOMP_CMD :
		if (! logged)
			return handleLogComp(hBuf);
		else {
			log("shouldn't handle logcomp command when logged");
			return 200;
		}

	case JK_AJP14_CONTEXT_QRY_CMD :
		if (logged)
			return handleContextQuery(hBuf);
		else {
			log("can't handle contextquery command until logged");
			return 200;
		}

	case JK_AJP14_STATUS_CMD :
		if (logged)
			return handleStatus(hBuf);
		else {
			log("can't handle status command until logged");
			return 200;
		}

	case JK_AJP14_SHUTDOWN_CMD :
		if (logged)
			return handleShutdown(hBuf);
		else {
			log("can't handle AJP14 shutdown command until logged");
			return 200;
		}

	case JK_AJP14_CONTEXT_STATE_CMD :
		if (logged)
			return handleContextState(hBuf);
		else {
			log("can't handle contextstate command until logged");
			return 200;
		}

	case JK_AJP14_UNKNOW_PACKET_CMD :
		if (logged)
			return handleUnknowPacket(hBuf);
		else {
			log("can't handle unknown packet command until logged");
			return 200;
		}
	}
	log("unknown command " + type + " received");
	return 200; // XXX This is actually an error condition 
    }

    //-------------------- Implementation for various protocol commands --------------------

    /**
     * Try to decode Headers - AJP13 will do nothing but descendant will	
     * override this method to handle new headers (ie SSL_KEY_SIZE in AJP14)
     */
    int decodeMoreHeaders(BaseRequest req, byte attribute, Ajp14Packet msg)
    {
	if (attribute == SC_A_SSL_KEY_SIZE) {
	    req.setAttribute("javax.servlet.request.key_size", Integer.toString(msg.getInt()));
	    return 200;
	}
	return 500; // Error
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
    protected int decodeRequest(BaseRequest req, Ajp14Packet msg)
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
    	if(req.getContentLength() > 0) {

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
            log(req.toString());
        }

        return 200; // Success
    }
    
    /**
     * Handle the Initial Login Message from Web-Server
     *
     * Get the requested Negociation Flags
     * Get also the Web-Server Name
     * 
	 * Send Login Seed (MD5 of seed)
	 */
    private int handleLogInit( Ajp14Packet msg ) throws IOException
    {
	webserverNegociation = msg.getLongInt();
	webserverName 		 = msg.getString();
	log("in handleLogInit with nego " +
	    decodeNegociation(webserverNegociation) + " from webserver " + webserverName);
	
	outBuf.reset();
        outBuf.appendByte(JK_AJP14_LOGSEED_CMD);
	String[] credentials = new String[1];
	credentials[0] = getSeed();
        outBuf.appendXBytes(getSeed().getBytes(), 0, AJP14_ENTROPY_SEED_LEN);
	log("in handleLogInit: sent entropy " + getSeed());
        outBuf.end();
        send(outBuf);
	return 304;
    }
    
    /**
     * Handle the Second Phase of Login (accreditation)
     * 
     * Get the MD5 digest of entropy + secret password
     * If the authentification is valid send back LogOk
     * If the authentification failed send back LogNok
     */
    private int handleLogComp( Ajp14Packet msg ) throws IOException
    {
	// log("in handleLogComp :");
	
	byte [] rdigest = new byte[AJP14_ENTROPY_SEED_LEN];
	
	if (msg.getXBytes(rdigest, AJP14_ENTROPY_SEED_LEN) < 0)
	    return 200;
	
	String[] credentials = new String[2];
	credentials[0] = getSeed();
	credentials[1] = getPassword();
	String computed = digest(credentials, "md5");
	String received = new String(rdigest);
	
	// XXX temp workaround, to test the rest of the connector.
	
	if ( ! computed.equalsIgnoreCase(received)) {
	    log("in handleLogComp : authentification failure received=" +
		received + " awaited=" + computed);
	}
	
	if (false ) { // ! computed.equalsIgnoreCase(received)) {
	    log("in handleLogComp : authentification failure received=" +
		received + " awaited=" + computed);
	    
	    // we should have here a security mecanism which could maintain
	    // a list of remote IP which failed too many times
	    // so we could reject them quickly at next connect
	    outBuf.reset();
	    outBuf.appendByte(JK_AJP14_LOGNOK_CMD);
	    outBuf.appendLongInt(AJP14_BAD_KEY_ERR);
	    outBuf.end();
	    send(outBuf);
	    return 200;
	} else {
	    logged = true;		// logged we can go process requests
	    outBuf.reset();
	    outBuf.appendByte(JK_AJP14_LOGOK_CMD);
	    outBuf.appendLongInt(getProtocolFlags(webserverNegociation));
	    outBuf.appendString( containerSignature );
	    outBuf.end(); 
	    send(outBuf);
	}
	
	return (304);
    }

    private int handleContextQuery( Ajp14Packet msg ) throws IOException
    {
	log("in handleContextQuery :");
	return (304);
    }
    
    private int handleStatus( Ajp14Packet msg ) throws IOException
    {
	log("in handleStatus :");
	return (304);
    }

    private int handleShutdown( Ajp14Packet msg ) throws IOException
    {
	log("in handleShutdown :");
	return (304);
    }
    
    private int handleContextState( Ajp14Packet msg ) throws IOException
    {
	log("in handleContextState :");
	return (304);
    }
    
    private int handleUnknowPacket( Ajp14Packet msg ) throws IOException
    {
	log("in handleUnknowPacket :");
	return (304);
    }

    // ==================== Servlet Input Support =================

    public int available() throws IOException {
        if (debug > 0) {
            log("available()");
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
            log("doRead()");
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
            log("doRead(byte[], int, int)");
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
            log("refillReadBuffer()");
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
     */
    public void beginSendHeaders(int status,
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
            log("finish()");
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
            log("doWrite(byte[], " + off + ", " + len + ")");
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
    protected void send( Ajp14Packet msg ) throws IOException {
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

	logged = false;	// no more logged now 
    }

    // -------------------- Utils --------------------
    /**
     * Compute the Protocol Negociation Flags
     * 
     * Depending the protocol fatures implemented on servet-engine,
     * we'll drop requested features which could be asked by web-server
     *
     * Hopefully this functions could be overrided by decendants (AJP15/AJP16...)
     */
    private int getProtocolFlags(int wanted)
    {
	wanted &= ~(AJP14_CONTEXT_UPDATE_NEG	| // no real-time context update yet 
		    AJP14_GZIP_STREAM_NEG   	| // no gzip compression yet
		    AJP14_DES56_STREAM_NEG  	| // no DES56 cyphering yet
		    AJP14_SSL_VSERVER_NEG	| // no Extended info on server SSL vars yet
		    AJP14_SSL_VCLIENT_NEG	| // no Extended info on client SSL vars yet
		    AJP14_SSL_VCRYPTO_NEG	| // no Extended info on crypto SSL vars yet
		    AJP14_SSL_VMISC_NEG		| // no Extended info on misc SSL vars yet
		    AJP14_PROTO_SUPPORT_AJPXX_NEG); // Reset AJP protocol mask
	
	return (wanted | AJP14_PROTO_SUPPORT_AJP14_NEG);	// Only strict AJP14 supported
    }

    /**
     * Compute a digest (MD5 in AJP14) for an array of String
     */
    public final String digest(String[] credentials, String algorithm) {
        try {
            // Obtain a new message digest with MD5 encryption
            MessageDigest md = (MessageDigest)MessageDigest.getInstance(algorithm).clone();
            // encode the credentials items
	    for (int i = 0; i < credentials.length; i++) {
		if( debug > 0 ) log("Credentials : " + i + " " + credentials[i]);
		if( credentials[i] != null  )
		    md.update(credentials[i].getBytes());
	    }
            // obtain the byte array from the digest
            byte[] dig = md.digest();
	    return HexUtils.convert(dig);
        } catch (Exception ex) {
            ex.printStackTrace();
            return null;
        }
    }

    // -------------------- Debugging --------------------
    // Very usefull for develoment

    /**
     * Display Negociation field in human form 
     */
    private String decodeNegociation(int nego)
    {
	StringBuffer buf = new StringBuffer(128);
	
	if ((nego & AJP14_CONTEXT_INFO_NEG) != 0) 
	    buf.append(" CONTEXT-INFO");
	
	if ((nego & AJP14_CONTEXT_UPDATE_NEG) != 0)
	    buf.append(" CONTEXT-UPDATE");
	
	if ((nego & AJP14_GZIP_STREAM_NEG) != 0)
	    buf.append(" GZIP-STREAM");
	
	if ((nego & AJP14_DES56_STREAM_NEG) != 0)
	    buf.append(" DES56-STREAM");
	
	if ((nego & AJP14_SSL_VSERVER_NEG) != 0)
	    buf.append(" SSL-VSERVER");
	
	if ((nego & AJP14_SSL_VCLIENT_NEG) != 0)
	    buf.append(" SSL-VCLIENT");
	
	if ((nego & AJP14_SSL_VCRYPTO_NEG) != 0)
	    buf.append(" SSL-VCRYPTO");
	
	if ((nego & AJP14_SSL_VMISC_NEG) != 0)
	    buf.append(" SSL-VMISC");
	
	if ((nego & AJP14_PROTO_SUPPORT_AJP14_NEG) != 0)
	    buf.append(" AJP14");
	
	if ((nego & AJP14_PROTO_SUPPORT_AJP15_NEG) != 0)
	    buf.append(" AJP15");
	
	if ((nego & AJP14_PROTO_SUPPORT_AJP16_NEG) != 0)
	    buf.append(" AJP16");
	
	return (buf.toString());
    }
    
    
    // Debugging names
    public final String MSG_NAME[]={
	null, // 0
	null, // 1
	"JK_AJP13_FORWARD_REQUEST", // 2
	"JK_AJP13_SEND_BODY_CHUNK", // 3
	"JK_AJP13_SEND_HEADERS", // 4
	"JK_AJP13_END_RESPONSE", // 5
	"JK_AJP13_GET_BODY_CHUNK", // 6
	"JK_AJP13_SHUTDOWN", // 7
	null, null, null, null, // 8, 9, A, B
	null, null, null, null,
	"JK_AJP14_LOGINIT_CMD", // 0x10
	"JK_AJP14_LOGSEED_CMD", // 0x11
	"JK_AJP14_LOGCOMP_CMD", // 0x12
	"JK_AJP14_LOGOK_CMD", // 0x13
	"JK_AJP14_LOGNOK_CMD", // 0x14
	"JK_AJP14_CONTEXT_QRY_CMD", // 0x15
	"JK_AJP14_CONTEXT_INFO_CMD", // 0x16
	"JK_AJP14_CONTEXT_UPDATE_CMD", // 0x17
	"JK_AJP14_STATUS_CMD", // 0x18
	"JK_AJP14_SHUTDOWN_CMD", // 0x19
	"JK_AJP14_SHUTOK_CMD", // 0x1A
	"JK_AJP14_SHUTNOK_CMD", // 0x1B
	"JK_AJP14_CONTEXT_STATE_CMD ", // 0x1C
	"JK_AJP14_CONTEXT_STATE_REP_CMD ", // 0x1D
	"JK_AJP14_UNKNOW_PACKET_CMD " // 0x1E,
    };

    
    private static int debug=10;
    void log(String s) {
	System.out.println("Ajp14: " + s );
    }
 }
