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
package org.apache.jk.server.tomcat40;

import java.io.*;

import java.util.Iterator;

import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpSession;

import org.apache.catalina.connector.HttpResponseBase;
import org.apache.catalina.*;
import org.apache.catalina.util.CookieTools;

import org.apache.jk.core.*;
import org.apache.jk.common.*;
import org.apache.tomcat.util.http.MimeHeaders;

public class JkResponse40 extends HttpResponseBase {

    private boolean finished = false;
    private boolean headersSent = false;
    private MimeHeaders headers = new MimeHeaders();
    private StringBuffer cookieValue = new StringBuffer();
    Channel ch;
    Endpoint ep;
    int headersMsgNote;
    int utfC2bNote;
    
    public JkResponse40(WorkerEnv we) 
    {
	super();
        headersMsgNote=we.getNoteId( WorkerEnv.ENDPOINT_NOTE, "headerMsg" );
        utfC2bNote=we.getNoteId( WorkerEnv.ENDPOINT_NOTE, "utfC2B" );
    }
    
    String getStatusMessage() {
        return getStatusMessage(getStatus());
    }

    public void recycle() {
        // We're a pair - preserve
        Request req=request;
        super.recycle();
        request=req;
        this.finished = false;
        this.headersSent = false;
        this.stream=null;
        this.headers.recycle();
    }

    protected void sendHeaders()  throws IOException {
        if( dL>0 ) d("sendHeaders " + headersSent);
        if (headersSent) {
            // don't send headers twice
            return;
        }
        headersSent = true;

        int numHeaders = 0;

        if (getContentType() != null) {
            numHeaders++;
	}
        
	if (getContentLength() >= 0) {
            numHeaders++;
	}

	// Add the session ID cookie if necessary
	HttpServletRequest hreq = (HttpServletRequest)request.getRequest();
	HttpSession session = hreq.getSession(false);

	if ((session != null) && session.isNew() && (getContext() != null) 
            && getContext().getCookies()) {
	    Cookie cookie = new Cookie(Globals.SESSION_COOKIE_NAME,
				       session.getId());
	    cookie.setMaxAge(-1);
	    String contextPath = null;
            if (context != null)
                contextPath = context.getPath();
	    if ((contextPath != null) && (contextPath.length() > 0))
		cookie.setPath(contextPath);
	    else
	        cookie.setPath("/");
	    if (hreq.isSecure())
		cookie.setSecure(true);
	    addCookie(cookie);
	}

        // Send all specified cookies (if any)
	synchronized (cookies) {
	    Iterator items = cookies.iterator();
	    while (items.hasNext()) {
		Cookie cookie = (Cookie) items.next();

                cookieValue.delete(0, cookieValue.length());
                CookieTools.getCookieHeaderValue(cookie, cookieValue);
                
                addHeader(CookieTools.getCookieHeaderName(cookie),
                          cookieValue.toString());
	    }
	}

        // figure out how many headers...
        // can have multiple headers of the same name...
        // need to loop through headers once to get total
        // count, once to add header to outBuf
        String[] hnames = getHeaderNames();
        Object[] hvalues = new Object[hnames.length];

        int i;
        for (i = 0; i < hnames.length; ++i) {
            String[] tmp = getHeaderValues(hnames[i]);
            numHeaders += tmp.length;
            hvalues[i] = tmp;
        }

        C2B c2b=(C2B)ep.getNote( utfC2bNote );
        if( c2b==null ) {
            c2b=new C2B(  "UTF8" );
            ep.setNote( utfC2bNote, c2b );
        }
        
        if( dL>0 ) d("sendHeaders " + numHeaders );
        MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
        msg.reset();
        msg.appendByte(HandlerRequest.JK_AJP13_SEND_HEADERS);
        msg.appendInt( getStatus() );
        
        c2b.convert( getStatusMessage(getStatus()));
        c2b.flushBuffer();
        msg.appendByteChunk(c2b.getByteChunk());
        c2b.recycle();

        String contentType=getContentType();
        int cl=getContentLength();
        
        msg.appendInt(numHeaders);

        if ( contentType != null) {
            sendHeader(msg, c2b, "Content-Type", contentType );
 	}
 	if ( cl >= 0) {
            sendHeader(msg, c2b, "Content-Length", String.valueOf(cl));
        }
        
        // XXX do we need this  ? If so, we need to adjust numHeaders
        // and avoid duplication

        for (i = 0; i < hnames.length; ++i) {
	    String name = hnames[i];
            String[] values = (String[])hvalues[i];
            for (int j = 0; j < values.length; ++j) {
                sendHeader( msg, c2b, name, values[j] );
            }
        }

        msg.send( ch, ep );

        // The response is now committed
        committed = true;
    }

    private void sendHeader( Msg msg, C2B c2b, String n, String v )
        throws IOException
    {
        if( dL > 0 ) d( "SendHeader " + n + " " + v );
        c2b.convert( n );
        c2b.flushBuffer();
        msg.appendByteChunk(c2b.getByteChunk());
        c2b.recycle();
        c2b.convert( v );
        c2b.flushBuffer();
        msg.appendByteChunk(c2b.getByteChunk());
        c2b.recycle();
    }

    public void finishResponse() throws IOException {
        if( dL>0 ) d("finishResponse " + this.finished ); 
	if(!this.finished) {
	    super.finishResponse();
            this.finished = true; // Avoid END_OF_RESPONSE sent 2 times

            MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
            msg.reset();
            msg.appendByte( HandlerRequest.JK_AJP13_END_RESPONSE );
            msg.appendInt( 1 );
            
            msg.send(ch, ep );
	}        
    }

    public void write(byte b[], int off, int len ) throws IOException {
        if( dL>0 ) d("write " + len); 
        if( !headersSent ) sendHeaders();
        MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
        msg.reset();
        msg.appendByte( HandlerRequest.JK_AJP13_SEND_BODY_CHUNK);
        msg.appendBytes( b, off, len );
        msg.send( ch, ep );
     }

    void setEndpoint(Channel ch, Endpoint ep) {
        this.ch=ch;
        this.ep=ep;
        MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
        if( msg==null ) {
            msg=new MsgAjp();
            ep.setNote( headersMsgNote, msg );
        }
        setStream( new JkOutputStream( this ));
    }

    private static final int dL=10;
    private static void d(String s ) {
        System.err.println( "JkResponse40: " + s );
    }

}

class JkOutputStream extends OutputStream {
    JkResponse40 resp;
    
    JkOutputStream(JkResponse40 resp) {
        this.resp=resp;
    }

    //XXX buffer
    byte singeByteWrite[]=new byte[1];
    
    public void write(int b) throws IOException {
        singeByteWrite[0]=(byte)b;
        write(singeByteWrite, 0, 1);
    }

    public void write(byte[] b, int off, int len) throws IOException {
        resp.write( b, off, len );
    }

    public void close() throws IOException {
    }

    public void flush() throws IOException {
    }
}
