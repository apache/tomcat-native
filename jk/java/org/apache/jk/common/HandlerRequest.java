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

package org.apache.jk.common;

import java.io.*;
import java.net.Socket;
import java.util.Enumeration;
import java.security.*;
import java.security.cert.*;

import org.apache.jk.core.*;

import org.apache.tomcat.util.http.*;
import org.apache.tomcat.util.buf.*;


/**
 * Handle messages related with basic request information.
 *
 * This object can handle the following incoming messages:
 * - "FORWARD_REQUEST" input message ( sent when a request is passed from the
 *   web server )
 * - "RECEIVE_BODY_CHUNK" input ( sent by container to pass more body, in
 *   response to GET_BODY_CHUNK )
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
public class HandlerRequest extends Handler
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

    public HandlerRequest() 
    {
    }

    public void init() {
	// register incoming message handlers
	we.registerMessageType( JK_AJP13_FORWARD_REQUEST,
                                "JK_AJP13_FORWARD_REQUEST",
                                this, null); // 2

	// register outgoing messages handler
	we.registerMessageType( JK_AJP13_SEND_BODY_CHUNK, // 3
                                "JK_AJP13_SEND_BODY_CHUNK",
                                this,null );
	we.registerMessageType( JK_AJP13_SEND_HEADERS,  // 4
                                "JK_AJP13_SEND_HEADERS",
                                this,null );
	we.registerMessageType( JK_AJP13_END_RESPONSE, // 5
                                "JK_AJP13_END_RESPONSE",
                                this,null );
	we.registerMessageType( JK_AJP13_GET_BODY_CHUNK, // 6
                                "JK_AJP13_GET_BODY_CHUNK",
                                this, null );
    }
    
    // -------------------- Incoming message --------------------
    int reqNote=4;
    int postMsgNote=5;
    int tmpBufNote=6;

    public int callback(int type, Channel ch, Endpoint ep, Msg msg)
        throws IOException
    {

        // FORWARD_REQUEST handler
        BaseRequest req=(BaseRequest)ep.getNote( reqNote );
        if( req==null ) {
            req=new BaseRequest();
            ep.setNote( reqNote, req );
        }

        decodeRequest( msg, req, ch, ep );

        /* XXX it should be computed from request, by workerEnv */
        worker.service( req, ch, ep );
        return OK;
    }

    private int decodeRequest( Msg msg, BaseRequest req, Channel ch,
                               Endpoint ep )
        throws IOException
    {

        // Translate the HTTP method code to a String.
        byte methodCode = msg.getByte();
        String mName=methodTransArray[(int)methodCode - 1];

        req.method().setString(mName);

        msg.getBytes(req.protocol()); 
        msg.getBytes(req.requestURI());

        msg.getBytes(req.remoteAddr());
        msg.getBytes(req.remoteHost());
        msg.getBytes(req.serverName());
        req.setServerPort(msg.getInt());

        boolean isSSL = msg.getByte() != 0;
        if( isSSL ) req.setSecure( true );

        decodeHeaders( ep, msg, req );

        decodeAttributes( ep, msg, req );

        if(req.getSecure() ) {
            req.setScheme(req.SCHEME_HTTPS);
        }

        // set cookies on request now that we have all headers
        req.cookies().setHeaders(req.headers());

	// Check to see if there should be a body packet coming along
	// immediately after
    	if(req.getContentLength() > 0) {
            Msg postMsg=(Msg)ep.getNote( postMsgNote );
            if( postMsg==null ) {
                postMsg=new MsgAjp();
                ep.setNote( postMsgNote, postMsg );
            }
        
	    /* Read present data */
	    int err = ch.receive(postMsg, ep);
    	}
    
        if (dL > 5) {
            d(req.toString());
         }

        return OK;
    }
        
    private int decodeAttributes( Endpoint ep, Msg msg, BaseRequest req ) {
        boolean moreAttr=true;

        MessageBytes tmpMB=(MessageBytes)ep.getNote( tmpBufNote );
        if( tmpMB==null ) {
            tmpMB=new MessageBytes();
            ep.setNote( tmpBufNote, tmpMB);
        }
        while( moreAttr ) {
            byte attributeCode=msg.getByte();
            if( attributeCode == SC_A_ARE_DONE )
                return 200;

            /* Special case ( XXX in future API make it separate type !)
             */
            if( attributeCode == SC_A_SSL_KEY_SIZE ) {
                // int ???... 
		req.setAttribute("javax.servlet.request.key_size",
				 Integer.toString(msg.getInt()));
            }

            if( attributeCode == SC_A_REQ_ATTRIBUTE ) {
                // 2 strings ???...
                msg.getBytes( tmpMB );
                String n=tmpMB.toString();
                msg.getBytes( tmpMB );
                String v=tmpMB.toString();
		req.setAttribute(n, v );
            }


            // 1 string attributes
            switch(attributeCode) {
	    case SC_A_CONTEXT      :
                msg.getBytes( tmpMB );
                // nothing
                break;
		
	    case SC_A_SERVLET_PATH :
                msg.getBytes( tmpMB );
                // nothing 
                break;
		
	    case SC_A_REMOTE_USER  :
                msg.getBytes(req.remoteUser());
                break;
		
	    case SC_A_AUTH_TYPE    :
                msg.getBytes(req.authType());
                break;
		
	    case SC_A_QUERY_STRING :
		msg.getBytes(req.queryString());
                break;
		
	    case SC_A_JVM_ROUTE    :
                msg.getBytes(req.jvmRoute());
                break;
		
	    case SC_A_SSL_CERT     :
		req.setSecure(  true );
                // Transform the string into certificate.
                msg.getBytes(tmpMB);
                String certString = tmpMB.toString();
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
                    d("Certificate convertion failed" + e );
                    e.printStackTrace();
                }
 
                req.setAttribute("javax.servlet.request.X509Certificate",
                                 jsseCerts);
                break;
		
 	    case SC_A_SSL_CIPHER   :
		req.setSecure( true );
                msg.getBytes(tmpMB);
		req.setAttribute("javax.servlet.request.cipher_suite",
				 tmpMB.toString());
                break;
		
	    case SC_A_SSL_SESSION  :
		req.setSecure( true );
                msg.getBytes(tmpMB);
		req.setAttribute("javax.servlet.request.ssl_session",
				  tmpMB.toString());
                break;
		
	    default:
		return 500; // Error
	    }
        }
        return 200;
    }
    
    private void decodeHeaders( Endpoint ep, Msg msg, BaseRequest req ) {
        // Decode headers
        MimeHeaders headers = req.headers();

        MessageBytes tmpMB=(MessageBytes)ep.getNote( tmpBufNote );
        if( tmpMB==null ) {
            tmpMB=new MessageBytes();
            ep.setNote( tmpBufNote, tmpMB);
        }

        
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
            } else {
                // reset hId -- if the header currently being read
                // happens to be 7 or 8 bytes long, the code below
                // will think it's the content-type header or the
                // content-length header - SC_REQ_CONTENT_TYPE=7,
                // SC_REQ_CONTENT_LENGTH=8 - leading to unexpected
                // behaviour.  see bug 5861 for more information.
                hId = -1;
                msg.getBytes( tmpMB );
                hName=tmpMB.toString();
            }

            vMB=headers.addValue( hName );
            msg.getBytes(vMB);

            if (hId == SC_REQ_CONTENT_LENGTH) {
                // just read the content-length header, so set it
                int contentLength = (vMB == null) ? -1 : vMB.getInt();
                req.setContentLength(contentLength);
            } else if (hId == SC_REQ_CONTENT_TYPE) {
                // just read the content-type header, so set it
                ByteChunk bchunk = vMB.getByteChunk();
                req.contentType().setBytes(bchunk.getBytes(),
                                           bchunk.getOffset(),
                                           bchunk.getLength());
            }
        }
    }

    private static final int dL=0;
    private static void d(String s ) {
        System.err.println( "HandlerRequest: " + s );
    }


 }
