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
 * Handler for the protocol negotiation. It will authenticate and 
 * exchange information about supported messages on each end.
 * 
 * 
 * @author Henri Gomez [hgomez@slib.fr]
 * @author Dan Milstein [danmil@shore.net]
 * @author Keith Wannamaker [Keith@Wannamaker.org]
 * @author Costin Manolache
 */
public class NegociationHandler extends AjpHandler
{
    public static final byte JK_AJP13_SHUTDOWN=7;
	
    // Initial Login Phase (web server -> servlet engine)
    public static final byte JK_AJP14_LOGINIT_CMD=0x10;
    
    // Second Login Phase (servlet engine -> web server), md5 seed is received
    public static final byte JK_AJP14_LOGSEED_CMD=0x11;
    
    // Third Login Phase (web server -> servlet engine),
    // md5 of seed + secret is sent
    public static final byte JK_AJP14_LOGCOMP_CMD=0x12;
    
    // Login Accepted (servlet engine -> web server)
    public static final byte JK_AJP14_LOGOK_CMD=0x13;

    // Login Rejected (servlet engine -> web server), will be logged
    public static final byte JK_AJP14_LOGNOK_CMD=0x14;
    
    // Context Query (web server -> servlet engine),
    // which URI are handled by servlet engine ?
    public static final byte JK_AJP14_CONTEXT_QRY_CMD=0x15;
    
    // Context Info (servlet engine -> web server), URI handled response
    public static final byte JK_AJP14_CONTEXT_INFO_CMD= 0x16;
    
    // Context Update (servlet engine -> web server), status of context changed
    public static final byte JK_AJP14_CONTEXT_UPDATE_CMD= 0x17;
    
    // Servlet Engine Status (web server -> servlet engine),
    // what's the status of the servlet engine ?
    public static final byte JK_AJP14_STATUS_CMD= 0x18;
    
    // Secure Shutdown command (web server -> servlet engine),
    //please servlet stop yourself.
    public static final byte JK_AJP14_SHUTDOWN_CMD= 0x19;
    
    // Secure Shutdown command Accepted (servlet engine -> web server)
    public static final byte JK_AJP14_SHUTOK_CMD= 0x1A;
    
    // Secure Shutdown Rejected (servlet engine -> web server)
    public static final byte JK_AJP14_SHUTNOK_CMD= 0x1B;
    
    // Context Status (web server -> servlet engine),
    //what's the status of the context ?
    public static final byte JK_AJP14_CONTEXT_STATE_CMD= 0x1C;
    
    // Context Status Reply (servlet engine -> web server), status of context
    public static final byte JK_AJP14_CONTEXT_STATE_REP_CMD = 0x1D;
    
    // Unknown Packet Reply (web server <-> servlet engine),
    //when a packet couldn't be decoded
    public static final byte JK_AJP14_UNKNOW_PACKET_CMD = 0x1E;


    // -------------------- Other constants -------------------- 

    // Entropy Packet Size
    public static final int AJP14_ENTROPY_SEED_LEN= 32;
    public static final int AJP14_COMPUTED_KEY_LEN= 32;
    
    // web-server want context info after login
    public static final int AJP14_CONTEXT_INFO_NEG= 0x80000000;
    
    // web-server want context updates
    public static final int AJP14_CONTEXT_UPDATE_NEG= 0x40000000;
    
    // web-server want compressed stream
    public static final int AJP14_GZIP_STREAM_NEG= 0x20000000;
    
    // web-server want crypted DES56 stream with secret key
    public static final int AJP14_DES56_STREAM_NEG= 0x10000000;
    
    // Extended info on server SSL vars
    public static final int AJP14_SSL_VSERVER_NEG= 0x08000000;
    
    // Extended info on client SSL vars
    public static final int AJP14_SSL_VCLIENT_NEG= 0x04000000;
    
    // Extended info on crypto SSL vars
    public static final int AJP14_SSL_VCRYPTO_NEG= 0x02000000;
    
    // Extended info on misc SSL vars
    public static final int  AJP14_SSL_VMISC_NEG= 0x01000000;
    
    // mask of protocol supported
    public static final int  AJP14_PROTO_SUPPORT_AJPXX_NEG=0x00FF0000;
    
    // communication could use AJP14
    public static final int  AJP14_PROTO_SUPPORT_AJP14_NEG=0x00010000;
    
    // communication could use AJP15
    public static final int  AJP14_PROTO_SUPPORT_AJP15_NEG=0x00020000;
    
    // communication could use AJP16
    public static final int  AJP14_PROTO_SUPPORT_AJP16_NEG=0x00040000;
    
    // Some failure codes
    public static final int AJP14_BAD_KEY_ERR= 0xFFFFFFFF;
    public static final int AJP14_ENGINE_DOWN_ERR = 0xFFFFFFFE;
    public static final int AJP14_RETRY_LATER_ERR = 0xFFFFFFFD;
    public static final int AJP14_SHUT_AUTHOR_FAILED_ERR = 0xFFFFFFFC;
    
