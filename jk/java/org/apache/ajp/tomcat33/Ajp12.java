/*
 *
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


/*
  Based on Ajp11ConnectionHandler and Ajp12 implementation of JServ
*/
package org.apache.ajp.tomcat33;

import java.io.*;
import java.net.*;
import java.util.*;
import org.apache.tomcat.core.*;
import org.apache.tomcat.util.net.*;
import org.apache.tomcat.util.*;

class Ajp12 {
    Socket socket;
    InputStream sin;
    BufferedInputStream ajpin;
    private boolean tomcatAuthentication=true;
    boolean shutdown=false;
    boolean isPing=false;
    boolean doLog;
    String secret=null;

    public Ajp12() {
    }

    // Debug only - use only to debug this component
    void d( String s ) {
	System.out.print("Ajp12: ");
	System.out.println( s );
    }
    
    public int doRead() throws IOException {
	return ajpin.read();
    }

    public  int doRead( byte b[], int off, int len ) throws IOException {
	return ajpin.read( b,off,len);
    }

    public void setSocket( Socket s ) throws IOException {
	this.socket = s;
	sin = s.getInputStream();
	ajpin = new BufferedInputStream(sin);
    }

    public void setSecret( String s ) {
	secret=s;
    }
    
    public void readNextRequest(Request req) throws IOException {
	String dummy,token1,token2;
	int marker;
	int signal;

	try {
	    boolean more=true;
            while (more) {
		marker = ajpin.read();
		switch (marker) {
		case 0:       //NOP marker useful for testing if stream is OK
		    break;
		    
		case 1: //beginning of request
		    req.method().setString(  readString(ajpin, null));
		     
		    //Zone
		    String contextPath = readString(ajpin, null);

		    // GS, the following commented line causes the Apache +
		    // Jserv + Tomcat combination to hang with a 404!!!
		    // if("ROOT".equals( contextPath ) ) contextPath="";
		    if("ROOT".equalsIgnoreCase( contextPath ) ) contextPath=null;

		    // XXX Do not set the context - we need extra info.
		    // this have to be rewritten !!!
		    //		    if( contextPath!= null )
		    //	req.setContext( contextM.getContext( contextPath ));
		    
		    //Servlet - not used
		    dummy=readString(ajpin, null);
		    
		    //Server hostname
		    req.serverName().setString(readString(ajpin, null) );
		    //System.out.println("XXX hostname: " + req.serverName());
		    
		    //Apache document root
		    dummy = readString(ajpin, null);               
		    req.pathInfo().setString( readString(ajpin, null));               		    //Apache parsed path-translated XXX Bug in mod_jserv !!!!!
		    dummy = readString(ajpin, null);
		    req.queryString().setString( readString(ajpin, null));  
		    req.remoteAddr().setString(readString(ajpin, ""));
		    req.remoteHost().setString( readString(ajpin, ""));
                    if (isTomcatAuthentication())
                        dummy=readString(ajpin, null);
                    else req.setRemoteUser( readString(ajpin, null));
		    req.setAuthType(readString(ajpin, null));
		    //remote port
		    dummy = readString(ajpin, null);
		    //		    System.out.println("XXX rport " + dummy );
		    
		    req.method().setString( readString(ajpin, null));
		    req.requestURI().setString( readString(ajpin, ""));

		    // XXX don't set lookup path - problems with URL rewriting.
		    // need to be fixed.
		    //if(contextPath!=null && contextPath.length() >0 )
		    //lookupPath=requestURI.substring( contextPath.length()+1 );

		    //script filename
		    dummy = readString(ajpin, null);
		    //script name
		    dummy = readString(ajpin, null);                   
		    req.serverName().setString( readString(ajpin, ""));       

		    int serverPort=80;
		    try {
			String p=readString(ajpin, null);
			//System.out.println("XXX p " + p);
			if(p==null ) p="80";
			serverPort= Integer.parseInt(p);
		    } catch (Exception any) {
			any.printStackTrace();
		    }
		    req.setServerPort( serverPort );

                    /* Quick and dirty patch to set https scheme in ajp12
                     * but I recommand using ajp13 instead 
                     */
                    if (serverPort == 443)
                       req.scheme().setString("https");

		    // System.out.println("XXX port: " + req.getServerPort());

		    //server protocol
		    String proto=readString(ajpin,null);
		    if( proto==null ) proto="HTTP/1.0";
		    req.protocol().setString (proto);  
		    //server signature
		    dummy = readString(ajpin, "");
		    //server software
		    dummy = readString(ajpin, "");
		    req.setJvmRoute( readString(ajpin, null));                   

		    dummy = readString(ajpin, "");
		    dummy = readString(ajpin, "");
		    break;


                    /**
                     * Marker = 5 will be used by mod_jserv to send environment
		     * vars as key+value (dynamically configurable).
                     * can be considered as "reserved", and safely ignored by
		     * other connectors.
		     * env_vars is (above in this  code) commented out for
		     * performance issues. so theses env vars are simply
		     * ignored. (just here for compatibility)
                     * but it is where mod_jserv would place SSL_* env vars
		     * (by exemple)
                     * See the new parameter for mod_jserv (version > 1.1):
                     * ApJServEnvVar localname remotename
                     *                                            - jluc
                     */
                case 5: // Environment vars
                    token1 = readString(ajpin, null);
                    token2 = readString(ajpin, "");
                    /*
                     * Env variables should go into the request attributes
                     * table. 
		     *
		     * Also, if one of the request attributes is HTTPS=on
                     * assume that there is an SSL connection.
		     */
                    req.setAttribute(token1, token2);
                    if(token1.equals("HTTPS") && token2.equals("on")) {
                        req.scheme().setString("https");
                    }
                    break;

		case 3: // Header
		    token1 = readString(ajpin, null);
		    token2 = readString(ajpin, "");
// 		    if( "Host".equalsIgnoreCase( token1 )) {
// 			System.out.println("XXX Host: " + token2);
// 		    }
		    req.getMimeHeaders().addValue(token1).setString(token2);
		    break;

		case 254: // Signal
		    signal = ajpin.read();

		    if (signal == 0) { // PING implemented as signal
			try {
			    // close the socket connection after we send reply
			    socket.getOutputStream().write(0); // PING reply
			    sin.close();
			} catch (IOException ignored) {
			    req.getContextManager().log("Exception closing, ignored",  ignored);
			}
                        isPing = true;
                        return;
		    } else {
			try {
			    // close the socket connection before handling any
			    // signal but get the addresses first so they are
			    // not corrupted
			    InetAddress serverAddr = socket.getLocalAddress();
			    InetAddress clientAddr = socket.getInetAddress();
			    if ( (signal== 15) &&
				 isSameAddress(serverAddr, clientAddr) ) {
				if( secret!=null ) {
				    String stopMsg=readString(ajpin, "");
				    if( ! secret.equals( stopMsg ) ) {
					req.getContextManager().log("Attempt to stop with the wrong secret");
					return;
				    }
				}
                                ContextManager cm=req.getContextManager();
				cm.shutdown();
				cm.log("Exiting" );
				// same behavior as in past, because it seems
				// that stopping everything doesn't work -
				// need to figure
				// out what happens with the threads ( XXX )
				System.exit(0);
				shutdown=true;
				return;
			    }
			    sin.close();
			} catch (Exception ignored) {
			    req.getContextManager().log("Ignored exception " +
						      "processing signal " +
						      signal, ignored);
			}
		    }
		    return;
		    
		case 4:
		case 255:
		    more=false;
		    break;
		    
		case -1:
		    throw new java.io.IOException("Stream closed prematurely");
		    
		    
		default:
		    throw new java.io.IOException("Stream broken");
		} // switch
            } // while
	} catch (IOException ioe) {
	    throw ioe;
        } catch (Exception e) {
	    req.getContextManager().log("Uncaught exception handling request", e);
        }
	
	// REQUEST_URI may includes query string
	String requestURI=req.requestURI().toString();
	int indexQ=requestURI.indexOf("?");
	int rLen=requestURI.length();
	if ( (indexQ >-1) && ( indexQ  < rLen) ) {
	    req.queryString().
		setString( requestURI.substring(indexQ + 1,
						requestURI.length()));
	    req.requestURI().setString( requestURI.substring(0, indexQ));
	} 
    }

