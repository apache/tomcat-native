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
package org.apache.jk.common;

import java.io.*;

import java.util.List;
import java.util.Iterator;

import javax.servlet.ServletInputStream;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.Cookie;

import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.Cookies;
import org.apache.tomcat.util.http.ServerCookie;
import org.apache.tomcat.util.http.BaseRequest;
import org.apache.tomcat.util.http.MimeHeaders;

import org.apache.jk.core.*;
import org.apache.jk.common.*;
import org.apache.jk.util.*;


/** Generic input stream impl on top of ajp
 */
public class JkInputStream extends InputStream {
    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( JkInputStream.class );

    public JkInputStream() {
    }

    public int available() throws IOException {
        return blen-pos;
    }

    public void close() throws IOException {
        this.closed=true;
    }

    public void mark(int readLimit) {
    }

    public boolean markSupported() {
        return false;
    }

    public void reset() throws IOException {
        throw new IOException("reset() not supported");
    }

    public int read() throws IOException {
        if( contentLength == -1 ) {
            return doRead1();
	}
	if( available <= 0 ) {
            if( log.isDebugEnabled() )
                log.debug("doRead() nothing available" );
            return -1;
        }
	available--;

        return doRead1();
    }

    public int read(byte[] b, int off, int len) throws IOException {
      	int rd=-1;
	if( contentLength == -1 ) {
	    rd=doRead1(b,off,len);
	    return rd;
	}
	if( available <= 0 ) {
            if( log.isDebugEnabled() ) log.debug("doRead() nothing available" );
	    return -1;
        }
        
	rd=doRead1( b,off, len );
	available -= rd;
	if( log.isDebugEnabled() )
            log.debug("Read: " + new String( b,off, len ));
	return rd;
    }

    public long skip(long n) throws IOException {
        if (n > Integer.MAX_VALUE) {
            throw new IOException("can't skip than many:  " + n);
        }
        // XXX if n is big, split this in multiple reads
        byte[] b = new byte[(int)n];
        return read(b, 0, b.length);
    }


    // -------------------- Jk specific methods --------------------

    Msg bodyMsg=new MsgAjp();
    MsgContext mc;

    // Total length of the body - maximum we can read
    // If -1, we don't use any limit, and we don't count available
    int contentLength;
    // How much remains unread.
    int available;

    boolean closed=false;

    // Ajp13 specific -  needs refactoring for the new model
    public static final int MAX_PACKET_SIZE=8192;
    public static final int H_SIZE=4;  // Size of basic packet header
    public static final int  MAX_READ_SIZE = MAX_PACKET_SIZE - H_SIZE - 2;
    public static final byte JK_AJP13_GET_BODY_CHUNK    = 6;

    
    // Holds incoming chunks of request body data
    // XXX We do a copy that could be avoided !
    byte []bodyBuff = new byte[MAX_READ_SIZE];
    int blen;  // Length of current chunk of body data in buffer
    int pos;   // Current read position within that buffer

    boolean end_of_stream; // true if we've received an empty packet
    
    private int doRead1() throws IOException {
        if( log.isDebugEnabled() ) log.debug("doRead1 " );
        if(pos >= blen) {
            if( ! refillReadBuffer()) {
		return -1;
	    }
        }
        return bodyBuff[pos++] & 0xFF;  // prevent sign extension of byte value
    }

    public int doRead1(byte[] b, int off, int len) throws IOException 
    {
        if( log.isDebugEnabled() ) log.debug("doRead1 " );
	if(pos >= blen) {
	    if( ! refillReadBuffer()) {
		return -1;
	    }
	}

	if(pos + len <= blen) { // Fear the off by one error
	    // Sanity check b.length > off + len?
	    System.arraycopy(bodyBuff, pos, b, off, len);
	    if( log.isDebugEnabled() )
		log.debug("doRead1: " + pos + " " + len + " " + blen + " " +
		  new String( b, off, len ) + " " + Thread.currentThread());
	    pos += len;
	    return len;
	}

	// Not enough data (blen < pos + len) or chunked encoded
	int toCopy = len;
	while(toCopy > 0) {
	    int bytesRemaining = blen - pos;
	    if(bytesRemaining < 0) 
		bytesRemaining = 0;
	    int c = bytesRemaining < toCopy ? bytesRemaining : toCopy;

	    System.arraycopy(bodyBuff, pos, b, off, c);
	    if( log.isDebugEnabled() )
                log.debug("doRead2: " + pos + " " + len + " " + blen + " " +
                          c + " " + new String( b, off, c ) + " " +
                          new String( bodyBuff, pos, c ));

	    toCopy    -= c;

	    off       += c;
	    pos       += c; // In case we exactly consume the buffer

	    if(toCopy > 0) 
		if( ! refillReadBuffer()) { // Resets blen and pos
		    break;
		}
	}

	return len - toCopy;
    }

    /** Must be called after the request is parsed, before
     *  any input
     */
    public void setContentLength( int i ) {
        contentLength=i;
        available=i;
    }

    /** Must be called when the stream is created
     */
    public void setMsgContext( MsgContext mc ) {
        this.mc=mc;
    }

    /** Must be called before or after each request
     */
    public void recycle() {
        available=0;
        blen = 0;
        pos = 0;
        closed=false;
        end_of_stream = false;
        contentLength=-1;
    }

    /** Receive a chunk of data. Called to implement the
     *  'special' packet in ajp13 and to receive the data
     *  after we send a GET_BODY packet
     */
    public boolean receive() throws IOException
    {
        int err = mc.getChannel().receive(bodyMsg, mc);
        if(err < 0) {
	    throw new IOException();
	}

	// No data received.
	if( bodyMsg.getLen() == 0 ) {
	    pos=0;
	    blen=0;
            end_of_stream = true;
	    return false;
	}
    	blen = bodyMsg.peekInt();
    	pos = 0;
    	int cpl=bodyMsg.getBytes(bodyBuff);
	if( log.isDebugEnabled() )
            log.debug( "Copy into body buffer2 " + bodyBuff + " " +
               cpl + " " + blen + " "  +
               new String( bodyBuff, 0, cpl ));

	return (blen > 0);
    }
    
    /**
     * Get more request body data from the web server and store it in the 
     * internal buffer.
     *
     * @return true if there is more data, false if not.    
     */
    private boolean refillReadBuffer() throws IOException 
    {
	// If the server returns an empty packet, assume that that end of
	// the stream has been reached (yuck -- fix protocol??).
        if (end_of_stream) {
            if( log.isDebugEnabled() ) log.debug("refillReadBuffer: end of stream " );
          return false;
        }

	// Why not use outBuf??
	bodyMsg.reset();
	bodyMsg.appendByte(JK_AJP13_GET_BODY_CHUNK);
	bodyMsg.appendInt(MAX_READ_SIZE);
        
	if( log.isDebugEnabled() )
            log.debug("refillReadBuffer " + Thread.currentThread());

	mc.getChannel().send(bodyMsg, mc);
	
        return receive();
    }

}
