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


package org.apache.coyote.tomcat4;

import java.io.IOException;

import javax.servlet.ServletInputStream;

import org.apache.coyote.Request;
import org.apache.tomcat.util.buf.ByteChunk;


/**
 * This class handles the readin input bytes, as well as the input buffering.
 * 
 * @author Remy Maucherat
 */
public class CoyoteInputStream extends ServletInputStream {


    // ----------------------------------------------------- Instance Variables


    private boolean closed = false;

    private Request coyoteRequest;

    private ByteChunk readChunk = new ByteChunk();

    /**
     * Current read position in the byte buffer.
     */
    private int pos = -1;
    private int end = -1;
    private byte[] readBuffer = null;


    // --------------------------------------------------------- Public Methods


    void setRequest(Request coyoteRequest) {
        this.coyoteRequest = coyoteRequest;
    }


    /**
     * Recycle the input stream.
     */
    void recycle() {
        closed = false;
        pos = -1;
        end = -1;
        readBuffer = null;
    }


    // --------------------------------------------- ServletInputStream Methods


    public int read()
        throws IOException {

        if (closed)
            throw new IOException("Stream closed");

        // Read additional bytes if none are available
        while (pos >= end) {
            if (readBytes() < 0)
                return -1;
        }

        return (readBuffer[pos++] & 0xFF);

    }

    public int available() throws IOException {
        if( pos < end ) return end-pos;
        return 0;
    }

    public int read(byte[] b) throws IOException {

        return read(b, 0, b.length);

    }


    public int read(byte[] b, int off, int len)
        throws IOException {

        if (closed)
            throw new IOException("Stream closed");

        // Read additional bytes if none are available
        while (pos >= end) {
            if (readBytes() < 0)
                return -1;
        }

        int n = -1;
        if ((end - pos) > len) {
            n = len;
        } else {
            n = end - pos;
        }

        System.arraycopy(readBuffer, pos, b, off, n);

        pos += n;
        return n;

    }


    public int readLine(byte[] b, int off, int len) throws IOException {
        return super.readLine(b, off, len);
    }


    /** 
     * Close the stream
     * Since we re-cycle, we can't allow the call to super.close()
     * which would permantely disable us.
     */
    public void close() {
        closed = true;
    }


    // ------------------------------------------------------ Protected Methods


    /**
     * Read bytes to the read chunk buffer.
     */
    protected int readBytes()
        throws IOException {

        int result = coyoteRequest.doRead(readChunk);
        if (result > 0) {
            readBuffer = readChunk.getBytes();
            end = readChunk.getEnd();
            pos = readChunk.getStart();
        }
        return result;

    }


}
