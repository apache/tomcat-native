/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

package org.apache.coyote.tomcat4;

import java.io.File;
import java.net.InetAddress;
import java.net.ServerSocket;


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
 *     set to <code>true</code>. Want client authentication if set to
 *     <code>want</code>. (Note: Only supported in the JSSE included with 
 *     J2SDK 1.4 and above.  Prior versions of JSSE and PureTLS will treat 
 *     'want' as 'false'.) [false]</li>
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
    private String clientAuth = "false";

    public String getClientAuth() {
        return (this.clientAuth);
    }

    public void setClientAuth(String clientAuth) {
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
