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

package org.apache.tomcat.util.buf;

import java.text.*;
import java.util.*;
import java.io.Serializable;
import java.io.IOException;

/*
 * In a server it is very important to be able to operate on
 * the original byte[] without converting everything to chars.
 * Some protocols are ASCII only, and some allow different
 * non-UNICODE encodings. The encoding is not known beforehand,
 * and can even change during the execution of the protocol.
 * ( for example a multipart message may have parts with different
 *  encoding )
 *
 * For HTTP it is not very clear how the encoding of RequestURI
 * and mime values can be determined, but it is a great advantage
 * to be able to parse the request without converting to string.
 */


/**
 * This class is used to represent a chunk of bytes, and
 * utilities to manipulate byte[].
 *
 * The buffer can be modified and used for both input and output.
 *
 * @author dac@eng.sun.com
 * @author James Todd [gonzo@eng.sun.com]
 * @author Costin Manolache
 */
public final class ByteChunk implements Cloneable, Serializable {
    // Output interface, used when the buffer is filled.
    public static interface ByteOutputChannel {
	/** Send the bytes ( usually the internal conversion buffer ).
	 *  Expect 8k output if the buffer is full.
	 */
	public void realWriteBytes( byte cbuf[], int off, int len)
	    throws IOException;
    }

    // --------------------
    
    // byte[]
    private byte[] buff;

    private int start=0;
    private int end;

    private String enc;

    private boolean isSet=false; // XXX

    // How much can it grow, when data is added
    private int limit=-1;

    private ByteOutputChannel out=null;

    private boolean isOutput=false;
    
    /**
     * Creates a new, uninitialized ByteChunk object.
     */
    public ByteChunk() {
    }

    public ByteChunk( int initial ) {
	allocate( initial, -1 );
    }

    //--------------------
        public ByteChunk getClone() {
	try {
	    return (ByteChunk)this.clone();
	} catch( Exception ex) {
	    return null;
	}
    }

    public boolean isNull() {
	return ! isSet; // buff==null;
    }
    
    /**
     * Resets the message buff to an uninitialized state.
     */
    public void recycle() {
	//	buff = null;
	enc=null;
	start=0;
	end=0;
	isSet=false;
    }

    public void reset() {
	buff=null;
    }

    // -------------------- Setup --------------------

    public void allocate( int initial, int limit  ) {
	isOutput=true;
	if( buff==null || buff.length < initial ) {
	    buff=new byte[initial];
	}    
	this.limit=limit;
	start=0;
	end=0;
	isSet=true;
    }

    /**
     * Sets the message bytes to the specified subarray of bytes.
     * 
     * @param b the ascii bytes
     * @param off the start offset of the bytes
     * @param len the length of the bytes
     */
    public void setBytes(byte[] b, int off, int len) {
	buff = b;
	start = off;
	end = start+ len;
	isSet=true;
    }

    public void setEncoding( String enc ) {
	this.enc=enc;
    }

    /**
     * Returns the message bytes.
     */
    public byte[] getBytes() {
	return getBuffer();
    }

    /**
     * Returns the message bytes.
     */
    public byte[] getBuffer() {
	return buff;
    }

    /**
     * Returns the start offset of the bytes.
     * For output this is the end of the buffer.
     */
    public int getStart() {
	return start;
    }

    public int getOffset() {
	return getStart();
    }

    public void setOffset(int off) {
        if (end < off ) end=off;
	start=off;
    }

    /**
     * Returns the length of the bytes.
     * XXX need to clean this up
     */
    public int getLength() {
	return end-start;
    }

    /** Maximum amount of data in this buffer.
     *
     *  If -1 or not set, the buffer will grow undefinitely.
     *  Can be smaller than the current buffer size ( which will not shrink ).
     *  When the limit is reached, the buffer will be flushed ( if out is set )
     *  or throw exception.
     */
    public void setLimit(int limit) {
	this.limit=limit;
    }
    
    public int getLimit() {
	return limit;
    }

    /** When the buffer is full, write the data to the output channel.
     * 	Also used when large amount of data is appended.
     *
     *  If not set, the buffer will grow to the limit.
     */
    public void setByteOutputChannel(ByteOutputChannel out) {
	this.out=out;
    }

    public int getEnd() {
	return end;
    }

    public void setEnd( int i ) {
	end=i;
    }

    // -------------------- Adding data to the buffer --------------------
    public void append( char c )
	throws IOException
    {
	append( (byte)c);
    }

    public void append( byte b )
	throws IOException
    {
	makeSpace( 1 );

	// couldn't make space
	if( limit >0 && end >= limit ) {
	    flushBuffer();
	}
	buff[end++]=b;
    }

    public void append( ByteChunk src )
	throws IOException
    {
	append( src.getBytes(), src.getStart(), src.getLength());
    }

