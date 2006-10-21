/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
package org.apache.jk.status;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.ProtocolException;
import java.net.URL;
import java.net.URLConnection;

import org.apache.catalina.util.Base64;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.tomcat.util.digester.Digester;

/**
 * Create connection to mod_jk jkstatus page.
 * Optional you can use Http basic auth user and password.
 * @author Peter Rossbach
 * @version $Revision$ $Date$
 * @see org.apache.jk.status.JkStatusParser
 * @since 5.5.10
 */
public class JkStatusAccessor {
    
    private static Log log = LogFactory.getLog(JkStatusAccessor.class);
    /**
     * The descriptive information about this implementation.
     */
    protected static final String info = "org.apache.jk.status.JkStatusAccessor/1.0";

    /**
     * Parse Apache mod_jk Status  from base url http://host:port/jkstatus)
     * @param url
     * @param username
     * @param password
     *  
     */
    public JkStatus status(String url, String username, String password)
            throws Exception {

        if(url == null || "".equals(url))
            return null ;
        HttpURLConnection hconn = null;
        JkStatus status = null;

        try {
            hconn = openConnection(url + "?cmd=show&mime=xml", username, password);
            Digester digester = JkStatusParser.getDigester();
            synchronized (digester) {
                status = (JkStatus) digester.parse(hconn.getInputStream());
            }
        } catch (Throwable t) {
            throw new Exception(t);
        } finally {
            if (hconn != null) {
                try {
                    hconn.disconnect();
                } catch (Throwable u) {
                    ;
                }
                hconn = null;
            }
        }
        return status;
    }

    /**
     * Create a auth http connection for this url
     * 
     * @param url
     * @param username
     * @param password
     * @return HttpConnection
     * @throws IOException
     * @throws MalformedURLException
     * @throws ProtocolException
     */
    protected HttpURLConnection openConnection(String url, String username,
            String password) throws IOException, MalformedURLException,
            ProtocolException {
        URLConnection conn;
        conn = (new URL(url)).openConnection();
        HttpURLConnection hconn = (HttpURLConnection) conn;

        // Set up standard connection characteristics
        hconn.setAllowUserInteraction(false);
        hconn.setDoInput(true);
        hconn.setUseCaches(false);
        hconn.setDoOutput(false);
        hconn.setRequestMethod("GET");
        hconn.setRequestProperty("User-Agent", "JkStatus-Client/1.0");

        if(username != null && password != null ) {
             setAuthHeader(hconn, username, password);
        }
        // Establish the connection with the server
        hconn.connect();
        return hconn;
    }

    /**
     * Set Basic Auth Header
     * 
     * @param hconn
     * @param username
     * @param password
     */
    protected void setAuthHeader(HttpURLConnection hconn, String username,
            String password) {
        // Set up an authorization header with our credentials
        String input = username + ":" + password;
        String output = new String(Base64.encode(input.getBytes()));
        hconn.setRequestProperty("Authorization", "Basic " + output);
    }

}