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
import java.security.Principal;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Locale;
import java.util.TreeMap;

import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;

import org.apache.catalina.Context;
import org.apache.catalina.Globals;
import org.apache.catalina.Host;
import org.apache.catalina.util.RequestUtil;
import org.apache.catalina.util.StringParser;

public class WarpRequestHandler {

    private StringParser parser = new StringParser();
    private static final String match=";"+Globals.SESSION_PARAMETER_NAME+"=";

    /* ==================================================================== */
    /* Constructor                                                          */
    /* ==================================================================== */

    public WarpRequestHandler() {
        super();
    }

    public boolean handle(WarpConnection connection, WarpPacket packet)
    throws IOException {
        WarpLogger logger=new WarpLogger(this);
        WarpConnector connector=connection.getConnector();
        logger.setContainer(connector.getContainer());
        WarpRequest request=new WarpRequest();
        WarpResponse response=new WarpResponse();
        response.setRequest(request);
        response.setConnection(connection);
        response.setPacket(packet);
        request.setConnection(connection);
        request.setConnector(connector);

        // Prepare the Proceed packet
        packet.reset();
        packet.setType(Constants.TYPE_CONF_PROCEED);
        connection.send(packet);

        // Loop for configuration packets
        while (true) {
            connection.recv(packet);

            switch (packet.getType()) {
                case Constants.TYPE_REQ_INIT: {
                    int id=packet.readInteger();
                    String meth=packet.readString();
                    String ruri=packet.readString();
                    String args=packet.readString();
                    String prot=packet.readString();
                    if (Constants.DEBUG)
                        logger.debug("Request ID="+id+" \""+meth+" "+ruri+
                                     "?"+args+" "+prot+"\"");

                    request.recycle();
                    response.recycle();
                    response.setRequest(request);
                    response.setConnection(connection);
                    response.setPacket(packet);

                    request.setMethod(meth);
                    this.processUri(logger,request,ruri);
                    if (args.length()>0) request.setQueryString(args);
                    else request.setQueryString(null);
                    request.setProtocol(prot);
                    request.setConnection(connection);
                    Context ctx=connector.applicationContext(id);
                    if (ctx!=null) {
                        request.setContext(ctx);
                        request.setContextPath(ctx.getPath());
                        request.setHost((Host)ctx.getParent());
                    }
                    break;
                }

                case Constants.TYPE_REQ_CONTENT: {
                    String ctyp=packet.readString();
                    int clen=packet.readInteger();
                    if (Constants.DEBUG)
                        logger.debug("Request content type="+ctyp+" length="+
                                     clen);
                    if (ctyp.length()>0) request.setContentType(ctyp);
                    if (clen>0) request.setContentLength(clen);
                    break;
                }

                case Constants.TYPE_REQ_SCHEME: {
                    String schm=packet.readString();
                    if (Constants.DEBUG)
                        logger.debug("Request scheme="+schm);
                    request.setScheme(schm);
                    if (schm.equals("https"))
                       request.setSecure(true);
                    break;
                }

                case Constants.TYPE_REQ_AUTH: {
                    String user=packet.readString();
                    String auth=packet.readString();
                    if (Constants.DEBUG)
                        logger.debug("Request user="+user+" auth="+auth);
                    request.setAuthType(auth);
                    // What to do for user name?
                    if(user != null && auth != null && auth.equals("Basic")) {
                        Principal prin = new BasicPrincipal(user);
                        request.setUserPrincipal(prin);
                    }

                    break;
                }

                case Constants.TYPE_REQ_HEADER: {
                    String hnam=packet.readString();
                    String hval=packet.readString();
                    this.processHeader(logger,request,hnam,hval);
                    break;
                }

                case Constants.TYPE_REQ_SERVER: {
                    String host=packet.readString();
                    String addr=packet.readString();
                    int port=packet.readUnsignedShort();
                    if (Constants.DEBUG)
                        logger.debug("Server detail "+host+":"+port+
                                     " ("+addr+")");
                    request.setServerName(host);
                    request.setServerPort(port);
                    break;
                }

                case Constants.TYPE_REQ_CLIENT: {
                    String host=packet.readString();
                    String addr=packet.readString();
                    int port=packet.readUnsignedShort();
                    if (Constants.DEBUG)
                        logger.debug("Client detail "+host+":"+port+
                                     " ("+addr+")");
                    request.setRemoteHost(host);
                    request.setRemoteAddr(addr);
                    break;
                }

                case Constants.TYPE_REQ_PROCEED: {
                    if (Constants.DEBUG)
                        logger.debug("Request is about to be processed");
                    try {
                        connector.getContainer().invoke(request,response);
                    } catch (Exception e) {
                        logger.log(e);
                    }
                    request.finishRequest();
                    response.finishResponse();
                    if (Constants.DEBUG)
                        logger.debug("Request has been processed");
                    break;
                }

                default: {
                    String msg="Invalid packet "+packet.getType();
                    logger.log(msg);
                    packet.reset();
                    packet.setType(Constants.TYPE_FATAL);
                    packet.writeString(msg);
                    connection.send(packet);
                    return(false);
                }
            }
        }
    }

