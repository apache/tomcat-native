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

package org.apache.catalina.connector.warp;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Iterator;

import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpSession;

import org.apache.catalina.Globals;
import org.apache.catalina.connector.HttpResponseBase;
import org.apache.catalina.util.CookieTools;

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
