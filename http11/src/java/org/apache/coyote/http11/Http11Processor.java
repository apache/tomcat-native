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
import org.apache.tomcat.util.http.FastHttpDateFormat;
import org.apache.tomcat.util.http.MimeHeaders;

import org.apache.coyote.ActionHook;
import org.apache.coyote.ActionCode;
import org.apache.coyote.Adapter;
import org.apache.coyote.InputBuffer;
import org.apache.coyote.OutputBuffer;
import org.apache.coyote.Processor;
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
public class Http11Processor implements Processor, ActionHook {


    // ----------------------------------------------------------- Constructors


    /**
     * Default constructor.
     */
    public Http11Processor() {

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
     * State flag.
     */
    protected boolean started = false;


    /**
     * Error flag.
     */
    protected boolean error = false;


    /**
     * Keep-alive.
     */
    protected boolean keepAlive = true;


    /**
     * HTTP/1.1 flag.
     */
    protected boolean http11 = true;


    /**
     * HTTP/0.9 flag.
     */
    protected boolean http09 = false;


    /**
     * Content delimitator for the request (if false, the connection will
     * be closed at the end of the request).
     */
    protected boolean contentDelimitation = true;


    /**
     * List of restricted user agents.
     */
    protected String[] restrictedUserAgents = null;


    /**
     * Logger.
     */
    protected static org.apache.commons.logging.Log log 
        = org.apache.commons.logging.LogFactory.getLog(Http11Processor.class);


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

        while (started && !error && keepAlive) {

            try {
                inputBuffer.parseRequestLine();
                inputBuffer.parseHeaders();
            } catch (IOException e) {
                error = true;
                break;
            } catch (Exception e) {
                log.warn("Error parsing HTTP request", e);
                // 500 - Bad Request
                response.setStatus(400);
                error = true;
            }

            // Setting up filters, and parse some request headers
            prepareRequest();

            // Process the request in the adapter
            if (!error) {
                try {
                    adapter.service(request, response);
                } catch (InterruptedIOException e) {
                    error = true;
                } catch (Throwable t) {
                    log.error("Error processing request", t);
                    // 500 - Internal Server Error
                    response.setStatus(500);
                    error = true;
                }
            }

            // Finish the handling of the request
            try {
                inputBuffer.endRequest();
            } catch (IOException e) {
                error = true;
            } catch (Throwable t) {
                log.error("Error finishing request", t);
                // 500 - Internal Server Error
                response.setStatus(500);
                error = true;
            }
            try {
                outputBuffer.endRequest();
            } catch (IOException e) {
                error = true;
            } catch (Throwable t) {
                log.error("Error finishing response", t);
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
                // Set error flag
                error = true;
            }

        } else if (actionCode == ActionCode.ACTION_ACK) {

            // Acknowlege request

            // Send a 100 status back if it makes sense (response not committed
            // yet, and client specified an expectation for 100-continue)

            if (response.isCommitted())
                return;

            MessageBytes expectMB = 
                request.getMimeHeaders().getValue("expect");
            if ((expectMB != null)
                && (expectMB.indexOfIgnoreCase("100-continue", 0) != -1)) {
                try {
                    outputBuffer.sendAck();
                } catch (IOException e) {
                    // Set error flag
                    error = true;
                }
            }

        } else if (actionCode == ActionCode.ACTION_CLOSE) {

            // Close

            // End the processing of the current request, and stop any further
            // transactions with the client

            try {
                outputBuffer.endRequest();
            } catch (IOException e) {
                // Set error flag
                error = true;
            }

        } else if (actionCode == ActionCode.ACTION_RESET) {

            // Reset response

            // Note: This must be called before the response is committed

            outputBuffer.reset();

        } else if (actionCode == ActionCode.ACTION_CUSTOM) {

            // Do nothing

        } else if (actionCode == ActionCode.ACTION_START) {

            started = true;

        } else if (actionCode == ActionCode.ACTION_STOP) {

            started = false;

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

        http11 = true;
        http09 = false;
        contentDelimitation = false;

        MessageBytes protocolMB = request.protocol();
        if (protocolMB.equals(Constants.HTTP_11)) {
            http11 = true;
        } else if (protocolMB.equals(Constants.HTTP_10)) {
            http11 = false;
            keepAlive = false;
        } else if (protocolMB.equals("")) {
            // HTTP/0.9
            http09 = true;
            http11 = false;
            keepAlive = false;
        } else {
            // Unsupported protocol
            http11 = false;
            error = true;
            // Send 505; Unsupported HTTP version
            response.setStatus(505);
        }

        MessageBytes methodMB = request.method();

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

        // Check for a full URI (including protocol://host:port/)
        ByteChunk uriBC = request.requestURI().getByteChunk();
        if (uriBC.startsWithIgnoreCase("http", 0)) {

            // If the first character of the URI is 'h', the URI is either
            // invalid, or is a full URI

            int pos = uriBC.indexOf("://", 0, 3, 4);
            int uriBCStart = uriBC.getStart();
            int slashPos = -1;
            if (pos != -1) {
                byte[] uriB = uriBC.getBytes();
                slashPos = uriBC.indexOf('/', pos + 3);
                if (slashPos == -1) {
                    slashPos = uriBC.getLength();
                    // Set URI as "/"
                    request.requestURI().setBytes
                        (uriB, uriBCStart + pos + 1, 1);
                } else {
                    request.requestURI().setBytes
                        (uriB, uriBCStart + slashPos, 
                         uriBC.getLength() - slashPos);
                }
                MessageBytes hostMB = 
                    request.getMimeHeaders().setValue("host");
                hostMB.setBytes(uriB, uriBCStart + pos + 3, 
                                slashPos - pos - 3);
            }

        }

        // URI decoding
        try {
            request.decodedURI().duplicate(request.requestURI());
            request.getURLDecoder().convert(request.decodedURI(), true);
            request.decodedURI().setEncoding("UTF-8");
        } catch (IOException e) {
            // 500 - Internal Server Error
            response.setStatus(500);
            log.warn("Error decoding URI");
            error = true;
        }

        // Input filter setup
        InputFilter[] inputFilters = inputBuffer.getFilters();

        // Parse content-length header
        int contentLength = request.getContentLength();
        if (contentLength >= 0) {
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
                    response.setStatus(501);
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
                response.setStatus(501);
            }
        }

        // Check host header
        if (http11 && (request.getMimeHeaders().getValue("host") == null)) {
            error = true;
            // 400 - Bad request
            response.setStatus(400);
        }

        if (!contentDelimitation) {
            // If there's no content length and we're using HTTP/1.1, assume
            // the client is not broken and didn't send a body
            if (http11) {
                inputBuffer.addActiveFilter
                    (inputFilters[Constants.VOID_FILTER]);
                contentDelimitation = true;
            }
        }

        if (!contentDelimitation)
            keepAlive = false;

    }


