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
 * @author Bill Barker
 */
public class JSSE13SocketFactory extends JSSESocketFactory
{
    public JSSE13SocketFactory () {
        super();
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
     void init() throws IOException {
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
}
