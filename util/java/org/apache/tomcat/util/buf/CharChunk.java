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

import java.io.IOException;
import java.io.Serializable;

/**
 * Utilities to manipluate char chunks. While String is
 * the easiest way to manipulate chars ( search, substrings, etc),
 * it is known to not be the most efficient solution - Strings are
 * designed as imutable and secure objects.
 * 
 * @author dac@sun.com
 * @author James Todd [gonzo@sun.com]
 * @author Costin Manolache
 * @author Remy Maucherat
 */
public final class CharChunk implements Cloneable, Serializable {

    // Input interface, used when the buffer is emptied.
    public static interface CharInputChannel {
        /** 
         * Read new bytes ( usually the internal conversion buffer ).
         * The implementation is allowed to ignore the parameters, 
         * and mutate the chunk if it wishes to implement its own buffering.
         */
        public int realReadChars(char cbuf[], int off, int len)
            throws IOException;
    }
    /**
     *  When we need more space we'll either
     *  grow the buffer ( up to the limit ) or send it to a channel.
     */
    public static interface CharOutputChannel {
	/** Send the bytes ( usually the internal conversion buffer ).
	 *  Expect 8k output if the buffer is full.
	 */
        public void realWriteChars(char cbuf[], int off, int len)
            throws IOException;
    }
    
    // -------------------- 
    // char[]
    private char buff[];

    private int start;
    private int end;

    private boolean isSet=false;  // XXX 

    private boolean isOutput=false;

    // -1: grow undefinitely
    // maximum amount to be cached
    private int limit=-1;

    private CharInputChannel in = null;
    private CharOutputChannel out = null;
    
    /**
     * Creates a new, uninitialized CharChunk object.
     */
    public CharChunk() {
    }

    public CharChunk(int size) {
	allocate( size, -1 );
    }

    // --------------------
    
    public CharChunk getClone() {
	try {
	    return (CharChunk)this.clone();
	} catch( Exception ex) {
	    return null;
	}
    }

    public boolean isNull() {
	if( end > 0 ) return false;
	return !isSet; //XXX 
    }
    
    /**
     * Resets the message bytes to an uninitialized state.
     */
    public void recycle() {
	//	buff=null;
	isSet=false; // XXX
	start=0;
	end=0;
    }

    public void reset() {
	buff=null;
    }

    // -------------------- Setup --------------------

    public void allocate( int initial, int limit  ) {
	isOutput=true;
	if( buff==null || buff.length < initial ) {
	    buff=new char[initial];
	}
	this.limit=limit;
	start=0;
	end=0;
	isOutput=true;
	isSet=true;
    }