    private void processUri(WarpLogger logger, WarpRequest req, String uri) {

        // Parse any requested session ID out of the request URI
        int semicolon = uri.indexOf(match);
        if (semicolon >= 0) {
            String rest = uri.substring(semicolon + match.length());
            int semicolon2 = rest.indexOf(';');
            if (semicolon2 >= 0) {
                req.setRequestedSessionId(rest.substring(0, semicolon2));
                rest = rest.substring(semicolon2);
            } else {
                req.setRequestedSessionId(rest);
                rest = "";
            }
            req.setRequestedSessionURL(true);
            uri = uri.substring(0, semicolon) + rest;
            if (Constants.DEBUG) {
                logger.log("Requested URL session id is " +
                    ((HttpServletRequest) req.getRequest())
                    .getRequestedSessionId());
            }
        } else {
            req.setRequestedSessionId(null);
            req.setRequestedSessionURL(false);
        }

        req.setRequestURI(uri);
    }

    private void processHeader(WarpLogger logger, WarpRequest req,
                 String name, String value) {

        if (Constants.DEBUG)
            logger.debug("Request header "+name+": "+value);

        if ("cookie".equalsIgnoreCase(name)) {
            Cookie cookies[] = RequestUtil.parseCookieHeader(value);
            for (int i = 0; i < cookies.length; i++) {
                if (cookies[i].getName().equals
                    (Globals.SESSION_COOKIE_NAME)) {
                    // Override anything requested in the URL
                    if (!req.isRequestedSessionIdFromCookie()) {
                        // Accept only the first session id cookie
                        req.setRequestedSessionId
                            (cookies[i].getValue());
                        req.setRequestedSessionCookie(true);
                        req.setRequestedSessionURL(false);
                        if (Constants.DEBUG) {
                            logger.debug("Requested cookie session id is " +
                                ((HttpServletRequest) req.getRequest())
                                .getRequestedSessionId());
                        }
                    }
                }
                if (Constants.DEBUG) {
                    logger.debug("Adding cookie "+cookies[i].getName()+"="+
                        cookies[i].getValue());
                }
                req.addCookie(cookies[i]);
            }
        }
        if (name.equalsIgnoreCase("Accept-Language"))
            parseAcceptLanguage(logger,req,value);

        if (name.equalsIgnoreCase("Authorization"))
            req.setAuthorization(value);

        req.addHeader(name,value);
    }

    /**
     * Parse the value of an <code>Accept-Language</code> header, and add
     * the corresponding Locales to the current request.
     *
     * @param value The value of the <code>Accept-Language</code> header.
     */
    private void parseAcceptLanguage(WarpLogger logger, WarpRequest request, 
                                     String value) {

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
        parser.setString(value);        // ASSERT: parser is available to us
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
            int dash = entry.indexOf('-');
            if (dash < 0) {
                language = entry;
                country = "";
            } else {
                language = entry.substring(0, dash);
                country = entry.substring(dash + 1);
            }

            // Add a new Locale to the list of Locales for this quality level
            Locale locale = new Locale(language, country);
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
                if (Constants.DEBUG) {
                    logger.debug("Adding locale '" + locale + "'");
                }
                request.addLocale(locale);
            }
        }
    }

    class BasicPrincipal implements Principal {
        private String user;

        BasicPrincipal(String user) {
            this.user = user;
        }

        public boolean equals(Object another) {
            return (another instanceof Principal &&
                ((Principal)another).getName().equals(user));
        }

        public String getName() {
            return user;
        }

        public String toString() {
            return getName();
        }
    }
}
