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


package org.apache.coyote;

import java.io.IOException;
import java.util.Locale;

import org.apache.tomcat.util.buf.ByteChunk;

import org.apache.tomcat.util.res.StringManager;

import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.http.ContentType;

/**
 * Response object.
 * 
 * @author James Duncan Davidson [duncan@eng.sun.com]
 * @author Jason Hunter [jch@eng.sun.com]
 * @author James Todd [gonzo@eng.sun.com]
 * @author Harish Prabandham
 * @author Hans Bergsten <hans@gefionsoftware.com>
 * @author Remy Maucherat
 */
public final class Response {


    // ----------------------------------------------------------- Constructors


    public Response() {
    }


    // ----------------------------------------------------- Instance Variables


    /**
     * Status code.
     */
    protected int status = 200;


    /**
     * Status message.
     */
    protected String message = null;


    /**
     * Response headers.
     */
    protected MimeHeaders headers = new MimeHeaders();


    /**
     * Associated output buffer.
     */
    protected OutputBuffer outputBuffer;


    /**
     * Notes.
     */
    protected Object notes[] = new Object[Constants.MAX_NOTES];


    /**
     * Committed flag.
     */
    protected boolean commited = false;


    /**
     * Action hook.
     */
    public ActionHook hook;


    /**
     * HTTP specific fields (remove them ?)
     */
    protected String contentType = Constants.DEFAULT_CONTENT_TYPE;
    protected String contentLanguage = null;
    protected String characterEncoding = Constants.DEFAULT_CHARACTER_ENCODING;
    protected int contentLength = -1;
    private Locale locale = Constants.DEFAULT_LOCALE;

    /**
     * FIXME: Remove.
     */
    // holds request error exception
    // set this just once during request processing
    Exception errorException = null;
    // holds request error URI
    String errorURI = null;


    // ------------------------------------------------------------- Properties


    public OutputBuffer getOutputBuffer() {
	return outputBuffer;
    }


    public void setOutputBuffer(OutputBuffer outputBuffer) {
        this.outputBuffer = outputBuffer;
    }


    public MimeHeaders getMimeHeaders() {
	return headers;
    }


    public ActionHook getHook() {
        return hook;
    }


    public void setHook(ActionHook hook) {
        this.hook = hook;
    }


    // -------------------- Per-Response "notes" --------------------


    public final void setNote(int pos, Object value) {
	notes[pos] = value;
    }


    public final Object getNote(int pos) {
	return notes[pos];
    }


    // -------------------- Actions --------------------


    public void action(ActionCode actionCode, Object param) {
        if (hook != null) {
            hook.action(actionCode, param);
        }
    }


    // -------------------- State --------------------


    public int getStatus() {
        return status;
    }

    
    /** 
     * Set the response status 
     */ 
    public void setStatus( int status ) {
	this.status = status;
    }


    /**
     * Get the status message.
     */
    public String getMessage() {
        return message;
    }


    /**
     * Set the status message.
     */
    public void setMessage(String message) {
        this.message = message;
    }


    public boolean isCommitted() {
	return commited;
    }


    public void setCommitted(boolean v) {
	this.commited = v;
    }


    // -----------------Error State --------------------


    /** 
     * Set the error Exception that occurred during
     * request processing.
     */
    public void setErrorException(Exception ex) {
	errorException = ex;
    }


    /** 
     * Get the Exception that occurred during request
     * processing.
     */
    public Exception getErrorException() {
	return errorException;
    }


    public boolean isExceptionPresent() {
	return ( errorException != null );
    }


    /** 
     * Set request URI that caused an error during
     * request processing.
     */
    public void setErrorURI(String uri) {
	errorURI = uri;
    }


    /** Get the request URI that caused the original error.
     */
    public String getErrorURI() {
	return errorURI;
    }


