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
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Locale;
import java.util.TreeMap;

import javax.servlet.ServletInputStream;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;

import org.apache.catalina.Globals;
import org.apache.catalina.connector.HttpRequestBase;
import org.apache.catalina.util.StringParser;
import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.BaseRequest;
import org.apache.tomcat.util.http.Cookies;
import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.http.ServerCookie;

public class Ajp13Request extends HttpRequestBase {

    private static final String match =
	";" + Globals.SESSION_PARAMETER_NAME + "=";

    private static int id = 1;

    private Ajp13Logger logger = new Ajp13Logger();
    private int debug;

    public Ajp13Request(Ajp13Connector connector) {
        super();
        this.debug = connector.getDebug();
        this.logger.setConnector(connector);
        this.logger.setName("Ajp13Request[" + (id++) + "]");
    }

    public void recycle() {
        super.recycle();        
    }

    void setAjpRequest(BaseRequest ajp) throws UnsupportedEncodingException {
        // XXX make this guy wrap AjpRequest so
        // we're more efficient (that's the whole point of
        // all of the MessageBytes in AjpRequest)

        setMethod(ajp.method().toString());
        setProtocol(ajp.protocol().toString());
        setRequestURI(ajp.requestURI().toString());
        setRemoteAddr(ajp.remoteAddr().toString());
        setRemoteHost(ajp.remoteHost().toString());
        setServerName(ajp.serverName().toString());
        setServerPort(ajp.getServerPort());

        if ((!(((Ajp13Connector) connector).getTomcatAuthentication())) 
            && (ajp.remoteUser() != null)) {
            setUserPrincipal(new Ajp13Principal(ajp.remoteUser().toString()));
        } else {
            setUserPrincipal(null);
        }

        setAuthType(ajp.authType().toString());
        setAuthorization(ajp.authorization().toString());
        setQueryString(ajp.queryString().toString());
        setScheme(ajp.getScheme());
        setSecure(ajp.getSecure());
        setContentLength(ajp.getContentLength());

        String contentType = ajp.contentType().toString();
        if (contentType != null) {
            setContentType(contentType);
        }

        MimeHeaders mheaders = ajp.headers();
        int nheaders = mheaders.size();
        for (int i = 0; i < nheaders; ++i) {
            MessageBytes name = mheaders.getName(i);
            MessageBytes value = mheaders.getValue(i);
            addHeader(name.toString(), value.toString());
            if ("accept-language".equals(name.toString()))
                parseLocalesHeader(value.toString());	
        }

        Iterator itr = ajp.getAttributeNames();
        while (itr.hasNext()) {
            String name = (String)itr.next();
            setAttribute(name, ajp.getAttribute(name));
        }

        addCookies(ajp.cookies());
    }

//      public Object getAttribute(String name) {
//          return ajp.getAttribute(name);
//      }

//      public Enumeration getAttributeNames() {
//          return new Enumerator(ajp.getAttributeNames());
//      }

    public void setRequestURI(String uri) {
	int semicolon = uri.indexOf(match);
	if (semicolon >= 0) {
	    String rest = uri.substring(semicolon + match.length());
	    int semicolon2 = rest.indexOf(";");
	    if (semicolon2 >= 0) {
		setRequestedSessionId(rest.substring(0, semicolon2));
		rest = rest.substring(semicolon2);
	    } else {
		setRequestedSessionId(rest);
		rest = "";
	    }
	    setRequestedSessionURL(true);
	    uri = uri.substring(0, semicolon) + rest;
	    if (debug >= 1)
	        logger.log(" Requested URL session id is " +
                           ((HttpServletRequest) getRequest())
                           .getRequestedSessionId());
	} else {
	    setRequestedSessionId(null);
	    setRequestedSessionURL(false);
	}

        super.setRequestURI(uri);
    }

    private void addCookies(Cookies cookies) {
        int ncookies = cookies.getCookieCount();
        for (int j = 0; j < ncookies; j++) {
            ServerCookie scookie = cookies.getCookie(j);
            Cookie cookie = new Cookie(scookie.getName().toString(),
                                       scookie.getValue().toString());
            if (cookie.getName().equals(Globals.SESSION_COOKIE_NAME)) {
                // Override anything requested in the URL
                if (!isRequestedSessionIdFromCookie()) {
                                // Accept only the first session id cookie
                    setRequestedSessionId(cookie.getValue());
                    setRequestedSessionCookie(true);
                    setRequestedSessionURL(false);
                    if (debug > 0) {
                        logger.log(" Requested cookie session id is " +
                                   ((HttpServletRequest) getRequest())
                                   .getRequestedSessionId());
                    }
                }
            }
            if (debug > 0) {
                logger.log(" Adding cookie " + cookie.getName() + "=" +
                           cookie.getValue());
            }
            addCookie(cookie);                    
        }        
    }

