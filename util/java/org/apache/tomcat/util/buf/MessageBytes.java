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

/**
 * This class is used to represent a subarray of bytes in an HTTP message.
 * It represents all request/response elements. The byte/char conversions are
 * delayed and cached. Everything is recyclable.
 *
 * The object can represent a byte[], a char[], or a (sub) String. All
 * operations can be made in case sensitive mode or not.
 *
 * @author dac@eng.sun.com
 * @author James Todd [gonzo@eng.sun.com]
 * @author Costin Manolache
 */
public final class MessageBytes implements Cloneable, Serializable {
    // primary type ( whatever is set as original value )
    private int type = T_NULL;

    public static final int T_NULL = 0;
    /** getType() is T_STR if the the object used to create the MessageBytes
        was a String */
    public static final int T_STR  = 1;
    /** getType() is T_STR if the the object used to create the MessageBytes
        was a byte[] */ 
    public static final int T_BYTES = 2;
    /** getType() is T_STR if the the object used to create the MessageBytes
        was a char[] */ 
    public static final int T_CHARS = 3;

    private int hashCode=0;
    // did we computed the hashcode ? 
    private boolean hasHashCode=false;

    // Is the represented object case sensitive ?
    private boolean caseSensitive=true;

    // Internal objects to represent array + offset, and specific methods
    private ByteChunk byteC=new ByteChunk();
    private CharChunk charC=new CharChunk();
    
    // String
    private String strValue;
    // true if a String value was computed. Probably not needed,
    // strValue!=null is the same
    private boolean hasStrValue=false;

    /**
     * Creates a new, uninitialized MessageBytes object.
     * @deprecated. Use static newInstance() in order to allow
     * future hooks.
     */
    public MessageBytes() {
    }

    /** Construct a new MessageBytes instance
     */
    public static MessageBytes newInstance() {
	return factory.newInstance();
    }

    /** Configure the case sensitivity
     */
    public void setCaseSenitive( boolean b ) {
	caseSensitive=b;
    }

    public MessageBytes getClone() {
	try {
	    return (MessageBytes)this.clone();
	} catch( Exception ex) {
	    return null;
	}
    }

    public boolean isNull() {
//		should we check also hasStrValue ???
		return byteC.isNull() && charC.isNull() && ! hasStrValue;
	// bytes==null && strValue==null;
    }
    
    /**
     * Resets the message bytes to an uninitialized (NULL) state.
     */
    public void recycle() {
	type=T_NULL;
	byteC.recycle();
	charC.recycle();

	strValue=null;
	caseSensitive=true;

	hasStrValue=false;
	hasHashCode=false;
	hasIntValue=false;
	hasDateValue=false;	
    }


    /**
     * Sets the content to the specified subarray of bytes.
     *
     * @param b the ascii bytes
     * @param off the start offset of the bytes
     * @param len the length of the bytes
     */
    public void setBytes(byte[] b, int off, int len) {
	recycle(); // a new value is set, cached values must reset
	byteC.setBytes( b, off, len );
	type=T_BYTES;
    }

    /** Set the encoding. If the object was constructed from bytes[]. any
     *  previous conversion is reset.
     *  If no encoding is set, we'll use 8859-1.
     */
    public void setEncoding( String enc ) {
	if( !byteC.isNull() ) {
	    // if the encoding changes we need to reset the converion results
	    charC.recycle();
	    hasStrValue=false;
	}
	byteC.setEncoding(enc);
    }

    /** Sets the content to be a char[]
     */
    public void setChars( char[] c, int off, int len ) {
	recycle();
	charC.setChars( c, off, len );
	type=T_CHARS;
    }

    /** Remove the cached string value. Use it after a conversion on the
     *	byte[] or after the encoding is changed
     *  XXX Is this needed ?
     */
    public void resetStringValue() {
	if( type != T_STR ) {
	    // If this was cread as a byte[] or char[], we remove
	    // the old string value
	    hasStrValue=false;
	    strValue=null;
	}
    }

