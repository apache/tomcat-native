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

/**
 * This class is used to represent a chunk of bytes, and
 * utilities to manipulate byte[].
 *
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
 *
 * @author dac@eng.sun.com
 * @author James Todd [gonzo@eng.sun.com]
 * @author Costin Manolache
 */
public final class ByteChunk implements Cloneable, Serializable {
    // byte[]
    private byte[] bytes;
    private int bytesOff;
    private int bytesLen;
    private String enc;
    private boolean isSet=false;
    
    /**
     * Creates a new, uninitialized ByteChunk object.
     */
    public ByteChunk() {
    }

    public ByteChunk getClone() {
	try {
	    return (ByteChunk)this.clone();
	} catch( Exception ex) {
	    return null;
	}
    }

    public boolean isNull() {
	return ! isSet; // bytes==null;
    }
    
    /**
     * Resets the message bytes to an uninitialized state.
     */
    public void recycle() {
	bytes = null;
	enc=null;
	isSet=false;
    }

    /**
     * Sets the message bytes to the specified subarray of bytes.
     * 
     * @param b the ascii bytes
     * @param off the start offset of the bytes
     * @param len the length of the bytes
     */
    public void setBytes(byte[] b, int off, int len) {
	bytes = b;
	bytesOff = off;
	bytesLen = len;
	isSet=true;
    }

    public void setEncoding( String enc ) {
	this.enc=enc;
    }

    // convert an int to byte[]
    public void setInt(int i) {
	// XXX TODO
    }
    // -------------------- Conversion and getters --------------------
    public static boolean isUTF8Compatible(String enc) {
	if( enc==null ) return true;
	// add known encodings
	return false;
    }
    
    public String toString() {
	if (null == bytes) {
	    return null;
	}
	String strValue=null;
	try {
	    if( enc==null )
		strValue=toStringUTF8();
	    else {
		strValue=new String(bytes, bytesOff, bytesLen, enc);
		// this will display when we implement I18N
		System.out.println("Converting from bytes to string using " +
				   enc + ":" + strValue  );
	    }
	    return strValue;
	} catch (java.io.UnsupportedEncodingException e) {
	    return null;  // can't happen
	}
    }

    private char[] conversionBuff;
    
    private String toStringUTF8() {
	if( conversionBuff==null || bytesLen > conversionBuff.length ) {
	    conversionBuff=new char[bytesLen];
	}

	// 	int j=bytesOff;
	// 	for( int i=0; i< bytesLen; i++ ) {
	// 	    conversionBuff[i]=(char)bytes[j++];
	// 	}
	int charsLen=byte2charUTF8();
	if( charsLen==-1) return null; // or throw exception
	
	return new String( conversionBuff, 0, charsLen);
    }


    private int byte2charUTF8() {
	if( conversionBuff==null || bytesLen > conversionBuff.length ) {
	    conversionBuff=new char[bytesLen];
	}
	// source: xerces' UTF8Reader.copyMultiByteCharData() 
	int j=bytesOff;
	int charOff=0;
	int end=j+bytesLen;
	while( j< end ) {
	    int b0=(int)bytes[j];
	    
	    if( b0 < 0x80 ) { 
		conversionBuff[charOff]=(char)b0;
		charOff++;
		j++;
		continue;
	    }
	    // 2 byte ?
	    if( j++ >= end ) {
		// ok, just ignore - we could throw exception
		return -1;
	    }
	    int b1=(int)bytes[j];
	    
	    // ok, let's the fun begin - we're handling UTF8
	    if ((0xe0 & b0) == 0xc0) { // 110yyyyy 10xxxxxx (0x80 to 0x7ff)
		int ch = ((0x1f & b0)<<6) + (0x3f & b1);
		conversionBuff[charOff]=(char)ch;
		charOff++;
		continue;
	    }
	    
	    if( j++ >= end ) 
		return -1;
	    int b2=(int)bytes[j];
	    
	    if( (b0 & 0xf0 ) == 0xe0 ) {
		if ((b0 == 0xED && b1 >= 0xA0) ||
		    (b0 == 0xEF && b1 == 0xBF && b2 >= 0xBE)) {
		    return -1;
		}

		int ch = ((0x0f & b0)<<12) + ((0x3f & b1)<<6) + (0x3f & b2);
		conversionBuff[charOff]=(char)ch;
		charOff++;
		continue;
	    }

	    if( j++ >= end ) 
		return -1;
	    int b3=(int)bytes[j];

	    if (( 0xf8 & b0 ) == 0xf0 ) {
		if (b0 > 0xF4 || (b0 == 0xF4 && b1 >= 0x90))
		    return -1;
		int ch = ((0x0f & b0)<<18) + ((0x3f & b1)<<12) +
		    ((0x3f & b2)<<6) + (0x3f & b3);

		if (ch < 0x10000) {
		    conversionBuff[charOff]=(char)ch;
		    charOff++;
		} else {
		    conversionBuff[charOff]=(char)(((ch-0x00010000)>>10)+
						   0xd800);
		    charOff++;
		    conversionBuff[charOff]=(char)(((ch-0x00010000)&0x3ff)+
						   0xdc00);
		    charOff++;
		}
		continue;
	    } else {
		return -1;
	    }
	}
	return charOff;
    }
    