    // Some status codes
    public static final byte AJP14_CONTEXT_DOWN= 0x01;
    public static final byte AJP14_CONTEXT_UP= 0x02;
    public static final byte AJP14_CONTEXT_OK= 0x03;


    // -------------------- Parameters --------------------
    String  containerSignature="Ajp14-based container";
    String  seed="seed";// will use random
    String  password;

    int	webserverNegociation=0;
    //    String  webserverName;
    
    public NegociationHandler() {
        setSeed("myveryrandomentropy");
	setPassword("myverysecretkey");
    }

    public void setContainerSignature( String s ) {
	containerSignature=s;
    }

    // -------------------- State --------------------

    //     public String getWebserverName() {
    // 	return webserverName;
    //     }
    
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

    // -------------------- Initialization --------------------

    public void init( Ajp13 ajp14 ) {
        super.init(ajp14);
	// register incoming message handlers
	ajp14.registerMessageType( JK_AJP14_LOGINIT_CMD,"JK_AJP14_LOGINIT_CMD",
				   this, null); //
	ajp14.registerMessageType( JK_AJP14_LOGCOMP_CMD,"JK_AJP14_LOGCOMP_CMD",
				   this, null); //
	ajp14.registerMessageType( JK_AJP13_SHUTDOWN,"JK_AJP13_SHUTDOWN",
				   this, null); //
	ajp14.registerMessageType( JK_AJP14_CONTEXT_QRY_CMD,
				   "JK_AJP14_CONTEXT_QRY_CMD",
				   this, null); //
	ajp14.registerMessageType( JK_AJP14_STATUS_CMD,"JK_AJP14_STATUS_CMD",
				   this, null); //
	ajp14.registerMessageType( JK_AJP14_SHUTDOWN_CMD,
                                   "JK_AJP14_SHUTDOWN_CMD",
				   this, null); //
	ajp14.registerMessageType( JK_AJP14_CONTEXT_STATE_CMD,
				   "JK_AJP14_CONTEXT_STATE_CMD",
				   this, null); //
	ajp14.registerMessageType( JK_AJP14_UNKNOW_PACKET_CMD,
				   "JK_AJP14_UNKNOW_PACKET_CMD",
				   this, null); //
	
	// register outgoing messages handler
	ajp14.registerMessageType( JK_AJP14_LOGNOK_CMD,"JK_AJP14_LOGNOK_CMD",
				   this,null );
	
    }
    

    
    // -------------------- Dispatch --------------------

    public int handleAjpMessage( int type, Ajp13 ch, Ajp13Packet hBuf,
				 BaseRequest req )
	throws IOException
    {
        System.out.println("handleAjpMessage: " + type );
	Ajp13Packet outBuf=ch.outBuf;
	// Valid requests when not logged:
	switch( type ) {
	case JK_AJP14_LOGINIT_CMD :
	    return handleLogInit(ch, hBuf, outBuf);
	case JK_AJP14_LOGCOMP_CMD :
	    return handleLogComp(ch, hBuf, outBuf);
	case JK_AJP13_SHUTDOWN:
	    return -2;
	case JK_AJP14_CONTEXT_QRY_CMD :
	    return handleContextQuery(ch, hBuf, outBuf);
	case JK_AJP14_STATUS_CMD :
	    return handleStatus(hBuf, outBuf);
	case JK_AJP14_SHUTDOWN_CMD :
	    return handleShutdown(hBuf, outBuf);
	case JK_AJP14_CONTEXT_STATE_CMD :
	    return handleContextState(hBuf, outBuf);
	case JK_AJP14_UNKNOW_PACKET_CMD :
	    return handleUnknowPacket(hBuf, outBuf);
	default:
	    log("unknown command " + type + " received");
	    return 200; // XXX This is actually an error condition
	}
	//return UNKNOWN;
    }
    
    //----------- Implementation for various protocol commands -----------

    /**
     * Handle the Initial Login Message from Web-Server
     *
     * Get the requested Negociation Flags
     * Get also the Web-Server Name
     * 
     * Send Login Seed (MD5 of seed)
     */
    private int handleLogInit( Ajp13 ch, Ajp13Packet msg,
			       Ajp13Packet outBuf )
	throws IOException
    {
	webserverNegociation = msg.getLongInt();
	String webserverName  = msg.getString();
	log("in handleLogInit with nego " +
	    decodeNegociation(webserverNegociation) +
            " from webserver " + webserverName);
	
	outBuf.reset();
        outBuf.appendByte(JK_AJP14_LOGSEED_CMD);
	String[] credentials = new String[1];
	credentials[0] = getSeed();
        outBuf.appendXBytes(getSeed().getBytes(), 0, AJP14_ENTROPY_SEED_LEN);
	log("in handleLogInit: sent entropy " + getSeed());
        outBuf.end();
        ch.send(outBuf);
	return 304;
    }
    
