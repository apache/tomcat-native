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

import java.net.Socket;
import java.security.Principal;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import javax.net.ssl.X509KeyManager;

/**
 * X509KeyManager which allows selection of a specific keypair and certificate
 * chain (identified by their keystore alias name) to be used by the server to
 * authenticate itself to SSL clients.
 *
 * @author Jan Luehe
 */
public final class JSSEKeyManager implements X509KeyManager {

    private X509KeyManager delegate;
    private String serverKeyAlias;

    /**
     * Constructor.
     *
     * @param mgr The X509KeyManager used as a delegate
     * @param serverKeyAlias The alias name of the server's keypair and
     * supporting certificate chain
     */
    public JSSEKeyManager(X509KeyManager mgr, String serverKeyAlias) {
        this.delegate = mgr;
        this.serverKeyAlias = serverKeyAlias;
    }

    /**
     * Choose an alias to authenticate the client side of a secure socket,
     * given the public key type and the list of certificate issuer authorities
     * recognized by the peer (if any).
     *
     * @param keyType The key algorithm type name(s), ordered with the
     * most-preferred key type first
     * @param issuers The list of acceptable CA issuer subject names, or null
     * if it does not matter which issuers are used
     * @param socket The socket to be used for this connection. This parameter
     * can be null, in which case this method will return the most generic
     * alias to use
     *
     * @return The alias name for the desired key, or null if there are no
     * matches
     */
    public String chooseClientAlias(String[] keyType, Principal[] issuers,
                                    Socket socket) {
        return delegate.chooseClientAlias(keyType, issuers, socket);
    }

    /**
     * Returns this key manager's server key alias that was provided in the
     * constructor.
     *
     * @param keyType The key algorithm type name (ignored)
     * @param issuers The list of acceptable CA issuer subject names, or null
     * if it does not matter which issuers are used (ignored)
     * @param socket The socket to be used for this connection. This parameter
     * can be null, in which case this method will return the most generic
     * alias to use (ignored)
     *
     * @return Alias name for the desired key
     */
    public String chooseServerAlias(String keyType, Principal[] issuers,
                                    Socket socket) {
        return serverKeyAlias;
    }

    /**
     * Returns the certificate chain associated with the given alias.
     *
     * @param alias The alias name
     *
     * @return Certificate chain (ordered with the user's certificate first
     * and the root certificate authority last), or null if the alias can't be
     * found
     */
    public X509Certificate[] getCertificateChain(String alias) {
        return delegate.getCertificateChain(alias); 
    }

    /**
     * Get the matching aliases for authenticating the client side of a secure
     * socket, given the public key type and the list of certificate issuer
     * authorities recognized by the peer (if any).
     *
     * @param keyType The key algorithm type name
     * @param issuers The list of acceptable CA issuer subject names, or null
     * if it does not matter which issuers are used
     *
     * @return Array of the matching alias names, or null if there were no
     * matches
     */
    public String[] getClientAliases(String keyType, Principal[] issuers) {
        return delegate.getClientAliases(keyType, issuers);
    }

    /**
     * Get the matching aliases for authenticating the server side of a secure
     * socket, given the public key type and the list of certificate issuer
     * authorities recognized by the peer (if any).
     *
     * @param keyType The key algorithm type name
     * @param issuers The list of acceptable CA issuer subject names, or null
     * if it does not matter which issuers are used
     *
     * @return Array of the matching alias names, or null if there were no
     * matches
     */
    public String[] getServerAliases(String keyType, Principal[] issuers) {
        return delegate.getServerAliases(keyType, issuers);
    }

    /**
     * Returns the key associated with the given alias.
     *
     * @param alias The alias name
     *
     * @return The requested key, or null if the alias can't be found
     */
    public PrivateKey getPrivateKey(String alias) {
        return delegate.getPrivateKey(alias);
    }
}
