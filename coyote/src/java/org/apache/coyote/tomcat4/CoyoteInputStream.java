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
