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
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;

/** Efficient conversion of bytes  to character .
 *  
 *  This uses the standard JDK mechansim - a reader - but provides mechanisms
 *  to recycle all the objects that are used. It is compatible with JDK1.1
 *  and up,
 *  ( nio is better, but it's not available even in 1.2 or 1.3 )
 *
 *  Not used in the current code, the performance gain is not very big
 *  in the current case ( since String is created anyway ), but it will
 *  be used in a later version or after the remaining optimizations.
 */
public class B2CConverter {
    private IntermediateInputStream iis;
    private ReadConvertor conv;
    private String encoding;

    protected B2CConverter() {
    }
    
    /** Create a converter, with bytes going to a byte buffer
     */
    public B2CConverter(String encoding)
	throws IOException
    {
	this.encoding=encoding;
	reset();
    }

    
    /** Reset the internal state, empty the buffers.
     *  The encoding remain in effect, the internal buffers remain allocated.
     */
    public  void recycle() {
	conv.recycle();
    }

    static final int BUFFER_SIZE=8192;
    char result[]=new char[BUFFER_SIZE];

    /** Convert a buffer of bytes into a chars
     */
    public  void convert( ByteChunk bb, CharChunk cb )
	throws IOException
    {
	// Set the ByteChunk as input to the Intermediate reader
	iis.setByteChunk( bb );
	convert(cb);
    }

    private void convert(CharChunk cb)
	throws IOException
    {
	try {
	    // read from the reader
	    while( true ) { // conv.ready() ) {
		int cnt=conv.read( result, 0, BUFFER_SIZE );
		if( cnt <= 0 ) {
		    // End of stream ! - we may be in a bad state
		    if( debug>0)
			log( "EOF" );
		    //		    reset();
		    return;
		}
		if( debug > 1 )
		    log("Converted: " + new String( result, 0, cnt ));

		// XXX go directly
		cb.append( result, 0, cnt );
	    }
	} catch( IOException ex) {
	    if( debug>0)
		log( "Reseting the converter " + ex.toString() );
	    reset();
	    throw ex;
	}
    }

    public void reset()
	throws IOException
    {
	// destroy the reader/iis
	iis=new IntermediateInputStream();
	conv=new ReadConvertor( iis, encoding );
    }

    private final int debug=0;
    void log( String s ) {
	System.out.println("B2CConverter: " + s );
    }

    // -------------------- Not used - the speed improvemnt is quite small

    /*
    private Hashtable decoders;
    public static final boolean useNewString=false;
    public static final boolean useSpecialDecoders=true;
    private UTF8Decoder utfD;
    // private char[] conversionBuff;
    CharChunk conversionBuf;


    private  static String decodeString(ByteChunk mb, String enc)
	throws IOException
    {
	byte buff=mb.getBuffer();
	int start=mb.getStart();
	int end=mb.getEnd();
	if( useNewString ) {
	    if( enc==null) enc="UTF8";
	    return new String( buff, start, end-start, enc );
	}
	B2CConverter b2c=null;
	if( useSpecialDecoders &&
	    (enc==null || "UTF8".equalsIgnoreCase(enc))) {
	    if( utfD==null ) utfD=new UTF8Decoder();
	    b2c=utfD;
	}
	if(decoders == null ) decoders=new Hashtable();
	if( enc==null ) enc="UTF8";
	b2c=(B2CConverter)decoders.get( enc );
	if( b2c==null ) {
	    if( useSpecialDecoders ) {
		if( "UTF8".equalsIgnoreCase( enc ) ) {
		    b2c=new UTF8Decoder();
		}
	    }
	    if( b2c==null )
		b2c=new B2CConverter( enc );
	    decoders.put( enc, b2c );
	}
	if( conversionBuf==null ) conversionBuf=new CharChunk(1024);

	try {
	    conversionBuf.recycle();
	    b2c.convert( this, conversionBuf );
	    //System.out.println("XXX 1 " + conversionBuf );
	    return conversionBuf.toString();
	} catch( IOException ex ) {
	    ex.printStackTrace();
	    return null;
	}
    }

    */
}

// -------------------- Private implementation --------------------



/**
 * 
 */
final class  ReadConvertor extends InputStreamReader {
    // stream with flush() and close(). overriden.
    private IntermediateInputStream iis;
    
    // Has a private, internal byte[8192]
    
    /** Create a converter.
     */
    public ReadConvertor( IntermediateInputStream in, String enc )
	throws UnsupportedEncodingException
    {
	super( in, enc );
	iis=in;
    }
    
    /** Overriden - will do nothing but reset internal state.
     */
    public  final void close() throws IOException {
	// NOTHING
	// Calling super.close() would reset out and cb.
    }
    
    public  final int read(char cbuf[], int off, int len)
	throws IOException
    {
	// will do the conversion and call write on the output stream
	return super.read( cbuf, off, len );
    }
    
    /** Reset the buffer
     */
    public  final void recycle() {
    }
}


/** Special output stream where close() is overriden, so super.close()
    is never called.
    
    This allows recycling. It can also be disabled, so callbacks will
    not be called if recycling the converter and if data was not flushed.
*/
final class IntermediateInputStream extends InputStream {
    byte buf[];
    int pos;
    int len;
    int end;
    
    public IntermediateInputStream() {
    }
    
    public  final void close() throws IOException {
	// shouldn't be called - we filter it out in writer
	throw new IOException("close() called - shouldn't happen ");
    }
    
    public  final  int read(byte cbuf[], int off, int len) throws IOException {
	if( pos >= end ) return -1;
	if (pos + len > end) {
	    len = end - pos;
	}
	if (len <= 0) {
	    return 0;
	}
	System.arraycopy(buf, pos, cbuf, off, len);
	pos += len;
	return len;
    }
    
    public  final int read() throws IOException {
	return (pos < end ) ? (buf[pos++] & 0xff) : -1;
    }

    // -------------------- Internal methods --------------------

    void setBuffer( byte b[], int p, int l ) {
	buf=b;
	pos=p;
	len=l;
	end=pos+len;
    }

    void setByteChunk( ByteChunk mb ) {
	buf=mb.getBytes();
	pos=mb.getStart();
	len=mb.getLength();
	end=pos+len;
    }

}