    // -------------------- Methods --------------------
    
    
    public void reset() 
        throws IllegalStateException {
        
        // Reset the headers only if this is the main request,
        // not for included
        contentType = Constants.DEFAULT_CONTENT_TYPE;
        locale = Constants.DEFAULT_LOCALE;
        characterEncoding = Constants.DEFAULT_CHARACTER_ENCODING;
        contentLength = -1;

        status = 200;
        message = null;
        headers.clear();
        
	// Force the PrintWriter to flush its data to the output
	// stream before resetting the output stream
	//
	// Reset the stream
	if (commited) {
	    //String msg = sm.getString("servletOutputStreamImpl.reset.ise"); 
	    throw new IllegalStateException();
	}
        
        action(ActionCode.ACTION_RESET, null);
    }


    public void finish() throws IOException {
        action(ActionCode.ACTION_CLOSE, null);
    }


    // -------------------- Headers --------------------
    public boolean containsHeader(String name) {
	return headers.getHeader(name) != null;
    }


    public void setHeader(String name, String value) {
	char cc=name.charAt(0);
	if( cc=='C' || cc=='c' ) {
	    if( checkSpecialHeader(name, value) )
		return;
	}
	headers.setValue(name).setString( value);
    }


    public void addHeader(String name, String value) {
	char cc=name.charAt(0);
	if( cc=='C' || cc=='c' ) {
	    if( checkSpecialHeader(name, value) )
		return;
	}
	headers.addValue(name).setString( value );
    }

    
    /** 
     * Set internal fields for special header names. 
     * Called from set/addHeader.
     * Return true if the header is special, no need to set the header.
     */
    private boolean checkSpecialHeader( String name, String value) {
	// XXX Eliminate redundant fields !!!
	// ( both header and in special fields )
	if( name.equalsIgnoreCase( "Content-Type" ) ) {
	    setContentType( value );
	    return true;
	}
	if( name.equalsIgnoreCase( "Content-Length" ) ) {
	    try {
		int cL=Integer.parseInt( value );
		setContentLength( cL );
		return true;
	    } catch( NumberFormatException ex ) {
		// Do nothing - the spec doesn't have any "throws" 
		// and the user might know what he's doing
		return false;
	    }
	}
	if( name.equalsIgnoreCase( "Content-Language" ) ) {
	    // XXX XXX Need to construct Locale or something else
	}
	return false;
    }


    /** Signal that we're done with the headers, and body will follow.
     *  Any implementation needs to notify ContextManager, to allow
     *  interceptors to fix headers.
     */
    public void sendHeaders() throws IOException {
	commited = true;
    }


    // -------------------- I18N --------------------


    public Locale getLocale() {
        return locale;
    }

    /**
     * Called explicitely by user to set the Content-Language and
     * the default encoding
     */
    public void setLocale(Locale locale) {
        if (locale == null) {
            return;  // throw an exception?
        }

        // Save the locale for use by getLocale()
        this.locale = locale;

        // Set the contentLanguage for header output
        contentLanguage = locale.getLanguage();

	// only one header !
	headers.setValue("Content-Language").setString(contentLanguage);
    }

    public String getCharacterEncoding() {
	return characterEncoding;
    }

    public void setContentType(String contentType) {
	this.contentType = contentType;
	String encoding = ContentType.getCharsetFromContentType(contentType);
        if (encoding != null) {
	    characterEncoding = encoding;
        }
	headers.setValue("Content-Type").setString(contentType);
    }

    public String getContentType() {
	return contentType;
    }
    
    public void setContentLength(int contentLength) {
	this.contentLength = contentLength;
	headers.setValue("Content-Length").setInt(contentLength);
    }

    public int getContentLength() {
	return contentLength;
    }


    /** 
     * Write a chunk of bytes.
     */
    public void doWrite(ByteChunk chunk/*byte buffer[], int pos, int count*/)
        throws IOException {
        outputBuffer.doWrite(chunk);
    }


    // --------------------
    
    public void recycle() {

	contentType = Constants.DEFAULT_CONTENT_TYPE;
	contentLanguage = null;
        locale = Constants.DEFAULT_LOCALE;
	characterEncoding = Constants.DEFAULT_CHARACTER_ENCODING;
	contentLength = -1;
	status = 200;
        message = null;
	commited = false;
	errorException = null;
	errorURI = null;
	headers.clear();

    }

}