    /**
     * Return <code>true</code> if the specified client and server addresses
     * are the same.  This method works around a bug in the IBM 1.1.8 JVM on
     * Linux, where the address bytes are returned reversed in some
     * circumstances.
     *
     * @param server The server's InetAddress
     * @param client The client's InetAddress
     */
    public static boolean isSameAddress(InetAddress server, InetAddress client) {
	// Compare the byte array versions of the two addresses
	byte serverAddr[] = server.getAddress();
	byte clientAddr[] = client.getAddress();
	if (serverAddr.length != clientAddr.length)
	    return (false);
	boolean match = true;
	for (int i = 0; i < serverAddr.length; i++) {
	    if (serverAddr[i] != clientAddr[i]) {
		match = false;
		break;
	    }
	}
	if (match)
	    return (true);

	// Compare the reversed form of the two addresses
	for (int i = 0; i < serverAddr.length; i++) {
	    if (serverAddr[i] != clientAddr[(serverAddr.length-1)-i])
		return (false);
	}
	return (true);
    }

    
    public static int readWord(InputStream in ) throws java.io.IOException {
        int b1 = in.read();
        if( b1 == -1)
            return -1;

        int b2 = in.read();
        if ( b2==-1)
            return -1;

        return ((int)((b1 << 8) | b2)) & 0xffff;
    }

    // UTF8 is a strict superset of ASCII.
    final static String CHARSET = "UTF8";

    public static String readString(InputStream in, String def)
	throws java.io.IOException {
        int len = readWord(in);

        if( len == 0xffff)
            return def;

        if( len == -1)
            throw new java.io.IOException("Stream broken");

        byte[] b = new byte[len];
        int p = 0;
        int r;
        while(p<len) {
            r = in.read(b,p, len - p);
            if( r< 0) {
                throw new IOException("Stream broken, couldn't " +
				      "demarshal string :"+len+":"+p);
            }
            p = p+r;
        }
        return new String(b, CHARSET);
    }

    public boolean isTomcatAuthentication() {
        return tomcatAuthentication;
    }

    public void setTomcatAuthentication(boolean newTomcatAuthentication) {
        tomcatAuthentication = newTomcatAuthentication;
    }

    
}

