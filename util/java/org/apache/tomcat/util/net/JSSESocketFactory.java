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
package org.apache.tomcat.util.net;

import java.io.*;
import java.net.*;

import java.security.KeyStore;

import java.security.Security;
import javax.net.ServerSocketFactory;
import javax.net.ssl.SSLServerSocket;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLServerSocketFactory;
import javax.net.ssl.HandshakeCompletedListener;
import javax.net.ssl.HandshakeCompletedEvent;

/*
  1. Make the JSSE's jars available, either as an installed
     extension (copy them into jre/lib/ext) or by adding
     them to the Tomcat classpath.
  2. keytool -genkey -alias tomcat -keyalg RSA
     Use "changeit" as password ( this is the default we use )
 */

/**
 * SSL server socket factory. It _requires_ a valid RSA key and
 * JSSE. 
 *
 * @author Harish Prabandham
 * @author Costin Manolache
 * @author Stefan Freyr Stefansson
 * @author EKR -- renamed to JSSESocketFactory
 */
public class JSSESocketFactory
    extends org.apache.tomcat.util.net.ServerSocketFactory
{
    private String keystoreType;

    static String defaultKeystoreType = "JKS";
    static String defaultProtocol = "TLS";
    static String defaultAlgorithm = "SunX509";
    static boolean defaultClientAuth = false;

    private boolean clientAuth = false;
    private SSLServerSocketFactory sslProxy = null;
    
    // defaults
    static String defaultKeystoreFile=System.getProperty("user.home") +
	"/.keystore";
    static String defaultKeyPass="changeit";

    
    public JSSESocketFactory () {
    }

    public ServerSocket createSocket (int port)
	throws IOException
    {
	if( sslProxy == null ) initProxy();
	ServerSocket socket = 
	    sslProxy.createServerSocket(port);
	initServerSocket(socket);
	return socket;
    }
    
    public ServerSocket createSocket (int port, int backlog)
	throws IOException
    {
	if( sslProxy == null ) initProxy();
	ServerSocket socket = 
	    sslProxy.createServerSocket(port, backlog);
	initServerSocket(socket);
	return socket;
    }
    
    public ServerSocket createSocket (int port, int backlog,
				      InetAddress ifAddress)
	throws IOException
    {	
	if( sslProxy == null ) initProxy();
	ServerSocket socket = 
	    sslProxy.createServerSocket(port, backlog, ifAddress);
	initServerSocket(socket);
	return socket;
    }
    
    
    // -------------------- Internal methods
    /** Read the keystore, init the SSL socket factory
     */
    private void initProxy() throws IOException {
	try {
	    Security.addProvider (new sun.security.provider.Sun());
	    Security.addProvider (new com.sun.net.ssl.internal.ssl.Provider());

	    // Please don't change the name of the attribute - other
	    // software may depend on it ( j2ee for sure )
	    String keystoreFile=(String)attributes.get("keystore");
	    if( keystoreFile==null) keystoreFile=defaultKeystoreFile;

	    keystoreType=(String)attributes.get("keystoreType");
	    if( keystoreType==null) keystoreType=defaultKeystoreType;

	    //determine whether we want client authentication
	    // the presence of the attribute enables client auth
	    String clientAuthStr=(String)attributes.get("clientauth");
	    if(clientAuthStr != null){
		if(clientAuthStr.equals("true")){
		    clientAuth=true;
		} else if(clientAuthStr.equals("false")) {
		    clientAuth=false;
		} else {
		    throw new IOException("Invalid value '" +
					  clientAuthStr + 
					  "' for 'clientauth' parameter:");
		}
	    }

	    String keyPass=(String)attributes.get("keypass");
	    if( keyPass==null) keyPass=defaultKeyPass;

	    String keystorePass=(String)attributes.get("keystorePass");
	    if( keystorePass==null) keystorePass=keyPass;

	    //protocol for the SSL ie - TLS, SSL v3 etc.
	    String protocol = (String)attributes.get("protocol");
	    if(protocol == null) protocol = defaultProtocol;
	    
	    //Algorithm used to encode the certificate ie - SunX509
	    String algorithm = (String)attributes.get("algorithm");
	    if(algorithm == null) algorithm = defaultAlgorithm;
	    
	    // You can't use ssl without a server certificate.
	    // Create a KeyStore ( to get server certs )
	    KeyStore kstore = initKeyStore( keystoreFile, keystorePass );
	    
	    // Create a SSLContext ( to create the ssl factory )
	    // This is the only way to use server sockets with JSSE 1.0.1
	    com.sun.net.ssl.SSLContext context = 
		com.sun.net.ssl.SSLContext.getInstance(protocol); //SSL

	    // Key manager will extract the server key
	    com.sun.net.ssl.KeyManagerFactory kmf = 
		com.sun.net.ssl.KeyManagerFactory.getInstance(algorithm);
	    kmf.init( kstore, keyPass.toCharArray());

	    //  set up TrustManager
	    com.sun.net.ssl.TrustManager[] tm = null;
	    String trustStoreFile = System.getProperty("javax.net.ssl.trustStore");
	    String trustStorePassword =
	        System.getProperty("javax.net.ssl.trustStorePassword");
	    if ( trustStoreFile != null && trustStorePassword != null ){
            KeyStore trustStore = initKeyStore( trustStoreFile, trustStorePassword);
            
            com.sun.net.ssl.TrustManagerFactory tmf =
                com.sun.net.ssl.TrustManagerFactory.getInstance("SunX509");

            tmf.init(trustStore);
            tm = tmf.getTrustManagers();
        }

	    // init context with the key managers
	    context.init(kmf.getKeyManagers(), tm, 
			 new java.security.SecureRandom());

	    // create proxy
	    sslProxy = context.getServerSocketFactory();

	    return;
	} catch(Exception e) {
	    if( e instanceof IOException )
		throw (IOException)e;
	    throw new IOException(e.getMessage());
	}
    }

    public Socket acceptSocket(ServerSocket socket)
	throws IOException
    {
	SSLSocket asock = null;
	try {
	     asock = (SSLSocket)socket.accept();
	     asock.setNeedClientAuth(clientAuth);
	} catch (SSLException e){
	  throw new SocketException("SSL handshake error" + e.toString());
	}
	return asock;
    }
     
    /** Set server socket properties ( accepted cipher suites, etc)
     */
    private void initServerSocket(ServerSocket ssocket) {
	SSLServerSocket socket=(SSLServerSocket)ssocket;

	// We enable all cipher suites when the socket is
	// connected - XXX make this configurable 
	String cipherSuites[] = socket.getSupportedCipherSuites();
	socket.setEnabledCipherSuites(cipherSuites);

	// we don't know if client auth is needed -
	// after parsing the request we may re-handshake
	socket.setNeedClientAuth(clientAuth);
    }

    private KeyStore initKeyStore( String keystoreFile,
				   String keyPass)
	throws IOException
    {
	InputStream istream = null;
	try {
	    KeyStore kstore=KeyStore.getInstance( keystoreType );
	    istream = new FileInputStream(keystoreFile);
	    kstore.load(istream, keyPass.toCharArray());
	    return kstore;
	}
	catch (FileNotFoundException fnfe) {
	    throw fnfe;
	}
	catch (IOException ioe) {
	    throw ioe;	    
	}
	catch(Exception ex) {
	    ex.printStackTrace();
	    throw new IOException( "Exception trying to load keystore " +
				   keystoreFile + ": " + ex.getMessage() );
	}
    }

    public void handshake(Socket sock)
	 throws IOException
    {
	((SSLSocket)sock).startHandshake();
    }
}
