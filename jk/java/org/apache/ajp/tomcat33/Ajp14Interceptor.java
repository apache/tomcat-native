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

package org.apache.ajp.tomcat33;

import java.io.IOException;
import java.net.InetAddress;
import java.net.Socket;

import org.apache.ajp.Ajp13;
import org.apache.ajp.NegociationHandler;
import org.apache.ajp.RequestHandler;
import org.apache.tomcat.core.Context;
import org.apache.tomcat.core.ContextManager;
import org.apache.tomcat.core.Request;
import org.apache.tomcat.core.Response;
import org.apache.tomcat.core.TomcatException;
import org.apache.tomcat.modules.server.PoolTcpConnector;
import org.apache.tomcat.util.buf.UDecoder;
import org.apache.tomcat.util.http.BaseRequest;
import org.apache.tomcat.util.http.Cookies;
import org.apache.tomcat.util.http.HttpMessages;
import org.apache.tomcat.util.net.TcpConnection;
import org.apache.tomcat.util.net.TcpConnectionHandler;

/** Note. PoolTcpConnector is a convenience base class used for
    TCP-based connectors in tomcat33. It allows all those modules
    to share the thread pool and listener code.

    In future it's likely other optimizations will be implemented in
    the PoolTcpConnector, so it's better to use it if you don't have
    a good reason not to ( like a connector for J2ME, where you want
    minimal footprint and don't care about high load )
*/

/** Tomcat 33 module implementing the Ajp14 protocol.
 *
 *  The actual protocol implementation is in Ajp14.java, this is just an
 *  adapter to plug it into tomcat.
 */
