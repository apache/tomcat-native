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

package org.apache.ajp.tomcat33;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.util.Enumeration;
import java.security.*;

import org.apache.tomcat.core.*;
import org.apache.tomcat.util.*;
import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.HttpMessages;
import org.apache.tomcat.util.buf.HexUtils;


/**
 * A single packet for communication between the web server and the
 * container.  Designed to be reused many times with no creation of
 * garbage.  Understands the format of data types for these packets.
 * Can be used (somewhat confusingly) for both incoming and outgoing
 * packets.  
 *
 * @see Ajp14/Ajp13Packet 
 *
 * @author Henri Gomez [hgomez@slib.fr]
 * @author Dan Milstein [danmil@shore.net]
 * @author Keith Wannamaker [Keith@Wannamaker.org]
 */

public class Ajp14Packet extends Ajp13Packet {

	/**
	 * Create a new packet with an internal buffer of given size.
	 */
	public Ajp14Packet( int size ) {
		super(size);
	}

	public Ajp14Packet( byte b[] ) {
		super(b);
	}

	public Ajp14Packet( OutputBuffer ob ) {
		super(ob);
	}
	
	/** 
	 * Parse the packet header for a packet sent from the web server to
	 * the container.  Set the read position to immediately after
	 * the header.
	 *
	 * @return The length of the packet payload, as encoded in the
	 * header, or -1 if the packet doesn't have a valid header.  
	 */
	public int checkIn() {
	    pos = 0;
	    int mark = getInt();
	    len      = getInt();
	    
	    if( mark != 0x1235 ) {
		// XXX Logging
		System.out.println("BAD packet " + mark);
		dump( "In: " );
		return -1;
	    }
	    return len;
	}
	
	/**
	 * Prepare this packet for accumulating a message from the container to
	 * the web server.  Set the write position to just after the header
	 * (but leave the length unwritten, because it is as yet unknown).  
	 */
	public void reset() {
	    len = 4;
	    pos = 4;
	    buff[0] = (byte)0x12;
	    buff[1] = (byte)0x35;
	}
}
