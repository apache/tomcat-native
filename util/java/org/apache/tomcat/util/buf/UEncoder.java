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

import org.apache.tomcat.util.buf.*;
import java.util.BitSet;
import java.io.*;

/** Efficient implementation for encoders.
 *  This class is not thread safe - you need one encoder per thread.
 *  The encoder will save and recycle the internal objects, avoiding
 *  garbage.
 * 
 *  You can add extra characters that you want preserved, for example
 *  while encoding a URL you can add "/".
 *
 *  @author Costin Manolache
 */
public final class UEncoder {

    // Not static - the set may differ ( it's better than adding
    // an extra check for "/", "+", etc
    private BitSet safeChars=null;
    private C2BConverter c2b=null;
    private ByteChunk bb=null;

    private String encoding="UTF8";
    private static final int debug=0;
    
    public UEncoder() {
	initSafeChars();
    }

    public void setEncoding( String s ) {
	encoding=s;
    }

    public void addSafeCharacter( char c ) {
	safeChars.set( c );
    }


    /** URL Encode string, using a specified encoding.
     *  @param s string to be encoded
     *  @param enc character encoding, for chars >%80 ( use UTF8 if not set,
     *         as recommended in RFCs)
     *  @param reserved extra characters to preserve ( "/" - if s is a URL )
     */
    public void urlEncode( Writer buf, String s )
	throws IOException
    {
	if( c2b==null ) {
	    bb=new ByteChunk(16); // small enough.
	    c2b=new C2BConverter( bb, encoding );
	}

	for (int i = 0; i < s.length(); i++) {
	    int c = (int) s.charAt(i);
	    if( safeChars.get( c ) ) {
		if( debug > 0 ) log("Safe: " + (char)c);
		buf.write((char)c);
	    } else {
		if( debug > 0 ) log("Unsafe:  " + (char)c);
		c2b.convert( (char)c );
		
		// "surrogate" - UTF is _not_ 16 bit, but 21 !!!!
		// ( while UCS is 31 ). Amazing...
		if (c >= 0xD800 && c <= 0xDBFF) {
		    if ( (i+1) < s.length()) {
			int d = (int) s.charAt(i+1);
			if (d >= 0xDC00 && d <= 0xDFFF) {
			    if( debug > 0 ) log("Unsafe:  " + c);
			    c2b.convert( (char)d);
			    i++;
			}
		    }
		}

		c2b.flushBuffer();
		
		urlEncode( buf, bb.getBuffer(), bb.getOffset(),
			   bb.getLength() );
		bb.recycle();
	    }
	}
    }

    /**
     */
    public void urlEncode( Writer buf, byte bytes[], int off, int len)
	throws IOException
    {
	for( int j=off; j< len; j++ ) {
	    buf.write( '%' );
	    char ch = Character.forDigit((bytes[j] >> 4) & 0xF, 16);
	    if( debug > 0 ) log("Encode:  " + ch);
	    buf.write(ch);
	    ch = Character.forDigit(bytes[j] & 0xF, 16);
	    if( debug > 0 ) log("Encode:  " + ch);
	    buf.write(ch);
	}
    }



    // -------------------- Internal implementation --------------------
    
    // 
    private void init() {
	
    }
    
    private void initSafeChars() {
	safeChars=new BitSet(128);
	int i;
	for (i = 'a'; i <= 'z'; i++) {
	    safeChars.set(i);
	}
	for (i = 'A'; i <= 'Z'; i++) {
	    safeChars.set(i);
	}
	for (i = '0'; i <= '9'; i++) {
	    safeChars.set(i);
	}
	//safe
	safeChars.set('$');
	safeChars.set('-');
	safeChars.set('_');
	safeChars.set('.');

	// Dangerous: someone may treat this as " "
	// RFC1738 does allow it, it's not reserved
	//    safeChars.set('+');
	//extra
	safeChars.set('!');
	safeChars.set('*');
	safeChars.set('\'');
	safeChars.set('(');
	safeChars.set(')');
	safeChars.set(',');	
    }

    private static void log( String s ) {
	System.out.println("Encoder: " + s );
    }
}
