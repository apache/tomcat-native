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

/**
 * Simple object pool. Based on ThreadPool and few other classes
 *
 * The pool will ignore overflow and return null if empty.
 *
 * @author Gal Shachor
 * @author Costin Manolache
 */
public final class SimplePool  {
    /*
     * Where the threads are held.
     */
    private Object pool[];

    private int max;
    private int last;
    private int current=-1;
    
    private Object lock;
    public static final int DEFAULT_SIZE=32;
    static final int debug=0;
    
    public SimplePool() {
	this(DEFAULT_SIZE,DEFAULT_SIZE);
    }

    public SimplePool(int size) {
	this(size, size);
    }

    public SimplePool(int size, int max) {
	this.max=max;
	pool=new Object[size];
	this.last=size-1;
	lock=new Object();
    }

    public  void set(Object o) {
	put(o);
    }

    /**
     * Add the object to the pool, silent nothing if the pool is full
     */
    public  void put(Object o) {
	synchronized( lock ) {
	    if( current < last ) {
		current++;
		pool[current] = o;
            } else if( current < max ) {
		// realocate
		int newSize=pool.length*2;
		if( newSize > max ) newSize=max+1;
		Object tmp[]=new Object[newSize];
		last=newSize-1;
		System.arraycopy( pool, 0, tmp, 0, pool.length);
		pool=tmp;
		current++;
		pool[current] = o;
	    }
	    if( debug > 0 ) log("put " + o + " " + current + " " + max );
	}
    }

    /**
     * Get an object from the pool, null if the pool is empty.
     */
    public  Object get() {
	Object item = null;
	synchronized( lock ) {
	    if( current >= 0 ) {
		item = pool[current];
		current -= 1;
	    }
	    if( debug > 0 ) 
		log("get " + item + " " + current + " " + max);
	}
	return item;
    }

    /**
     * Return the size of the pool
     */
    public int getMax() {
	return max;
    }

    /**
     * Number of object in the pool
     */
    public int getCount() {
	return current+1;
    }


    public void shutdown() {
    }
    
    private void log( String s ) {
	System.out.println("SimplePool: " + s );
    }
}
