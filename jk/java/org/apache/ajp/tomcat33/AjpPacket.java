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

package org.apache.ajp.tomcat33;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.util.Enumeration;

import org.apache.tomcat.core.*;
import org.apache.tomcat.util.*;
import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.HttpMessages;

/**
 * A single packet for communication between the web server and the
 * container.  Designed to be reused many times with no creation of
 * garbage.  Understands the format of data types for these packets.
 * Can be used (somewhat confusingly) for both incoming and outgoing
 * packets.  
 *
 * @author Dan Milstein [danmil@shore.net]
 * @author Keith Wannamaker [Keith@Wannamaker.org]
 */
public interface AjpPacket {
	
	public byte[] getBuff();

	public int getLen();
	
	public int getByteOff();

	public void setByteOff(int c);

	/** 
	 * Parse the packet header for a packet sent from the web server to
	 * the container.  Set the read position to immediately after
	 * the header.
	 *
	 * @return The length of the packet payload, as encoded in the
	 * header, or -1 if the packet doesn't have a valid header.  
	 */
	public int checkIn();
	
	/**
	 * Prepare this packet for accumulating a message from the container to
	 * the web server.  Set the write position to just after the header
	 * (but leave the length unwritten, because it is as yet unknown).  
	 */
	public void reset();
	
	/**
	 * For a packet to be sent to the web server, finish the process of
	 * accumulating data and write the length of the data payload into
	 * the header.  
	 */
	public void end();

	// ============ Data Writing Methods ===================

	/**
	 * Write an integer at an arbitrary position in the packet, but don't
	 * change the write position.
	 *
	 * @param bpos The 0-indexed position within the buffer at which to
	 * write the integer (where 0 is the beginning of the header).
	 * @param val The integer to write.
	 */
	public void setInt( int bPos, int val );

	public void appendInt( int val );
	
	public void appendByte( byte val );
	
	public void appendBool( boolean val);

	/**
	 * Write a String out at the current write position.  Strings are
	 * encoded with the length in two bytes first, then the string, and
	 * then a terminating \0 (which is <B>not</B> included in the
	 * encoded length).  The terminator is for the convenience of the C
	 * code, where it saves a round of copying.  A null string is
	 * encoded as a string with length 0.  
	 */
	public void appendString( String str );

	/** 
	 * Copy a chunk of bytes into the packet, starting at the current
	 * write position.  The chunk of bytes is encoded with the length
	 * in two bytes first, then the data itself, and finally a
	 * terminating \0 (which is <B>not</B> included in the encoded
	 * length).
	 *
	 * @param b The array from which to copy bytes.
	 * @param off The offset into the array at which to start copying
	 * @param len The number of bytes to copy.  
	 */
	public void appendBytes( byte b[], int off, int numBytes );

    /**
     * Write a 32 bits integer at an arbitrary position in the packet, but don't
     * change the write position.
     *
     * @param bpos The 0-indexed position within the buffer at which to
     * write the integer (where 0 is the beginning of the header).
     * @param val The integer to write.
     */
    public void appendLongInt( int val );

    /**
     * Copy a chunk of bytes into the packet, starting at the current
     * write position.  The chunk of bytes IS NOT ENCODED with ANY length
     * header.
     *
     * @param b The array from which to copy bytes.
     * @param off The offset into the array at which to start copying
     * @param len The number of bytes to copy.
     */
    public void appendXBytes(byte[] b, int off, int numBytes);
	
	// ============ Data Reading Methods ===================

	/**
	 * Read an integer from packet, and advance the read position past
	 * it.  Integers are encoded as two unsigned bytes with the
	 * high-order byte first, and, as far as I can tell, in
	 * little-endian order within each byte.  
	 */
	public int getInt();

	/**
	 * Read an integer from the packet, but don't advance the read
	 * position past it.  
	 */
	public int peekInt();

	public byte getByte();

	public byte peekByte();

	public boolean getBool();

	public void getMessageBytes( MessageBytes mb );

	public MessageBytes addHeader( MimeHeaders headers );
	
	/**
	 * Read a String from the packet, and advance the read position
	 * past it.  See appendString for details on string encoding.
	 **/
	public String getString() throws java.io.UnsupportedEncodingException;

	/**
	 * Copy a chunk of bytes from the packet into an array and advance
	 * the read position past the chunk.  See appendBytes() for details
	 * on the encoding.
	 *
	 * @return The number of bytes copied.
	 */
	public int getBytes(byte dest[]);

    /**
     * Read a 32 bits integer from packet, and advance the read position past
     * it.  Integers are encoded as four unsigned bytes with the
     * high-order byte first, and, as far as I can tell, in
     * little-endian order within each byte.
     */
    public int getLongInt();

    /**
     * Copy a chunk of bytes from the packet into an array and advance
     * the read position past the chunk.  See appendXBytes() for details
     * on the encoding.
     *
     * @return The number of bytes copied.
     */
    public int getXBytes(byte[] dest, int length);

    /**
     * Read a 32 bits integer from the packet, but don't advance the read
     * position past it.
     */
    public int peekLongInt();

	public void dump(String msg);
}
