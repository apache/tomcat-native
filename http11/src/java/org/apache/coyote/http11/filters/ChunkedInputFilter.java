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

package org.apache.coyote.http11.filters;

import java.io.IOException;

import org.apache.tomcat.util.buf.ByteChunk;
import org.apache.tomcat.util.buf.HexUtils;

import org.apache.coyote.InputBuffer;
import org.apache.coyote.Request;
import org.apache.coyote.http11.Constants;
import org.apache.coyote.http11.InputFilter;

/**
 * Chunked input filter.
 * 
 * @author Remy Maucherat
 */
public class ChunkedInputFilter implements InputFilter {


    // -------------------------------------------------------------- Constants


    protected static final String ENCODING_NAME = "chunked";
    protected static final ByteChunk ENCODING = new ByteChunk();


    // ----------------------------------------------------- Static Initializer


    static {
        ENCODING.setBytes(ENCODING_NAME.getBytes(), 0, ENCODING_NAME.length());
    }


    // ----------------------------------------------------- Instance Variables


    /**
     * Next buffer in the pipeline.
     */
    protected InputBuffer buffer;


    /**
     * Number of bytes remaining in the current chunk.
     */
    protected int remaining = 0;


    /**
     * Position in the buffer.
     */
    protected int pos = 0;


    /**
     * Last valid byte in the buffer.
     */
    protected int lastValid = 0;


    /**
     * Read bytes buffer.
     */
    protected byte[] buf = null;


    /**
     * Byte chunk used to read bytes.
     */
    protected ByteChunk readChunk = new ByteChunk();


    /**
     * Flag set to true when the end chunk has been read.
     */
    protected boolean endChunk = false;

    /**
     * Flag set to true if the next call to doRead() must parse a CRLF pair
     * before doing anything else.
     */
    protected boolean needCRLFParse = false;

    // ------------------------------------------------------------- Properties


    // ---------------------------------------------------- InputBuffer Methods


    /**
     * Read bytes.
     * 
     * @return If the filter does request length control, this value is
     * significant; it should be the number of bytes consumed from the buffer,
     * up until the end of the current request body, or the buffer length, 
     * whichever is greater. If the filter does not do request body length
     * control, the returned value should be -1.
     */
    public int doRead(ByteChunk chunk, Request req)
        throws IOException {

        if (endChunk)
            return -1;

        if(needCRLFParse) {
            needCRLFParse = false;
            parseCRLF();
        }

        if (remaining <= 0) {
            if (!parseChunkHeader()) {
                throw new IOException("Invalid chunk");
            }
            if (endChunk) {
                parseEndChunk();
                return -1;
            }
        }

        int result = 0;

        if (pos >= lastValid) {
            readBytes();
        }

        if (remaining > (lastValid - pos)) {
            result = lastValid - pos;
            remaining = remaining - result;
            chunk.setBytes(buf, pos, result);
            pos = lastValid;
        } else {
            result = remaining;
            chunk.setBytes(buf, pos, remaining);
            pos = pos + remaining;
            remaining = 0;
            needCRLFParse = true;
        }

        return result;

    }


    // ---------------------------------------------------- InputFilter Methods


    /**
     * Read the content length from the request.
     */
    public void setRequest(Request request) {
    }


    /**
     * End the current request.
     */
    public long end()
        throws IOException {

        // Consume extra bytes : parse the stream until the end chunk is found
        while (doRead(readChunk, null) >= 0) {
        }

        // Return the number of extra bytes which were consumed
        return (lastValid - pos);

    }


    /**
     * Set the next buffer in the filter pipeline.
     */
    public void setBuffer(InputBuffer buffer) {
        this.buffer = buffer;
    }


    /**
     * Make the filter ready to process the next request.
     */
    public void recycle() {
        remaining = 0;
        pos = 0;
        lastValid = 0;
        endChunk = false;
    }


    /**
     * Return the name of the associated encoding; Here, the value is 
     * "identity".
     */
    public ByteChunk getEncodingName() {
        return ENCODING;
    }


    // ------------------------------------------------------ Protected Methods


    /**
     * Read bytes from the previous buffer.
     */
    protected int readBytes()
        throws IOException {

        int nRead = buffer.doRead(readChunk, null);
        pos = readChunk.getStart();
        lastValid = pos + nRead;
        buf = readChunk.getBytes();

        return nRead;

    }


    /**
     * Parse the header of a chunk.
     */
    protected boolean parseChunkHeader()
        throws IOException {

        int result = 0;
        boolean eol = false;
        int begin = pos;
        int end = begin;
        boolean readDigit = false;

        while (!eol) {

            if (pos >= lastValid) {
                if (readBytes() <= 0)
                    return false;
            }

            if (buf[pos] == Constants.CR) {
            } else if (buf[pos] == Constants.LF) {
                eol = true;
            } else {
                if (HexUtils.DEC[buf[pos]] != -1) {
                    if (!readDigit) {
                        readDigit = true;
                        begin = pos;
                    }
                    end = pos;
                }
            }

            pos++;

        }

        if (!readDigit)
            return false;

        int offset = 1;
        for (int i = end; i >= begin; i--) {
            int val = HexUtils.DEC[buf[i]];
            if (val == -1)
                return false;
            result = result + val * offset;
            offset = offset * 16;
        }

        if (result == 0)
            endChunk = true;

        remaining = result;
        if (remaining < 0)
            return false;

        return true;

    }


    /**
     * Parse CRLF at end of chunk.
     */
    protected boolean parseCRLF()
        throws IOException {

        boolean eol = false;

        while (!eol) {

            if (pos >= lastValid) {
                if (readBytes() <= 0)
                    throw new IOException("Invalid CRLF");
            }

            if (buf[pos] == Constants.CR) {
            } else if (buf[pos] == Constants.LF) {
                eol = true;
            } else {
                throw new IOException("Invalid CRLF");
            }

            pos++;

        }

        return true;

    }


    /**
     * Parse end chunk data.
     * FIXME: Handle trailers
     */
    protected boolean parseEndChunk()
        throws IOException {

        return parseCRLF(); // FIXME

    }


}
