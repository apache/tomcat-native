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

package org.apache.tomcat.util.net.puretls;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.Vector;

import org.apache.tomcat.util.buf.HexUtils;
import org.apache.tomcat.util.net.SSLSupport;

import COM.claymoresystems.cert.X509Cert;
import COM.claymoresystems.ptls.SSLSocket;
import COM.claymoresystems.sslg.SSLPolicyInt;


/* PureTLSSupport

   Concrete implementation class for PureTLS
   Support classes.

   This will only work with JDK 1.2 and up since it
   depends on JDK 1.2's certificate support

   @author EKR
*/

class PureTLSSupport implements SSLSupport {
    static org.apache.commons.logging.Log logger =
	org.apache.commons.logging.LogFactory.getLog(PureTLSSupport.class);

    private COM.claymoresystems.ptls.SSLSocket ssl;

    PureTLSSupport(SSLSocket sock){
        ssl=sock;
    }

    public String getCipherSuite() throws IOException {
        int cs=ssl.getCipherSuite();
        return SSLPolicyInt.getCipherSuiteName(cs);
    }

    public Object[] getPeerCertificateChain()
        throws IOException {
	return getPeerCertificateChain(false);
    }

    public Object[] getPeerCertificateChain(boolean force)
        throws IOException {
        Vector v=ssl.getCertificateChain();

	if(v == null && force) {
	    SSLPolicyInt policy=new SSLPolicyInt();
	    policy.requireClientAuth(true);
	    policy.handshakeOnConnect(false);
	    policy.waitOnClose(false);
	    ssl.renegotiate(policy);
	    v = ssl.getCertificateChain();
	}

        if(v==null)
            return null;
        
        java.security.cert.X509Certificate[] chain=
            new java.security.cert.X509Certificate[v.size()];

        try {
          for(int i=1;i<=v.size();i++){
            // PureTLS provides cert chains with the peer
            // cert last but the Servlet 2.3 spec (S 4.7) requires
            // the opposite order so we reverse the chain as we go
            byte buffer[]=((X509Cert)v.elementAt(
                 v.size()-i)).getDER();
            
            CertificateFactory cf =
              CertificateFactory.getInstance("X.509");
            ByteArrayInputStream stream =
              new ByteArrayInputStream(buffer);

            X509Certificate xCert = (X509Certificate)cf.generateCertificate(stream);
            chain[i-1]= xCert;
            if(logger.isTraceEnabled()) {
		logger.trace("Cert # " + i + " = " + xCert);
	    }
          }
        } catch (java.security.cert.CertificateException e) {
	    logger.info("JDK's broken cert handling can't parse this certificate (which PureTLS likes)",e);
            throw new IOException("JDK's broken cert handling can't parse this certificate (which PureTLS likes)");
        }
        return chain;
    }

    /**
     * Lookup the symmetric key size.
     */
    public Integer getKeySize() 
        throws IOException {

        int cs=ssl.getCipherSuite();
        String cipherSuite = SSLPolicyInt.getCipherSuiteName(cs);
        int size = 0;
        for (int i = 0; i < ciphers.length; i++) {
            if (cipherSuite.indexOf(ciphers[i].phrase) >= 0) {
                size = ciphers[i].keySize;
                break;
            }
        }
        Integer keySize = new Integer(size);
        return keySize;
    }

    public String getSessionId()
        throws IOException {
        byte [] ssl_session = ssl.getSessionID();
        if(ssl_session == null)
            return null;
        return HexUtils.convert(ssl_session);
    }

}