    /** Set the content to be a string
     */
    public void setString( String s ) {
	recycle();
        if (s == null)
            return;
	strValue=s;
	hasStrValue=true;
	type=T_STR;
    }

    // -------------------- Conversion and getters --------------------

    /** Compute the string value
     */
    public String toString() {
	if( hasStrValue ) return strValue;
	hasStrValue=true;
	
	switch (type) {
	case T_CHARS:
	    strValue=charC.toString();
	    return strValue;
	case T_BYTES:
	    strValue=byteC.toString();
	    return strValue;
	}
	return null;
    }

    //----------------------------------------
    /** Return the type of the original content. Can be
     *  T_STR, T_BYTES, T_CHARS or T_NULL
     */
    public int getType() {
	return type;
    }
    
    /**
     * Returns the byte chunk, representing the byte[] and offset/length.
     * Valid only if T_BYTES or after a conversion was made.
     */
    public ByteChunk getByteChunk() {
	return byteC;
    }

    /**
     * Returns the char chunk, representing the char[] and offset/length.
     * Valid only if T_CHARS or after a conversion was made.
     */
    public CharChunk getCharChunk() {
	return charC;
    }

    /**
     * Returns the string value.
     * Valid only if T_STR or after a conversion was made.
     */
    public String getString() {
	return strValue;
    }

    /** Unimplemented yet. Do a char->byte conversion.
     */
    public void toBytes() {
	// XXX todo - not used yet
    }

    /** Convert to char[] and fill the CharChunk.
     *  XXX Not optimized - it converts to String first.
     */
    public void toChars() {
	if( ! charC.isNull() ) {
	    return;
	}
	// inefficient
	toString();
	char cc[]=strValue.toCharArray();
	charC.setChars(cc, 0, cc.length);
	type=T_CHARS;
    }
    

    /**
     * Returns the length of the original buffer.
     * Note that the length in bytes may be different from the length
     * in chars.
     */
    public int getLength() {
	if(type==T_BYTES)
	    return byteC.getLength();
	if(type==T_CHARS) {
	    return charC.getLength();
	}
	if(type==T_STR)
	    return strValue.length();
	toString();
	if( strValue==null ) return 0;
	return strValue.length();
    }

    // -------------------- equals --------------------

    /**
     * Compares the message bytes to the specified String object.
     * @param s the String to compare
     * @return true if the comparison succeeded, false otherwise
     */
    public boolean equals(String s) {
	if( ! caseSensitive )
	    return equalsIgnoreCase( s );
	switch (type) {
	case T_STR:
	    if( strValue==null && s!=null) return false;
	    return strValue.equals( s );
	case T_CHARS:
	    return charC.equals( s );
	case T_BYTES:
	    return byteC.equals( s );
	default:
	    return false;
	}
    }

    /**
     * Compares the message bytes to the specified String object.
     * @param s the String to compare
     * @return true if the comparison succeeded, false otherwise
     */
    public boolean equalsIgnoreCase(String s) {
	switch (type) {
	case T_STR:
	    if( strValue==null && s!=null) return false;
	    return strValue.equalsIgnoreCase( s );
	case T_CHARS:
	    return charC.equalsIgnoreCase( s );
	case T_BYTES:
	    return byteC.equalsIgnoreCase( s );
	default:
	    return false;
	}
    }