public class Ajp14Interceptor extends PoolTcpConnector
    implements  TcpConnectionHandler
{
    int ajp14_note=-1;
    String password;
    RequestHandler reqHandler=new RequestHandler();
    NegociationHandler negHandler=new NegociationHandler();
    
    public Ajp14Interceptor()
    {
        super();
 	super.setSoLinger( 100 );
	super.setTcpNoDelay( true );
    }

    // initialization
    public void engineInit(ContextManager cm) throws TomcatException {
	log("engineInit");
    }

    public void engineStart(ContextManager cm) throws TomcatException {
	super.engineInit( cm );
	ajp14_note=cm.getNoteId( ContextManager.REQUEST_NOTE, "ajp14" );
	super.engineStart(cm);
   }

    
    // -------------------- Ajp14 specific parameters --------------------

    public void setPassword( String s ) {
	this.password=s;
    }

    /**
     * Set the original entropy seed
     */
    public void setSeed(String pseed) 
    {
	negHandler.setSeed( pseed );
    }
    
    // -------------------- PoolTcpConnector --------------------

    /** Called by PoolTcpConnector to allow childs to init.
     */
    protected void localInit() throws Exception {
	ep.setConnectionHandler( this );
    }

    // -------------------- Handler implementation --------------------

    /*  The TcpConnectionHandler interface is used by the PoolTcpConnector to
     *  handle incoming connections.
     */

    /** Called by the thread pool when a new thread is added to the pool,
	in order to create the (expensive) objects that will be stored
	as thread data.
	XXX we should use a single object, not array ( several reasons ),
	XXX Ajp14 should be storead as a request note, to be available in
	all modules
    */
    public Object[] init()
    {
	if( debug > 0 ) log("Init ");
        Object thData[]=new Object[1];
	thData[0]=initRequest( null );
	return thData;
    }

    /** Construct the request object, with probably unnecesary
	sanity tests ( should work without thread pool - but that is
	not supported in PoolTcpConnector, maybe in future )
    */
    private Ajp14Request initRequest(Object thData[] ) {
	if( ajp14_note < 0 ) throw new RuntimeException( "assert: ajp14_note>0" );
	Ajp14Request req=null;
	if( thData != null ) {
	    req=(Ajp14Request)thData[0];
	}
	if( req != null ) {
	    Response res=req.getResponse();
	    req.recycle();
	    res.recycle();
	    // make the note available to other modules
	    req.setNote( ajp14_note, req.ajp13);
	    return req;
	}
	// either thData==null or broken ( req==null)
       	Ajp13 ajp13=new Ajp13(reqHandler);
        negHandler.init( ajp13 );

	negHandler.setContainerSignature( ContextManager.TOMCAT_NAME +
                                          " v" + ContextManager.TOMCAT_VERSION);
	if( password!= null ) {
            negHandler.setPassword( password );
            ajp13.setBackward(false); 
        }

	BaseRequest ajpreq=new BaseRequest();

	req=new Ajp14Request(ajp13, ajpreq);
	Ajp14Response res=new Ajp14Response(ajp13);
	cm.initRequest(req, res);
	return  req;
    }
    
    /** Called whenever a new TCP connection is received. The connection
	is reused.
     */
    public void processConnection(TcpConnection connection, Object thData[])
    {
        try {
	    if( debug>0)
		log( "Received ajp14 connection ");
            Socket socket = connection.getSocket();
	    // assert: socket!=null, connection!=null ( checked by PoolTcpEndpoint )
	    
            socket.setSoLinger( true, 100);

            Ajp14Request req=initRequest( thData );
            Ajp14Response res= (Ajp14Response)req.getResponse();
            Ajp13 ajp13=req.ajp13;
	    BaseRequest ajpReq=req.ajpReq;

            ajp13.setSocket(socket);

	    // first request should be the loginit.
	    int status=ajp13.receiveNextRequest( ajpReq );
	    if( status != 304 )  { // XXX use better codes
		log( "Failure in logInit ");
		return;
	    }

	    status=ajp13.receiveNextRequest( ajpReq );
	    if( status != 304 ) { // XXX use better codes
		log( "Failure in login ");
		return;
	    }
	    
            boolean moreRequests = true;
            while(moreRequests) {
		status=ajp13.receiveNextRequest( ajpReq );

		if( status==-2) {
		    // special case - shutdown
		    // XXX need better communication, refactor it
		    if( !doShutdown(socket.getLocalAddress(),
				    socket.getInetAddress())) {
			moreRequests = false;
			continue;
		    }                        
		}
		
		// 999 low level requests are just ignored (ie cping/cpong)
		if( status  == 200)
		    cm.service(req, res);
		else if (status == 500) {
		    log( "Invalid request received " + req );
		    break;
		}
		
		req.recycle();
		res.recycle();
            }
            if( debug > 0 ) log("Closing ajp14 connection");
            ajp13.close();
	    socket.close();
        } catch (Exception e) {
	    log("Processing connection " + connection, e);
        }
    }

    // We don't need to check isSameAddress if we authenticate !!!
    protected boolean doShutdown(InetAddress serverAddr,
                                 InetAddress clientAddr)
    {
        try {
	    // close the socket connection before handling any signal
	    // but get the addresses first so they are not corrupted			
            if(isSameAddress(serverAddr, clientAddr)) {
		cm.stop();
		// same behavior as in past, because it seems that
		// stopping everything doesn't work - need to figure
		// out what happens with the threads ( XXX )

		// XXX It should work now - but will fail if servlets create
		// threads
		System.exit(0);
	    }
	} catch(Exception ignored) {
	    log("Ignored " + ignored);
	}
	log("Shutdown command ignored");
	return false;
    }

    // legacy, should be removed 
    public void setServer(Object contextM)
    {
        this.cm=(ContextManager)contextM;
    }

    public Object getInfo( Context ctx, Request request,
			   int id, String key ) {
	if( ! ( request instanceof Ajp14Request ) ) {
	    return null;
	}
	Ajp14Request ajp14req = (Ajp14Request)request;
	return ajp14req.ajpReq.getAttribute( key );
    }
    public int setInfo( Context ctx, Request request,
			int id, String key, Object obj ) {
	if( ! ( request instanceof Ajp14Request ) ) {
	    return DECLINED;
	}
	Ajp14Request ajp14req = (Ajp14Request)request;
	ajp14req.ajpReq.setAttribute(key, obj);
	return OK;
    }
		


}


//-------------------- Glue code for request/response.
// Probably not needed ( or can be simplified ), but it's
// not that bad.

