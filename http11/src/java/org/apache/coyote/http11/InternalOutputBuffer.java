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

import java.io.IOException;
import java.io.OutputStream;

import org.apache.tomcat.util.buf.ByteChunk;
import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.HttpMessages;
import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.res.StringManager;

import org.apache.coyote.ActionCode;
import org.apache.coyote.OutputBuffer;
import org.apache.coyote.Response;

/**
 * Output buffer.
 * 
 * @author <a href="mailto:remm@apache.org">Remy Maucherat</a>
 */
public class InternalOutputBuffer implements OutputBuffer {


    // -------------------------------------------------------------- Constants


    // ----------------------------------------------------------- Constructors


    /**
     * Default constructor.
     */
    public InternalOutputBuffer(Response response) {
        this(response, Constants.DEFAULT_HTTP_HEADER_BUFFER_SIZE);
    }


    /**
     * Alternate constructor.
     */
    public InternalOutputBuffer(Response response, int headerBufferSize) {

        this.response = response;
        headers = response.getMimeHeaders();

        headerBuffer = new byte[headerBufferSize];
        buf = headerBuffer;

        outputStreamOutputBuffer = new OutputStreamOutputBuffer();

        filterLibrary = new OutputFilter[0];
        activeFilters = new OutputFilter[0];
        lastActiveFilter = -1;

        committed = false;

    }


    // -------------------------------------------------------------- Variables


    /**
     * The string manager for this package.
     */
    protected static StringManager sm =
        StringManager.getManager(Constants.Package);


    // ----------------------------------------------------- Instance Variables


    /**
     * Associated Coyote response.
     */
    protected Response response;


    /**
     * Headers of the associated request.
     */
    protected MimeHeaders headers;


    /**
     * State.
     */
    protected boolean committed;


    /**
     * Pointer to the current read buffer.
     */
    protected byte[] buf;


    /**
     * Position in the buffer.
     */
    protected int pos;


    /**
     * HTTP header buffer.
     */
    protected byte[] headerBuffer;


    /**
     * Underlying output stream.
     */
    protected OutputStream outputStream;


    /**
     * Underlying output buffer.
     */
    protected OutputBuffer outputStreamOutputBuffer;


    /**
     * Filter library.
     * Note: Filter[0] is always the "chunked" filter.
     */
    protected OutputFilter[] filterLibrary;


    /**
     * Active filter (which is actually the top of the pipeline).
     */
    protected OutputFilter[] activeFilters;


    /**
     * Index of the last active filter.
     */
    protected int lastActiveFilter;


    // ------------------------------------------------------------- Properties


    /**
     * Set the underlying socket output stream.
     */
    public void setOutputStream(OutputStream outputStream) {

        // FIXME: Check for null ?

        this.outputStream = outputStream;

    }


    /**
     * Get the underlying socket output stream.
     */
    public OutputStream getOutputStream() {

        return outputStream;

    }


    /**
     * Add an output filter to the filter library.
     */
    public void addFilter(OutputFilter filter) {

        OutputFilter[] newFilterLibrary = 
            new OutputFilter[filterLibrary.length + 1];
        for (int i = 0; i < filterLibrary.length; i++) {
            newFilterLibrary[i] = filterLibrary[i];
        }
        newFilterLibrary[filterLibrary.length] = filter;
        filterLibrary = newFilterLibrary;

        activeFilters = new OutputFilter[filterLibrary.length];

    }


    /**
     * Get filters.
     */
    public OutputFilter[] getFilters() {

        return filterLibrary;

    }


    /**
     * Clear filters.
     */
    public void clearFilters() {

        filterLibrary = new OutputFilter[0];
        lastActiveFilter = -1;

    }


    /**
     * Add an output filter to the filter library.
     */
    public void addActiveFilter(OutputFilter filter) {

        if (lastActiveFilter == -1) {
            filter.setBuffer(outputStreamOutputBuffer);
        } else {
            for (int i = 0; i <= lastActiveFilter; i++) {
                if (activeFilters[i] == filter)
                    return;
            }
            filter.setBuffer(activeFilters[lastActiveFilter]);
        }

        activeFilters[++lastActiveFilter] = filter;

        filter.setResponse(response);

    }


    // --------------------------------------------------------- Public Methods


    /**
     * Reset current response.
     * 
     * @throws IllegalStateException if the response has already been committed
     */
    public void reset() {

        if (committed)
            throw new IllegalStateException(/*FIXME:Put an error message*/);

        // Recycle Request object
        response.recycle();

    }


    /**
     * Recycle the output buffer. This should be called when closing the 
     * connection.
     */
    public void recycle() {

        // Recycle Request object
        response.recycle();

        outputStream = null;
        buf = headerBuffer;
        pos = 0;
        lastActiveFilter = -1;
        committed = false;

    }


    /**
     * End processing of current HTTP request.
     * Note: All bytes of the current request should have been already 
     * consumed. This method only resets all the pointers so that we are ready
     * to parse the next HTTP request.
     */
    public void nextRequest() {

        // Recycle Request object
        response.recycle();

        // Determine the header buffer used for next request
        buf = headerBuffer;

        // Recycle filters
        for (int i = 0; i <= lastActiveFilter; i++) {
            activeFilters[i].recycle();
        }

        // Reset pointers
        pos = 0;
        lastActiveFilter = -1;
        committed = false;

    }


