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
import java.net.*;
import java.util.*;

import org.apache.jk.core.*;

import org.apache.tomcat.util.http.*;
import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.net.SSLSupport;
import org.apache.tomcat.util.threads.ThreadWithAttributes;

import org.apache.coyote.Request;
import org.apache.coyote.*;
import org.apache.commons.modeler.Registry;

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
public class HandlerRequest extends JkHandler
{
    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( HandlerRequest.class );

    // XXX Will move to a registry system.
    
    // Prefix codes for message types from server to container
    public static final byte JK_AJP13_FORWARD_REQUEST   = 2;

    public static final byte JK_AJP13_SHUTDOWN   = 7;

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
    public static final byte SC_A_SECRET        = 12;

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
        "SEARCH",
        "MKWORKSPACE",
        "UPDATE",
        "LABEL",
        "MERGE",
        "BASELINE-CONTROL",
        "MKACTIVITY"
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

    HandlerDispatch dispatch;
    String ajpidDir="conf";
    

    public HandlerRequest() 
    {
    }

    public void init() {
        dispatch=(HandlerDispatch)wEnv.getHandler( "dispatch" );
        if( dispatch != null ) {
            // register incoming message handlers
            dispatch.registerMessageType( JK_AJP13_FORWARD_REQUEST,
                                          "JK_AJP13_FORWARD_REQUEST",
                                          this, null); // 2
            
            dispatch.registerMessageType( JK_AJP13_FORWARD_REQUEST,
                                          "JK_AJP13_SHUTDOWN",
                                          this, null); // 7
            
            // register outgoing messages handler
            dispatch.registerMessageType( JK_AJP13_SEND_BODY_CHUNK, // 3
                                          "JK_AJP13_SEND_BODY_CHUNK",
                                          this,null );
        }

        bodyNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "jkInputStream" );
        tmpBufNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "tmpBuf" );
        secretNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "secret" );

        if( next==null )
            next=wEnv.getHandler( "container" );
        if( log.isDebugEnabled() )
            log.debug( "Container handler " + next + " " + next.getName() +
                       " " + next.getClass().getName());

        // should happen on start()
        generateAjp13Id();
    }

    public void setSecret( String s ) {
        requiredSecret=s;
    }

    public void setUseSecret( boolean b ) {
        requiredSecret=Double.toString(Math.random());
    }

    public void setDecodedUri( boolean b ) {
	decoded=b;
    }

    public boolean isTomcatAuthentication() {
        return tomcatAuthentication;
    }

    public void setTomcatAuthentication(boolean newTomcatAuthentication) {
        tomcatAuthentication = newTomcatAuthentication;
    }
    
    public void setAjpidDir( String path ) {
        if( "".equals( path ) ) path=null;
        ajpidDir=path;
    }
    
    // -------------------- Ajp13.id --------------------

    private void generateAjp13Id() {
        int portInt=8009; // tcpCon.getPort();
        InetAddress address=null; // tcpCon.getAddress();

        if( requiredSecret == null )
            return;
        
        File f1=new File( wEnv.getJkHome() );
        File f2=new File( f1, "conf" );
        
        if( ! f2.exists() ) {
            log.error( "No conf dir for ajp13.id " + f2 );
            return;
        }
        
        File sf=new File( f2, "ajp13.id");
        
        if( log.isDebugEnabled())
            log.debug( "Using stop file: "+sf);

        try {
            Properties props=new Properties();

            props.put( "port", Integer.toString( portInt ));
            if( address!=null ) {
                props.put( "address", address.getHostAddress() );
            }
            if( requiredSecret !=null ) {
                props.put( "secret", requiredSecret );
            }

            FileOutputStream stopF=new FileOutputStream( sf );
            props.save( stopF, "Automatically generated, don't edit" );
        } catch( IOException ex ) {
            log.debug( "Can't create stop file: "+sf );
            ex.printStackTrace();
        }
    }
    
    // -------------------- Incoming message --------------------
    String requiredSecret=null;
    int bodyNote;
    int tmpBufNote;
    int secretNote;

    boolean decoded=true;
    boolean tomcatAuthentication=true;
    
    public int invoke(Msg msg, MsgContext ep ) 
        throws IOException
    {
        int type=msg.getByte();
        ThreadWithAttributes twa=(ThreadWithAttributes)Thread.currentThread();
        Object control=ep.getControl();

        MessageBytes tmpMB=(MessageBytes)ep.getNote( tmpBufNote );
        if( tmpMB==null ) {
            tmpMB=new MessageBytes();
            ep.setNote( tmpBufNote, tmpMB);
        }
        if( log.isDebugEnabled() )
            log.debug( "Handling " + type );
        
        switch( type ) {
        case JK_AJP13_FORWARD_REQUEST:
            try {
                twa.setCurrentStage(control, "JkDecode");
                decodeRequest( msg, ep, tmpMB );
                twa.setCurrentStage(control, "JkService");
                twa.setParam(control,
                        ((Request)ep.getRequest()).unparsedURI().toString());
            } catch( Exception ex ) {
                log.error( "Error decoding request ", ex );
                msg.dump( "Incomming message");
                return ERROR;
            }

            if( requiredSecret != null ) {
                String epSecret=(String)ep.getNote( secretNote );
                if( epSecret==null || ! requiredSecret.equals( epSecret ) )
                    return ERROR;
            }
            /* XXX it should be computed from request, by workerEnv */
            if(log.isDebugEnabled() )
                log.debug("Calling next " + next.getName() + " " +
                  next.getClass().getName());

            int err= next.invoke( msg, ep );
            twa.setCurrentStage(control, "JkDone");

            if( log.isDebugEnabled() )
                log.debug( "Invoke returned " + err );
            return err;
        case JK_AJP13_SHUTDOWN:
            String epSecret=null;
            if( msg.getLen() > 3 ) {
                // we have a secret
                msg.getBytes( tmpMB );
                epSecret=tmpMB.toString();
            }
            
            if( requiredSecret != null &&
                requiredSecret.equals( epSecret ) ) {
                if( log.isDebugEnabled() )
                    log.debug("Received wrong secret, no shutdown ");
                return ERROR;
            }

            // XXX add isSameAddress check
            JkHandler ch=ep.getSource();
            if( ch instanceof ChannelSocket ) {
                if( ! ((ChannelSocket)ch).isSameAddress(ep) ) {
                    log.error("Shutdown request not from 'same address' ");
                    return ERROR;
                }
            }

            // forward to the default handler - it'll do the shutdown
            next.invoke( msg, ep );

            log.info("Exiting");
            System.exit(0);
            
	    return OK;
        default:
            System.err.println("Unknown message " + type );
            msg.dump("Unknown message" );
	}

        return OK;
    }

    static int count=0;

    private int decodeRequest( Msg msg, MsgContext ep, MessageBytes tmpMB )
        throws IOException
    {
        // FORWARD_REQUEST handler
        Request req=(Request)ep.getRequest();
        if( req==null ) {
            req=new Request();
            Response res=new Response();
            req.setResponse(res);
            ep.setRequest( req );
            RequestProcessor rp=new RequestProcessor(req);
            if( this.getDomain() != null ) {
                try {
                    Registry.getRegistry().registerComponent( rp,
                            getDomain(), "RequestProcessor", "name=Request" + count++ );
                } catch( Exception ex ) {
                    log.warn("Error registering request");
                }
            }
        }

        JkInputStream jkBody=(JkInputStream)ep.getNote( bodyNote );
        if( jkBody==null ) {
            jkBody=new JkInputStream();
            jkBody.setMsgContext( ep );

            ep.setNote( bodyNote, jkBody );
        }

        jkBody.recycle();
        
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
        if( isSSL ) {
            // XXX req.setSecure( true );
            req.scheme().setString("https");
        }

        decodeHeaders( ep, msg, req, tmpMB );

        decodeAttributes( ep, msg, req, tmpMB );

//         if(req.getSecure() ) {
//             req.setScheme(req.SCHEME_HTTPS);
//         }

        // set cookies on request now that we have all headers
        req.getCookies().setHeaders(req.getMimeHeaders());

	// Check to see if there should be a body packet coming along
	// immediately after
        int cl=req.getContentLength();
    	if(cl > 0) {
            jkBody.setContentLength( cl );
            jkBody.receive();
    	}
    
        if (log.isTraceEnabled()) {
            log.trace(req.toString());
         }

        return OK;
    }
        
    private int decodeAttributes( MsgContext ep, Msg msg, Request req,
                                  MessageBytes tmpMB) {
        boolean moreAttr=true;

        while( moreAttr ) {
            byte attributeCode=msg.getByte();
            if( attributeCode == SC_A_ARE_DONE )
                return 200;

            /* Special case ( XXX in future API make it separate type !)
             */
            if( attributeCode == SC_A_SSL_KEY_SIZE ) {
                // Bug 1326: it's an Integer.
		req.setAttribute(SSLSupport.KEY_SIZE_KEY,
                                 new Integer( msg.getInt()));
	       //Integer.toString(msg.getInt()));
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
                if( tomcatAuthentication ) {
                    // ignore server
                    msg.getBytes( tmpMB );
                } else {
                    msg.getBytes(req.getRemoteUser());
                }
                break;
		
	    case SC_A_AUTH_TYPE    :
                msg.getBytes(req.getAuthType());
                break;
		
	    case SC_A_QUERY_STRING :
		msg.getBytes(req.queryString());
                break;
		
	    case SC_A_JVM_ROUTE    :
                msg.getBytes(req.instanceId());
                break;
		
            case SC_A_SSL_CERT     :
                req.scheme().setString( "https" );
                // Transform the string into certificate.
                MessageBytes tmpMB2 = new MessageBytes();
                msg.getBytes(tmpMB2);
                // SSL certificate extraction is costy, moved to JkCoyoteHandler
                req.setNote(WorkerEnv.SSL_CERT_NOTE, tmpMB2);
                break;
                
            case SC_A_SSL_CIPHER   :
		req.scheme().setString( "https" );
                msg.getBytes(tmpMB);
		req.setAttribute(SSLSupport.CIPHER_SUITE_KEY,
				 tmpMB.toString());
                break;
		
	    case SC_A_SSL_SESSION  :
		req.scheme().setString( "https" );
                msg.getBytes(tmpMB);
		req.setAttribute(SSLSupport.SESSION_ID_KEY, 
				  tmpMB.toString());
                break;
                
	    case SC_A_SECRET  :
                msg.getBytes(tmpMB);
                String secret=tmpMB.toString();
                log.info("Secret: " + secret );
                // endpoint note
                ep.setNote( secretNote, secret );
                break;
	    default:
		break; // ignore, we don't know about it - backward compat
	    }
        }
        return 200;
    }
    
    private void decodeHeaders( MsgContext ep, Msg msg, Request req,
                                MessageBytes tmpMB ) {
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
                vMB=headers.addValue( hName );
            } else {
                // reset hId -- if the header currently being read
                // happens to be 7 or 8 bytes long, the code below
                // will think it's the content-type header or the
                // content-length header - SC_REQ_CONTENT_TYPE=7,
                // SC_REQ_CONTENT_LENGTH=8 - leading to unexpected
                // behaviour.  see bug 5861 for more information.
                hId = -1;
                msg.getBytes( tmpMB );
                ByteChunk bc=tmpMB.getByteChunk();
                //hName=tmpMB.toString();
                //                vMB=headers.addValue( hName );
                vMB=headers.addValue( bc.getBuffer(),
                                      bc.getStart(), bc.getLength() );
            }

            msg.getBytes(vMB);

            if (hId == SC_REQ_CONTENT_LENGTH ||
                tmpMB.equalsIgnoreCase("Content-Length")) {
                // just read the content-length header, so set it
                int contentLength = (vMB == null) ? -1 : vMB.getInt();
                req.setContentLength(contentLength);
            } else if (hId == SC_REQ_CONTENT_TYPE ||
                       tmpMB.equalsIgnoreCase("Content-Type")) {
                // just read the content-type header, so set it
                ByteChunk bchunk = vMB.getByteChunk();
                req.contentType().setBytes(bchunk.getBytes(),
                                           bchunk.getOffset(),
                                           bchunk.getLength());
            }
        }
    }

}