    public void setChars( char[] c, int off, int len ) {
	recycle();
	isSet=true;
	buff=c;
	start=off;
	end=start + len;
	limit=end;
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

    /**
     * When the buffer is empty, read the data from the input channel.
     */
    public void setCharInputChannel(CharInputChannel in) {
        this.in = in;
    }

    /** When the buffer is full, write the data to the output channel.
     * 	Also used when large amount of data is appended.
     *
     *  If not set, the buffer will grow to the limit.
     */
    public void setCharOutputChannel(CharOutputChannel out) {
	this.out=out;
    }

    // compat 
    public char[] getChars()
    {
	return getBuffer();
    }
    
    public char[] getBuffer()
    {
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

    /**
     * Returns the start offset of the bytes.
     */
    public void setOffset(int off) {
	start=off;
    }

    /**
     * Returns the length of the bytes.
     */
    public int getLength() {
	return end-start;
    }


    public int getEnd() {
	return end;
    }

    public void setEnd( int i ) {
	end=i;
    }

    // -------------------- Adding data --------------------
    
    public void append( char b )
	throws IOException
    {
	makeSpace( 1 );

	// couldn't make space
	if( limit >0 && end >= limit ) {
	    flushBuffer();
	}
	buff[end++]=b;
    }
    
    public void append( CharChunk src )
	throws IOException
    {
	append( src.getBuffer(), src.getOffset(), src.getLength());
    }

    /** Add data to the buffer
     */
    public void append( char src[], int off, int len )
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

        // Optimize on a common case.
        // If the source is going to fill up all the space in buffer, may
        // as well write it directly to the output, and avoid an extra copy
        if ( len == limit && end == 0) {
            out.realWriteChars( src, off, len );
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
	    
	    out.realWriteChars( src, off, len );
	}
    }


    /** Add data to the buffer
     */
    public void append( StringBuffer sb )
	throws IOException
    {
	int len=sb.length();

	// will grow, up to limit
	makeSpace( len );

	// if we don't have limit: makeSpace can grow as it wants
	if( limit < 0 ) {
	    // assert: makeSpace made enough space
	    sb.getChars(0, len, buff, end );
	    end+=len;
	    return;
	}

	int off=0;
	int sbOff = off;
	int sbEnd = off + len;
	while (sbOff < sbEnd) {
	    int d = min(limit - end, sbEnd - sbOff);
	    sb.getChars( sbOff, sbOff+d, buff, end);
	    sbOff += d;
	    end += d;
	    if (end >= limit)
		flushBuffer();
	}
    }

    /** Append a string to the buffer
     */
    public void append(String s, int off, int len) throws IOException {
	if (s==null) return;
	
	// will grow, up to limit
	makeSpace( len );

	// if we don't have limit: makeSpace can grow as it wants
	if( limit < 0 ) {
	    // assert: makeSpace made enough space
	    s.getChars(off, off+len, buff, end );
	    end+=len;
	    return;
	}

	int sOff = off;
	int sEnd = off + len;
	while (sOff < sEnd) {
	    int d = min(limit - end, sEnd - sOff);
	    s.getChars( sOff, sOff+d, buff, end);
	    sOff += d;
	    end += d;
	    if (end >= limit)
		flushBuffer();
	}
    }
    
    // -------------------- Removing data from the buffer --------------------

    public int substract()
        throws IOException {

        if ((end - start) == 0) {
            if (in == null)
                return -1;
            int n = in.realReadChars(buff, 0, buff.length);
            if (n < 0)
                return -1;
        }

        return (buff[start++]);

    }

    public int substract(CharChunk src)
        throws IOException {

        if ((end - start) == 0) {
            if (in == null)
                return -1;
            int n = in.realReadChars( buff, 0, buff.length );
            if (n < 0)
                return -1;
        }

        int len = getLength();
        src.append(buff, start, len);
        start = end;
        return len;

    }

    public int substract( char src[], int off, int len )
        throws IOException {

        if ((end - start) == 0) {
            if (in == null)
                return -1;
            int n = in.realReadChars( buff, 0, buff.length );
            if (n < 0)
                return -1;
        }

        int n = len;
        if (len > getLength()) {
            n = getLength();
        }
        System.arraycopy(buff, start, src, off, n);
        start += n;
        return n;

    }


    public void flushBuffer()
	throws IOException
    {
	//assert out!=null
	if( out==null ) {
	    throw new IOException( "Buffer overflow, no sink " + limit + " " +
				   buff.length  );
	}
	out.realWriteChars( buff, start, end - start );
	end=start;
    }

    /** Make space for len chars. If len is small, allocate
     *	a reserve space too. Never grow bigger than limit.
     */
    private void makeSpace(int count)
    {
	char[] tmp = null;

	int newSize;
	int desiredSize=end + count;

	// Can't grow above the limit
	if( limit > 0 &&
	    desiredSize > limit -start  ) {
	    desiredSize=limit-start;
	}

	if( buff==null ) {
	    if( desiredSize < 256 ) desiredSize=256; // take a minimum
	    buff=new char[desiredSize];
	}

	// limit < buf.length ( the buffer is already big )
	// or we already have space XXX
	if( desiredSize <= buff.length ) {
	    return;
	}
	// grow in larger chunks
	if( desiredSize < 2 * buff.length ) {
	    newSize= buff.length * 2;
	    if( limit >0 &&
		newSize > limit ) newSize=limit;
	    tmp=new char[newSize];
	} else {
	    newSize= buff.length * 2 + count ;
	    if( limit > 0 &&
		newSize > limit ) newSize=limit;
	    tmp=new char[newSize];
	}
	
	System.arraycopy(buff, start, tmp, 0, end-start);
	buff = tmp;
	tmp = null;
	end=end-start;
	start=0;
    }
    
    // -------------------- Conversion and getters --------------------

    public String toString() {
	if( buff==null ) return null;
	return new String( buff, start, end-start);
    }

    public int getInt()
    {
	return Ascii.parseInt(buff, start,
				end-start);
    }
    
    // -------------------- equals --------------------

    /**
     * Compares the message bytes to the specified String object.
     * @param s the String to compare
     * @return true if the comparison succeeded, false otherwise
     */
    public boolean equals(String s) {
	char[] c = buff;
	int len = end-start;
	if (c == null || len != s.length()) {
	    return false;
	}
	int off = start;
	for (int i = 0; i < len; i++) {
	    if (c[off++] != s.charAt(i)) {
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
	char[] c = buff;
	int len = end-start;
	if (c == null || len != s.length()) {
	    return false;
	}
	int off = start;
	for (int i = 0; i < len; i++) {
	    if (Ascii.toLower( c[off++] ) != Ascii.toLower( s.charAt(i))) {
		return false;
	    }
	}
	return true;
    }

    public boolean equals(CharChunk cc) {
	return equals( cc.getChars(), cc.getOffset(), cc.getLength());
    }

    public boolean equals(char b2[], int off2, int len2) {
	char b1[]=buff;
	if( b1==null && b2==null ) return true;
	
	if (b1== null || b2==null || end-start != len2) {
	    return false;
	}
	int off1 = start;
	int len=end-start;
	while ( len-- > 0) {
	    if (b1[off1++] != b2[off2++]) {
		return false;
	    }
	}
	return true;
    }

    public boolean equals(byte b2[], int off2, int len2) {
	char b1[]=buff;
	if( b2==null && b1==null ) return true;

	if (b1== null || b2==null || end-start != len2) {
	    return false;
	}
	int off1 = start;
	int len=end-start;
	
	while ( len-- > 0) {
	    if ( b1[off1++] != (char)b2[off2++]) {
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
	char[] c = buff;
	int len = s.length();
	if (c == null || len > end-start) {
	    return false;
	}
	int off = start;
	for (int i = 0; i < len; i++) {
	    if (c[off++] != s.charAt(i)) {
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
	char[] c = buff;
	int len = s.length();
	if (c == null || len+pos > end-start) {
	    return false;
	}
	int off = start+pos;
	for (int i = 0; i < len; i++) {
	    if (Ascii.toLower( c[off++] ) != Ascii.toLower( s.charAt(i))) {
		return false;
	    }
	}
	return true;
    }
    

    // -------------------- Hash code  --------------------

    // normal hash. 
    public int hash() {
	int code=0;
	for (int i = start; i < start + end-start; i++) {
	    code = code * 37 + buff[i];
	}
	return code;
    }

    // hash ignoring case
    public int hashIgnoreCase() {
	int code=0;
	for (int i = start; i < end; i++) {
	    code = code * 37 + Ascii.toLower(buff[i]);
	}
	return code;
    }

    public int indexOf(char c) {
	return indexOf( c, start);
    }
    
    /**
     * Returns true if the message bytes starts with the specified string.
     * @param s the string
     */
    public int indexOf(char c, int starting) {
	int ret = indexOf( buff, start+starting, end, c );
	return (ret >= start) ? ret - start : -1;
    }

    public static int indexOf( char chars[], int off, int cend, char qq )
    {
	while( off < cend ) {
	    char b=chars[off];
	    if( b==qq )
		return off;
	    off++;
	}
	return -1;
    }
    

    public int indexOf( String src, int srcOff, int srcLen, int myOff ) {
	char first=src.charAt( srcOff );

	// Look for first char 
	int srcEnd = srcOff + srcLen;
        
	for( int i=myOff+start; i <= (end - srcLen); i++ ) {
	    if( buff[i] != first ) continue;
	    // found first char, now look for a match
            int myPos=i+1;
	    for( int srcPos=srcOff + 1; srcPos< srcEnd; ) {
                if( buff[myPos++] != src.charAt( srcPos++ ))
		    break;
                if( srcPos==srcEnd ) return i-start; // found it
	    }
	}
	return -1;
    }

    // -------------------- utils
    private int min(int a, int b) {
	if (a < b) return a;
	return b;
    }

}
