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
import java.io.OutputStream;
import java.io.PrintWriter;
import java.net.MalformedURLException;
import java.net.URL;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Locale;
import java.util.TimeZone;
import javax.servlet.ServletResponse;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
//import javax.servlet.http.HttpUtils;
import org.apache.catalina.HttpResponse;
import org.apache.catalina.Globals;
import org.apache.catalina.Logger;
import org.apache.catalina.connector.HttpResponseBase;
import org.apache.catalina.util.CookieTools;
import org.apache.catalina.util.RequestUtil;

public class WarpResponse extends HttpResponseBase {
    /** The local stream */
    private Stream localstream;
    /** The packet used for processing headers */
    private WarpPacket packet;
    /** The connection to which we are associated */
    private WarpConnection connection;

    /**
     * Create a new instance of a <code>WarpResponse</code>.
     */
    public WarpResponse() {
        super();
        // A WarpResponse is _always_ associated with a Stream
        this.localstream=new Stream(this);
        this.setStream(localstream);
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
     * Set the <code>WarpPacket</code> instance used to process headers.
     */
    public void setPacket(WarpPacket packet) {
        this.packet=packet;
    }

    /**
     * Return the <code>WarpPacket</code> instance used to process headers.
     */
    public WarpPacket getPacket() {
        return(this.packet);
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

    /**
     * Flush output and finish.
     */
    public void finishResponse()
    throws IOException {
        super.finishResponse();
        this.localstream.finish();
    }

    /**
     * Send the HTTP response headers, if this has not already occurred.
     */
    protected void sendHeaders() throws IOException {
        if (isCommitted()) return;
        if ("HTTP/0.9".equals(request.getRequest().getProtocol())) {
            committed = true;
            return;
        }

        this.packet.reset();
        this.packet.setType(Constants.TYPE_RES_STATUS);
        this.packet.writeUnsignedShort(status);
        this.packet.writeString(message);
        this.connection.send(this.packet);

        if (getContentType() != null) {
            this.packet.reset();
            this.packet.setType(Constants.TYPE_RES_HEADER);
            this.packet.writeString("Content-Type");
            this.packet.writeString(getContentType());
            this.connection.send(this.packet);
        }
        if (getContentLength() >= 0) {
            this.packet.reset();
            this.packet.setType(Constants.TYPE_RES_HEADER);
            this.packet.writeString("Content-Length");
            this.packet.writeString(Integer.toString(getContentLength()));
            this.connection.send(this.packet);
        }

        synchronized (headers) {
                Iterator names = headers.keySet().iterator();
            while (names.hasNext()) {
                String name = (String) names.next();
                ArrayList values = (ArrayList) headers.get(name);
                Iterator items = values.iterator();
                while (items.hasNext()) {
                        String value = (String) items.next();
                    this.packet.reset();
                    this.packet.setType(Constants.TYPE_RES_HEADER);
                    this.packet.writeString(name);
                    this.packet.writeString(value);
                    this.connection.send(this.packet);
                    }
            }
        }

        // Add the session ID cookie if necessary
        HttpServletRequest hreq = (HttpServletRequest) request.getRequest();
        HttpSession session = hreq.getSession(false);

        if ((session != null) && session.isNew() && (getContext() != null)
                && getContext().getCookies()) {
            Cookie cookie = new Cookie(Globals.SESSION_COOKIE_NAME,
                                       session.getId());
            cookie.setMaxAge(-1);
            String contextPath = null;
                if (context != null)
                    contextPath = context.getPath();
            if ((contextPath != null) && (contextPath.length() > 0))
                cookie.setPath(contextPath);
            else
                cookie.setPath("/");
            if (hreq.isSecure())
                cookie.setSecure(true);
            addCookie(cookie);
        }

        // Send all specified cookies (if any)
        synchronized (cookies) {
            Iterator items = cookies.iterator();
            while (items.hasNext()) {
                Cookie cookie = (Cookie) items.next();
                String name=CookieTools.getCookieHeaderName(cookie);
                StringBuffer value=new StringBuffer();
                CookieTools.getCookieHeaderValue(cookie,value);
                    this.packet.reset();
                this.packet.setType(Constants.TYPE_RES_HEADER);
                this.packet.writeString(name);
                this.packet.writeString(value.toString());
                this.connection.send(this.packet);
            }
        }

            this.packet.reset();
        this.packet.setType(Constants.TYPE_RES_COMMIT);
        this.connection.send(this.packet);

        committed = true;
    }

    /**
     * The <code>OutputStream</code> that will handle all response body
     * transmission.
     */
    protected class Stream extends OutputStream {
        /** The response associated with this stream instance. */
        private WarpResponse response=null;
        /** The packet used by this stream instance. */
        private WarpPacket packet=null;
        /** Wether <code>close()</code> was called or not. */
        private boolean closed=false;

        /**
         * Construct a new instance of a <code>WarpResponse.Stream</code>
         * associated with a parent <code>WarpResponse</code>.
         */
        protected Stream(WarpResponse response) {
            super();
            this.response=response;
            this.packet=new WarpPacket();
        }

        /**
         * Write one byte of data to the <code>WarpPacket</code> nested
         * within this <code>WarpResponse.Stream</code>. All data is buffered
         * until the <code>flush()</code> or <code>close()</code> method is
         * not called.
         */
        public void write(int b)
        throws IOException {
            if (closed) throw new IOException("Stream closed");
            if (packet.size>=packet.buffer.length) this.flush();
            packet.buffer[packet.size++]=(byte)b;
        }

        /**
         * Flush the current packet to the WARP client.
         */
        public void flush()
        throws IOException {
            if (closed) throw new IOException("Stream closed");
            packet.setType(Constants.TYPE_RES_BODY);
            response.getConnection().send(packet);
            packet.reset();
        }

        /**
         * Flush this <code>WarpResponse.Stream</code> and close it.
         */
        public void close()
        throws IOException {
            if (closed) throw new IOException("Stream closed");
            flush();
            packet.setType(Constants.TYPE_RES_DONE);
            response.getConnection().send(packet);
            packet.reset();
        }

        /**
         * Flush this <code>WarpResponse.Stream</code> and close it.
         */
        public void finish()
        throws IOException {
            if (closed) return;
            else this.close();
        }

        /**
         * Recycle this <code>WarpResponse.Stream</code> instance.
         */
        public void recycle() {
            this.packet.reset();
            this.closed=false;
        }
    }
}
