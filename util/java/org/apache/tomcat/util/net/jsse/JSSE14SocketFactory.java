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

import java.io.IOException;
import java.security.KeyStore;
import java.security.SecureRandom;
import java.util.Vector;

import javax.net.ssl.KeyManager;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLServerSocket;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509KeyManager;

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
 * @author Jan Luehe
 */
public class JSSE14SocketFactory  extends JSSESocketFactory {

    public JSSE14SocketFactory () {
        super();
    }

    /**
     * Reads the keystore and initializes the SSL socket factory.
     */
    void init() throws IOException {
        try {

            String clientAuthStr = (String) attributes.get("clientauth");
            if (clientAuthStr != null){
                clientAuth = Boolean.valueOf(clientAuthStr).booleanValue();
            }

            // SSL protocol variant (e.g., TLS, SSL v3, etc.)
            String protocol = (String) attributes.get("protocol");
            if (protocol == null) {
                protocol = defaultProtocol;
            }

            // Certificate encoding algorithm (e.g., SunX509)
            String algorithm = (String) attributes.get("algorithm");
            if (algorithm == null) {
                algorithm = defaultAlgorithm;
            }

            String keystoreType = (String) attributes.get("keystoreType");
            if (keystoreType == null) {
                keystoreType = defaultKeystoreType;
            }

	    String trustAlgorithm = (String)attributes.get("truststoreAlgorithm");
	    if( trustAlgorithm == null ) {
		trustAlgorithm = algorithm;
	    }
            // Create and init SSLContext
            SSLContext context = SSLContext.getInstance(protocol); 
            context.init(getKeyManagers(keystoreType, algorithm,
                                        (String) attributes.get("keyAlias")),
                         getTrustManagers(keystoreType, trustAlgorithm),
                         new SecureRandom());

            // create proxy
            sslProxy = context.getServerSocketFactory();

            // Determine which cipher suites to enable
            String requestedCiphers = (String)attributes.get("ciphers");
            if (requestedCiphers != null) {
                enabledCiphers = getEnabledCiphers(requestedCiphers,
                                                   sslProxy.getSupportedCipherSuites());
            }

        } catch(Exception e) {
            if( e instanceof IOException )
                throw (IOException)e;
            throw new IOException(e.getMessage());
        }
    }

    /**
     * Gets the initialized key managers.
     */
    protected KeyManager[] getKeyManagers(String keystoreType,
                                          String algorithm,
                                          String keyAlias)
                throws Exception {

        KeyManager[] kms = null;

        String keystorePass = getKeystorePassword();

        KeyManagerFactory kmf = KeyManagerFactory.getInstance(algorithm);
        kmf.init(getKeystore(keystoreType, keystorePass),
                 keystorePass.toCharArray());

        kms = kmf.getKeyManagers();
        if (keyAlias != null) {
            for(int i=0; i<kms.length; i++) {
                kms[i] = new JSSEKeyManager((X509KeyManager)kms[i], keyAlias);
            }
        }

        return kms;
    }

    /**
     * Gets the intialized trust managers.
     */
    protected TrustManager[] getTrustManagers(String keystoreType, String algorithm)
                throws Exception {

        TrustManager[] tms = null;

        KeyStore trustStore = getTrustStore(keystoreType);
        if (trustStore != null) {
            TrustManagerFactory tmf = TrustManagerFactory.getInstance(algorithm);
            tmf.init(trustStore);
            tms = tmf.getTrustManagers();
        }

        return tms;
    }
    protected void setEnabledProtocols(SSLServerSocket socket, String []protocols){
        if (protocols != null) {
            socket.setEnabledProtocols(protocols);
        }
    }

    protected String[] getEnabledProtocols(SSLServerSocket socket,
                                           String requestedProtocols){
        String[] supportedProtocols = socket.getSupportedProtocols();

        String[] enabledProtocols = null;

        if (requestedProtocols != null) {
            Vector vec = null;
            String protocol = requestedProtocols;
            int index = requestedProtocols.indexOf(',');
            if (index != -1) {
                int fromIndex = 0;
                while (index != -1) {
                    protocol = requestedProtocols.substring(fromIndex, index).trim();
                    if (protocol.length() > 0) {
                        /*
                         * Check to see if the requested protocol is among the
                         * supported protocols, i.e., may be enabled
                         */
                        for (int i=0; supportedProtocols != null
                                     && i<supportedProtocols.length; i++) {
                            if (supportedProtocols[i].equals(protocol)) {
                                if (vec == null) {
                                    vec = new Vector();
                                }
                                vec.addElement(protocol);
                                break;
                            }
                        }
                    }
                    fromIndex = index+1;
                    index = requestedProtocols.indexOf(',', fromIndex);
                } // while
                protocol = requestedProtocols.substring(fromIndex);
            }

            if (protocol != null) {
                protocol = protocol.trim();
                if (protocol.length() > 0) {
                    /*
                     * Check to see if the requested protocol is among the
                     * supported protocols, i.e., may be enabled
                     */
                    for (int i=0; supportedProtocols != null
                                 && i<supportedProtocols.length; i++) {
                        if (supportedProtocols[i].equals(protocol)) {
                            if (vec == null) {
                                vec = new Vector();
                            }
                            vec.addElement(protocol);
                            break;
                        }
                    }
                }
            }           

            if (vec != null) {
                enabledProtocols = new String[vec.size()];
                vec.copyInto(enabledProtocols);
            }
        }

        return enabledProtocols;
    }
}
