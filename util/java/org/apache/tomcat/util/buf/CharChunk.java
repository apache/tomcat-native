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
 * Utilities to manipluate char chunks. While String is
 * the easiest way to manipulate chars ( search, substrings, etc),
 * it is known to not be the most efficient solution - Strings are
 * designed as imutable and secure objects.
 * 
 * @author dac@eng.sun.com
 * @author James Todd [gonzo@eng.sun.com]
 * @author Costin Manolache
 */
public final class CharChunk implements Cloneable, Serializable {
    // char[]
    private char chars[];
    private int charsOff;
    private int charsLen;
    private boolean isSet=false;    
    /**
     * Creates a new, uninitialized CharChunk object.
     */
    public CharChunk() {
    }

    public CharChunk getClone() {
	try {
	    return (CharChunk)this.clone();
	} catch( Exception ex) {
	    return null;
	}
    }

    public boolean isNull() {
	return !isSet;
    }
    
    /**
     * Resets the message bytes to an uninitialized state.
     */
    public void recycle() {
	//	chars=null;
	isSet=false;
    }

    public void setChars( char[] c, int off, int len ) {
	recycle();
	chars=c;
	charsOff=off;
	charsLen=len;
    }

    // -------------------- Conversion and getters --------------------

    public String toString() {
	if( chars==null ) return null;
	return new String( chars, charsOff, charsLen);
    }

    public int getInt()
    {
	return Ascii.parseInt(chars, charsOff,
				charsLen);
    }
    
    public char[] getChars()
    {
	return chars;
    }
    
    /**
     * Returns the start offset of the bytes.
     */
    public int getOffset() {
	return charsOff;
    }

    /**
     * Returns the length of the bytes.
     */
    public int getLength() {
	return charsLen;
    }

    // -------------------- equals --------------------

    /**
     * Compares the message bytes to the specified String object.
     * @param s the String to compare
     * @return true if the comparison succeeded, false otherwise
     */
    public boolean equals(String s) {
	char[] c = chars;
	int len = charsLen;
	if (c == null || len != s.length()) {
	    return false;
	}
	int off = charsOff;
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
	char[] c = chars;
	int len = charsLen;
	if (c == null || len != s.length()) {
	    return false;
	}
	int off = charsOff;
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
	char b1[]=chars;
	if( b1==null && b2==null ) return true;
	
	if (b1== null || b2==null || charsLen != len2) {
	    return false;
	}
	int off1 = charsOff;
	int len=charsLen;
	while ( len-- > 0) {
	    if (b1[off1++] != b2[off2++]) {
		return false;
	    }
	}
	return true;
    }

    public boolean equals(byte b2[], int off2, int len2) {
	char b1[]=chars;
	if( b2==null && b1==null ) return true;

	if (b1== null || b2==null || charsLen != len2) {
	    return false;
	}
	int off1 = charsOff;
	int len=charsLen;
	
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
	char[] c = chars;
	int len = s.length();
	if (c == null || len > charsLen) {
	    return false;
	}
	int off = charsOff;
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
	char[] c = chars;
	int len = s.length();
	if (c == null || len+pos > charsLen) {
	    return false;
	}
	int off = charsOff+pos;
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
	for (int i = charsOff; i < charsOff + charsLen; i++) {
	    code = code * 37 + chars[i];
	}
	return code;
    }

    // hash ignoring case
    public int hashIgnoreCase() {
	int code=0;
	for (int i = charsOff; i < charsOff + charsLen; i++) {
	    code = code * 37 + Ascii.toLower(chars[i]);
	}
	return code;
    }

    public int indexOf(char c) {
	return indexOf( c, charsOff);
    }
    
    /**
     * Returns true if the message bytes starts with the specified string.
     * @param s the string
     */
    public int indexOf(char c, int starting) {
	return indexOf( chars, charsOff+starting, charsOff+charsLen, c );
    }

    public static int indexOf( char chars[], int off, int end, char qq )
    {
	while( off < end ) {
	    char b=chars[off];
	    if( b==qq )
		return off;
	    off++;
	}
	return -1;
    }

    // based on ap_unescape_url ( util.c, Apache2.0 )
    public int unescapeURL()
    {
	int end=charsOff+ charsLen;
	int idx= indexOf( chars, charsOff, end, '%' );
	if( idx<0) return 0;

	for( int j=idx; j<end; j++, idx++ ) {
	    if( chars[ j ] != '%' ) {
		chars[idx]=chars[j];
	    } else {
		// read next 2 digits
		if( j+2 >= end ) {
		    // invalid
		    return 400; // BAD_REQUEST
		}
		char b1= chars[j+1];
		char b2=chars[j+2];
		if( !isHexDigit( b1 ) || ! isHexDigit(b2 ))
		    return 400;
		
		j+=2;
		int res=x2c( b1, b2 );
		if( res=='/' || res=='\0' )
		    return 400;
		chars[idx]=(char)res;
		charsLen-=2;
	    }
	}
	return 0;
    }

    public static boolean isHexDigit( int c ) {
	return ( ( c>='0' && c<='9' ) ||
		 ( c>='a' && c<='f' ) ||
		 ( c>='A' && c<='F' ));
    }
    
    public static int x2c( char b1, char b2 ) {
	int digit= (b1>='A') ? ( (b1 & 0xDF)-'A') + 10 :
	    (b1 -'0');
	digit*=16;
	digit +=(b2>='A') ? ( (b2 & 0xDF)-'A') + 10 :
	    (b2 -'0');
	return digit;
    }


    
    /**
     * This method decodes the given urlencoded string.
     *
     * @param  str the url-encoded string
     * @return the decoded string
     * @exception IllegalArgumentException If a '%' is not
     * followed by a valid 2-digit hex number.
     *
     * @author: cut & paste from JServ, much faster that previous tomcat impl 
     */
    public final static String unescapeURL(String str)
    {
	// old code
	//System.out.println("XXX old unescape URL "+ str);

        if (str == null)  return  null;
	
	// pay for what you use - unencoded requests will not get
	// less overhead
	// XXX this should be in the caller ?
	if( str.indexOf( '+' ) <0 && str.indexOf( '%' ) < 0 )
	    return str;
	
        StringBuffer dec = new StringBuffer();    // decoded string output
        int strPos = 0;
        int strLen = str.length();

        dec.ensureCapacity(str.length());
        while (strPos < strLen) {
            int laPos;        // lookahead position

            // look ahead to next URLencoded metacharacter, if any
            for (laPos = strPos; laPos < strLen; laPos++) {
                char laChar = str.charAt(laPos);
                if ((laChar == '+') || (laChar == '%')) {
                    break;
                }
            }

            // if there were non-metacharacters, copy them all as a block
            if (laPos > strPos) {
                dec.append(str.substring(strPos,laPos));
                strPos = laPos;
            }

            // shortcut out of here if we're at the end of the string
            if (strPos >= strLen) {
                break;
            }

            // process next metacharacter
            char metaChar = str.charAt(strPos);
            if (metaChar == '+') {
                dec.append(' ');
                strPos++;
                continue;
            } else if (metaChar == '%') {
		// We throw the original exception - the super will deal with
		// it
		//                try {
		dec.append((char)Integer.
			   parseInt(str.substring(strPos + 1, strPos + 3),16));
                strPos += 3;
            }
        }

        return dec.toString();
    }




}
