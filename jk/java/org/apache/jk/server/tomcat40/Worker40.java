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
import java.net.*;
import java.util.*;

import org.apache.jk.*;
import org.apache.tomcat.modules.server.PoolTcpConnector;

import org.apache.tomcat.util.net.*;
import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.log.*;
import org.apache.tomcat.util.http.*;

import org.apache.jk.core.*;

import org.apache.catalina.*;

/** Tomcat 40 worker
 *
 */
public class Worker40 extends Worker
{
    private int reqNote;
    Container container;
    
    public Worker40()
    {
        super();
    }

    public void setContainer( Container ct40 ) {
        this.container=ct40;
    }

    public void init(WorkerEnv we) throws IOException {
        reqNote=we.getNoteId( WorkerEnv.REQUEST_NOTE, "tomcat40Request" );
    }
    
    public void service( BaseRequest req, Channel ch, Endpoint ep )
        throws IOException
    {
        d("Incoming request " );
                
        JkRequest40 treq=(JkRequest40)req.getNote( reqNote );
        JkResponse40  tres;
        if( treq==null ) {
            treq=new JkRequest40();
            req.setNote( reqNote, treq );
            tres=new JkResponse40(we);
            treq.setResponse( tres );
            tres.setRequest( treq );
        }
        tres=(JkResponse40)treq.getResponse();
        treq.setEndpoint( ch, ep );
        treq.setBaseRequest( req );
        tres.setEndpoint( ch, ep );

        try {
            container.invoke( treq, tres );
        } catch(Throwable ex ) {
            ex.printStackTrace();
        }
        d("Finishing response");
        tres.finishResponse();
        treq.finishRequest();

        treq.recycle();
        tres.recycle();
    }

    private static final int dL=0;
    private static void d(String s ) {
        System.err.println( "Worker40: " + s );
    }

    
    // -------------------- Handler implementation --------------------

//     /** Construct the request object, with probably unnecesary
// 	sanity tests ( should work without thread pool - but that is
// 	not supported in PoolTcpConnector, maybe in future )
//     */
//     private Ajp14Request initRequest(Object thData[] ) {
// 	if( ajp14_note < 0 ) throw new RuntimeException( "assert: ajp14_note>0" );
// 	Ajp14Request req=null;
// 	if( thData != null ) {
// 	    req=(Ajp14Request)thData[0];
// 	}
// 	if( req != null ) {
// 	    Response res=req.getResponse();
// 	    req.recycle();
// 	    res.recycle();
// 	    // make the note available to other modules
// 	    req.setNote( ajp14_note, req.ajp13);
// 	    return req;
// 	}
// 	// either thData==null or broken ( req==null)
//        	Ajp13 ajp13=new Ajp13(reqHandler);
//         negHandler.init( ajp13 );

// 	negHandler.setContainerSignature( ContextManager.TOMCAT_NAME +
//                                           " v" + ContextManager.TOMCAT_VERSION);
// 	if( password!= null ) {
//             negHandler.setPassword( password );
//             ajp13.setBackward(false); 
//         }

// 	BaseRequest ajpreq=new BaseRequest();

// 	req=new Ajp14Request(ajp13, ajpreq);
// 	Ajp14Response res=new Ajp14Response(ajp13);
// 	cm.initRequest(req, res);
// 	return  req;
//     }
    
//     /** Called whenever a new TCP connection is received. The connection
// 	is reused.
//      */
//     public void processConnection(TcpConnection connection, Object thData[])
//     {
//         try {
// 	    if( debug>0)
// 		log( "Received ajp14 connection ");
//             Socket socket = connection.getSocket();
// 	    // assert: socket!=null, connection!=null ( checked by PoolTcpEndpoint )
	    
//             socket.setSoLinger( true, 100);

//             Ajp14Request req=initRequest( thData );
//             Ajp14Response res= (Ajp14Response)req.getResponse();
//             Ajp13 ajp13=req.ajp13;
// 	    BaseRequest ajpReq=req.ajpReq;

//             ajp13.setSocket(socket);

// 	    // first request should be the loginit.
// 	    int status=ajp13.receiveNextRequest( ajpReq );
// 	    if( status != 304 )  { // XXX use better codes
// 		log( "Failure in logInit ");
// 		return;
// 	    }

// 	    status=ajp13.receiveNextRequest( ajpReq );
// 	    if( status != 304 ) { // XXX use better codes
// 		log( "Failure in login ");
// 		return;
// 	    }
	    
//             boolean moreRequests = true;
//             while(moreRequests) {
// 		status=ajp13.receiveNextRequest( ajpReq );

// 		if( status==-2) {
// 		    // special case - shutdown
// 		    // XXX need better communication, refactor it
// 		    if( !doShutdown(socket.getLocalAddress(),
// 				    socket.getInetAddress())) {
// 			moreRequests = false;
// 			continue;
// 		    }                        
// 		}
		
// 		if( status  == 200)
// 		    cm.service(req, res);
// 		else if (status == 500) {
// 		    log( "Invalid request received " + req );
// 		    break;
// 		}
		
// 		req.recycle();
// 		res.recycle();
//             }
//             if( debug > 0 ) log("Closing ajp14 connection");
//             ajp13.close();
// 	    socket.close();
//         } catch (Exception e) {
// 	    log("Processing connection " + connection, e);
//         }
//     }

//     // We don't need to check isSameAddress if we authenticate !!!
//     protected boolean doShutdown(InetAddress serverAddr,
//                                  InetAddress clientAddr)
//     {
//         try {
// 	    // close the socket connection before handling any signal
// 	    // but get the addresses first so they are not corrupted			
//             if(isSameAddress(serverAddr, clientAddr)) {
// 		cm.stop();
// 		// same behavior as in past, because it seems that
// 		// stopping everything doesn't work - need to figure
// 		// out what happens with the threads ( XXX )

// 		// XXX It should work now - but will fail if servlets create
// 		// threads
// 		System.exit(0);
// 	    }
// 	} catch(Exception ignored) {
// 	    log("Ignored " + ignored);
// 	}
// 	log("Shutdown command ignored");
// 	return false;
//     }

//     // legacy, should be removed 
//     public void setServer(Object contextM)
//     {
//         this.cm=(ContextManager)contextM;
//     }
    

}
