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

import org.apache.coyote.InputBuffer;
import org.apache.coyote.Request;
import org.apache.coyote.http11.InputFilter;

/**
 * Identity input filter.
 * 
 * @author Remy Maucherat
 */
public class IdentityInputFilter implements InputBuffer {


    // -------------------------------------------------------------- Constants


    protected static final String ENCODING_NAME = "identity";
    protected static final ByteChunk ENCODING = new ByteChunk();


    // ----------------------------------------------------- Static Initializer


    static {
        ENCODING.setBytes(ENCODING_NAME.getBytes(), 0, ENCODING_NAME.length());
    }


    // ----------------------------------------------------- Instance Variables


    /**
     * Content length.
     */
    protected long contentLength = -1;


    /**
     * Remaining bytes.
     */
    protected long remaining = -1;


    /**
     * Next buffer in the pipeline.
     */
    protected InputBuffer buffer;


    // ------------------------------------------------------------- Properties


    /**
     * Get content length.
     */
    public long getContentLength() {
        return contentLength;
    }


    /**
     * Get remaining bytes.
     */
    public long getRemaining() {
        return remaining;
    }


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
    public int doRead(ByteChunk chunk)
        throws IOException {

        buffer.doRead(chunk);

        int result = chunk.getLength();

        if (result <= 0) {
            return -1;
        }

        if (contentLength > 0) {
            if (remaining > 0) {
                if (chunk.getLength() > remaining) {
                    // The chunk is longer than the number of bytes remaining
                    // in the body; changing the chunk length to the number
                    // of bytes remaining
                    chunk.setBytes(chunk.getBytes(), chunk.getStart(), 
                                   (int) remaining);
                    result = (int) remaining;
                    remaining = -1;
                } else {
                    remaining = remaining - result;
                }
            } else {
                // No more bytes left to be read : return -1 and clear the 
                // buffer
                chunk.recycle();
                result = -1;
            }
        } else {
            result = -1;
        }

        return result;

    }


    // ---------------------------------------------------- InputFilter Methods


    /**
     * Read the content length from the request.
     */
    public void setRequest(Request request) {
        contentLength = request.getContentLength();
        remaining = contentLength;
    }


    /**
     * End the current request.
     */
    public long end()
        throws IOException {

        // FIXME: Consume extra bytes.
        // FIXME: If too many bytes were read, return the amount.
        return 0;

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
        contentLength = -1;
        remaining = -1;
    }


    /**
     * Return the name of the associated encoding; Here, the value is 
     * "identity".
     */
    public ByteChunk getEncodingName() {
        return ENCODING;
    }


}
