/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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
        this.haveCharacterEncoding = true;
        // maintain Tomcat 3.3 behavior by setting the header too
        headers.setValue("Content-Type").setString(contentType);
    }

}
