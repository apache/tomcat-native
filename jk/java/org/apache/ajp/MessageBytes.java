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
package org.apache.ajp;

import java.io.UnsupportedEncodingException;

/**
 * A cheap rip-off of MessageBytes from tomcat 3.x.
 *
 * The idea is simple:  delay expensive conversion from bytes -> String
 * until necessary.
 *
 * A <code>MessageBytes</code> object will contain a reference to an
 * array of bytes that it provides a view into, via an offset and
 * a length.
 *
 * @author Kevin Seguin [kseguin@yahoo.com]
 */
public class MessageBytes {

    private static final String DEFAULT_ENCODING = "ISO-8859-1";
    private int off;
    private int len;
    private byte[] bytes;
    private String str;
    private boolean gotStr;
    private String enc;

    /**
     * creates and uninitialized MessageBytes object
     */
    public MessageBytes() {
        recycle();
    }

    /**
     * recycles this object.
     */
    public void recycle() {
        off = 0;
        len = 0;
        bytes = null;
        str = null;
        gotStr = false;
        enc = DEFAULT_ENCODING;
    }

    /**
     * Set the bytes that will be represented by this object.
     * @param bytes the bytes that will be represented by this object.
     * @param off offset into <code>bytes</code>
     * @param len number of bytes from <code>offset</code> in
     *            <code>bytes</code> that this object will represent.
     */
    public void setBytes(byte[] bytes, int off, int len) {
        recycle();
        this.bytes = bytes;
        this.off = off;
        this.len = len;
    }

    /**
     * Get the byte array into which this object provides
     * a view.
     * @return the byte array into which this object provides
     *         a view. 
     */
    public byte[] getBytes() {
        return bytes;
    }

    /**
     * Get the offset into the byte array contained by this
     * object.
     * @return offset into contained array
     */
    public int getOffset() {
        return off;
    }

    /**
     * Get the number of bytes this object represents in the
     * contained byte array
     * @return number of bytes represented.
     */
    public int getLength() {
        return len;
    }

    /**
     * Set the encoding that will be used when (if) the represented
     * bytes are converted into a <code>String</code>.
     * @param enc encoding to use for conversion to string.
     */
    public void setEncoding(String enc) {
        this.enc = enc;
    }

    /**
     * Get the encoding that will be used when (if) the represented
     * bytes are converted into a <code>String</code>.
     * @return encoding that will be used for conversion to string.
     */
    public String getEncoding() {
        return enc;
    }

    /**
     * Set the string this object will represent.  Any contained
     * bytes will be ignored.
     * @param str string this object will represent.
     */
    public void setString(String str) {
        this.str = str;
        gotStr = true;
    }

    /**
     * Get the string this object represents.  If the underlying bytes
     * have not yet been converted to a string, they will be.  Any
     * encoding set with {@link #setEncoding} will be used, otherwise
     * the default encoding will be used (ISO-8859-1).
     * @return the string this object represents
     */
    public String getString() throws UnsupportedEncodingException {
        if (!gotStr) {
            if (bytes == null || len == 0) {
                setString(null);
            } else {
                setString(new String(bytes, off, len, enc));
            }
        }
        return str;
    }

    /**
     * Get the string this object represents.  If the underlying bytes
     * have not yet been converted to a string, they will be.  Any
     * encoding set with {@link #setEncoding} will be used, otherwise
     * the default encoding will be used (ISO-8859-1).
     * @return the string this object represents
     */
    public String toString() {
        try {
            return getString();
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException("root cause:  " + e.toString());
        }
    }
}
