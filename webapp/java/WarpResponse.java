/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *         Copyright (c) 1999, 2000  The Apache Software Foundation.         *
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

import org.apache.catalina.connector.HttpResponseBase;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.MalformedURLException;
import java.net.URL;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Locale;
import java.util.TimeZone;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpUtils;
import org.apache.catalina.HttpResponse;
import org.apache.catalina.Globals;
import org.apache.catalina.Logger;
import org.apache.catalina.util.CookieTools;
import org.apache.catalina.util.RequestUtil;

/**
 *
 *
 * @author <a href="mailto:pier.fumagalli@eng.sun.com">Pier Fumagalli</a>
 * @author Copyright &copy; 1999, 2000 <a href="http://www.apache.org">The
 *         Apache Software Foundation.
 * @version CVS $Id$
 */
public class WarpResponse extends HttpResponseBase {

    // -------------------------------------------------------------- CONSTANTS

    /** Our debug flag status (Used to compile out debugging information). */
    private static final boolean DEBUG=WarpDebug.DEBUG;

    /** The WarpRequestHandler associated with this response. */
    private WarpRequestHandler handler=null;

    /** The WarpPacket used to write data. */
    private WarpPacket packet=new WarpPacket();
    
    private boolean committed=false;

    /**
     * Return the WarpRequestHandler associated with this response.
     */
    protected WarpRequestHandler getWarpRequestHandler() {
        return(this.handler);
    }

    /**
     * Set the WarpRequestHandler associated with this response.
     */
    protected void setWarpRequestHandler(WarpRequestHandler handler) {
        this.handler=handler;
    }

    public boolean isCommitted() {
        return(this.committed);
    }

    protected void sendHeaders() throws IOException {
        if (DEBUG) this.debug("Sending status and headers");

        if (isCommitted()) return;

        this.packet.reset();
        String prot=request.getRequest().getProtocol();
        this.packet.writeString(prot);
        this.packet.writeShort(status);
        if (message != null) this.packet.writeString(message);
        else this.packet.writeString("");
        this.handler.send(WarpConstants.TYP_REQUEST_STA,this.packet);

        if (DEBUG)
            this.debug(prot+" "+status+((message==null)?(""):(" "+message)));

        // Send the content-length and content-type headers (if any)
        if (getContentType() != null) {
            this.packet.reset();
            this.packet.writeString("Content-Type");
            this.packet.writeString(getContentType());
            this.handler.send(WarpConstants.TYP_REQUEST_HDR,this.packet);
            if (DEBUG) this.debug("Content-Type: "+getContentType());
        }
        
        if (getContentLength() >= 0) {
            this.packet.reset();
            this.packet.writeString("Content-Length");
            this.packet.writeString(Integer.toString(getContentLength()));
            this.handler.send(WarpConstants.TYP_REQUEST_HDR,this.packet);
            if (DEBUG) this.debug("Content-Type: "+getContentType());
        }
    
        // Send all specified headers (if any)
        synchronized (headers) {
            Iterator names = headers.keySet().iterator();
            while (names.hasNext()) {
                String name = (String) names.next();
                ArrayList values = (ArrayList) headers.get(name);
                Iterator items = values.iterator();
                while (items.hasNext()) {
                    String value = (String) items.next();
                    this.packet.reset();
                    this.packet.writeString(name);
                    this.packet.writeString(value);
                    this.handler.send(WarpConstants.TYP_REQUEST_HDR,this.packet);
                    if (DEBUG) this.debug(name+": "+value);
                }
            }
        }

        // Add the session ID cookie if necessary
        HttpServletRequest hreq=(HttpServletRequest)request.getRequest();
        HttpSession session=hreq.getSession(false);

        if ((session!=null) && session.isNew() && getContext().getCookies()) {

            Cookie cookie=new Cookie(Globals.SESSION_COOKIE_NAME,session.getId());
            cookie.setMaxAge(-1);
            String contextPath = null;
            
            if (context != null) contextPath = context.getPath();
            
            if ((contextPath != null) && (contextPath.length() > 0)) {
                cookie.setPath(contextPath);
            } else {
                cookie.setPath("/");
            }
            
            if (hreq.isSecure()) cookie.setSecure(true);
            addCookie(cookie);
        }

        // Send all specified cookies (if any)
        synchronized (cookies) {
            Iterator items = cookies.iterator();
            while (items.hasNext()) {
                Cookie cookie = (Cookie) items.next();
                String name=CookieTools.getCookieHeaderName(cookie);
                String value=CookieTools.getCookieHeaderValue(cookie);
                this.packet.reset();
                this.packet.writeString(name);
                this.packet.writeString(value);
                this.handler.send(WarpConstants.TYP_REQUEST_HDR,this.packet);
                if (DEBUG) this.debug(name+": "+value);
            }
        }

        // Commit the headers
        this.packet.reset();
        this.handler.send(WarpConstants.TYP_REQUEST_CMT,this.packet);
        this.committed = true;
    }

    // ------------------------------------------------------ DEBUGGING METHODS

    /**
     * Dump a debug message.
     */
    private void debug(String msg) {
        if (DEBUG) WarpDebug.debug(this,msg);
    }

    /**
     * Dump information for an Exception.
     */
    private void debug(Exception exc) {
        if (DEBUG) WarpDebug.debug(this,exc);
    }

}