    /** Add data to the buffer
     */
    public void append( byte src[], int off, int len )
	throws IOException
    {
	// will grow, up to limit
	makeSpace( len );

	// if we don't have limit: makeSpace can grow as it wants
	if( limit < 0 ) {
	    // assert: makeSpace made enough space
	    System.arraycopy( src, off, buff, end, len );
	    end+=len;
	    return;
	}

	// if we have limit and we're below
	if( len <= limit - end ) {
	    // makeSpace will grow the buffer to the limit,
	    // so we have space
	    System.arraycopy( src, off, buff, end, len );
	    end+=len;
	    return;
	}

	// need more space than we can afford, need to flush
	// buffer

	// the buffer is already at ( or bigger than ) limit

	// Optimization:
	// If len-avail < length ( i.e. after we fill the buffer with
	// what we can, the remaining will fit in the buffer ) we'll just
	// copy the first part, flush, then copy the second part - 1 write
	// and still have some space for more. We'll still have 2 writes, but
	// we write more on the first.

	if( len + end < 2 * limit ) {
	    /* If the request length exceeds the size of the output buffer,
	       flush the output buffer and then write the data directly.
	       We can't avoid 2 writes, but we can write more on the second
	    */
	    int avail=limit-end;
	    System.arraycopy(src, off, buff, end, avail);
	    end += avail;

	    flushBuffer();

	    System.arraycopy(src, off+avail, buff, end, len - avail);
	    end+= len - avail;

	} else {	// len > buf.length + avail
	    // long write - flush the buffer and write the rest
	    // directly from source
	    flushBuffer();
	    
	    out.realWriteBytes( src, off, len );
	}
    }

    public void flushBuffer()
	throws IOException
    {
	//assert out!=null
	if( out==null ) {
	    throw new IOException( "Buffer overflow, no sink " + limit + " " +
				   buff.length  );
	}
	out.realWriteBytes( buff, start, end-start );
	end=start;
    }

    /** Make space for len chars. If len is small, allocate
     *	a reserve space too. Never grow bigger than limit.
     */
    private void makeSpace(int count)
    {
	byte[] tmp = null;

	int newSize;
	int desiredSize=end + count;

	// Can't grow above the limit
	if( limit > 0 &&
	    desiredSize > limit -start  ) {
	    desiredSize=limit -start;
	}

	if( buff==null ) {
	    if( desiredSize < 256 ) desiredSize=256; // take a minimum
	    buff=new byte[desiredSize];
	}
	
	// limit < buf.length ( the buffer is already big )
	// or we already have space XXX
	if( desiredSize < buff.length ) {
	    return;
	}
	// grow in larger chunks
	if( desiredSize < 2 * buff.length ) {
	    newSize= buff.length * 2;
	    if( limit >0 &&
		newSize > limit ) newSize=limit;
	    tmp=new byte[newSize];
	} else {
	    newSize= buff.length * 2 + count ;
	    if( limit > 0 &&
		newSize > limit ) newSize=limit;
	    tmp=new byte[newSize];
	}
	
	System.arraycopy(buff, start, tmp, 0, end-start);
	buff = tmp;
	tmp = null;
	start=0;
	end=end-start;
    }
    
    // -------------------- Conversion and getters --------------------

    public String toString() {
	if (null == buff) {
	    return null;
	}
	String strValue=null;
	try {
	    if( enc==null ) enc="UTF8";
	    return new String( buff, start, end-start, enc );
	    /*
	      Does not improve the speed too much on most systems,
	      it's safer to use the "clasical" new String().

	      Most overhead is in creating char[] and copying,
	      the internal implementation of new String() is very close to
	      what we do. The decoder is nice for large buffers and if
	      we don't go to String ( so we can take advantage of reduced GC)

	      // Method is commented out, in:
	      return B2CConverter.decodeString( enc );
	    */
	} catch (java.io.IOException e) {
	    return null;  // XXX 
	}
    }

    public int getInt()
    {
	return Ascii.parseInt(buff, start,end-start);
    }

    // -------------------- equals --------------------

    /**
     * Compares the message bytes to the specified String object.
     * @param s the String to compare
     * @return true if the comparison succeeded, false otherwise
     */
    public boolean equals(String s) {
	// XXX ENCODING - this only works if encoding is UTF8-compat
	// ( ok for tomcat, where we compare ascii - header names, etc )!!!
	
	byte[] b = buff;
	int blen = end-start;
	if (b == null || blen != s.length()) {
	    return false;
	}
	int boff = start;
	for (int i = 0; i < blen; i++) {
	    if (b[boff++] != s.charAt(i)) {
		return false;
	    }
	}
	return true;
    }

    /**
     * Compares the message bytes to the specified String object.
     * @param s the String to compare
     * @return true if the comparison succeeded, false otherwise
     */
    public boolean equalsIgnoreCase(String s) {
	byte[] b = buff;
	int blen = end-start;
	if (b == null || blen != s.length()) {
	    return false;
	}
	int boff = start;
	for (int i = 0; i < blen; i++) {
	    if (Ascii.toLower(b[boff++]) != Ascii.toLower(s.charAt(i))) {
		return false;
	    }
	}
	return true;
    }

    public boolean equals( ByteChunk bb ) {
	return equals( bb.getBytes(), bb.getStart(), bb.getLength());
    }
    