    public ServletInputStream createInputStream() throws IOException {
        return (ServletInputStream)getStream();
    }


    /**
     * Parse accept-language header value.
     */
    protected void parseLocalesHeader(String value) {

        // Store the accumulated languages that have been requested in
        // a local collection, sorted by the quality value (so we can
        // add Locales in descending order).  The values will be ArrayLists
        // containing the corresponding Locales to be added
        TreeMap locales = new TreeMap();

        // Preprocess the value to remove all whitespace
        int white = value.indexOf(' ');
        if (white < 0)
            white = value.indexOf('\t');
        if (white >= 0) {
            StringBuffer sb = new StringBuffer();
            int len = value.length();
            for (int i = 0; i < len; i++) {
                char ch = value.charAt(i);
                if ((ch != ' ') && (ch != '\t'))
                    sb.append(ch);
            }
            value = sb.toString();
        }

        // Process each comma-delimited language specification
	StringParser parser = new StringParser();
        parser.setString(value);        
        int length = parser.getLength();
        while (true) {

            // Extract the next comma-delimited entry
            int start = parser.getIndex();
            if (start >= length)
                break;
            int end = parser.findChar(',');
            String entry = parser.extract(start, end).trim();
            parser.advance();   // For the following entry

            // Extract the quality factor for this entry
            double quality = 1.0;
            int semi = entry.indexOf(";q=");
            if (semi >= 0) {
                try {
                    quality = Double.parseDouble(entry.substring(semi + 3));
                } catch (NumberFormatException e) {
                    quality = 0.0;
                }
                entry = entry.substring(0, semi);
            }

            // Skip entries we are not going to keep track of
            if (quality < 0.00005)
                continue;       // Zero (or effectively zero) quality factors
            if ("*".equals(entry))
                continue;       // FIXME - "*" entries are not handled

            // Extract the language and country for this entry
            String language = null;
            String country = null;
            String variant = null;
            int dash = entry.indexOf('-');
            if (dash < 0) {
                language = entry;
                country = "";
                variant = "";
            } else {
                language = entry.substring(0, dash);
                country = entry.substring(dash + 1);
                int vDash = country.indexOf('-');
                if (vDash > 0) {
                    String cTemp = country.substring(0, vDash);
                    variant = country.substring(vDash + 1);
                    country = cTemp;
                } else {
                    variant = "";
                }
            }

            // Add a new Locale to the list of Locales for this quality level
            Locale locale = new Locale(language, country, variant);
            Double key = new Double(-quality);  // Reverse the order
            ArrayList values = (ArrayList) locales.get(key);
            if (values == null) {
                values = new ArrayList();
                locales.put(key, values);
            }
            values.add(locale);

        }

        // Process the quality values in highest->lowest order (due to
        // negating the Double value when creating the key)
        Iterator keys = locales.keySet().iterator();
        while (keys.hasNext()) {
            Double key = (Double) keys.next();
            ArrayList list = (ArrayList) locales.get(key);
            Iterator values = list.iterator();
            while (values.hasNext()) {
                Locale locale = (Locale) values.next();
                addLocale(locale);
            }
        }

    }
}

class Ajp13Principal implements java.security.Principal {
    String user;
    
    Ajp13Principal(String user) {
        this.user = user;
    }
    public boolean equals(Object o) {
        if (o == null) {
            return false;
        } else if (!(o instanceof Ajp13Principal)) {
            return false;
        } else if (o == this) {
            return true;
        } else if (this.user == null && ((Ajp13Principal)o).user == null) {
            return true;
        } else if (user != null) {
            return user.equals( ((Ajp13Principal)o).user);
        } else {
            return false;
        }
    }
    
    public String getName() {
        return user;
    }
    
    public int hashCode() {
        if (user == null) return 0;
        else return user.hashCode();
    }
    
    public String toString() {
        return getName();
    }
}
