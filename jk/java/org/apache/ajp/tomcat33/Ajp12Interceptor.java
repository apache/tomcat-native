/*
 *
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


/*
  Based on Ajp11ConnectionHandler and Ajp12 implementation of JServ
*/
package org.apache.ajp.tomcat33;

import java.io.*;
import java.net.*;
import java.util.*;
import org.apache.tomcat.core.*;
import org.apache.tomcat.util.net.*;
import org.apache.tomcat.util.*;
import org.apache.tomcat.util.http.*;

/* 
 */
public class Ajp12Interceptor extends PoolTcpConnector
    implements  TcpConnectionHandler{
    private boolean tomcatAuthentication=true;
    String secret;
    
    public Ajp12Interceptor() {
	super();
    }
    // -------------------- PoolTcpConnector --------------------

    protected void localInit() throws Exception {
	ep.setConnectionHandler( this );
    }

    /** Enable the use of a stop secret. The secret will be
     *  randomly generated.
     */
    public void setUseSecret(boolean b ) {
	secret=Double.toString(Math.random());
    }

    /** Explicitely set the stop secret
     */
    public void setSecret( String s ) {
	secret=s;
    }
    
    public void engineState(ContextManager cm, int state )
	throws TomcatException
    {
	if( state!=ContextManager.STATE_START )
	    return;
	// the engine is now started, create the ajp12.id
	// file that will allow us to stop the server and
	// know that the server is started ok.
	Ajp12Interceptor tcpCon=this;
	int portInt=tcpCon.getPort();
	InetAddress address=tcpCon.getAddress();
	try {
	    PrintWriter stopF=new PrintWriter
		(new FileWriter(cm.getHome() + "/conf/ajp12.id"));
	    stopF.println( portInt );
	    if( address==null )
		stopF.println( "" );
	    else
		stopF.println( address.getHostAddress() );
	    if( secret !=null )
		stopF.println( secret );
	    else
		stopF.println();
	    stopF.close();
	} catch( IOException ex ) {
	    log( "Can't create ajp12.id " + ex );
	}
    }

    // -------------------- Handler implementation --------------------

    public Object[] init() {
	Object thData[]=new Object[2];
	AJP12Request reqA=new AJP12Request();
	reqA.setSecret( secret );
	AJP12Response resA=new AJP12Response();
	cm.initRequest( reqA, resA );
	thData[0]=reqA;
	thData[1]=resA;

	return  thData;
    }

    public void setServer( Object cm ) {
	this.cm=(ContextManager )cm;
    }

    public void processConnection(TcpConnection connection, Object[] thData) {
        try {
	    // XXX - Add workarounds for the fact that the underlying
	    // serverSocket.accept() call can now time out.  This whole
	    // architecture needs some serious review.
	    if (connection == null)
		return;
	    Socket socket=connection.getSocket();
	    if (socket == null)
		return;

	    socket.setSoLinger( true, 100);
	    //	    socket.setSoTimeout( 1000); // or what ?

	    AJP12Request reqA=null;
	    AJP12Response resA=null;

	    if( thData != null ) {
		reqA=(AJP12Request)thData[0];
		resA=(AJP12Response)thData[1];
		if( reqA!=null ) reqA.recycle();
		if( resA!=null ) resA.recycle();
//XXX is needed to revert the tomcat auth state? if yes put it into recycle and
//    and uncomment here                
//                ((AJP12Request)reqA).setTomcatAuthentication(isTomcatAuthtentication());
	    }

	    if( reqA==null || resA==null ) {
		reqA = new AJP12Request();
		reqA.setSecret( secret );
                ((AJP12Request)reqA).setTomcatAuthentication(
                                        isTomcatAuthentication());
		resA=new AJP12Response();
		cm.initRequest( reqA, resA );
	    }

	    reqA.setSocket( socket );
	    resA.setSocket( socket );

	    reqA.readNextRequest();

	    if( reqA.internalAjp() )
		return;

	    cm.service( reqA, resA );
	    socket.close();
	} catch (Exception e) {
	    log("HANDLER THREAD PROBLEM", e);
	}
    }

    public boolean isTomcatAuthentication() {
        return tomcatAuthentication;
    }

    public void setTomcatAuthentication(boolean newTomcatAuthentication) {
        tomcatAuthentication = newTomcatAuthentication;
    }
}

class AJP12Request extends Request {
    Ajp12 ajp12=new Ajp12();

    public AJP12Request() {
    }

    void setSecret( String s ) {
	ajp12.setSecret( s );
    }
    
    public boolean internalAjp() {
	return ajp12.isPing ||
	    ajp12.shutdown;
    }

    public void readNextRequest() throws IOException {
	ajp12.readNextRequest( this );
    }

    public void setSocket( Socket s ) throws IOException {
	ajp12.setSocket( s );
    }

    public int doRead() throws IOException {
	if( available <= 0 )
	    return -1;
	available--;
	return ajp12.doRead();
    }

    public  int doRead( byte b[], int off, int len ) throws IOException {
	if( available <= 0 )
	    return -1;
	int rd=ajp12.doRead( b,off,len);
	available -= rd;
	return rd;
    }

    public boolean isTomcatAuthentication() {
        return ajp12.isTomcatAuthentication();
    }

    public void setTomcatAuthentication(boolean newTomcatAuthentication) {
        ajp12.setTomcatAuthentication(newTomcatAuthentication);
    }
}


// Ajp use Status: instead of Status
class AJP12Response extends Response {
    Http10 http=new Http10();

    public void recycle() {
        super.recycle();
        http.recycle();
    }

    public void setSocket( Socket s ) throws IOException {
	http.setSocket( s );
    }

    public void endHeaders()  throws IOException {
	super.endHeaders();
	sendStatus( status, HttpMessages.getMessage( status ));
	http.sendHeaders( getMimeHeaders() );
    }

    public void doWrite( byte buffer[], int pos, int count)
	throws IOException
    {
	http.doWrite( buffer, pos, count);
    }

    /** Override setStatus
     */
    protected void sendStatus( int status, String message)  throws IOException {
	http.printHead("Status: " );
	http.printHead( String.valueOf( status ));
	http.printHead( " " );
	http.printHead( message );
	http.printHead("\r\n");

	// Servlet Engine header will be set per/adapter - smarter adapters will
	// not send it every time ( have it in C side ), and we may also want
	// to add informations about the adapter used 
	if( request.getContext() != null)
	    setHeader("Servlet-Engine", request.getContext().getEngineHeader());

    }
}
