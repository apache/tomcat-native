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
import org.apache.tomcat.util.buf.*;

/**
 * Moved from ByteChunk - code to convert from UTF8 bytes to chars.
 * Not used in the current tomcat3.3 : the performance gain is not very
 * big if the String is created, only if we avoid that and work only
 * on char[]. Until than, it's better to be safe. ( I tested this code
 * with 2 and 3 bytes chars, and it works fine in xerces )
 * 
 * Cut from xerces' UTF8Reader.copyMultiByteCharData() 
 *
 * @author Costin Manolache
 * @author ( Xml-Xerces )
 */
public final class UTF8Decoder extends B2CConverter {
    // may have state !!
    
    public UTF8Decoder() {

    }
    
    public void recycle() {
    }

    public void convert(ByteChunk mb, CharChunk cb )
	throws IOException
    {
	int bytesOff=mb.getOffset();
	int bytesLen=mb.getLength();
	byte bytes[]=mb.getBytes();
	
	int j=bytesOff;
	int end=j+bytesLen;

	while( j< end ) {
	    int b0=0xff & bytes[j];

	    if( (b0 & 0x80) == 0 ) {
		cb.append((char)b0);
		j++;
		continue;
	    }
	    
	    // 2 byte ?
	    if( j++ >= end ) {
		// ok, just ignore - we could throw exception
		throw new IOException( "Conversion error - EOF " );
	    }
	    int b1=0xff & bytes[j];
	    
	    // ok, let's the fun begin - we're handling UTF8
	    if ((0xe0 & b0) == 0xc0) { // 110yyyyy 10xxxxxx (0x80 to 0x7ff)
		int ch = ((0x1f & b0)<<6) + (0x3f & b1);
		if(debug>0)
		    log("Convert " + b0 + " " + b1 + " " + ch + ((char)ch));
		
		cb.append((char)ch);
		j++;
		continue;
	    }
	    
	    if( j++ >= end ) 
		return ;
	    int b2=0xff & bytes[j];
	    
	    if( (b0 & 0xf0 ) == 0xe0 ) {
		if ((b0 == 0xED && b1 >= 0xA0) ||
		    (b0 == 0xEF && b1 == 0xBF && b2 >= 0xBE)) {
		    if(debug>0)
			log("Error " + b0 + " " + b1+ " " + b2 );

		    throw new IOException( "Conversion error 2"); 
		}

		int ch = ((0x0f & b0)<<12) + ((0x3f & b1)<<6) + (0x3f & b2);
		cb.append((char)ch);
		if(debug>0)
		    log("Convert " + b0 + " " + b1+ " " + b2 + " " + ch +
			((char)ch));
		j++;
		continue;
	    }

	    if( j++ >= end ) 
		return ;
	    int b3=0xff & bytes[j];

	    if (( 0xf8 & b0 ) == 0xf0 ) {
		if (b0 > 0xF4 || (b0 == 0xF4 && b1 >= 0x90)) {
		    if(debug>0)
			log("Convert " + b0 + " " + b1+ " " + b2 + " " + b3);
		    throw new IOException( "Conversion error ");
		}
		int ch = ((0x0f & b0)<<18) + ((0x3f & b1)<<12) +
		    ((0x3f & b2)<<6) + (0x3f & b3);

		if(debug>0)
		    log("Convert " + b0 + " " + b1+ " " + b2 + " " + b3 + " " +
			ch + ((char)ch));

		if (ch < 0x10000) {
		    cb.append( (char)ch );
		} else {
		    cb.append((char)(((ch-0x00010000)>>10)+
						   0xd800));
		    cb.append((char)(((ch-0x00010000)&0x3ff)+
						   0xdc00));
		}
		j++;
		continue;
	    } else {
		// XXX Throw conversion exception !!!
		if(debug>0)
		    log("Convert " + b0 + " " + b1+ " " + b2 + " " + b3);
		throw new IOException( "Conversion error 4" );
	    }
	}
    }

    private static int debug=1;
    void log(String s ) {
	System.out.println("UTF8Decoder: " + s );
    }
    
}
