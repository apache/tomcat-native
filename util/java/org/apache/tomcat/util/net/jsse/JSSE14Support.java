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

import org.apache.tomcat.util.net.SSLSupport;
import java.io.*;
import java.net.*;
import java.util.Vector;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.security.cert.Certificate;
import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLException;
import javax.net.ssl.HandshakeCompletedListener;
import javax.net.ssl.HandshakeCompletedEvent;
import java.security.cert.CertificateFactory;


/* JSSESupport

   Concrete implementation class for JSSE
   Support classes.

   This will only work with JDK 1.2 and up since it
   depends on JDK 1.2's certificate support

   @author EKR
   @author Craig R. McClanahan
   Parts cribbed from JSSECertCompat       
   Parts cribbed from CertificatesValve
*/

class JSSE14Support extends JSSESupport {

    private static org.apache.commons.logging.Log logger =
        org.apache.commons.logging.LogFactory.getLog(JSSE14Support.class);

    Listener listener = new Listener();

    public JSSE14Support(SSLSocket sock){
        super(sock);
        sock.addHandshakeCompletedListener(listener);
    }

    protected void handShake() throws IOException {
        ssl.setNeedClientAuth(true);
        synchronousHandshake(ssl);
    }

    /**
     * JSSE in JDK 1.4 has an issue/feature that requires us to do a
     * read() to get the client-cert.  As suggested by Andreas
     * Sterbenz
     */
    private  void synchronousHandshake(SSLSocket socket) 
        throws IOException {
        InputStream in = socket.getInputStream();
        int oldTimeout = socket.getSoTimeout();
        socket.setSoTimeout(1000);
        byte[] b = new byte[0];
        listener.reset();
        socket.startHandshake();
        int maxTries = 60; // 60 * 1000 = example 1 minute time out
        for (int i = 0; i < maxTries; i++) {
	    if(logger.isTraceEnabled())
		logger.trace("Reading for try #" +i);
            try {
                int x = in.read(b);
            } catch(SSLException sslex) {
                logger.info("SSL Error getting client Certs",sslex);
                throw sslex;
            } catch (IOException e) {
                // ignore - presumably the timeout
            }
            if (listener.completed) {
                break;
            }
        }
        socket.setSoTimeout(oldTimeout);
        if (listener.completed == false) {
            throw new SocketException("SSL Cert handshake timeout");
        }
    }

    /** Return the X509certificates or null if we can't get them.
     *  XXX We should allow unverified certificates 
     */ 
    protected X509Certificate [] getX509Certificates(SSLSession session) 
	throws IOException 
    {
        Certificate [] certs=null;
        try {
	    certs = session.getPeerCertificates();
        } catch( Throwable t ) {
            logger.debug("Error getting client certs",t);
            return null;
        }
        if( certs==null ) return null;
        
        X509Certificate [] x509Certs = new X509Certificate[certs.length];
	for(int i=0; i < certs.length; i++) {
	    if( certs[i] instanceof X509Certificate ) {
		// always currently true with the JSSE 1.1.x
		x509Certs[i] = (X509Certificate)certs[i];
	    } else {
		try {
		    byte [] buffer = certs[i].getEncoded();
		    CertificateFactory cf =
			CertificateFactory.getInstance("X.509");
		    ByteArrayInputStream stream =
			new ByteArrayInputStream(buffer);
		    x509Certs[i] = (X509Certificate)
			cf.generateCertificate(stream);
		} catch(Exception ex) { 
		    logger.info("Error translating cert " + certs[i], ex);
		    return null;
		}
	    }
	    if(logger.isTraceEnabled())
		logger.trace("Cert #" + i + " = " + x509Certs[i]);
	}
	if(x509Certs.length < 1)
	    return null;
	return x509Certs;
    }


    private static class Listener implements HandshakeCompletedListener {
        volatile boolean completed = false;
        public void handshakeCompleted(HandshakeCompletedEvent event) {
            completed = true;
        }
        void reset() {
            completed = false;
        }
    }

}