    /**
     * End request.
     * 
     * @throws IOException an undelying I/O error occured
     */
    public void endRequest()
        throws IOException {

        if (!committed) {

            // Send the connector a request for commit. The connector should
            // then validate the headers, send them (using sendHeader) and 
            // set the filters accordingly.
            response.action(ActionCode.ACTION_COMMIT, null);

        }

        if (lastActiveFilter != -1)
            activeFilters[lastActiveFilter].end();

    }


    // ------------------------------------------------ HTTP/1.1 Output Methods


    /**
     * Send an acknoledgement.
     */
    public void sendAck()
        throws IOException {

        if (!committed)
            outputStream.write(Constants.ACK);

    }


    /**
     * Send the response status line.
     */
    public void sendStatus() {

        // Write protocol name
        write("HTTP/1.1 ");

        // Write status code
        int status = response.getStatus();
	switch (status) {
	case 200:
            write("200");
	    break;
	case 400:
            write("400");
	    break;
	case 404:
            write("404");
	    break;
        default:
	    write(status);
	}

        write(" ");

        // Write message
        if (response.getMessage() == null) {
            write(HttpMessages.getMessage(status));
        } else {
            write(response.getMessage());
        }

        // End the response status line
        write(Constants.CRLF);

    }


    /**
     * Send a header.
     * 
     * @param name Header name
     * @param value Header value
     */
    public void sendHeader(MessageBytes name, MessageBytes value) {

        write(name);
        write(": ");
        write(value);
        write(Constants.CRLF);

    }


    /**
     * Send a header.
     * 
     * @param name Header name
     * @param value Header value
     */
    public void sendHeader(ByteChunk name, ByteChunk value) {

        write(name);
        write(": ");
        write(value);
        write(Constants.CRLF);

    }


    /**
     * Send a header.
     * 
     * @param name Header name
     * @param value Header value
     */
    public void sendHeader(String name, String value) {

        write(name);
        write(": ");
        write(value);
        write(Constants.CRLF);

    }


    /**
     * End the header block.
     */
    public void endHeaders() {

        write(Constants.CRLF);

    }


    // --------------------------------------------------- OutputBuffer Methods


    /**
     * Write the contents of a byte chunk.
     * 
     * @param chunk byte chunk
     * @return number of bytes written
     * @throws IOException an undelying I/O error occured
     */
    public int doWrite(ByteChunk chunk) 
        throws IOException {

        if (!committed) {

            // Send the connector a request for commit. The connector should
            // then validate the headers, send them (using sendHeaders) and 
            // set the filters accordingly.
            response.action(ActionCode.ACTION_COMMIT, null);

        }

        if (lastActiveFilter == -1)
            return outputStreamOutputBuffer.doWrite(chunk);
        else
            return activeFilters[lastActiveFilter].doWrite(chunk);

    }


    // ------------------------------------------------------ Protected Methods


    /**
     * Commit the response.
     * 
     * @throws IOException an undelying I/O error occured
     */
    protected void commit()
        throws IOException {

        if (pos > 0) {
            // Sending the response header buffer
            outputStream.write(buf, 0, pos);
            outputStream.flush(); // Is it really necessary ?
        }

        // The response is now committed
        committed = true;
        response.setCommitted(true);

    }


    /**
     * This method will write the contents of the specyfied message bytes 
     * buffer to the output stream, without filtering. This method is meant to
     * be used to write the response header.
     * 
     * @param mb data to be written
     */
    protected void write(MessageBytes mb) {

        mb.toBytes();
        ByteChunk bc = mb.getByteChunk();
        if (!bc.isNull()) {
            // Writing the byte chunk to the output buffer
            write(bc);
        } else {
            // Using toString
            write(mb.toString());
        }

    }


    /**
     * This method will write the contents of the specyfied message bytes 
     * buffer to the output stream, without filtering. This method is meant to
     * be used to write the response header.
     * 
     * @param bc data to be written
     */
    protected void write(ByteChunk bc) {

        // Writing the byte chunk to the output buffer
        System.arraycopy(bc.getBytes(), bc.getStart(), buf, pos,
                         bc.getLength());
        pos = pos + bc.getLength();

    }


    /**
     * This method will write the contents of the specyfied String to the 
     * output stream, without filtering. This method is meant to be used to 
     * write the response header.
     * 
     * @param s data to be written
     */
    protected void write(String s) {

        // From the Tomcat 3.3 HTTP/1.0 connector
        int len = s.length();
        for (int i = 0; i < len; i++) {
            char c = s.charAt (i);
            // Note:  This is clearly incorrect for many strings,
            // but is the only consistent approach within the current
            // servlet framework.  It must suffice until servlet output
            // streams properly encode their output.
            if ((c & 0xff00) != 0) {
                // High order byte must be zero
                //log("Header character is not iso8859_1, " +
                //"not supported yet: " + c, Log.ERROR ) ;
            }
            buf[pos++] = (byte) c;
        }

    }


    /**
     * This method will print the specified integer to the output stream, 
     * without filtering. This method is meant to be used to write the 
     * response header.
     * 
     * @param i data to be written
     */
    protected void write(int i) {

        write(String.valueOf(i));

    }


    // ----------------------------------- OutputStreamOutputBuffer Inner Class


    /**
     * This class is an output buffer which will write data to an output
     * stream.
     */
    protected class OutputStreamOutputBuffer 
        implements OutputBuffer {


        /**
         * Write chunk.
         */
        public int doWrite(ByteChunk chunk) 
            throws IOException {

            outputStream.write(chunk.getBuffer(), chunk.getStart(), 
                               chunk.getLength());
            return chunk.getLength();

        }


    }


}
