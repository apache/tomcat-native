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

package org.apache.coyote.http11;

import java.io.EOFException;
import java.io.InterruptedIOException;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;

import org.apache.tomcat.util.buf.ByteChunk;
import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.MimeHeaders;

import org.apache.coyote.ActionHook;
import org.apache.coyote.ActionCode;
import org.apache.coyote.Adapter;
import org.apache.coyote.Connector;
import org.apache.coyote.InputBuffer;
import org.apache.coyote.OutputBuffer;
import org.apache.coyote.Request;
import org.apache.coyote.Response;

import org.apache.coyote.http11.filters.ChunkedInputFilter;
import org.apache.coyote.http11.filters.ChunkedOutputFilter;
import org.apache.coyote.http11.filters.IdentityInputFilter;
import org.apache.coyote.http11.filters.IdentityOutputFilter;
import org.apache.coyote.http11.filters.VoidInputFilter;
import org.apache.coyote.http11.filters.VoidOutputFilter;


/**
 * Processes HTTP requests.
 * 
 * @author Remy Maucherat
 */
public class Http11Connector implements Connector, ActionHook {


    // ----------------------------------------------------------- Constructors


    /**
     * Default constructor.
     */
    public Http11Connector() {

        request = new Request();
        inputBuffer = new InternalInputBuffer(request);
        request.setInputBuffer(inputBuffer);

        response = new Response();
        response.setHook(this);
        outputBuffer = new InternalOutputBuffer(response);
        response.setOutputBuffer(outputBuffer);

        initializeFilters();

    }


    // ----------------------------------------------------- Instance Variables


    /**
     * Associated adapter.
     */
    protected Adapter adapter = null;


    /**
     * Request object.
     */
    protected Request request = null;


    /**
     * Response object.
     */
    protected Response response = null;


    /**
     * Input.
     */
    protected InternalInputBuffer inputBuffer = null;


    /**
     * Output.
     */
    protected InternalOutputBuffer outputBuffer = null;


    /**
     * Error flag.
     */
    protected boolean error = false;


    /**
     * Keep-alive.
     */
    protected boolean keepAlive = true;


    /**
     * Content delimitator for the request (if false, the connection will
     * be closed at the end of the request).
     */
    protected boolean contentDelimitation = true;


    /**
     * List of restricted user agents.
     */
    protected String[] restrictedUserAgents = null;


    // --------------------------------------------------------- Public Methods


    /**
     * Add input or output filter.
     * 
     * @param className class name of the filter
     */
    public void addFilter(String className) {
        try {
            Class clazz = Class.forName(className);
            Object obj = clazz.newInstance();
            if (obj instanceof InputFilter) {
                inputBuffer.addFilter((InputFilter) obj);
            } else if (obj instanceof OutputFilter) {
                outputBuffer.addFilter((OutputFilter) obj);
            } else {
                // Not a valid filter: log and ignore
            }
        } catch (Exception e) {
            // Log and ignore
        }
    }


    /**
     * Add restricted user-agent (which will downgrade the connector 
     * to HTTP/1.0 mode). The user agent String given will be exactly matched
     * to the user-agent header submitted by the client.
     * 
     * @param userAgent user-agent string
     */
    public void addRestrictedUserAgent(String userAgent) {
        if (restrictedUserAgents == null)
            restrictedUserAgents = new String[0];
        String[] results = new String[restrictedUserAgents.length + 1];
        for (int i = 0; i < restrictedUserAgents.length; i++)
            results[i] = restrictedUserAgents[i];
        results[restrictedUserAgents.length] = userAgent;
        restrictedUserAgents = results;
    }


    /**
     * Set restricted user agent list (this method is best when used with 
     * a large number of connectors, where it would be better to have all of 
     * them referenced a single array).
     */
    public void setRestrictedUserAgents(String[] restrictedUserAgents) {
        this.restrictedUserAgents = restrictedUserAgents;
    }


    /**
     * Return the list of restricted user agents.
     */
    public String[] findRestrictedUserAgents() {
        return (restrictedUserAgents);
    }