    /**
     * When committing the response, we have to validate the set of headers, as
     * well as setup the response filters.
     */
    protected void prepareResponse() {

        boolean entityBody = true;
        contentDelimitation = false;

        OutputFilter[] outputFilters = outputBuffer.getFilters();

        if (http09 == true) {
            // HTTP/0.9
            outputBuffer.addActiveFilter
                (outputFilters[Constants.IDENTITY_FILTER]);
            return;
        }

        int statusCode = response.getStatus();
        if ((statusCode == 204) || (statusCode == 205) 
            || (statusCode == 304)) {
            // No entity body
            outputBuffer.addActiveFilter
                (outputFilters[Constants.VOID_FILTER]);
            entityBody = false;
        }

        MessageBytes methodMB = request.method();
        if (methodMB.equals("HEAD")) {
            // No entity body
            outputBuffer.addActiveFilter
                (outputFilters[Constants.VOID_FILTER]);
        }

        int contentLength = response.getContentLength();
        if (contentLength != -1) {
            outputBuffer.addActiveFilter
                (outputFilters[Constants.IDENTITY_FILTER]);
            contentDelimitation = true;
        } else {
            if (entityBody && http11) {
                outputBuffer.addActiveFilter
                    (outputFilters[Constants.CHUNKED_FILTER]);
                contentDelimitation = true;
                response.addHeader("Transfer-Encoding", "chunked");
            }
        }

        // Add date header
        response.addHeader("Date", FastHttpDateFormat.getCurrentDate());

        // Add server header
        response.addHeader("Server", Constants.SERVER);

        // Add transfer encoding header
        // FIXME

        if ((entityBody) && (!contentDelimitation)) {
            // Mark as close the connection after the request, and add the 
            // connection: close header
            keepAlive = false;
        }

        if (!keepAlive) {
            response.addHeader("Connection", "close");
        }

        // Build the response header
        outputBuffer.sendStatus();

        MimeHeaders headers = response.getMimeHeaders();
        int size = headers.size();
        for (int i = 0; i < size; i++) {
            outputBuffer.sendHeader(headers.getName(i), headers.getValue(i));
        }
        outputBuffer.endHeaders();

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
