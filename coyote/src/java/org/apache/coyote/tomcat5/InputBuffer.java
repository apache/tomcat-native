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

package org.apache.coyote.tomcat5;

import java.io.InputStream;
import java.io.IOException;
import java.io.Reader;
import java.io.UnsupportedEncodingException;
import java.util.Hashtable;

import org.apache.tomcat.util.buf.B2CConverter;
import org.apache.tomcat.util.buf.ByteChunk;
import org.apache.tomcat.util.buf.CharChunk;

import org.apache.coyote.Request;


/**
 * The buffer used by Tomcat request. This is a derivative of the Tomcat 3.3
 * OutputBuffer, adapted to handle input instead of output. This allows 
 * complete recycling of the facade objects (the ServletInputStream and the
 * BufferedReader).
 *
 * @author Remy Maucherat
 */
public class InputBuffer extends Reader
    implements ByteChunk.ByteInputChannel, CharChunk.CharInputChannel,
               CharChunk.CharOutputChannel {


    // -------------------------------------------------------------- Constants


    public static final String DEFAULT_ENCODING = 
        org.apache.coyote.Constants.DEFAULT_CHARACTER_ENCODING;
    public static final int DEFAULT_BUFFER_SIZE = 8*1024;
    static final int debug = 0;


    // The buffer can be used for byte[] and char[] reading
    // ( this is needed to support ServletInputStream and BufferedReader )
    public final int INITIAL_STATE = 0;
    public final int CHAR_STATE = 1;
    public final int BYTE_STATE = 2;


    // ----------------------------------------------------- Instance Variables


    /**
     * The byte buffer.
     */
    private ByteChunk bb;


    /**
     * The chunk buffer.
     */
    private CharChunk cb;


    /**
     * State of the output buffer.
     */
    private int state = 0;


    /**
     * Number of bytes read.
     */
    private int bytesRead = 0;


    /**
     * Number of chars read.
     */
    private int charsRead = 0;


    /**
     * Flag which indicates if the input buffer is closed.
     */
    private boolean closed = false;


    /**
     * Byte chunk used to input bytes.
     */
    private ByteChunk inputChunk = new ByteChunk();


    /**
     * Encoding to use.
     */
    private String enc;


    /**
     * Encoder is set.
     */
    private boolean gotEnc = false;


    /**
     * List of encoders.
     */
    protected Hashtable encoders = new Hashtable();


    /**
     * Current byte to char converter.
     */
    protected B2CConverter conv;


    /**
     * Associated Coyote request.
     */
    private Request coyoteRequest;


    /**
     * Buffer position.
     */
    private int markPos = -1;


    // ----------------------------------------------------------- Constructors


    /**
     * Default constructor. Allocate the buffer with the default buffer size.
     */
    public InputBuffer() {

        this(DEFAULT_BUFFER_SIZE);

    }


    /**
     * Alternate constructor which allows specifying the initial buffer size.
     * 
     * @param size Buffer size to use
     */
    public InputBuffer(int size) {

        bb = new ByteChunk(size);
        bb.setLimit(size);
        bb.setByteInputChannel(this);
        cb = new CharChunk(size);
        cb.setLimit(size);
        cb.setCharInputChannel(this);
        cb.setCharOutputChannel(this);

    }


    // ------------------------------------------------------------- Properties


    /**
     * Associated Coyote request.
     * 
     * @param coyoteRequest Associated Coyote request
     */
    public void setRequest(Request coyoteRequest) {
	this.coyoteRequest = coyoteRequest;
    }


    /**
     * Get associated Coyote request.
     * 
     * @return the associated Coyote request
     */
    public Request getRequest() {
        return this.coyoteRequest;
    }


    // --------------------------------------------------------- Public Methods


    /**
     * Recycle the output buffer.
     */
    public void recycle() {

	if (debug > 0)
            log("recycle()");

	state = INITIAL_STATE;
	bytesRead = 0;
	charsRead = 0;

        cb.recycle();
        markPos = -1;
        bb.recycle(); 
        closed = false;

        if (conv != null) {
            conv.recycle();
        }

        gotEnc = false;
        enc = null;

    }


    /**
     * Close the input buffer.
     * 
     * @throws IOException An underlying IOException occurred
     */
    public void close()
        throws IOException {
        closed = true;
    }


    public int available()
        throws IOException {
        if (state == BYTE_STATE) {
            return bb.getLength();
        } else if (state == CHAR_STATE) {
            return cb.getLength();
        } else {
            return 0;
        }
    }


    // ------------------------------------------------- Bytes Handling Methods


    /** 
     * Reads new bytes in the byte chunk.
     * 
     * @param buf Byte buffer to be written to the response
     * @param off Offset
     * @param cnt Length
     * 
     * @throws IOException An underlying IOException occurred
     */
    public int realReadBytes(byte cbuf[], int off, int len)
	throws IOException {

        if (debug > 2)
            log("realRead() " + coyoteRequest);

        if (closed)
            return -1;
        if (coyoteRequest == null)
            return -1;

        state = BYTE_STATE;

        int result = coyoteRequest.doRead(bb);

        return result;

    }


    public int readByte()
        throws IOException {
        return bb.substract();
    }


    public int read(byte[] b, int off, int len)
        throws IOException {
        return bb.substract(b, off, len);
    }


    // ------------------------------------------------- Chars Handling Methods


    /**
     * Since the converter will use append, it is possible to get chars to
     * be removed from the buffer for "writing". Since the chars have already
     * been read before, they are ignored. If a mark was set, then the
     * mark is lost.
     */
    public void realWriteChars(char c[], int off, int len) 
        throws IOException {
        markPos = -1;
    }


    public void setEncoding(String s) {
        enc = s;
    }


    public int realReadChars(char cbuf[], int off, int len)
        throws IOException {

        if (debug > 0)
            log("realRead() " + cb.getOffset() + " " + len);

        if (!gotEnc)
            setConverter();

        if (debug > 0)
            log("encoder:  " + conv + " " + gotEnc);

        if (bb.getLength() <= 0) {
            int nRead = realReadBytes(bb.getBytes(), 0, bb.getBytes().length);
            if (nRead < 0) {
                return -1;
            } else {
                bb.setBytes(bb.getBytes(), 0, nRead);
            }
        }

        if (markPos == -1) {
            cb.setChars(cb.getChars(), 0, 0);
        }

        conv.convert(bb, cb);
        state = CHAR_STATE;

        return cb.getLength();

    }


    public int read()
        throws IOException {
        return cb.substract();
    }


    public int read(char[] cbuf)
        throws IOException {
        return read(cbuf, 0, cbuf.length);
    }


    public int read(char[] cbuf, int off, int len)
        throws IOException {
        return cb.substract(cbuf, off, len);
    }


    public long skip(long n)
        throws IOException {

        if (n < 0) {
            throw new IllegalArgumentException();
        }

        long nRead = 0;
        while (nRead < n) {
            if (cb.getLength() > n) {
                cb.setOffset(cb.getStart() + (int) n);
                nRead = n;
            } else {
                nRead += cb.getLength();
                cb.setOffset(cb.getEnd());
                int nb = realReadChars(cb.getChars(), 0, cb.getEnd());
                if (nb < 0)
                    break;
            }
        }

        return nRead;

    }


    public boolean ready()
        throws IOException {
        return (cb.getLength() > 0);
    }


    public boolean markSupported() {
        return true;
    }


    public void mark(int readAheadLimit)
        throws IOException {
        cb.setLimit(cb.getEnd() + readAheadLimit);
        markPos = cb.getStart();
    }


    public void reset()
        throws IOException {
        bb.recycle();
        if (state == CHAR_STATE) {
            if (markPos < 0) {
                cb.recycle();
                markPos = -1;
                throw new IOException("Mark not set"); //FIXME: i18n
            }
        }
        /*
        cb.recycle();
        gotEnc = false;
        enc = null;
        */
    }


    protected void setConverter() {

        if (coyoteRequest != null)
            enc = coyoteRequest.getCharacterEncoding();

        if (debug > 0)
            log("Got encoding: " + enc);

        gotEnc = true;
        if (enc == null)
            enc = DEFAULT_ENCODING;
        conv = (B2CConverter) encoders.get(enc);
        if (conv == null) {
            try {
                conv = new B2CConverter(enc);
                encoders.put(enc, conv);
            } catch (IOException e) {
                conv = (B2CConverter) encoders.get(DEFAULT_ENCODING);
                if (conv == null) {
                    try {
                        conv = new B2CConverter(DEFAULT_ENCODING);
                        encoders.put(DEFAULT_ENCODING, conv);
                    } catch (IOException ex) {
                        // Ignore
                    }
                }
            }
        }

    }


    protected void log( String s ) {
	System.out.println("InputBuffer: " + s);
    }


}
