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


package org.apache.coyote.tomcat3;

import java.io.IOException;
import java.util.Locale;

import org.apache.coyote.ActionCode;
import org.apache.tomcat.core.Response;
import org.apache.tomcat.util.buf.ByteChunk;

/** The Response to connect with Coyote.
 *  This class mostly handles the I/O between Tomcat and Coyte.
 *  @Author Bill Barker
 */

class Tomcat3Response extends  Response {
    String reportedname=null;

    org.apache.coyote.Response coyoteResponse=null;

    ByteChunk outputChunk = new ByteChunk();

    boolean  acknowledged=false;
    
    public Tomcat3Response() {
        super();
    }

    /** Attach a Coyote Request to this request.
     */
    public void setCoyoteResponse(org.apache.coyote.Response cRes) {
	coyoteResponse = cRes;
	headers = coyoteResponse.getMimeHeaders();
    }

    public void init() {
	super.init();
    }

    public void recycle() {
	super.recycle();
	if(coyoteResponse != null) coyoteResponse.recycle();
	outputChunk.recycle();
	acknowledged=false;
    }

    // XXX What is this ? */
    public void setReported(String reported) {
        reportedname = reported;
    }

    public void endHeaders()  throws IOException {
	super.endHeaders();
	coyoteResponse.setStatus(getStatus());
	// Check that the content-length has been set.
	int cLen = getContentLength();
	if( cLen >= 0 ) {
	    coyoteResponse.setContentLength(cLen);
	}
        // Calls a sendHeaders callback to the protocol
	coyoteResponse.sendHeaders();
    }

    public void clientFlush() throws IOException {
        coyoteResponse.action( ActionCode.ACTION_CLIENT_FLUSH, coyoteResponse );
    }
    
    public void doWrite( byte buffer[], int pos, int count)
	throws IOException
    {
	if( count > 0 ) {
            // XXX should be an explicit callback as well.
	    outputChunk.setBytes(buffer, pos, count);
	    coyoteResponse.doWrite( outputChunk );
	}
    }

    public void reset() throws IllegalStateException {
	super.reset();
	if( ! included )
	    coyoteResponse.reset();
    }
    
    public void finish() throws IOException {
	super.finish();
	coyoteResponse.finish();
    }

    /**
     * Send an acknowledgment of a request.
     * 
     * @exception IOException if an input/output error occurs
     */
    public void sendAcknowledgement()
        throws IOException {

	if( status >= 300 ) // Don't ACK on errors.
	    acknowledged = true;
        // Don't ACK twice on the same request. (e.g. on a forward)
	if(acknowledged)
	    return;
        // Ignore any call from an included servlet
        if (isIncluded())
            return; 
        if (isBufferCommitted())
            throw new IllegalStateException
                (sm.getString("hsrf.error.ise"));

	coyoteResponse.acknowledge();
	acknowledged=true;
    }

    public void setLocale(Locale locale) {
        if (locale == null || included) {
            return;  // throw an exception?
        }
        this.locale = locale;
        coyoteResponse.setLocale(locale);
        contentLanguage = coyoteResponse.getContentLanguage();
        // maintain Tomcat 3.3 behavior by setting the header too
        // and by not trying to guess the characterEncoding
        headers.setValue("Content-Language").setString(contentLanguage);
    }

    public void setContentType(String contentType) {
        if (included) {
            return;
        }
        coyoteResponse.setContentType(contentType);
        this.contentType = coyoteResponse.getContentType();
        this.characterEncoding = coyoteResponse.getCharacterEncoding();
        // maintain Tomcat 3.3 behavior by setting the header too
        headers.setValue("Content-Type").setString(contentType);
    }

}
