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

import org.apache.tomcat.util.buf.MessageBytes;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Vector;

// XXX Shouldn't be here - has nothing to do with buffers.

/**
 * Main tool for object expiry. 
 * Marks creation and access time of an "expirable" object,
 * and extra properties like "id", "valid", etc.
 *
 * Used for objects that expire - originally Sessions, but 
 * also Contexts, Servlets, cache - or any other object that
 * expires.
 * 
 * @author Costin Manolache
 */
public final class TimeStamp implements  Serializable {
    private long creationTime = 0L;
    private long lastAccessedTime = creationTime;
    private long thisAccessedTime = creationTime;
    private boolean isNew = true;
    private long maxInactiveInterval = -1;
    private boolean isValid = false;
    MessageBytes name;
    int id=-1;
    
    Object parent;
    
    public TimeStamp() {
    }

    // -------------------- Active methods --------------------

    /**
     *  Access notification. This method takes a time parameter in order
     *  to allow callers to efficiently manage expensive calls to
     *  System.currentTimeMillis() 
     */
    public void touch(long time) {
	this.lastAccessedTime = this.thisAccessedTime;
	this.thisAccessedTime = time;
	this.isNew=false;
    }

    // -------------------- Property access --------------------

    /** Return the "name" of the timestamp. This can be used
     *  to associate unique identifier with each timestamped object.
     *  The name is a MessageBytes - i.e. a modifiable byte[] or char[]. 
     */
    public MessageBytes getName() {
	if( name==null ) name=MessageBytes.newInstance();//lazy
	return name;
    }

    /** Each object can have an unique id, similar with name but
     *  providing faster access ( array vs. hashtable lookup )
     */
    public int getId() {
	return id;
    }

    public void setId( int id ) {
	this.id=id;
    }
    
    /** Returns the owner of this stamp ( the object that is
     *  time-stamped ).
     *  For a 
     */
    public void setParent( Object o ) {
	parent=o;
    }

    public Object getParent() {
	return parent;
    }

    public void setCreationTime(long time) {
	this.creationTime = time;
	this.lastAccessedTime = time;
	this.thisAccessedTime = time;
    }


    public long getLastAccessedTime() {
	return lastAccessedTime;
    }

    /** Inactive interval in millis - the time is computed
     *  in millis, convert to secs in the upper layer
     */
    public long getMaxInactiveInterval() {
	return maxInactiveInterval;
    }

    public void setMaxInactiveInterval(long interval) {
	maxInactiveInterval = interval;
    }

    public boolean isValid() {
	return isValid;
    }

    public void setValid(boolean isValid) {
	this.isValid = isValid;
    }

    public boolean isNew() {
	return isNew;
    }

    public void setNew(boolean isNew) {
	this.isNew = isNew;
    }

    public long getCreationTime() {
	return creationTime;
    }

    // -------------------- Maintainance --------------------

    public void recycle() {
	creationTime = 0L;
	lastAccessedTime = 0L;
	maxInactiveInterval = -1;
	isNew = true;
	isValid = false;
	id=-1;
	if( name!=null) name.recycle();
    }

}