    public int getInt()
    {
	return Ascii.parseInt(bytes, bytesOff,bytesLen);
    }

    // --------------------
    
    /**
     * Returns the message bytes.
     */
    public byte[] getBytes() {
	return bytes;
    }

    /**
     * Returns the start offset of the bytes.
     */
    public int getOffset() {
	return bytesOff;
    }

    /**
     * Returns the length of the bytes.
     */
    public int getLength() {
	return bytesLen;
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
	
	byte[] b = bytes;
	int blen = bytesLen;
	if (b == null || blen != s.length()) {
	    return false;
	}
	int boff = bytesOff;
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
	byte[] b = bytes;
	int blen = bytesLen;
	if (b == null || blen != s.length()) {
	    return false;
	}
	int boff = bytesOff;
	for (int i = 0; i < blen; i++) {
	    if (Ascii.toLower(b[boff++]) != Ascii.toLower(s.charAt(i))) {
		return false;
	    }
	}
	return true;
    }

    public boolean equals( ByteChunk bb ) {
	return equals( bb.getBytes(), bb.getOffset(), bb.getLength());
    }
    
    public boolean equals( byte b2[], int off2, int len2) {
	byte b1[]=bytes;
	if( b1==null && b2==null ) return true;

	int len=bytesLen;
	if ( len2 != len || b1==null || b2==null ) 
	    return false;
		
	int off1 = bytesOff;

	while ( len-- > 0) {
	    if (b1[off1++] != b2[off2++]) {
		return false;
	    }
	}
	return true;
    }

    public boolean equals( CharChunk cc ) {
	return equals( cc.getChars(), cc.getOffset(), cc.getLength());
    }
    
    public boolean equals( char c2[], int off2, int len2) {
	// XXX works only for enc compatible with ASCII/UTF !!!
	byte b1[]=bytes;
	if( c2==null && b1==null ) return true;
	
	if (b1== null || c2==null || bytesLen != len2 ) {
	    return false;
	}
	int off1 = bytesOff;
	int len=bytesLen;
	
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
	byte[] b = bytes;
	int blen = s.length();
	if (b == null || blen > bytesLen) {
	    return false;
	}
	int boff = bytesOff;
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
	byte[] b = bytes;
	int len = s.length();
	if (b == null || len+pos > bytesLen) {
	    return false;
	}
	int off = bytesOff+pos;
	for (int i = 0; i < len; i++) {
	    if (Ascii.toLower( b[off++] ) != Ascii.toLower( s.charAt(i))) {
		return false;
	    }
	}
	return true;
    }
    


    // based on ap_unescape_url ( util.c, Apache2.0 )
    public int unescapeURL()
    {
	int end=bytesOff+ bytesLen;
	int idx= indexOf( bytes, bytesOff, end, '%' );
	if( idx<0) return 0;

	for( int j=idx; j<end; j++, idx++ ) {
	    if( bytes[ j ] != '%' ) {
		bytes[idx]=bytes[j];
	    } else {
		// read next 2 digits
		if( j+2 >= end ) {
		    // invalid
		    return 400; // BAD_REQUEST
		}
		byte b1= bytes[j+1];
		byte b2=bytes[j+2];
		if( !isHexDigit( b1 ) || ! isHexDigit(b2 ))
		    return 400;
		
		j+=2;
		int res=x2c( b1, b2 );
		if( res=='/' || res=='\0' )
		    return 400;
		bytes[idx]=(byte)res;
		bytesLen-=2;
	    }
	}
	return 0;
    }

    public static boolean isHexDigit( int c ) {
	return ( ( c>='0' && c<='9' ) ||
		 ( c>='a' && c<='f' ) ||
		 ( c>='A' && c<='F' ));
    }
    
    public static int x2c( byte b1, byte b2 ) {
	int digit= (b1>='A') ? ( (b1 & 0xDF)-'A') + 10 :
	    (b1 -'0');
	digit*=16;
	digit +=(b2>='A') ? ( (b2 & 0xDF)-'A') + 10 :
	    (b2 -'0');
	return digit;
    }

    // -------------------- Hash code  --------------------

    // normal hash. 
    public int hash() {
	return hashBytes( bytes, bytesOff, bytesLen);
    }

    // hash ignoring case
    public int hashIgnoreCase() {
	return hashBytesIC( bytes, bytesOff, bytesLen );
    }

    private static int hashBytes( byte bytes[], int bytesOff, int bytesLen ) {
	int max=bytesOff+bytesLen;
	byte bb[]=bytes;
	int code=0;
	for (int i = bytesOff; i < max ; i++) {
	    code = code * 37 + bb[i];
	}
	return code;
    }

    private static int hashBytesIC( byte bytes[], int bytesOff,
				    int bytesLen )
    {
	int max=bytesOff+bytesLen;
	byte bb[]=bytes;
	int code=0;
	for (int i = bytesOff; i < max ; i++) {
	    code = code * 37 + Ascii.toLower(bb[i]);
	}
	return code;
    }

    /**
     * Returns true if the message bytes starts with the specified string.
     * @param s the string
     */
    public int indexOf(char c, int starting) {
	return indexOf( bytes, bytesOff+starting, bytesOff+bytesLen, c);
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
