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

package org.apache.jk.server.tomcat33;

import java.io.*;
import java.net.*;
import java.util.*;

import org.apache.jk.*;
import org.apache.jk.core.*;
import org.apache.jk.common.*;
import org.apache.jk.util.*;
import org.apache.tomcat.modules.server.PoolTcpConnector;

import org.apache.tomcat.core.*;

import org.apache.tomcat.util.net.*;
import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.log.*;
import org.apache.tomcat.util.http.*;

class JkResponse33 extends Response 
{
    boolean finished=false;
    Channel ch;
    Endpoint ep;
    int headersMsgNote;
    int c2bConvertersNote;
    int utfC2bNote;
    
    public JkResponse33(WorkerEnv we) 
    {
	super();
        headersMsgNote=we.getNoteId( WorkerEnv.ENDPOINT_NOTE, "headerMsg" );
        utfC2bNote=we.getNoteId( WorkerEnv.ENDPOINT_NOTE, "utfC2B" );
    }

    public void setEndpoint( Channel ch, Endpoint ep ) {
        this.ch=ch;
        this.ep=ep;
        MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
        if( msg==null ) {
            msg=new MsgAjp();
            ep.setNote( headersMsgNote, msg );
        }
    }
    
    public void recycle() {
	super.recycle();
	finished=false;
    }

    // XXX if more headers that MAX_SIZE, send 2 packets!
    // XXX Can be implemented using module notification, no need to extend
    public void endHeaders() throws IOException 
    {
        super.endHeaders();
    
        if (request.protocol().isNull()) {
            return;
        }

        C2B c2b=(C2B)ep.getNote( utfC2bNote );
        if( c2b==null ) {
            c2b=new C2B(  "UTF8" );
            ep.setNote( utfC2bNote, c2b );
        }
        
        MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
        msg.reset();
        msg.appendByte(HandlerRequest.JK_AJP13_SEND_HEADERS);
        msg.appendInt( getStatus() );
        
        // s->b conversion, message
        msg.appendBytes( null );

        // XXX add headers
        
	int numHeaders = headers.size();
        msg.appendInt(numHeaders);
        for( int i=0; i<numHeaders; i++ ) {
            MessageBytes hN=headers.getName(i);
            // no header to sc conversion - there's little benefit
            // on this direction
            c2b.convert ( hN );
            msg.appendBytes( hN );
            
            MessageBytes hV=headers.getValue(i);
            c2b.convert( hV );
            msg.appendBytes( hV );
        }
        ch.send( msg, ep );
        if( dL > 0 ) d("Sending head");
    } 

    // XXX Can be implemented using module notification, no need to extend
    public void finish() throws IOException 
    {
	if(!finished) {
	    super.finish();
            finished = true; // Avoid END_OF_RESPONSE sent 2 times

            MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
            msg.reset();
            msg.appendByte( HandlerRequest.JK_AJP13_END_RESPONSE );
            msg.appendInt( 1 );
            
            ch.send(msg, ep );
            if( dL > 0 ) d( "sending end message " );
	}
    }

    // XXX Can be implemented using the buffers, no need to extend
    public void doWrite(  byte b[], int off, int len) throws IOException 
    {
        MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
        msg.reset();
        msg.appendByte( HandlerRequest.JK_AJP13_SEND_BODY_CHUNK);
        msg.appendBytes( b, off, len );
        ch.send( msg, ep );
        if( dL > 0 ) d( "sending block " + len );
    }

    private static final int dL=0;
    private static void d(String s ) {
        System.err.println( "JkResponse33: " + s );
    }

}
