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
package org.apache.tomcat.util.net.jsse;

import java.io.*;
import java.net.*;
import java.util.Vector;
import java.security.KeyStore;
import java.security.Security;
import java.security.SecureRandom;
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
    // defaults
    static String defaultProtocol = "TLS";
    static String defaultAlgorithm = "SunX509";
    static boolean defaultClientAuth = false;
    static String defaultKeystoreType = "JKS";
    private static final String defaultKeystoreFile
        = System.getProperty("user.home") + "/.keystore";
    private static final String defaultKeyPass = "changeit";

    protected boolean initialized;
    protected boolean clientAuth = false;
    protected SSLServerSocketFactory sslProxy = null;
    protected String[] enabledCiphers;
   

    public JSSESocketFactory () {
    }

    public ServerSocket createSocket (int port)
	throws IOException
    {
	if (!initialized) init();
	ServerSocket socket = sslProxy.createServerSocket(port);
	initServerSocket(socket);
	return socket;
    }
    
    public ServerSocket createSocket (int port, int backlog)
	throws IOException
    {
	if (!initialized) init();
	ServerSocket socket = sslProxy.createServerSocket(port, backlog);
	initServerSocket(socket);
	return socket;
    }
    
    public ServerSocket createSocket (int port, int backlog,
				      InetAddress ifAddress)
	throws IOException
    {	
	if (!initialized) init();
	ServerSocket socket = sslProxy.createServerSocket(port, backlog,
							  ifAddress);
	initServerSocket(socket);
	return socket;
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

    public void handshake(Socket sock) throws IOException {
	((SSLSocket)sock).startHandshake();
    }
     
    /*
     * Determines the SSL cipher suites to be enabled.
     *
     * @return Array of SSL cipher suites to be enabled, or null if the
     * cipherSuites property was not specified (meaning that all supported
     * cipher suites are to be enabled)
     */
    protected String[] getEnabledCiphers(String[] supportedCiphers) {

	String[] enabledCiphers = null;

	String attrValue = (String)attributes.get("ciphers");
	if (attrValue != null) {
	    Vector vec = null;
	    int fromIndex = 0;
	    int index = attrValue.indexOf(',', fromIndex);
	    while (index != -1) {
		String cipher = attrValue.substring(fromIndex, index).trim();
		/*
		 * Check to see if the requested cipher is among the supported
		 * ciphers, i.e., may be enabled
		 */
		for (int i=0; supportedCiphers != null
			     && i<supportedCiphers.length; i++) {
		    if (supportedCiphers[i].equals(cipher)) {
			if (vec == null) {
			    vec = new Vector();
			}
			vec.addElement(cipher);
			break;
		    }
		}
		fromIndex = index+1;
		index = attrValue.indexOf(',', fromIndex);
	    }

	    if (vec != null) {
		enabledCiphers = new String[vec.size()];
		vec.copyInto(enabledCiphers);
	    }
	}

	return enabledCiphers;
    }

    /*
     * Gets the SSL server's keystore password.
     */
    protected String getKeystorePassword() {
	String keyPass = (String)attributes.get("keypass");
	if (keyPass == null) {
	    keyPass = defaultKeyPass;
	}
	String keystorePass = (String)attributes.get("keystorePass");
	if (keystorePass == null) {
	    keystorePass = keyPass;
	}
	return keystorePass;
    }

    /*
     * Gets the SSL server's keystore.
     */
    protected KeyStore getKeystore(String type, String pass)
	    throws IOException {

	String keystoreFile = (String)attributes.get("keystore");
	if (keystoreFile == null)
	    keystoreFile = defaultKeystoreFile;

	return getStore(type, keystoreFile, pass);
    }

    /*
     * Gets the SSL server's truststore.
     */
    protected KeyStore getTrustStore(String keystoreType) throws IOException {
	KeyStore trustStore = null;

	String trustStoreFile = System.getProperty("javax.net.ssl.trustStore");
	String trustStorePassword =
	    System.getProperty("javax.net.ssl.trustStorePassword");
	if (trustStoreFile != null && trustStorePassword != null){
	    trustStore = getStore(keystoreType, trustStoreFile,
				  trustStorePassword);
	}

	return trustStore;
    }

    /*
     * Gets the key- or truststore with the specified type, path, and password.
     */
    private KeyStore getStore(String type, String path, String pass)
	    throws IOException {

	KeyStore ks = null;
	InputStream istream = null;
	try {
	    ks = KeyStore.getInstance(type);
	    istream = new FileInputStream(path);
	    ks.load(istream, pass.toCharArray());
	    istream.close();
	    istream = null;
	} catch (FileNotFoundException fnfe) {
	    throw fnfe;
	} catch (IOException ioe) {
	    throw ioe;	    
	} catch(Exception ex) {
	    ex.printStackTrace();
	    throw new IOException("Exception trying to load keystore " +
				  path + ": " + ex.getMessage() );
	} finally {
	    if (istream != null) {
		try {
		    istream.close();
		} catch (IOException ioe) {
		    // Do nothing
		}
	    }
	}

	return ks;
    }

    /**
     * Reads the keystore and initializes the SSL socket factory.
     *
     * NOTE: This method is identical in functionality to the method of the
     * same name in JSSE14SocketFactory, except that this method is used with
     * JSSE 1.0.x (which is an extension to the 1.3 JVM), whereas the other is
     * used with JSSE 1.1.x (which ships with the 1.4 JVM). Therefore, this
     * method uses classes in com.sun.net.ssl, which have since moved to
     * javax.net.ssl, and explicitly registers the required security providers,
     * which come standard in a 1.4 JVM.
     */
    private void init() throws IOException {
	try {
	    Security.addProvider (new sun.security.provider.Sun());
	    Security.addProvider (new com.sun.net.ssl.internal.ssl.Provider());

	    String clientAuthStr = (String)attributes.get("clientauth");
	    if (clientAuthStr != null){
		clientAuth = Boolean.valueOf(clientAuthStr).booleanValue();
	    }
	    
	    // SSL protocol variant (e.g., TLS, SSL v3, etc.)
	    String protocol = (String)attributes.get("protocol");
	    if (protocol == null) protocol = defaultProtocol;
	    
	    // Certificate encoding algorithm (e.g., SunX509)
	    String algorithm = (String)attributes.get("algorithm");
	    if (algorithm == null) algorithm = defaultAlgorithm;

            // Set up KeyManager, which will extract server key
	    com.sun.net.ssl.KeyManagerFactory kmf = 
		com.sun.net.ssl.KeyManagerFactory.getInstance(algorithm);
	    String keystoreType = (String)attributes.get("keystoreType");
	    if (keystoreType == null) {
		keystoreType = defaultKeystoreType;
	    }
	    String keystorePass = getKeystorePassword();
	    kmf.init(getKeystore(keystoreType, keystorePass),
		     keystorePass.toCharArray());

	    // Set up TrustManager
	    com.sun.net.ssl.TrustManager[] tm = null;
	    KeyStore trustStore = getTrustStore(keystoreType);
            if (trustStore != null) {
		com.sun.net.ssl.TrustManagerFactory tmf =
		    com.sun.net.ssl.TrustManagerFactory.getInstance("SunX509");
		tmf.init(trustStore);
		tm = tmf.getTrustManagers();
	    }

	    // Create and init SSLContext
	    com.sun.net.ssl.SSLContext context = 
		com.sun.net.ssl.SSLContext.getInstance(protocol); 
	    context.init(kmf.getKeyManagers(), tm, new SecureRandom());

	    // Create proxy
	    sslProxy = context.getServerSocketFactory();

	    // Determine which cipher suites to enable
	    enabledCiphers = getEnabledCiphers(sslProxy.getSupportedCipherSuites());

	} catch(Exception e) {
	    if( e instanceof IOException )
		throw (IOException)e;
	    throw new IOException(e.getMessage());
	}
    }

    /**
     * Sets the SSL server socket properties (such as enabled cipher suites,
     * etc.)
     */
    private void initServerSocket(ServerSocket ssocket) {
	SSLServerSocket socket=(SSLServerSocket)ssocket;

	if (enabledCiphers != null) {
	    socket.setEnabledCipherSuites(enabledCiphers);
	}

	// we don't know if client auth is needed -
	// after parsing the request we may re-handshake
	socket.setNeedClientAuth(clientAuth);
    }

}