    public boolean equals(MessageBytes mb) {
	switch (type) {
	case T_STR:
	    return mb.equals( strValue );
	}

	if( mb.type != T_CHARS &&
	    mb.type!= T_BYTES ) {
	    // it's a string or int/date string value
	    return equals( mb.toString() );
	}

	// mb is either CHARS or BYTES.
	// this is either CHARS or BYTES
	// Deal with the 4 cases ( in fact 3, one is simetric)
	
	if( mb.type == T_CHARS && type==T_CHARS ) {
	    return charC.equals( mb.charC );
	} 
	if( mb.type==T_BYTES && type== T_BYTES ) {
	    return byteC.equals( mb.byteC );
	}
	if( mb.type== T_CHARS && type== T_BYTES ) {
	    return byteC.equals( mb.charC );
	}
	if( mb.type== T_BYTES && type== T_CHARS ) {
	    return mb.byteC.equals( charC );
	}
	// can't happen
	return true;
    }

    
    /**
     * Returns true if the message bytes starts with the specified string.
     * @param s the string
     */
    public boolean startsWith(String s) {
	switch (type) {
	case T_STR:
	    return strValue.startsWith( s );
	case T_CHARS:
	    return charC.startsWith( s );
	case T_BYTES:
	    return byteC.startsWith( s );
	default:
	    return false;
	}
    }

    /**
     * Returns true if the message bytes starts with the specified string.
     * @param s the string
     */
    public boolean startsWithIgnoreCase(String s, int pos) {
	switch (type) {
	case T_STR:
	    if( strValue==null ) return false;
	    if( strValue.length() < pos + s.length() ) return false;
	    
	    for( int i=0; i<s.length(); i++ ) {
		if( Ascii.toLower( s.charAt( i ) ) !=
		    Ascii.toLower( strValue.charAt( pos + i ))) {
		    return false;
		}
	    }
	    return true;
	case T_CHARS:
	    return charC.startsWithIgnoreCase( s, pos );
	case T_BYTES:
	    return byteC.startsWithIgnoreCase( s, pos );
	default:
	    return false;
	}
    }

    

    // -------------------- Hash code  --------------------
    public  int hashCode() {
	if( hasHashCode ) return hashCode;
	int code = 0;

	if( caseSensitive ) 
	    code=hash(); 
	else
	    code=hashIgnoreCase();
	hashCode=code;
	hasHashCode=true;
	return code;
    }

    // normal hash. 
    private int hash() {
	int code=0;
	switch (type) {
	case T_STR:
	    // We need to use the same hash function
	    for (int i = 0; i < strValue.length(); i++) {
		code = code * 37 + strValue.charAt( i );
	    }
	    return code;
	case T_CHARS:
	    return charC.hash();
	case T_BYTES:
	    return byteC.hash();
	default:
	    return 0;
	}
    }

    // hash ignoring case
    private int hashIgnoreCase() {
	int code=0;
	switch (type) {
	case T_STR:
	    for (int i = 0; i < strValue.length(); i++) {
		code = code * 37 + Ascii.toLower(strValue.charAt( i ));
	    }
	    return code;
	case T_CHARS:
	    return charC.hashIgnoreCase();
	case T_BYTES:
	    return byteC.hashIgnoreCase();
	default:
	    return 0;
	}
    }

    public int indexOf(char c) {
	return indexOf( c, 0);
    }

    // Inefficient initial implementation. Will be replaced on the next
    // round of tune-up
    public int indexOf(String s, int starting) {
	toString();
	return strValue.indexOf( s, starting );
    }
    
    // Inefficient initial implementation. Will be replaced on the next
    // round of tune-up
    public int indexOf(String s) {
	return indexOf( s, 0 );
    }
    
    public int indexOfIgnoreCase(String s, int starting) {
	toString();
	String upper=strValue.toUpperCase();
	String sU=s.toUpperCase();
	return upper.indexOf( sU, starting );
    }
    
    /**
     * Returns true if the message bytes starts with the specified string.
     * @param s the string
     */
    public int indexOf(char c, int starting) {
	switch (type) {
	case T_STR:
	    return strValue.indexOf( c, starting );
	case T_CHARS:
	    return charC.indexOf( c, starting);
	case T_BYTES:
	    return byteC.indexOf( c, starting );
	default:
	    return -1;
	}
    }

