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

package org.apache.jk.server;

import java.io.*;
import java.net.*;
import java.util.*;

import org.apache.jk.*;
import org.apache.jk.core.*;
import org.apache.jk.common.*;

import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.log.*;
import org.apache.tomcat.util.http.*;

import org.apache.coyote.*;

/** Plugs Jk2 into Coyote
 */
public class JkCoyoteHandler extends JkHandler implements
    ProtocolHandler,
    ActionHook,
    org.apache.coyote.OutputBuffer,
    org.apache.coyote.InputBuffer
{
    protected static org.apache.commons.logging.Log log 
        = org.apache.commons.logging.LogFactory.getLog(JkCoyoteHandler.class);
    int headersMsgNote;
    int c2bConvertersNote;
    int utfC2bNote;
    int obNote;
    int epNote;
    int inputStreamNote;
    
    Adapter adapter;
    protected JkMain jkMain=new JkMain();
    
    public void setProperty( String name, String value ) {
        log.info("setProperty " + name + " " + value );
        jkMain.setProperty( name, value );
        properties.put( name, value );
    }

    public String getProperty( String name ) {
        return properties.getProperty(name) ;
    }

    /** Pass config info
     */
    public void setAttribute( String name, Object value ) {
        if( value instanceof String )
            this.setProperty( name, (String)value );
    }
    
    public Object getAttribute( String name ) {
        return null;
    }

    /** The adapter, used to call the connector 
     */
    public void setAdapter(Adapter adapter) {
        this.adapter=adapter;
    }

    public Adapter getAdapter() {
        return adapter;
    }

    boolean started=false;
    
    /** Start the protocol
     */
    public void init() {
        if( started ) return;

        started=true;
        jkMain.getWorkerEnv().addHandler("container", this );

        try {
            // jkMain.setJkHome() XXX;
            
            jkMain.init();
            jkMain.start();

            log.info("Jk2 started ");

            headersMsgNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "headerMsg" );
            utfC2bNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "utfC2B" );
            epNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "ep" );
            obNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "coyoteBuffer" );
            inputStreamNote= wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE,
                                             "jkInputStream");

        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }

    public void destroy() {
        jkMain.stop();
    }

    // -------------------- OutputBuffer implementation --------------------

        
    public int doWrite(ByteChunk chunk, Response res) 
        throws IOException
    {
        if (!res.isCommitted()) {
            // Send the connector a request for commit. The connector should
            // then validate the headers, send them (using sendHeader) and 
            // set the filters accordingly.
            res.action(ActionCode.ACTION_COMMIT, null);
        }
        if( log.isInfoEnabled() )
            log.info("doWrite " );
        MsgContext ep=(MsgContext)res.getNote( epNote );
        
        MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
        msg.reset();
        msg.appendByte( HandlerRequest.JK_AJP13_SEND_BODY_CHUNK);
        msg.appendBytes( chunk.getBytes(), chunk.getOffset(), chunk.getLength() );
        ep.setType( JkHandler.HANDLE_SEND_PACKET );
        ep.getSource().invoke( msg, ep );
        
        return 0;
    }
    
    public int doRead(ByteChunk chunk, Request req) 
        throws IOException
    {
        Response res=req.getResponse();
        if( log.isInfoEnabled() )
            log.info("doRead " + chunk.getBytes() + " " +  chunk.getOffset() + " " + chunk.getLength());
        MsgContext ep=(MsgContext)res.getNote( epNote );
        
        JkInputStream jkIS=(JkInputStream)ep.getNote( inputStreamNote );
        // return jkIS.read( chunk.getBytes(), chunk.getOffset(), chunk.getLength());
        return jkIS.doRead( chunk );
    }
    
    // -------------------- Jk handler implementation --------------------
    // Jk Handler mehod
    public int invoke( Msg msg, MsgContext ep ) 
        throws IOException
    {
        org.apache.coyote.Request req=(org.apache.coyote.Request)ep.getRequest();
        org.apache.coyote.Response res=req.getResponse();
        res.setHook( this );

        log.info( "Invoke " + req + " " + res + " " + req.requestURI().toString());
        
        res.setOutputBuffer( this );
        req.setInputBuffer( this );
        
        if( ep.getNote( headersMsgNote ) == null ) {
            Msg msg2=new MsgAjp();
            ep.setNote( headersMsgNote, msg );
        }
        
        res.setNote( epNote, ep );
        
        try {
            adapter.service( req, res );
        } catch( Exception ex ) {
            ex.printStackTrace();
        }

        req.recycle();
        res.recycle();
        return OK;
    }

    // -------------------- Coyote Action implementation --------------------
    
    public void action(ActionCode actionCode, Object param) {
        try {
            if( actionCode==ActionCode.ACTION_COMMIT ) {
                org.apache.coyote.Response res=(org.apache.coyote.Response)param;

                if( log.isInfoEnabled() )
                    log.info("COMMIT sending headers " + res + " " + res.getMimeHeaders() );
                
                
                C2BConverter c2b=(C2BConverter)res.getNote( utfC2bNote );
                if( c2b==null ) {
                    c2b=new C2BConverter(  "UTF8" );
                    res.setNote( utfC2bNote, c2b );
                }
                
                MsgContext ep=(MsgContext)res.getNote( epNote );
                MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
                msg.reset();
                msg.appendByte(HandlerRequest.JK_AJP13_SEND_HEADERS);
                msg.appendInt( res.getStatus() );
                
                // s->b conversion, message
                msg.appendBytes( null );
                
                // XXX add headers
                
                MimeHeaders headers=res.getMimeHeaders();
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
                ep.setType( JkHandler.HANDLE_SEND_PACKET );
                ep.getSource().invoke( msg, ep );
            } else if( actionCode==ActionCode.ACTION_RESET ) {
                if( log.isInfoEnabled() )
                    log.info("RESET " );
                
            } else if( actionCode==ActionCode.ACTION_CLOSE ) {
                if( log.isInfoEnabled() )
                    log.info("CLOSE " );
                org.apache.coyote.Response res=(org.apache.coyote.Response)param;
                MsgContext ep=(MsgContext)res.getNote( epNote );
                
                MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
                msg.reset();
                msg.appendByte( HandlerRequest.JK_AJP13_END_RESPONSE );
                msg.appendInt( 1 );
                
                ep.setType( JkHandler.HANDLE_SEND_PACKET );
                ep.getSource().invoke( msg, ep );
            } else if( actionCode==ActionCode.ACTION_REQ_SSL_ATTRIBUTE ) {
                
                
            } else if( actionCode==ActionCode.ACTION_REQ_HOST_ATTRIBUTE ) {

                
            } else if( actionCode==ActionCode.ACTION_ACK ) {
                if( log.isInfoEnabled() )
                    log.info("ACK " );
                // What should we do here ? Who calls it ? 
            }
        } catch( Exception ex ) {
            log.error( "Error in action code ", ex );
        }
    }


}