    /**
     * Process pipelined HTTP requests using the specified input and output
     * streams.
     * 
     * @param inputStream stream from which the HTTP requests will be read
     * @param outputStream stream which will be used to output the HTTP 
     * responses
     * @throws IOException error during an I/O operation
     */
    public void process(InputStream input, OutputStream output)
        throws IOException {

        // Setting up the I/O
        inputBuffer.setInputStream(input);
        outputBuffer.setOutputStream(output);

        // Error flag
        error = false;
        keepAlive = true;
        // TEMP
        boolean stopped = false;

        while (!stopped && !error && keepAlive) {

            try {
                inputBuffer.parseRequestLine();
                // Check for HTTP/0.9
                
                inputBuffer.parseHeaders();
            } catch (EOFException e) {
                e.printStackTrace();
                error = true;
                break;
            } catch (InterruptedIOException e) {
                e.printStackTrace();
                //HttpServletResponse.SC_BAD_REQUEST
                error = true;
            } catch (Exception e) {
                e.printStackTrace();
                //SC_BAD_REQUEST
                error = true;
            }

            // Setting up filters, and parse some request headers
            prepareRequest();

            // Process the request in the adapter
            try {
                adapter.service(request, response);
            } catch (InterruptedIOException e) {
                e.printStackTrace();
                error = true;
            } catch (Throwable t) {
                // ISE
                t.printStackTrace();
                error = true;
            }

            // Finish the handling of the request
            try {
                outputBuffer.endRequest();
            } catch (IOException e) {
                e.printStackTrace();
                error = true;
            } catch (Throwable t) {
                // Problem ...
                t.printStackTrace();
                error = true;
            }

            // Next request
            inputBuffer.nextRequest();
            outputBuffer.nextRequest();

        }

        // Recycle
        inputBuffer.recycle();
        outputBuffer.recycle();

    }


    // ----------------------------------------------------- ActionHook Methods


    /**
     * Send an action to the connector.
     * 
     * @param actionCode Type of the action
     * @param param Action parameter
     */
    public void action(ActionCode actionCode, Object param) {

        if (actionCode == ActionCode.ACTION_COMMIT) {

            // Commit current response

            // Validate and write response headers
            prepareResponse();
            try {
                outputBuffer.commit();
            } catch (IOException e) {
                // Log the error, and set error flag
                e.printStackTrace();
                error = true;
            }

        } else if (actionCode == ActionCode.ACTION_ACK) {

            // Acknowlege request

            // Send a 100 status back if it makes sense (response not committed
            // yet, and client specified an expectation for 100-continue)

        } else if (actionCode == ActionCode.ACTION_CLOSE) {

            // Close

            // End the processing of the current request, and stop any further
            // transactions with the client

            try {
                outputBuffer.endRequest();
            } catch (IOException e) {
                // Log the error, and set error flag
                e.printStackTrace();
                error = true;
            }

        } else if (actionCode == ActionCode.ACTION_RESET) {

            // Reset response

            // Note: This must be called before the response is committed

        } else if (actionCode == ActionCode.ACTION_CUSTOM) {

            // Do nothing

        }

    }


    // ------------------------------------------------------ Connector Methods


    /**
     * Set the associated adapter.
     * 
     * @param adapter the new adapter
     */
    public void setAdapter(Adapter adapter) {
        this.adapter = adapter;
    }


    /**
     * Get the associated adapter.
     * 
     * @return the associated adapter
     */
    public Adapter getAdapter() {
        return adapter;
    }


    // ------------------------------------------------------ Protected Methods


