/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Tomcat",  and  "Apache  Software *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */
package org.apache.catalina.connector.warp;

import java.io.IOException;
import java.io.InputStream;

import org.apache.catalina.Host;
import org.apache.catalina.connector.HttpRequestBase;

public class WarpRequest extends HttpRequestBase {
    /** The local stream */
    private Stream localstream;

    /** The connection to which we are associated */
    private WarpConnection connection;

    private Host host=null;

    public WarpRequest() {
        super();
        this.localstream=new Stream(this);
        this.setStream(this.localstream);
    }

    /** Process the SSL attributes */
    public Object getAttribute(String name) {

        /* Use cached values */
        Object object = super.getAttribute(name);
	if (object != null)
            return object;

	/* Fill the cache and return value if possible */
        if (!localstream.request.isSecure()) return null;

        /* Client Certificate */
        if (name.equals("javax.servlet.request.X509Certificate")) {
            WarpCertificates cert = null;
            try {
                cert = new WarpCertificates(localstream.getX509Certificates());
            } catch (IOException e) {
                return null;
            }
            super.setAttribute("javax.servlet.request.X509Certificate",
                cert.getCertificates());
        }

        /* other ssl parameters */
        if (name.equals("javax.servlet.request.cipher_suite") ||
            name.equals("javax.servlet.request.key_size") ||
            name.equals("javax.servlet.request.ssl_session")) {
            WarpSSLData ssldata = null;
            try {
                ssldata = localstream.getSSL();
            } catch (IOException e) {
                return null;
            }
            if (ssldata == null) return null;

            super.setAttribute("javax.servlet.request.cipher_suite",
                ssldata.ciph);
            if (ssldata.size!=0)
                super.setAttribute("javax.servlet.request.key_size",
                    new Integer (ssldata.size));
            super.setAttribute("javax.servlet.request.ssl_session",
                ssldata.sess);
        }
        return(super.getAttribute(name));
    }

    public void setHost(Host host) {
        this.host=host;
    }

    public Host getHost() {
        return(this.host);
    }

    /**
     * Recycle this <code>WarpResponse</code> instance.
     */
    public void recycle() {
        // Recycle our parent
        super.recycle();
        // Recycle the stream
        this.localstream.recycle();
        // Tell the parent that a stream is already in use.
        this.setStream(localstream);
    }

    /**
     * Associate this <code>WarpResponse</code> instance with a specific
     * <code>WarpConnection</code> instance.
     */
    public void setConnection(WarpConnection connection) {
        this.connection=connection;
    }

    /**
     * Return the <code>WarpConnection</code> associated this instance of
     * <code>WarpResponse</code>.
     */
    public WarpConnection getConnection() {
        return(this.connection);
    }

    protected class Stream extends InputStream {

        /** The response associated with this stream instance. */
        private WarpRequest request=null;
        /** The packet used by this stream instance. */
        private WarpPacket packet=null;
        /** Wether <code>close()</code> was called or not. */
        private boolean closed=false;

        protected Stream(WarpRequest request) {
            super();
            this.request=request;
            this.packet=new WarpPacket();
            this.packet.setType(Constants.TYPE_CBK_DATA);
        }

        
        public int read()
        throws IOException {
            if (closed) throw new IOException("Stream closed");

            if (this.packet.pointer<this.packet.size)
                return(((int)this.packet.buffer[this.packet.pointer++])&0x0ff);

            this.packet.reset();
            this.packet.setType(Constants.TYPE_CBK_READ);
            this.packet.writeUnsignedShort(65535);
            this.request.getConnection().send(packet);
            packet.reset();

            this.request.getConnection().recv(packet);

            if (packet.getType()==Constants.TYPE_CBK_DONE) return(-1);

            if (packet.getType()!=Constants.TYPE_CBK_DATA)
                throw new IOException("Invalid WARP packet type for body");

            return(this.read());
        }

        public String getX509Certificates()
        throws IOException {
            if (closed) throw new IOException("Stream closed");
            this.packet.reset();
            this.packet.setType(Constants.TYPE_ASK_SSL_CLIENT);
            this.request.getConnection().send(packet);
            packet.reset();

            this.request.getConnection().recv(packet);
            if (closed) throw new IOException("Stream closed");
            if (packet.getType()==Constants.TYPE_REP_SSL_NO) return(null);
            if (packet.getType()!=Constants.TYPE_REP_SSL_CERT)
               throw new IOException("Invalid WARP packet type for CC");
            return(this.packet.readString());
        }

        /** Read the data from the SSL environment. */
        public WarpSSLData getSSL()
        throws IOException {
          
            if (closed) throw new IOException("Stream closed");
            this.packet.reset();
            this.packet.setType(Constants.TYPE_ASK_SSL);
            this.request.getConnection().send(packet);
            packet.reset();

            this.request.getConnection().recv(packet);
            if (closed) throw new IOException("Stream closed");
            if (packet.getType()==Constants.TYPE_REP_SSL_NO) return(null);
            if (packet.getType()!=Constants.TYPE_REP_SSL)
               throw new IOException("Invalid WARP packet type for SSL data");
            WarpSSLData ssldata  = new WarpSSLData();
            ssldata.ciph = this.packet.readString();
            ssldata.sess = this.packet.readString();
            ssldata.size = this.packet.readInteger();
            return(ssldata);
        }


        
        public void close()
        throws IOException {
            if (closed) throw new IOException("Stream closed");
            this.packet.reset();
            this.packet.setType(Constants.TYPE_CBK_DONE);
            this.closed=true;
        }

        public void recycle() {
            this.packet.reset();
            this.packet.setType(Constants.TYPE_CBK_DATA);
            this.closed=false;
        }

    }
}
