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
import java.net.*;
import java.util.*;

import org.apache.jk.core.*;

import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.log.*;
import org.apache.tomcat.util.http.*;
import org.apache.tomcat.util.threads.*;


/** A dummy worker, will just send back a dummy response.
 *  Used for testing and tunning.
 */
public class WorkerDummy extends JkHandler
{
    public WorkerDummy()
    {
        String msg="HelloWorld";
        byte b[]=msg.getBytes();
        body.setBytes(b, 0, b.length);
    }

    /* ==================== Start/stop ==================== */

    /** Initialize the worker. After this call the worker will be
     *  ready to accept new requests.
     */
    public void init() throws IOException {
        headersMsgNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "headerMsg" );
    }
 
    MessageBytes body=new MessageBytes();
    private int headersMsgNote;
    
    public int invoke( Msg in, MsgContext ep ) // BaseRequest req, Channel ch, Endpoint ep )
        throws IOException
    {
        MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
        if( msg==null ) {
            msg=new MsgAjp();
            ep.setNote( headersMsgNote, msg );
        }

        msg.reset();
        msg.appendByte(HandlerRequest.JK_AJP13_SEND_HEADERS);
        msg.appendInt(200);
        msg.appendBytes(null);

        msg.appendInt(0);
        
        ep.getChannel().send( msg, ep );
        //         msg.dump("out:" );

        msg.reset();
        msg.appendByte( HandlerRequest.JK_AJP13_SEND_BODY_CHUNK);
        msg.appendInt( body.getLength() );
        msg.appendBytes( body );

        
        ep.getChannel().send(msg, ep);

        msg.reset();
        msg.appendByte( HandlerRequest.JK_AJP13_END_RESPONSE );
        msg.appendInt( 1 );
        
        ep.getChannel().send(msg, ep );
        return OK;
    }
    
    private static final int dL=0;
    private static void d(String s ) {
        System.err.println( "WorkerDummy: " + s );
    }
}

