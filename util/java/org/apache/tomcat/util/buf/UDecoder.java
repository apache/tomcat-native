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

/** 
 *  All URL decoding happens here. This way we can reuse, review, optimize
 *  without adding complexity to the buffers.
 *
 *  The conversion will modify the original buffer.
 * 
 *  @author Costin Manolache
 */
public final class UDecoder {
    
    public UDecoder() 
    {
    }

    /** URLDecode, will modify the source.
     */
    public void convert(ByteChunk mb)
	throws IOException
    {
	int start=mb.getOffset();
	byte buff[]=mb.getBytes();
	int end=mb.getEnd();

	int idx= mb.indexOf( buff, start, end, '%' );
	int idx2= mb.indexOf( buff, start, end, '+' );
	if( idx<0 && idx2<0 ) {
	    return;
	}
	
	if( idx2 >= 0 && idx2 < idx ) idx=idx2; 
	
	for( int j=idx; j<end; j++, idx++ ) {
	    if( buff[ j ] == '+' ) {
		buff[idx]= (byte)' ' ;
	    } else if( buff[ j ] != '%' ) {
		buff[idx]= buff[j];
	    } else {
		// read next 2 digits
		if( j+2 >= end ) {
		    throw new CharConversionException("EOF");
		}
		byte b1= buff[j+1];
		byte b2=buff[j+2];
		if( !isHexDigit( b1 ) || ! isHexDigit(b2 ))
		    throw new CharConversionException( "isHexDigit");
		
		j+=2;
		int res=x2c( b1, b2 );
		buff[idx]=(byte)res;
	    }
	}

	mb.setEnd( idx );

	return;
    }

    // -------------------- Additional methods --------------------
    // XXX What do we do about charset ????

    /** In-buffer processing - the buffer will be modified
     */
    public void convert( CharChunk mb )
	throws IOException
    {
	log( "Converting a char chunk ");
	int start=mb.getOffset();
	char buff[]=mb.getBuffer();
	int cend=mb.getEnd();

	int idx= mb.indexOf( buff, start, cend, '%' );
	int idx2= mb.indexOf( buff, start, cend, '+' );
	if( idx<0 && idx2<0 ) {
	    return;
	}
	
	if( idx2 >= 0 && idx2 < idx ) idx=idx2; 

	for( int j=idx; j<cend; j++, idx++ ) {
	    if( buff[ j ] == '+' ) {
		buff[idx]=( ' ' );
	    } else if( buff[ j ] != '%' ) {
		buff[idx]=buff[j];
	    } else {
		// read next 2 digits
		if( j+2 >= cend ) {
		    // invalid
		    throw new CharConversionException("EOF");
		}
		char b1= buff[j+1];
		char b2=buff[j+2];
		if( !isHexDigit( b1 ) || ! isHexDigit(b2 ))
		    throw new CharConversionException("isHexDigit");
		
		j+=2;
		int res=x2c( b1, b2 );
		buff[idx]=(char)res;
	    }
	}
	mb.setEnd( idx );
    }

    /** URLDecode, will modify the source
     */
    public void convert(MessageBytes mb)
	throws IOException
    {
	
	switch (mb.getType()) {
	case MessageBytes.T_STR:
	    String strValue=mb.toString();
	    if( strValue==null ) return;
	    mb.setString( convert( strValue ));
	    break;
	case MessageBytes.T_CHARS:
	    CharChunk charC=mb.getCharChunk();
	    convert( charC );
	    break;
	case MessageBytes.T_BYTES:
	    ByteChunk bytesC=mb.getByteChunk();
	    convert( bytesC );
	    break;
	}
    }

    // XXX Old code, needs to be replaced !!!!
    // 
    public final String convert(String str)
    {
        if (str == null)  return  null;
	
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



    private static boolean isHexDigit( int c ) {
	return ( ( c>='0' && c<='9' ) ||
		 ( c>='a' && c<='f' ) ||
		 ( c>='A' && c<='F' ));
    }
    
    private static int x2c( byte b1, byte b2 ) {
	int digit= (b1>='A') ? ( (b1 & 0xDF)-'A') + 10 :
	    (b1 -'0');
	digit*=16;
	digit +=(b2>='A') ? ( (b2 & 0xDF)-'A') + 10 :
	    (b2 -'0');
	return digit;
    }

    private static int x2c( char b1, char b2 ) {
	int digit= (b1>='A') ? ( (b1 & 0xDF)-'A') + 10 :
	    (b1 -'0');
	digit*=16;
	digit +=(b2>='A') ? ( (b2 & 0xDF)-'A') + 10 :
	    (b2 -'0');
	return digit;
    }

    private final static int debug=0;
    private static void log( String s ) {
	System.out.println("URLDecoder: " + s );
    }

}
