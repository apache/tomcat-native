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
import java.util.Vector;
import java.security.cert.CertificateFactory;
import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSocket;
import java.security.cert.CertificateFactory;
import javax.security.cert.X509Certificate;

/* JSSESupport

   Concrete implementation class for JSSE
   Support classes.

   This will only work with JDK 1.2 and up since it
   depends on JDK 1.2's certificate support

   @author EKR

   Parts cribbed from JSSECertCompat       
*/

class JSSESupport implements SSLSupport {
    /**
     * A mapping table to determine the number of effective bits in the key
     * when using a cipher suite containing the specified cipher name.  The
     * underlying data came from the TLS Specification (RFC 2246), Appendix C.
     */
    protected static final CipherData ciphers[] = {
        new CipherData("_WITH_NULL_", 0),
        new CipherData("_WITH_IDEA_CBC_", 128),
        new CipherData("_WITH_RC2_CBC_40_", 40),
        new CipherData("_WITH_RC4_40_", 40),
        new CipherData("_WITH_RC4_128_", 128),
        new CipherData("_WITH_DES40_CBC_", 40),
        new CipherData("_WITH_DES_CBC_", 56),
        new CipherData("_WITH_3DES_EDE_CBC_", 168)
    };

    private SSLSocket ssl;


    JSSESupport(SSLSocket sock){
        ssl=sock;
    }

    public String getCipherSuite() throws IOException {
        // Look up the current SSLSession
        SSLSession session = ssl.getSession();
        if (session == null)
            return null;
        return session.getCipherSuite();
    }

    public Object[] getPeerCertificateChain()
    throws IOException
    {
        // Look up the current SSLSession
        SSLSession session = ssl.getSession();
        if (session == null)
            return null;

        // Convert JSSE's certificate format to the ones we need
        X509Certificate jsseCerts[] = null;
        java.security.cert.X509Certificate x509Certs[] = null;
        try {
            jsseCerts = session.getPeerCertificateChain();
            if (jsseCerts == null)
                jsseCerts = new X509Certificate[0];
            x509Certs =
              new java.security.cert.X509Certificate[jsseCerts.length];
            for (int i = 0; i < x509Certs.length; i++) {
                byte buffer[] = jsseCerts[i].getEncoded();
                CertificateFactory cf =
                  CertificateFactory.getInstance("X.509");
                ByteArrayInputStream stream =
                  new ByteArrayInputStream(buffer);
                x509Certs[i] = (java.security.cert.X509Certificate)
                  cf.generateCertificate(stream);
            }
        } catch (Throwable t) {
            return null;
        }

        if ((x509Certs == null) || (x509Certs.length < 1))
            return null;

        return x509Certs;
    }

    /**
     * Copied from <code>org.apache.catalina.valves.CertificateValve</code>
     */
    public Integer getKeySize() 
        throws IOException {
        // Look up the current SSLSession
        SSLSession session = ssl.getSession();
        if (session == null)
            return null;
        Integer keySize = (Integer) session.getValue(KEY_SIZE_KEY);
        if (keySize == null) {
            int size = 0;
            String cipherSuite = session.getCipherSuite();
            for (int i = 0; i < ciphers.length; i++) {
                if (cipherSuite.indexOf(ciphers[i].phrase) >= 0) {
                    size = ciphers[i].keySize;
                    break;
                }
            }
            keySize = new Integer(size);
            session.putValue(KEY_SIZE_KEY, keySize);
        }
        return keySize;
    }

    public String getSessionId()
        throws IOException {
        // Look up the current SSLSession
        SSLSession session = ssl.getSession();
        if (session == null)
            return null;
        // Expose ssl_session (getId)
        byte [] ssl_session = session.getId();
        if ( ssl_session == null) 
            return null;
        StringBuffer buf=new StringBuffer("");
        for(int x=0; x<ssl_session.length; x++) {
            String digit=Integer.toHexString((int)ssl_session[x]);
            if (digit.length()<2) buf.append('0');
            if (digit.length()>2) digit=digit.substring(digit.length()-2);
            buf.append(digit);
        }
        return buf.toString();
    }
}

// ------------------------------------------------------------ Private Classes


/**
 * Simple data class that represents the cipher being used, along with the
 * corresponding effective key size.  The specified phrase must appear in the
 * name of the cipher suite to be recognized.
 */

final class CipherData {

    String phrase = null;

    int keySize = 0;

    public CipherData(String phrase, int keySize) {
        this.phrase = phrase;
        this.keySize = keySize;
    }

}