    public boolean equals( byte b2[], int off2, int len2) {
	byte b1[]=buff;
	if( b1==null && b2==null ) return true;

	int len=end-start;
	if ( len2 != len || b1==null || b2==null ) 
	    return false;
		
	int off1 = start;

	while ( len-- > 0) {
	    if (b1[off1++] != b2[off2++]) {
		return false;
	    }
	}
	return true;
    }

    public boolean equals( CharChunk cc ) {
	return equals( cc.getChars(), cc.getStart(), cc.getLength());
    }
    
    public boolean equals( char c2[], int off2, int len2) {
	// XXX works only for enc compatible with ASCII/UTF !!!
	byte b1[]=buff;
	if( c2==null && b1==null ) return true;
	
	if (b1== null || c2==null || end-start != len2 ) {
	    return false;
	}
	int off1 = start;
	int len=end-start;
	
	while ( len-- > 0) {
	    if ( (char)b1[off1++] != c2[off2++]) {
		return false;
	    }
	}
	return true;
    }

    /**
     * Returns true if the message bytes starts with the specified string.
     * @param s the string
     */
    public boolean startsWith(String s) {
	// Works only if enc==UTF
	byte[] b = buff;
	int blen = s.length();
	if (b == null || blen > end-start) {
	    return false;
	}
	int boff = start;
	for (int i = 0; i < blen; i++) {
	    if (b[boff++] != s.charAt(i)) {
		return false;
	    }
	}
	return true;
    }

    /**
     * Returns true if the message bytes starts with the specified string.
     * @param s the string
     */
    public boolean startsWithIgnoreCase(String s, int pos) {
	byte[] b = buff;
	int len = s.length();
	if (b == null || len+pos > end-start) {
	    return false;
	}
	int off = start+pos;
	for (int i = 0; i < len; i++) {
	    if (Ascii.toLower( b[off++] ) != Ascii.toLower( s.charAt(i))) {
		return false;
	    }
	}
	return true;
    }

    public int indexOf( String src, int srcOff, int srcLen, int myOff ) {
	char first=src.charAt( srcOff );

	// Look for first char 
	int srcEnd=srcOff + srcLen;
	
	for( int i=myOff; i< end - srcLen ; i++ ) {
	    if( buff[i] != first ) continue;
	    // found first char, now look for a match
	    int myPos=i+1;
	    for( int srcPos=srcOff; srcPos< srcEnd; ) {
		if( buff[myPos++] != src.charAt( srcPos++ ))
		    break;
		if( srcPos==srcEnd ) return i; // found it
	    }
	}
	return -1;
    }
    
    // -------------------- Hash code  --------------------

    // normal hash. 
    public int hash() {
	return hashBytes( buff, start, end-start);
    }

    // hash ignoring case
    public int hashIgnoreCase() {
	return hashBytesIC( buff, start, end-start );
    }

    private static int hashBytes( byte buff[], int start, int bytesLen ) {
	int max=start+bytesLen;
	byte bb[]=buff;
	int code=0;
	for (int i = start; i < max ; i++) {
	    code = code * 37 + bb[i];
	}
	return code;
    }

    private static int hashBytesIC( byte bytes[], int start,
				    int bytesLen )
    {
	int max=start+bytesLen;
	byte bb[]=bytes;
	int code=0;
	for (int i = start; i < max ; i++) {
	    code = code * 37 + Ascii.toLower(bb[i]);
	}
	return code;
    }

    /**
     * Returns true if the message bytes starts with the specified string.
     * @param s the string
     */
    public int indexOf(char c, int starting) {
	return indexOf( buff, start+starting, end, c);
    }

    public static int  indexOf( byte bytes[], int off, int end, char qq )
    {
	// Works only for UTF 
	while( off < end ) {
	    byte b=bytes[off];
	    if( b==qq )
		return off;
	    off++;
	}
	return -1;
    }

    /** Find a character, no side effects.
     *  @returns index of char if found, -1 if not
     */
    public static int findChar( byte buf[], int start, int end, char c ) {
	byte b=(byte)c;
	int offset = start;
	while (offset < end) {
	    if (buf[offset] == b) {
		return offset;
	    }
	    offset++;
	}
	return -1;
    }

    /** Find a character, no side effects.
     *  @returns index of char if found, -1 if not
     */
    public static int findChars( byte buf[], int start, int end, byte c[] ) {
	int clen=c.length;
	int offset = start;
	while (offset < end) {
	    for( int i=0; i<clen; i++ ) 
		if (buf[offset] == c[i]) {
		    return offset;
		}
	    offset++;
	}
	return -1;
    }

    /** Find the first character != c 
     *  @returns index of char if found, -1 if not
     */
    public static int findNotChars( byte buf[], int start, int end, byte c[] )
    {
	int clen=c.length;
	int offset = start;
	boolean found;
		
	while (offset < end) {
	    found=true;
	    for( int i=0; i<clen; i++ ) {
		if (buf[offset] == c[i]) {
		    found=false;
		    break;
		}
	    }
	    if( found ) { // buf[offset] != c[0..len]
		return offset;
	    }
	    offset++;
	}
	return -1;
    }


    
}
