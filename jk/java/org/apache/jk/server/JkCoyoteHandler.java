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
import java.security.*;
import java.security.cert.*;

import org.apache.jk.core.*;
import org.apache.jk.common.*;

import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.log.*;
import org.apache.tomcat.util.http.*;
import org.apache.tomcat.util.net.SSLSupport;

import org.apache.coyote.*;
import org.apache.commons.modeler.Registry;
import javax.management.ObjectName;
import javax.management.MBeanServer;

/** Plugs Jk2 into Coyote. Must be named "type=JkHandler,name=container"
 *
 * @jmx:notification-handler name="org.apache.jk.SEND_PACKET
 * @jmx:notification-handler name="org.apache.coyote.ACTION_COMMIT
 */
public class JkCoyoteHandler extends JkHandler implements
    ProtocolHandler,
    ActionHook,
    org.apache.coyote.OutputBuffer,
    org.apache.coyote.InputBuffer
{
    protected static org.apache.commons.logging.Log log 
        = org.apache.commons.logging.LogFactory.getLog(JkCoyoteHandler.class);
    // Set debug on this logger to see the container request time
    private static org.apache.commons.logging.Log logTime=
        org.apache.commons.logging.LogFactory.getLog( "org.apache.jk.REQ_TIME" );

    int headersMsgNote;
    int c2bConvertersNote;
    int tmpMessageBytesNote;
    int utfC2bNote;
    int obNote;
    int epNote;
    int inputStreamNote;
    
    Adapter adapter;
    protected JkMain jkMain=null;

    public final int JK_STATUS_NEW=0;
    public final int JK_STATUS_HEAD=1;
    public final int JK_STATUS_CLOSED=2;

    /** Set a property. Name is a "component.property". JMX should
     * be used instead.
     */
    public void setProperty( String name, String value ) {
        if( log.isTraceEnabled())
            log.trace("setProperty " + name + " " + value );
        getJkMain().setProperty( name, value );
        properties.put( name, value );
    }

    public String getProperty( String name ) {
        return properties.getProperty(name) ;
    }

    /** Pass config info
     */
    public void setAttribute( String name, Object value ) {
        if( log.isDebugEnabled())
            log.debug("setAttribute " + name + " " + value );
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

    public JkMain getJkMain() {
        if( jkMain == null ) {
            jkMain=new JkMain();
            jkMain.setWorkerEnv(wEnv);
            
            if( oname != null ) {
                try {
                    Registry.getRegistry().registerComponent(jkMain, oname.getDomain(),
                            "JkMain", "type=JkMain");
                } catch (Exception e) {
                    log.error( "Error registering jkmain " + e );
                }
            }
        }
        return jkMain;
    }
    
    boolean started=false;
    
    /** Start the protocol
     */
    public void init() {
        if( started ) return;

        started=true;
        
        if( wEnv==null ) {
            // we are probably not registered - not very good.
            wEnv=getJkMain().getWorkerEnv();
            wEnv.addHandler("container", this );
        }

        try {
            // jkMain.setJkHome() XXX;
            
            getJkMain().init();

            headersMsgNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "headerMsg" );
            tmpMessageBytesNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "tmpMessageBytes" );
            utfC2bNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "utfC2B" );
            epNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "ep" );
            obNote=wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE, "coyoteBuffer" );
            inputStreamNote= wEnv.getNoteId( WorkerEnv.ENDPOINT_NOTE,
                                             "jkInputStream");

        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }

    public void start() {
        try {
            getJkMain().start();
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }

    public void destroy() {
        if( !started ) return;

        started = false;
        getJkMain().stop();
    }

    // -------------------- OutputBuffer implementation --------------------

        
    public int doWrite(ByteChunk chunk, Response res) 
        throws IOException
    {
        if (!res.isCommitted()) {
            // Send the connector a request for commit. The connector should
            // then validate the headers, send them (using sendHeader) and 
            // set the filters accordingly.
            res.sendHeaders();
        }
        MsgContext ep=(MsgContext)res.getNote( epNote );

        MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );

        int len=chunk.getLength();
        byte buf[]=msg.getBuffer();
        // 4 - hardcoded, byte[] marshalling overhead 
        int chunkSize=buf.length - msg.getHeaderLength() - 4;
        int off=0;
        while( len > 0 ) {
            int thisTime=len;
            if( thisTime > chunkSize ) {
                thisTime=chunkSize;
            }
            len-=thisTime;
            
            msg.reset();
            msg.appendByte( HandlerRequest.JK_AJP13_SEND_BODY_CHUNK);
            if( log.isDebugEnabled() ) log.debug("doWrite " + off + " " + thisTime + " " + len );
            msg.appendBytes( chunk.getBytes(), chunk.getOffset() + off, thisTime );
            off+=thisTime;
            ep.setType( JkHandler.HANDLE_SEND_PACKET );
            ep.getSource().invoke( msg, ep );
        }
        return 0;
    }
    
    public int doRead(ByteChunk chunk, Request req) 
        throws IOException
    {
        Response res=req.getResponse();
        if( log.isDebugEnabled() )
            log.debug("doRead " + chunk.getBytes() + " " +  chunk.getOffset() + " " + chunk.getLength());
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
        if( logTime.isDebugEnabled() ) 
                ep.setLong( MsgContext.TIMER_PRE_REQUEST, System.currentTimeMillis());
        
        org.apache.coyote.Request req=(org.apache.coyote.Request)ep.getRequest();
        org.apache.coyote.Response res=req.getResponse();
        res.setHook( this );

        if( log.isDebugEnabled() )
            log.debug( "Invoke " + req + " " + res + " " + req.requestURI().toString());
        
        res.setOutputBuffer( this );
        req.setInputBuffer( this );
        
        if( ep.getNote( headersMsgNote ) == null ) {
            Msg msg2=new MsgAjp();
            ep.setNote( headersMsgNote, msg2 );
        }
        
        res.setNote( epNote, ep );
        ep.setStatus( JK_STATUS_HEAD );

        try {
            adapter.service( req, res );
        } catch( Exception ex ) {
            ex.printStackTrace();
        }

        ep.setStatus( JK_STATUS_NEW );

        req.recycle();
        res.recycle();
        return OK;
    }

    private void appendHead(org.apache.coyote.Response res)
        throws IOException
    {
        if( log.isDebugEnabled() )
            log.debug("COMMIT sending headers " + res + " " + res.getMimeHeaders() );
        
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
        
        MessageBytes mb=(MessageBytes)ep.getNote( tmpMessageBytesNote );
        if( mb==null ) {
            mb=new MessageBytes();
            ep.setNote( tmpMessageBytesNote, mb );
        }
        String message=res.getMessage();
        if( message==null ) message= HttpMessages.getMessage(res.getStatus());
        mb.setString( message );
        c2b.convert( mb );
        msg.appendBytes(mb);

        // XXX add headers
        
        MimeHeaders headers=res.getMimeHeaders();
        String contentType = res.getContentType();
        if( contentType != null ) {
            headers.setValue("Content-Type").setString(contentType);
        }
        String contentLanguage = res.getContentLanguage();
        if( contentLanguage != null ) {
            headers.setValue("Content-Language").setString(contentLanguage);
        }
	int contentLength = res.getContentLength();
        if( contentLength >= 0 ) {
            headers.setValue("Content-Length").setInt(contentLength);
        }
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
    }
    
    // -------------------- Coyote Action implementation --------------------
    
    public void action(ActionCode actionCode, Object param) {
        try {
            if( actionCode==ActionCode.ACTION_COMMIT ) {
                if( log.isDebugEnabled() ) log.debug("COMMIT " );
                org.apache.coyote.Response res=(org.apache.coyote.Response)param;

                if(  res.isCommitted() ) {
                    if( log.isInfoEnabled() )
                        log.info("Response already commited " );
                } else {
                    appendHead( res );
                }
            } else if( actionCode==ActionCode.ACTION_RESET ) {
                if( log.isDebugEnabled() )
                    log.debug("RESET " );

            } else if( actionCode==ActionCode.ACTION_CLIENT_FLUSH ) {
                if( log.isDebugEnabled() ) log.debug("CLIENT_FLUSH " );
                org.apache.coyote.Response res=(org.apache.coyote.Response)param;
                MsgContext ep=(MsgContext)res.getNote( epNote );
                ep.setType( JkHandler.HANDLE_FLUSH );
                ep.getSource().invoke( null, ep );
                
            } else if( actionCode==ActionCode.ACTION_CLOSE ) {
                if( log.isDebugEnabled() ) log.debug("CLOSE " );

                org.apache.coyote.Response res=(org.apache.coyote.Response)param;
                MsgContext ep=(MsgContext)res.getNote( epNote );
                if( ep.getStatus()== JK_STATUS_CLOSED ) {
                    // Double close - it may happen with forward 
                    if( log.isDebugEnabled() ) log.debug("Double CLOSE - forward ? " + res.getRequest().requestURI() );
                    return;
                }
                 
                if( !res.isCommitted() )
                    this.action( ActionCode.ACTION_COMMIT, param );
                
                MsgAjp msg=(MsgAjp)ep.getNote( headersMsgNote );
                msg.reset();
                msg.appendByte( HandlerRequest.JK_AJP13_END_RESPONSE );
                msg.appendByte( 1 );
                
                ep.setType( JkHandler.HANDLE_SEND_PACKET );
                ep.getSource().invoke( msg, ep );

                ep.setType( JkHandler.HANDLE_FLUSH );
                ep.getSource().invoke( msg, ep );

                ep.setStatus(JK_STATUS_CLOSED );

                if( logTime.isDebugEnabled() ) 
                    logTime(res.getRequest(), res);
            } else if( actionCode==ActionCode.ACTION_REQ_SSL_ATTRIBUTE ) {
                org.apache.coyote.Request req=(org.apache.coyote.Request)param;

                // Extract SSL certificate information (if requested)
                MessageBytes certString = (MessageBytes)req.getNote(WorkerEnv.SSL_CERT_NOTE);
                if( certString != null ) {
                    ByteChunk certData = certString.getByteChunk();
                    ByteArrayInputStream bais = 
                        new ByteArrayInputStream(certData.getBytes(),
                                                 certData.getStart(),
                                                 certData.getLength());
 
                    // Fill the first element.
                    X509Certificate jsseCerts[] = null;
                    try {
                        CertificateFactory cf =
                            CertificateFactory.getInstance("X.509");
                        X509Certificate cert = (X509Certificate)
                            cf.generateCertificate(bais);
                        jsseCerts =  new X509Certificate[1];
                        jsseCerts[0] = cert;
                    } catch(java.security.cert.CertificateException e) {
                        log.error("Certificate convertion failed" , e );
                        return;
                    }
 
                    req.setAttribute(SSLSupport.CERTIFICATE_KEY, 
                                     jsseCerts);
                }
                
            } else if( actionCode==ActionCode.ACTION_REQ_HOST_ATTRIBUTE ) {
                org.apache.coyote.Request req=(org.apache.coyote.Request)param;

				// If remoteHost not set by JK, get it's name from it's remoteAddr
            	if( req.remoteHost().isNull())
                	req.remoteHost().setString(InetAddress.getByName(req.remoteAddr().toString()).getHostName());

            // } else if( actionCode==ActionCode.ACTION_POST_REQUEST ) {

            } else if( actionCode==ActionCode.ACTION_ACK ) {
                if( log.isDebugEnabled() )
                    log.debug("ACK " );
                // What should we do here ? Who calls it ? 
            }
        } catch( Exception ex ) {
            log.error( "Error in action code ", ex );
        }
    }

    private void logTime(Request req, Response res ) {
        // called after the request
        //            org.apache.coyote.Request req=(org.apache.coyote.Request)param;
        //            Response res=req.getResponse();
        MsgContext ep=(MsgContext)res.getNote( epNote );
        String uri=req.requestURI().toString();
        if( uri.indexOf( ".gif" ) >0 ) return;
        
        ep.setLong( MsgContext.TIMER_POST_REQUEST, System.currentTimeMillis());
        long t1= ep.getLong( MsgContext.TIMER_PRE_REQUEST ) -
            ep.getLong( MsgContext.TIMER_RECEIVED );
        long t2= ep.getLong( MsgContext.TIMER_POST_REQUEST ) -
            ep.getLong( MsgContext.TIMER_PRE_REQUEST );
        
        logTime.debug("Time pre=" + t1 + "/ service=" + t2 + " " +
                      res.getContentLength() + " " + 
                      uri );
    }

    public ObjectName preRegister(MBeanServer server,
                                  ObjectName oname) throws Exception
    {
        // override - we must be registered as "container"
        this.name="container";        
        return super.preRegister(server, oname);
    }
}