class Ajp14Request extends Request 
{
    Ajp13 ajp13;
    BaseRequest ajpReq;
    
    public Ajp14Request(Ajp13 ajp13, BaseRequest ajpReq) 
    {
	headers = ajpReq.headers();
	methodMB=ajpReq.method();
	protoMB=ajpReq.protocol();
	uriMB = ajpReq.requestURI();
	queryMB = ajpReq.queryString();
	remoteAddrMB = ajpReq.remoteAddr();
	remoteHostMB = ajpReq.remoteHost();
	serverNameMB = ajpReq.serverName();

	// XXX sync cookies 
	scookies = new Cookies( headers );
	urlDecoder=new UDecoder();

	// XXX sync headers
	
	params.setQuery( queryMB );
	params.setURLDecoder( urlDecoder );
	params.setHeaders( headers );
	initRequest(); 	

        this.ajp13=ajp13;
	this.ajpReq=ajpReq;
    }

    // -------------------- Wrappers for changed method names, and to use the buffers
    // XXX Move BaseRequest into util !!! ( it's just a stuct with some MessageBytes )

    public int getServerPort() {
        return ajpReq.getServerPort();
    }

    public void setServerPort(int i ) {
	ajpReq.setServerPort( i );
    }

    public  void setRemoteUser( String s ) {
	super.setRemoteUser(s);
	ajpReq.remoteUser().setString(s);
    }

    public String getRemoteUser() {
	String s=ajpReq.remoteUser().toString();
	if( s == null )
	    s=super.getRemoteUser();
	return s;
    }

    public String getAuthType() {
	return ajpReq.authType().toString();
    }
    
    public void setAuthType(String s ) {
	ajpReq.authType().setString(s);
    }

    public String getJvmRoute() {
	return ajpReq.jvmRoute().toString();
    }
    
    public void setJvmRoute(String s ) {
	ajpReq.jvmRoute().setString(s);
    }

    // XXX scheme
    
    public boolean isSecure() {
	return ajpReq.getSecure();
    }
    
    public int getContentLength() {
        int i=ajpReq.getContentLength();
	if( i >= 0 ) return i;
	i= super.getContentLength();
	return i;
    }

    public void setContentLength( int i ) {
	super.setContentLength(i); // XXX sync
    }

    // XXX broken
//     public Iterator getAttributeNames() {
//         return attributes.keySet().iterator();
//     }


    // --------------------

    public void recycle() {
	super.recycle();
	ajpReq.recycle();
	if( ajp13!=null) ajp13.recycle();
    }

    public String dumpRequest() {
	return ajpReq.toString();
    }
    
    // -------------------- 
    
    // XXX This should go away if we introduce an InputBuffer.
    // We almost have it as result of encoding fixes, but for now
    // just keep this here, doesn't hurt too much.
    public int doRead() throws IOException 
    {
	if( available <= 0 )
	    return -1;
	available--;
	return ajp13.reqHandler.doRead(ajp13);
    }
    
    public int doRead(byte[] b, int off, int len) throws IOException 
    {
	if( available <= 0 )
	    return -1;
	int rd=ajp13.reqHandler.doRead( ajp13, b,off, len );
	available -= rd;
	return rd;
    }
    
}

class Ajp14Response extends Response 
{
    Ajp13 ajp13;
    boolean finished=false;
    
    public Ajp14Response(Ajp13 ajp13) 
    {
	super();
	this.ajp13=ajp13;
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

	ajp13.reqHandler.sendHeaders(ajp13, ajp13.outBuf, getStatus(),
				     HttpMessages.getMessage(status),
				     getMimeHeaders());
    } 

    // XXX Can be implemented using module notification, no need to extend
    public void finish() throws IOException 
    {
	if(!finished) {
	    super.finish();
		finished = true; // Avoid END_OF_RESPONSE sent 2 times
	    ajp13.reqHandler.finish(ajp13, ajp13.outBuf);
	}
    }

    // XXX Can be implemented using the buffers, no need to extend
    public void doWrite(  byte b[], int off, int len) throws IOException 
    {
	ajp13.reqHandler.doWrite(ajp13, ajp13.outBuf, b, off, len );
    }
    
}
