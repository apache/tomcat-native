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
import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.res.StringManager;

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

        filterLibrary = new OutputFilter[0];
        activeFilters = new OutputFilter[0];

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
     * Last valid byte.
     */
    protected int lastValid;


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
     * Filter library.
     * Note: Filter[0] is always the "chunked" filter.
     */
    protected OutputFilter[] filterLibrary;


    /**
     * Active filters (in order).
     */
    protected OutputFilter[] activeFilters;


    /**
     * Index of the last active filter.
     */
    protected int lastActiveFilter;


    /**
     * Chunk used when closing the response.
     */
    protected ByteChunk closeChunk = new ByteChunk(4096);


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

        // FIXME: Check for null ?

        OutputFilter[] newFilterLibrary = 
            new OutputFilter[filterLibrary.length + 1];
        for (int i = 0; i < filterLibrary.length; i++) {
            newFilterLibrary[i] = filterLibrary[i];
        }
        newFilterLibrary[filterLibrary.length] = filter;
        filterLibrary = newFilterLibrary;

        activeFilters = new OutputFilter[filterLibrary.length + 1];

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
        lastActiveFilter = 0;

    }


    /**
     * Add an output filter to the filter library.
     */
    public void addActiveFilter(OutputFilter filter) {

        // FIXME: Check for null ?
        // FIXME: Check index ?

        activeFilters[lastActiveFilter++] = filter;

    }


    // --------------------------------------------------------- Public Methods


    /**
     * Recycle the output buffer. This should be called when closing the 
     * connection.
     */
    public void recycle() {

        // FIXME: Recycle Request object (or do it elsewhere) ?

        outputStream = null;
        buf = headerBuffer;
        lastValid = 0;
        pos = 0;
        lastActiveFilter = 0;
        committed = false;
        closeChunk.recycle();

    }


    /**
     * End processing of current HTTP request.
     * Note: All bytes of the current request should have been already 
     * consumed. This method only resets all the pointers so that we are ready
     * to parse the next HTTP request.
     */
    public void nextRequest()
        throws IOException {

        // FIXME: Recycle Response object (or do it elsewhere) ?

        // Determine the header buffer used for next request
        buf = headerBuffer;

        // Recycle filters
        if (lastActiveFilter > 0) {
            for (int i = 0; i < lastActiveFilter; i++) {
                activeFilters[i].recycle();
            }
        }

        // Reset pointers
        lastValid = 0;
        pos = 0;
        lastActiveFilter = 0;
        committed = false;
        closeChunk.recycle();

    }


    /**
     * Send response header.
     */
    public void sendHeader()
        throws IOException {

        // FIXME

        committed = true;

    }


    /**
     * End request.
     */
    public void endRequest()
        throws IOException {

        if (lastActiveFilter > 0) {
            // Parsing through the filter list
            for (int i = 0; i < lastActiveFilter; i++) {
                activeFilters[i].close(closeChunk);
            }
        }

        if (closeChunk.getLength() > 0)
            outputStream.write(closeChunk.getBytes(), closeChunk.getStart(), 
                               closeChunk.getLength());

    }


    // --------------------------------------------------- OutputBuffer Methods


    public int doWrite(ByteChunk chunk) 
        throws IOException {

        if (!committed)
            sendHeader();

        int n = -1;

        if (lastActiveFilter > 0) {
            // Parsing through the filter list
            for (int i = 0; i < lastActiveFilter; i++) {
                n = activeFilters[i].doWrite(chunk);
            }
        }

        outputStream.write(chunk.getBytes(), chunk.getStart(), 
                           chunk.getLength());

        return n;

    }


}
