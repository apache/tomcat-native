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
import org.apache.tomcat.util.http.MimeHeaders;

public class Ajp13Response extends HttpResponseBase {

    private Ajp13 ajp13;
    private boolean finished = false;
    private boolean headersSent = false;
    private MimeHeaders headers = new MimeHeaders();
    private StringBuffer cookieValue = new StringBuffer();

    String getStatusMessage() {
        return getStatusMessage(getStatus());
    }

    public void recycle() {
        super.recycle();
        this.finished = false;
        this.headersSent = false;
        this.headers.recycle();
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
            try {
                super.finishResponse();
            } catch( Throwable t ) {
                t.printStackTrace();
            }
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
