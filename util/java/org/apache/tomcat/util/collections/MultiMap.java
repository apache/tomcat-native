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

package org.apache.tomcat.util.collections;

import org.apache.tomcat.util.buf.MessageBytes;
import java.io.*;
import java.util.*;
import java.text.*;

// Originally MimeHeaders

/**
 * An efficient representation for certain type of map. The keys 
 * can have a single or multi values, but most of the time there are
 * single values.
 *
 * The data is of "MessageBytes" type, meaning bytes[] that can be
 * converted to Strings ( if needed, and encoding is lazy-binded ).
 *
 * This is a base class for MimeHeaders, Parameters and Cookies.
 *
 * Data structures: each field is a single-valued key/value.
 * The fields are allocated when needed, and are recycled.
 * The current implementation does linear search, in future we'll
 * also use the hashkey.
 * 
 * @author dac@eng.sun.com
 * @author James Todd [gonzo@eng.sun.com]
 * @author Costin Manolache
 */
public class MultiMap {

    protected Field[] fields;
    // fields in use
    protected int count;

    /**
     * 
     */
    public MultiMap(int initial_size) {
	fields=new Field[initial_size];
    }

    /**
     * Clears all header fields.
     */
    public void recycle() {
	for (int i = 0; i < count; i++) {
	    fields[i].recycle();
	}
	count = 0;
    }

    // -------------------- Idx access to headers ----------
    // This allows external iterators.
    
    /**
     * Returns the current number of header fields.
     */
    public int size() {
	return count;
    }

    /**
     * Returns the Nth header name
     * This may be used to iterate through all header fields.
     *
     * An exception is thrown if the index is not valid ( <0 or >size )
     */
    public MessageBytes getName(int n) {
	// n >= 0 && n < count ? headers[n].getName() : null
	return fields[n].name;
    }

    /**
     * Returns the Nth header value
     * This may be used to iterate through all header fields.
     */
    public MessageBytes getValue(int n) {
	return fields[n].value;
    }

    /** Find the index of a field with the given name.
     */
    public int find( String name, int starting ) {
	// We can use a hash - but it's not clear how much
	// benefit you can get - there is an  overhead 
	// and the number of headers is small (4-5 ?)
	// Another problem is that we'll pay the overhead
	// of constructing the hashtable

	// A custom search tree may be better
        for (int i = starting; i < count; i++) {
	    if (fields[i].name.equals(name)) {
                return i;
            }
        }
        return -1;
    }

    /** Find the index of a field with the given name.
     */
    public int findIgnoreCase( String name, int starting ) {
	// We can use a hash - but it's not clear how much
	// benefit you can get - there is an  overhead 
	// and the number of headers is small (4-5 ?)
	// Another problem is that we'll pay the overhead
	// of constructing the hashtable

	// A custom search tree may be better
        for (int i = starting; i < count; i++) {
	    if (fields[i].name.equalsIgnoreCase(name)) {
                return i;
            }
        }
        return -1;
    }

    /**
     * Removes the field at the specified position.  
     *
     * MultiMap will preserve the order of field add unless remove()
     * is called. This is not thread-safe, and will invalidate all
     * iterators. 
     *
     * This is not a frequent operation for Headers and Parameters -
     * there are better ways ( like adding a "isValid" field )
     */
    public void remove( int i ) {
	// reset and swap with last header
	Field mh = fields[i];
	// reset the field
	mh.recycle();
	
	fields[i] = fields[count - 1];
	fields[count - 1] = mh;
	count--;
    }

    /** Create a new, unitialized entry. 
     */
    public int addField() {
	int len = fields.length;
	int pos=count;
	if (count >= len) {
	    // expand header list array
	    Field tmp[] = new Field[pos * 2];
	    System.arraycopy(fields, 0, tmp, 0, len);
	    fields = tmp;
	}
	if (fields[pos] == null) {
	    fields[pos] = new Field();
	}
	count++;
	return pos;
    }

    public MessageBytes get( String name) {
        for (int i = 0; i < count; i++) {
	    if (fields[i].name.equals(name)) {
		return fields[i].value;
	    }
	}
        return null;
    }

    public int findFirst( String name ) {
        for (int i = 0; i < count; i++) {
	    if (fields[i].name.equals(name)) {
		return i;
	    }
	}
        return -1;
    }

    public int findNext( int startPos ) {
	int next= fields[startPos].nextPos;
	if( next != MultiMap.NEED_NEXT ) {
	    return next;
	}

	// next==NEED_NEXT, we never searched for this header
	MessageBytes name=fields[startPos].name;
        for (int i = startPos; i < count; i++) {
	    if (fields[i].name.equals(name)) {
		// cache the search result
		fields[startPos].nextPos=i;
		return i;
	    }
	}
	fields[startPos].nextPos= MultiMap.LAST;
        return -1;
    }

    // workaround for JDK1.1.8/solaris
    static final int NEED_NEXT=-2;
    static final int LAST=-1;

    // -------------------- Internal representation --------------------
    final class Field {
	MessageBytes name;
	MessageBytes value;

	// Extra info for speed
	
	//  multiple fields with same name - a linked list will
	// speed up multiple name enumerations and search.
	int nextPos;

	// hashkey
	int hash;
	Field nextSameHash;

	Field() {
	    nextPos=MultiMap.NEED_NEXT;
	}
	
	void recycle() {
	    name.recycle();
	    value.recycle();
	    nextPos=MultiMap.NEED_NEXT;
	}
    }
}
