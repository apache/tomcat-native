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
package org.apache.coyote.tomcat4;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.UnrecoverableKeyException;
import java.security.KeyManagementException;
import java.security.Security;
import java.security.cert.CertificateException;


/**
 * This socket factory holds secure socket factory parameters. Besides the usual
 * configuration mechanism based on setting JavaBeans properties, this
 * component may also be configured by passing a series of attributes set
 * with calls to <code>setAttribute()</code>.  The following attribute
 * names are recognized, with default values in square brackets:
 * <ul>
 * <li><strong>algorithm</strong> - Certificate encoding algorithm
 *     to use. [SunX509]</li>
 * <li><strong>clientAuth</strong> - Require client authentication if
 *     set to <code>true</code>. [false]</li>
 * <li><strong>keystoreFile</strong> - Pathname to the Key Store file to be
 *     loaded.  This must be an absolute path, or a relative path that
 *     is resolved against the "catalina.base" system property.
 *     ["./keystore" in the user home directory]</li>
 * <li><strong>keystorePass</strong> - Password for the Key Store file to be
 *     loaded. ["changeit"]</li>
 * <li><strong>keystoreType</strong> - Type of the Key Store file to be
 *     loaded. ["JKS"]</li>
 * <li><strong>protocol</strong> - SSL protocol to use. [TLS]</li>
 * </ul>
 *
 * @author Harish Prabandham
 * @author Costin Manolache
 * @author Craig McClanahan
 */

public class CoyoteServerSocketFactory
    implements org.apache.catalina.net.ServerSocketFactory {


    // ------------------------------------------------------------- Properties


    /**
     * Certificate encoding algorithm to be used.
     */
    private String algorithm = null;

    public String getAlgorithm() {
        return (this.algorithm);
    }

    public void setAlgorithm(String algorithm) {
        this.algorithm = algorithm;
    }


    /**
     * Should we require client authentication?
     */
    private boolean clientAuth = false;

    public boolean getClientAuth() {
        return (this.clientAuth);
    }

    public void setClientAuth(boolean clientAuth) {
        this.clientAuth = clientAuth;
    }


    /**
     * Pathname to the key store file to be used.
     */
    private String keystoreFile =
        System.getProperty("user.home") + File.separator + ".keystore";

    public String getKeystoreFile() {
        return (this.keystoreFile);
    }

    public void setKeystoreFile(String keystoreFile) {
      
        File file = new File(keystoreFile);
        if (!file.isAbsolute())
            file = new File(System.getProperty("catalina.base"),
                            keystoreFile);
        this.keystoreFile = file.getAbsolutePath();
    }

    /**
     * Pathname to the random file to be used.
     */
    private String randomFile =
        System.getProperty("user.home") + File.separator + "random.pem";

    public String getRandomFile() {
        return (this.randomFile);
    }

    public void setRandomFile(String randomFile) {
      
        File file = new File(randomFile);
        if (!file.isAbsolute())
            file = new File(System.getProperty("catalina.base"),
                            randomFile);
        this.randomFile = file.getAbsolutePath();
    }

    /**
     * Pathname to the root list to be used.
     */
    private String rootFile =
        System.getProperty("user.home") + File.separator + "root.pem";

    public String getRootFile() {
        return (this.rootFile);
    }

    public void setRootFile(String rootFile) {
      
        File file = new File(rootFile);
        if (!file.isAbsolute())
            file = new File(System.getProperty("catalina.base"),
                            rootFile);
        this.rootFile = file.getAbsolutePath();
    }
     
    /**
     * Password for accessing the key store file.
     */
    private String keystorePass = "changeit";

    public String getKeystorePass() {
        return (this.keystorePass);
    }

    public void setKeystorePass(String keystorePass) {
        this.keystorePass = keystorePass;
    }


    /**
     * Storeage type of the key store file to be used.
     */
    private String keystoreType = "JKS";

    public String getKeystoreType() {
        return (this.keystoreType);
    }

    public void setKeystoreType(String keystoreType) {
        this.keystoreType = keystoreType;
    }


    /**
     * SSL protocol variant to use.
     */
    private String protocol = "TLS";

    public String getProtocol() {
        return (this.protocol);
    }

    public void setProtocol(String protocol) {
        this.protocol = protocol;
    }


    /**
     * SSL implementation to use.
     */
    private String sslImplementation = null;

    public String getSSLImplementation() {
        return (this.sslImplementation);
    }

    public void setSSLImplementation(String sslImplementation) {
        this.sslImplementation = sslImplementation;
    }



    // --------------------------------------------------------- Public Methods


    public ServerSocket createSocket(int port) {
        return (null);
    }


    public ServerSocket createSocket(int port, int backlog) {
        return (null);
    }


    public ServerSocket createSocket(int port, int backlog,
                                     InetAddress ifAddress) {
        return (null);
    }


}