    /**
     * Handle the Second Phase of Login (accreditation)
     * 
     * Get the MD5 digest of entropy + secret password
     * If the authentification is valid send back LogOk
     * If the authentification failed send back LogNok
     */
    private int handleLogComp( Ajp13 ch, Ajp13Packet msg,
			       Ajp13Packet outBuf )
	throws IOException
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
	    ch.send(outBuf);
	    return 200;
	} else {
            // logged we can go process requests
	    channel.setLogged(true);
	    outBuf.reset();
	    outBuf.appendByte(JK_AJP14_LOGOK_CMD);
	    outBuf.appendLongInt(getProtocolFlags(webserverNegociation));
	    outBuf.appendString( containerSignature );
	    outBuf.end(); 
	    ch.send(outBuf);
	}
	
	return (304);
    }

    private int handleContextQuery( Ajp13 ch, Ajp13Packet msg,
				    Ajp13Packet outBuf )
	throws IOException
    {
	log("in handleContextQuery :");
    String virtualHost = msg.getString();
    log("in handleContextQuery for virtual" + virtualHost); 

    outBuf.reset();
    outBuf.appendByte(JK_AJP14_CONTEXT_INFO_CMD);
    outBuf.appendString( virtualHost );

    log("in handleContextQuery for virtual " + virtualHost +
        "examples URI/MIMES");
    outBuf.appendString("examples");    // first context - examples
    outBuf.appendString("servlet/*");   // examples/servlet/*
    outBuf.appendString("*.jsp");       // examples/*.jsp
    outBuf.appendString("");            // no more URI/MIMES

    log("in handleContextQuery for virtual " + virtualHost +
        "send admin URI/MIMES"); 
    outBuf.appendString("admin");       // second context - admin
    outBuf.appendString("servlet/*");   // /admin//servlet/*
    outBuf.appendString("*.jsp");       // /admin/*.jsp
    outBuf.appendString("");            // no more URI/MIMES

    outBuf.appendString("");            // no more contexts
    outBuf.end();
    ch.send(outBuf);
    
	return (304);
    }
    
    private int handleStatus( Ajp13Packet msg, Ajp13Packet outBuf )
        throws IOException
    {
	log("in handleStatus :");
	return (304);
    }

    private int handleShutdown( Ajp13Packet msg, Ajp13Packet outBuf )
        throws IOException
    {
	log("in handleShutdown :");
	return (304);
    }
    
    private int handleContextState( Ajp13Packet msg , Ajp13Packet outBuf)
        throws IOException
    {
	log("in handleContextState :");
	return (304);
    }
    
    private int handleUnknowPacket( Ajp13Packet msg, Ajp13Packet outBuf )
        throws IOException
    {
	log("in handleUnknowPacket :");
	return (304);
    }

    // -------------------- Utils --------------------

    /**
     * Compute the Protocol Negociation Flags
     * 
     * Depending the protocol fatures implemented on servet-engine,
     * we'll drop requested features which could be asked by web-server
     *
     * Hopefully this functions could be overrided by decendants
     */
    private int getProtocolFlags(int wanted)
    {
                    // no real-time context update
	wanted &= ~(AJP14_CONTEXT_UPDATE_NEG |
                    // no gzip compression yet
		    AJP14_GZIP_STREAM_NEG    | 
                     // no DES56 cyphering yet
		    AJP14_DES56_STREAM_NEG   |
                    // no Extended info on server SSL vars yet
		    AJP14_SSL_VSERVER_NEG    |
                    // no Extended info on client SSL vars yet
		    AJP14_SSL_VCLIENT_NEG    |
                    // no Extended info on crypto SSL vars yet
		    AJP14_SSL_VCRYPTO_NEG    |
                    // no Extended info on misc SSL vars yet
		    AJP14_SSL_VMISC_NEG	     |
                    // Reset AJP protocol mask
		    AJP14_PROTO_SUPPORT_AJPXX_NEG);
        
	// Only strict AJP14 supported
	return (wanted | AJP14_PROTO_SUPPORT_AJP14_NEG);	
    }

    /**
     * Compute a digest (MD5 in AJP14) for an array of String
     */
    public final String digest(String[] credentials, String algorithm) {
        try {
            // Obtain a new message digest with MD5 encryption
            MessageDigest md =
                (MessageDigest)MessageDigest.getInstance(algorithm).clone();
            // encode the credentials items
	    for (int i = 0; i < credentials.length; i++) {
		if( debug > 0 )
                    log("Credentials : " + i + " " + credentials[i]);
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
    
    private static int debug=10;
    void log(String s) {
	System.out.println("Ajp14Negotiation: " + s );
    }
 }