    /**
     * After reading the request headers, we have to setup the request filters.
     */
    protected void prepareRequest() {

        boolean http11 = true;
        contentDelimitation = false;

        MessageBytes protocol = request.protocol();
        if (protocol.equals(Constants.HTTP_11)) {
            http11 = true;
        } else if (protocol.equals(Constants.HTTP_10)) {
            http11 = false;
            keepAlive = false;
        } else {
            // Unsupported protocol
            error = true;
            // Send 505; Unsupported HTTP version
            response.setStatus(505);
        }

        // Check connection header
        MessageBytes connectionValueMB = 
            request.getMimeHeaders().getValue("connection");
        if (connectionValueMB != null) {
            String connectionValue = 
                connectionValueMB.toString().toLowerCase().trim();
            // FIXME: This can be a comma separated list
            if (connectionValue.equals("close")) {
                keepAlive = false;
            } else if (connectionValue.equals("keep-alive")) {
                keepAlive = true;
            }
        }

        // Check user-agent header
        MessageBytes userAgentValueMB =  
            request.getMimeHeaders().getValue("user-agent");
        // Check in the restricted list, and adjust the http11 
        // and keepAlive flags accordingly
        if ((restrictedUserAgents != null) && ((http11) || (keepAlive))) {
            String userAgentValue = userAgentValueMB.toString();
            for (int i = 0; i < restrictedUserAgents.length; i++) {
                if (restrictedUserAgents[i].equals(userAgentValue)) {
                    http11 = false;
                    keepAlive = false;
                }
            }
        }

        InputFilter[] inputFilters = inputBuffer.getFilters();

        // Parse content-length header
        int contentLength = request.getContentLength();
        if (contentLength != -1) {
            inputBuffer.addActiveFilter
                (inputFilters[Constants.IDENTITY_FILTER]);
            contentDelimitation = true;
        }

        // Parse transfer-encoding header
        MessageBytes transferEncodingValueMB = null;
        if (http11)
            transferEncodingValueMB = 
                request.getMimeHeaders().getValue("transfer-encoding");
        if (transferEncodingValueMB != null) {
            String transferEncodingValue = transferEncodingValueMB.toString();
            // Parse the comma separated list. "identity" codings are ignored
            int startPos = 0;
            int commaPos = transferEncodingValue.indexOf(',');
            String encodingName = null;
            while (commaPos != -1) {
                encodingName = transferEncodingValue.substring
                    (startPos, commaPos).toLowerCase().trim();
                if (!addInputFilter(inputFilters, encodingName)) {
                    // Unsupported transfer encoding
                    error = true;
                    // Send 501; Unimplemented
                    response.setStatus(502);
                }
                startPos = commaPos + 1;
                commaPos = transferEncodingValue.indexOf(',', startPos);
            }
            encodingName = transferEncodingValue.substring(startPos)
                .toLowerCase().trim();
            if (!addInputFilter(inputFilters, encodingName)) {
                // Unsupported transfer encoding
                error = true;
                // Send 501; Unimplemented
                response.setStatus(502);
            }
        }

        // Check host header
        if (http11 && (request.getMimeHeaders().getValue("host") == null)) {
            error = true;
            // Send 400: Bad request
            response.setStatus(400);
        }

        if (!contentDelimitation)
            keepAlive = false;

    }


    /**
     * When committing the response, we have to validate the set of headers, as
     * well as setup the response filters.
     */
    protected void prepareResponse() {

        contentDelimitation = false;

        OutputFilter[] outputFilters = outputBuffer.getFilters();

        outputBuffer.addActiveFilter
            (outputFilters[Constants.IDENTITY_FILTER]);

        int contentLength = request.getContentLength();
        if (contentLength != -1) {
            contentDelimitation = true;
        }

        // Build the response header
        outputBuffer.sendStatus();

        MimeHeaders headers = response.getMimeHeaders();
        int size = headers.size();
        for (int i = 0; i < size; i++) {
            outputBuffer.sendHeader(headers.getName(i), headers.getValue(i));
        }
        outputBuffer.endHeaders();

        if (!contentDelimitation) {
            // Mark as close the connection after the request, and add the 
            // connection: close header
            keepAlive = false;
        }

    }


    /**
     * Initialize standard input and output filters.
     */
    protected void initializeFilters() {

        // Create and add the identity filters.
        inputBuffer.addFilter(new IdentityInputFilter());
        outputBuffer.addFilter(new IdentityOutputFilter());

        // Create and add the chunked filters.
        inputBuffer.addFilter(new ChunkedInputFilter());
        outputBuffer.addFilter(new ChunkedOutputFilter());

        // Create and add the void filters.
        inputBuffer.addFilter(new VoidInputFilter());
        outputBuffer.addFilter(new VoidOutputFilter());

    }


    /**
     * Add an input filter to the current request.
     * 
     * @return false if the encoding was not found (which would mean it is 
     * unsupported)
     */
    protected boolean addInputFilter(InputFilter[] inputFilters, 
                                     String encodingName) {
        if (encodingName.equals("identity")) {
            // Skip
        } else if (encodingName.equals("chunked")) {
            inputBuffer.addActiveFilter
                (inputFilters[Constants.CHUNKED_FILTER]);
            contentDelimitation = true;
        } else {
            for (int i = 2; i < inputFilters.length; i++) {
                if (inputFilters[i].getEncodingName()
                    .toString().equals(encodingName)) {
                    inputBuffer.addActiveFilter(inputFilters[i]);
                    return true;
                }
            }
            return false;
        }
        return true;
    }


}