    /** Copy the src into this MessageBytes, allocating more space if
     *  needed
     */
    public void duplicate( MessageBytes src ) throws IOException
    {
	switch( src.getType() ) {
	case MessageBytes.T_BYTES:
	    type=T_BYTES;
	    ByteChunk bc=src.getByteChunk();
	    byteC.allocate( bc.getLength(), -1 );
	    byteC.append( bc );
	    break;
	case MessageBytes.T_CHARS:
	    type=T_CHARS;
	    CharChunk cc=src.getCharChunk();
	    charC.allocate( cc.getLength(), -1 );
	    charC.append( cc );
	    break;
	case MessageBytes.T_STR:
	    type=T_STR;
	    String sc=src.getString();
	    this.setString( sc );
	    break;
	}
    }

    // -------------------- Deprecated code --------------------
    // efficient int and date
    // XXX used only for headers - shouldn't be
    // stored here.
    private int intValue;
    private boolean hasIntValue=false;
    private Date dateValue;
    private boolean hasDateValue=false;
    
    /**
     *  @deprecated The buffer are general purpose, caching for headers should
     *  be done in headers. The second parameter allows us to pass a date format
     * instance to avoid synchronization problems.
     */
    public void setTime(long t, DateFormat df) {
	// XXX replace it with a byte[] tool
	recycle();
	if( dateValue==null)
	    dateValue=new Date(t);
	else
	    dateValue.setTime(t);
	if( df==null )
	    strValue=DateTool.format1123(dateValue);
	else
	    strValue=DateTool.format1123(dateValue,df);
	hasStrValue=true;
	hasDateValue=true;
	type=T_STR;   
    }

    public void setTime(long t) {
	setTime( t, null );
    }

    /** Set the buffer to the representation of an int
     */
    public void setInt(int i) {
	recycle();
        byteC.allocate(16, 32);
        int current = i;
        byte[] buf = byteC.getBuffer();
        int start = 0;
        int end = 0;
        if (i == 0) {
            buf[end++] = (byte) '0';
        }
        if (i < 0) {
            current = -i;
            buf[end++] = (byte) '-';
        }
        while (current > 0) {
            int digit = current % 10;
            current = current / 10;
            buf[end++] = HexUtils.HEX[digit];
        }
        byteC.setEnd(end);
        // Inverting buffer
        end--;
        if (i < 0) {
            start++;
        }
        while (end > start) {
            byte temp = buf[start];
            buf[start] = buf[end];
            buf[end] = temp;
            start++;
            end--;
        }
        intValue=i;
        hasIntValue=true;
        type=T_BYTES;
    }


    /**
     *  @deprecated The buffer are general purpose, caching for headers should
     *  be done in headers
     */
    public  long getTime()
    {
     	if( hasDateValue ) {
	    if( dateValue==null) return -1;
	    return dateValue.getTime();
     	}
	
     	long l=DateTool.parseDate( this );
     	if( dateValue==null)
     	    dateValue=new Date(l);
     	else
     	    dateValue.setTime(l);
     	hasDateValue=true;
     	return l;
    }
    

    // Used for headers conversion
    /** Convert the buffer to an int, cache the value
     *  @deprecated The buffer are general purpose, caching for headers should
     *  be done in headers
     */ 
    public int getInt() 
    {
	if( hasIntValue )
	    return intValue;
	
	switch (type) {
	case T_BYTES:
	    intValue=byteC.getInt();
	    break;
	default:
	    intValue=Integer.parseInt(toString());
	}
	hasIntValue=true;
	return intValue;
    }

    // -------------------- Future may be different --------------------
    
    private static MessageBytesFactory factory=new MessageBytesFactory();

    public static void setFactory( MessageBytesFactory mbf ) {
	factory=mbf;
    }
    
    public static class MessageBytesFactory {
	protected MessageBytesFactory() {
	}
	public MessageBytes newInstance() {
	    return new MessageBytes();
	}
    }
}
