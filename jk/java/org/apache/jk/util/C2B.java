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
package org.apache.jk.util;

import org.apache.tomcat.util.buf.*;

import java.io.*;

// This is extended C2BConvertor. I need to add methods ( to convert strings )
// and it can't work with 3.3 ( since the old class will be loaded ).
// We should change the package name or find a different solution.

/** Efficient conversion of character to bytes.
 *  
 *  This uses the standard JDK mechansim - a writer - but provides mechanisms
 *  to recycle all the objects that are used. It is compatible with JDK1.1 and up,
 *  ( nio is better, but it's not available even in 1.2 or 1.3 )
 * 
 */
public final class C2B {
    private C2BIntermediateOutputStream ios;
    private C2BWriteConvertor conv;
    private ByteChunk bb;
    private String enc;
    
    /** Create a converter, with bytes going to a byte buffer
     */
    public C2B(ByteChunk output, String encoding) throws IOException {
	this.bb=output;
	ios=new C2BIntermediateOutputStream( output );
	conv=new C2BWriteConvertor( ios, encoding );
        this.enc=enc;
    }

    /** Create a converter
     */
    public C2B(String encoding) throws IOException {
	this( new ByteChunk(1024), encoding );
    }

    public ByteChunk getByteChunk() {
	return bb;
    }

    public String getEncoding() {
        return enc;
    }

    public void setByteChunk(ByteChunk bb) {
	this.bb=bb;
	ios.setByteChunk( bb );
    }

    /** Reset the internal state, empty the buffers.
     *  The encoding remain in effect, the internal buffers remain allocated.
     */
    public  final void recycle() {
	conv.recycle();
	bb.recycle();
    }

    /** Generate the bytes using the specified encoding
     */
    public  final void convert(char c[], int off, int len ) throws IOException {
	conv.write( c, off, len );
    }

    /** Generate the bytes using the specified encoding
     */
    public  final void convert(String s ) throws IOException {
	conv.write( s );
    }

    /** Generate the bytes using the specified encoding
     */
    public  final void convert(char c ) throws IOException {
	conv.write( c );
    }

    /** Convert a message bytes chars to bytes
     */
    public final void convert(MessageBytes mb ) throws IOException {
        int type=mb.getType();
        if( type==MessageBytes.T_BYTES )
            return;
        ByteChunk orig=bb;
        setByteChunk( mb.getByteChunk());
        bb.recycle();
        bb.allocate( 32, -1 );
        
        if( type==MessageBytes.T_STR ) {
            convert( mb.getString() );
            // System.out.println("XXX Converting " + mb.getString() );
        } else if( type==MessageBytes.T_CHARS ) {
            CharChunk charC=mb.getCharChunk();
            convert( charC.getBuffer(),
                                charC.getOffset(), charC.getLength());
            //System.out.println("XXX Converting " + mb.getCharChunk() );
        } else {
            System.out.println("XXX unknowon type " + type );
        }
        flushBuffer();
        //System.out.println("C2B: XXX " + bb.getBuffer() + bb.getLength()); 
        setByteChunk(orig);
    }

    /** Flush any internal buffers into the ByteOutput or the internal
     *  byte[]
     */
    public  final void flushBuffer() throws IOException {
	conv.flush();
    }

}

// -------------------- Private implementation --------------------



/**
 *  Special writer class, where close() is overritten. The default implementation
 *  would set byteOutputter to null, and the writter can't be recycled. 
 *
 *  Note that the flush method will empty the internal buffers _and_ call
 *  flush on the output stream - that's why we use an intermediary output stream
 *  that overrides flush(). The idea is to  have full control: flushing the
 *  char->byte converter should be independent of flushing the OutputStream.
 * 
 *  When a WriteConverter is created, it'll allocate one or 2 byte buffers,
 *  with a 8k size that can't be changed ( at least in JDK1.1 -> 1.4 ). It would
 *  also allocate a ByteOutputter or equivalent - again some internal buffers.
 *
 *  It is essential to keep  this object around and reuse it. You can use either
 *  pools or per thread data - but given that in most cases a converter will be
 *  needed for every thread and most of the time only 1 ( or 2 ) encodings will
 *  be used, it is far better to keep it per thread and eliminate the pool 
 *  overhead too.
 * 
 */
final class  C2BWriteConvertor extends OutputStreamWriter {
    // stream with flush() and close(). overriden.
    private C2BIntermediateOutputStream ios;
    
    // Has a private, internal byte[8192]
    
    /** Create a converter.
     */
    public C2BWriteConvertor( C2BIntermediateOutputStream out, String enc )
	throws UnsupportedEncodingException
    {
	super( out, enc );
	ios=out;
    }
    
    /** Overriden - will do nothing but reset internal state.
     */
    public  final void close() throws IOException {
	// NOTHING
	// Calling super.close() would reset out and cb.
    }
    
    /**
     *  Flush the characters only
     */ 
    public  final void flush() throws IOException {
	// Will flushBuffer and out()
	// flushBuffer put any remaining chars in the byte[] 
	super.flush();
    }
    
    public  final void write(char cbuf[], int off, int len) throws IOException {
	// will do the conversion and call write on the output stream
	super.write( cbuf, off, len );
    }
    
    /** Reset the buffer
     */
    public  final void recycle() {
	ios.disable();
	try {
	    //	    System.out.println("Reseting writer");
	    flush();
	} catch( Exception ex ) {
	    ex.printStackTrace();
	}
	ios.enable();
    }
    
}


/** Special output stream where close() is overriden, so super.close()
    is never called.
    
    This allows recycling. It can also be disabled, so callbacks will
    not be called if recycling the converter and if data was not flushed.
*/
final class C2BIntermediateOutputStream extends OutputStream {
    private ByteChunk tbuff;
    private boolean enabled=true;
    
    public C2BIntermediateOutputStream(ByteChunk tbuff) {
	this.tbuff=tbuff;
    }
    
    public  final void close() throws IOException {
	// shouldn't be called - we filter it out in writer
	throw new IOException("close() called - shouldn't happen ");
    }
    
    public  final void flush() throws IOException {
	// nothing - write will go directly to the buffer,
	// we don't keep any state
    }
    
    public  final  void write(byte cbuf[], int off, int len) throws IOException {
	// will do the conversion and call write on the output stream
	if( enabled ) {
	    tbuff.append( cbuf, off, len );
	}
    }
    
    public  final void write( int i ) throws IOException {
	throw new IOException("write( int ) called - shouldn't happen ");
    }

    // -------------------- Internal methods --------------------

    void setByteChunk( ByteChunk bb ) {
	tbuff=bb;
    }
    
    /** Temporary disable - this is used to recycle the converter without
     *  generating an output if the buffers were not flushed
     */
    final void disable() {
	enabled=false;
    }

    /** Reenable - used to recycle the converter
     */
    final void enable() {
	enabled=true;
    }
}
