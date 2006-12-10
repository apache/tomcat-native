/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.jk.status;

import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.ProtocolException;
import java.net.URL;
import java.net.URLConnection;

import org.apache.catalina.ant.AbstractCatalinaTask;
import org.apache.catalina.util.Base64;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.Project;

/**
 * Ant task that implements mod_jk 1.2.20 result message string
 * 
 * @author Peter Rossbach
 * @version $Revision:$
 * @since mod_jk 1.2.20
 */
public abstract class AbstractJkStatusTask extends AbstractCatalinaTask {

	/**
     * Execute the requested operation.
     * 
     * @exception BuildException
     *                if an error occurs
     */
    public void execute() throws BuildException {

        super.execute();
        checkParameter();
        StringBuffer sb = createLink();
        execute(sb.toString(), null, null, -1);

    }
    
    protected abstract void checkParameter() ;
    protected abstract StringBuffer createLink() ;
    	 
    /**
     * Execute the specified command, based on the configured properties.
     * The input stream will be closed upon completion of this task, whether
     * it was executed successfully or not.
     *
     * @param command Command to be executed
     * @param istream InputStream to include in an HTTP PUT, if any
     * @param contentType Content type to specify for the input, if any
     * @param contentLength Content length to specify for the input, if any
     *
     * @exception BuildException if an error occurs
     */
    public void execute(String command, InputStream istream,
                        String contentType, int contentLength)
        throws BuildException {

        InputStreamReader reader = null;
        try {

            HttpURLConnection hconn = send(command, istream, contentType, contentLength);

            // Process the response message
            reader = new InputStreamReader(hconn.getInputStream(), "UTF-8");
            String error = null;
            error = handleResult(reader, error);
            if (error != null && isFailOnError()) {
                // exception should be thrown only if failOnError == true
                // or error line will be logged twice
                throw new BuildException(error);
            }
        } catch (Throwable t) {
            if (isFailOnError()) {
                throw new BuildException(t);
            } else {
                handleErrorOutput(t.getMessage());
            }
        } finally {
            closeRedirector();
            if (reader != null) {
                try {
                    reader.close();
                } catch (Throwable u) {
                    ;
                }
                reader = null;
            }
            if (istream != null) {
                try {
                    istream.close();
                } catch (Throwable u) {
                    ;
                }
                istream = null;
            }
        }

    }

	private String handleResult(InputStreamReader reader, String error) throws IOException {
		StringBuffer buff = new StringBuffer();
		int msgPriority = Project.MSG_INFO;
		boolean first = true;
		while (true) {
		    int ch = reader.read();
		    if (ch < 0) {
		        break;
		    } else if ((ch == '\r') || (ch == '\n')) {
		        // in Win \r\n would cause handleOutput() to be called
		        // twice, the second time with an empty string,
		        // producing blank lines
		        if (buff.length() > 0) {
		            String line = buff.toString();
		            buff.setLength(0);
		            if (first) {
		                if (!line.startsWith("Result: type=OK")) {
		                    error = line;
		                    msgPriority = Project.MSG_ERR;
		                }
		                first = false;
		            }
		            handleOutput(line, msgPriority);
		        }
		    } else {
		        buff.append((char) ch);
		    }
		}
		if (buff.length() > 0) {
		    handleOutput(buff.toString(), msgPriority);
		}
		return error;
	}
    
	protected HttpURLConnection send(String command, InputStream istream, String contentType, int contentLength) throws IOException, MalformedURLException, ProtocolException {
		URLConnection conn;
		// Create a connection for this command
		conn = (new URL(url + command)).openConnection();
		HttpURLConnection hconn = (HttpURLConnection) conn;

		// Set up standard connection characteristics
		hconn.setAllowUserInteraction(false);
		hconn.setDoInput(true);
		hconn.setUseCaches(false);
		if (istream != null) {
		    hconn.setDoOutput(true);
		    hconn.setRequestMethod("PUT");
		    if (contentType != null) {
		        hconn.setRequestProperty("Content-Type", contentType);
		    }
		    if (contentLength >= 0) {
		        hconn.setRequestProperty("Content-Length",
		                                 "" + contentLength);
		    }
		} else {
		    hconn.setDoOutput(false);
		    hconn.setRequestMethod("GET");
		}
		hconn.setRequestProperty("User-Agent",
		                         "JkStatus-Ant-Task/1.1");

		// Set up an authorization header with our credentials
		String input = username + ":" + password;
		String output = new String(Base64.encode(input.getBytes()));
		hconn.setRequestProperty("Authorization",
		                         "Basic " + output);

		// Establish the connection with the server
		hconn.connect();
        // Send the request data (if any)
        if (istream != null) {
            BufferedOutputStream ostream =
                new BufferedOutputStream(hconn.getOutputStream(), 1024);
            byte buffer[] = new byte[1024];
            while (true) {
                int n = istream.read(buffer);
                if (n < 0) {
                    break;
                }
                ostream.write(buffer, 0, n);
            }
            ostream.flush();
            ostream.close();
            istream.close();
        }
		return hconn;
	}


}
