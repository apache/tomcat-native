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
package org.apache.ajp.tomcat4;

import java.io.IOException;

import java.util.Iterator;

import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpSession;

import org.apache.catalina.connector.HttpResponseBase;
import org.apache.catalina.Globals;
import org.apache.catalina.util.CookieTools;

import org.apache.ajp.Ajp13;

public class Ajp13Response extends HttpResponseBase {

    private Ajp13 ajp13;
    private boolean finished = false;
    private boolean headersSent = false;
    private StringBuffer cookieValue = new StringBuffer();

    String getStatusMessage() {
        return getStatusMessage(getStatus());
    }

    public void recycle() {
        super.recycle();
        this.finished = false;
        this.headersSent = false;
    }

    protected void sendHeaders()  throws IOException {

        if (headersSent) {
            // don't send headers twice
            return;
        }
        headersSent = true;

        int numHeaders = 0;

        if (getContentType() != null) {
            numHeaders++;
	}
        
	if (getContentLength() >= 0) {
            numHeaders++;
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

                cookieValue.delete(0, cookieValue.length());
                CookieTools.getCookieHeaderValue(cookie, cookieValue);
                
                addHeader(CookieTools.getCookieHeaderName(cookie),
                          cookieValue.toString());
	    }
	}

        // figure out how many headers...
        // can have multiple headers of the same name...
        // need to loop through headers once to get total
        // count, once to add header to outBuf
        String[] hnames = getHeaderNames();
        Object[] hvalues = new Object[hnames.length];

        int i;
        for (i = 0; i < hnames.length; ++i) {
            String[] tmp = getHeaderValues(hnames[i]);
            numHeaders += tmp.length;
            hvalues[i] = tmp;
        }

        ajp13.beginSendHeaders(getStatus(), getStatusMessage(getStatus()), numHeaders);

        // send each header
        if (getContentType() != null) {
	    ajp13.sendHeader("Content-Type", getContentType());
	}
        
	if (getContentLength() >= 0) {
	    ajp13.sendHeader("Content-Length", String.valueOf(getContentLength()));
	}

        for (i = 0; i < hnames.length; ++i) {
	    String name = hnames[i];
            String[] values = (String[])hvalues[i];

            for (int j = 0; j < values.length; ++j) {
                ajp13.sendHeader(name, values[j]);
            }
        }

        ajp13.endSendHeaders();

        // The response is now committed
        committed = true;
    }

    public void finishResponse() throws IOException {
	if(!this.finished) {
	    super.finishResponse();
            this.finished = true; // Avoid END_OF_RESPONSE sent 2 times
	    ajp13.finish();
	}        
    }

    void setAjp13(Ajp13 ajp13) {
        this.ajp13 = ajp13;
    }

    Ajp13 getAjp13() {
        return this.ajp13;
    }
}
