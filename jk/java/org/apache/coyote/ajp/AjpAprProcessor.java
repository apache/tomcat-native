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

package org.apache.coyote.ajp;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.InetAddress;

import org.apache.coyote.ActionCode;
import org.apache.coyote.ActionHook;
import org.apache.coyote.Adapter;
import org.apache.coyote.InputBuffer;
import org.apache.coyote.OutputBuffer;
import org.apache.coyote.Request;
import org.apache.coyote.RequestInfo;
import org.apache.coyote.Response;
import org.apache.jk.common.AjpConstants;
import org.apache.tomcat.jni.Address;
import org.apache.tomcat.jni.Sockaddr;
import org.apache.tomcat.jni.Socket;
import org.apache.tomcat.jni.Status;
import org.apache.tomcat.util.buf.ByteChunk;
import org.apache.tomcat.util.buf.HexUtils;
import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.HttpMessages;
import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.net.AprEndpoint;
import org.apache.tomcat.util.net.SSLSupport;
import org.apache.tomcat.util.res.StringManager;
import org.apache.tomcat.util.threads.ThreadWithAttributes;


/**
 * Processes HTTP requests.
 *
 * @author Remy Maucherat
 */
public class AjpAprProcessor implements ActionHook {


    /**
     * Logger.
     */
    protected static org.apache.commons.logging.Log log
        = org.apache.commons.logging.LogFactory.getLog(AjpAprProcessor.class);


    /**
     * The string manager for this package.
     */
    protected static StringManager sm =
        StringManager.getManager(Constants.Package);


    // ----------------------------------------------------------- Constructors


    public AjpAprProcessor(int headerBufferSize, AprEndpoint endpoint) {

        this.endpoint = endpoint;
        
        request = new Request();
        request.setInputBuffer(new SocketInputBuffer());

        response = new Response();
        response.setHook(this);
        response.setOutputBuffer(new SocketOutputBuffer());
        request.setResponse(response);

        readTimeout = endpoint.getFirstReadTimeout() * 1000;

        // Cause loading of HexUtils
        int foo = HexUtils.DEC[0];

    }


    // ----------------------------------------------------- Instance Variables


    /**
     * Associated adapter.
     */
    protected Adapter adapter = null;


    /**
     * Request object.
     */
    protected Request request = null;


    /**
     * Response object.
     */
    protected Response response = null;


    /**
     * Header message. Note that this header is merely the one used during the
     * processing of the first message of a "request", so it might not be a request
     * header. It will stay unchanged during the processing of the whole request. 
     */
    protected AjpMessage headerMessage = new AjpMessage(); 


    /**
     * Message used for output. 
     */
    protected AjpMessage outputMessage = new AjpMessage(); 


    /**
     * Char version of the message header.
     */
    //protected char[] headerChar = new char[8*1024];


    /**
     * Body message.
     */
    protected AjpMessage bodyMessage = new AjpMessage();

    
    /**
     * Body message.
     */
    protected MessageBytes bodyBytes = MessageBytes.newInstance();

    
    /**
     * All purpose response message.
     */
    protected AjpMessage responseMessage = new AjpMessage();
    
    
    /**
     * Read body message.
     */
    protected AjpMessage readBodyMessage = new AjpMessage();

    
    /**
     * State flag.
     */
    protected boolean started = false;


    /**
     * Error flag.
     */
    protected boolean error = false;


    /**
     * Is there an expectation ?
     */
    protected boolean expectation = false;


    /**
     * Use Tomcat authentication ?
     */
    protected boolean tomcatAuthentication = true;


    /**
     * Socket associated with the current connection.
     */
    protected long socket;


    /**
     * Remote Address associated with the current connection.
     */
    protected String remoteAddr = null;


    /**
     * Remote Host associated with the current connection.
     */
    protected String remoteHost = null;


    /**
     * Local Host associated with the current connection.
     */
    protected String localName = null;


    /**
     * Local port to which the socket is connected
     */
    protected int localPort = -1;


    /**
     * Remote port to which the socket is connected
     */
    protected int remotePort = -1;


    /**
     * The local Host address.
     */
    protected String localAddr = null;


    /**
     * Max post size.
     */
    protected int maxPostSize = 2 * 1024 * 1024;


    /**
     * Host name (used to avoid useless B2C conversion on the host name).
     */
    protected char[] hostNameC = new char[0];


    /**
     * Associated endpoint.
     */
    protected AprEndpoint endpoint;


    /**
     * Allow a customized the server header for the tin-foil hat folks.
     */
    protected String server = null;


    /**
     * The socket timeout used when reading the first block of the request
     * header.
     */
    protected long readTimeout;
    
    
    /**
     * Temp message bytes used for processing.
     */
    protected MessageBytes tmpMB = MessageBytes.newInstance();
    
    
    /**
     * Byte chunk for certs.
     */
    protected MessageBytes certificates = MessageBytes.newInstance();
    
    
    /**
     * End of stream flag.
     */
    protected boolean endOfStream = false;
    
    
    /**
     * Body empty flag.
     */
    protected boolean empty = true;
    
    
    /**
     * First read.
     */
    protected boolean first = true;
    
    
    // ------------------------------------------------------------- Properties


    // --------------------------------------------------------- Public Methods


    /**
     * Set the maximum size of a POST which will be buffered in SSL mode.
     */
    public void setMaxPostSize(int mps) {
        maxPostSize = mps;
    }


    /**
     * Return the maximum size of a POST which will be buffered in SSL mode.
     */
    public int getMaxPostSize() {
        return maxPostSize;
    }


    /**
     * Set the server header name.
     */
    public void setServer( String server ) {
        if (server==null || server.equals("")) {
            this.server = null;
        } else {
            this.server = server;
        }
    }

    
    /**
     * Get the server header name.
     */
    public String getServer() {
        return server;
    }


    /** Get the request associated with this processor.
     *
     * @return The request
     */
    public Request getRequest() {
        return request;
    }

    /**
     * Process pipelined HTTP requests using the specified input and output
     * streams.
     *
     * @throws IOException error during an I/O operation
     */
    public boolean process(long socket, long pool)
        throws IOException {
        ThreadWithAttributes thrA=
                (ThreadWithAttributes)Thread.currentThread();
        RequestInfo rp = request.getRequestProcessor();
        thrA.setCurrentStage(endpoint, "parsing http request");
        rp.setStage(org.apache.coyote.Constants.STAGE_PARSE);

        // Set the remote address
        remoteAddr = null;
        remoteHost = null;
        localAddr = null;
        remotePort = -1;
        localPort = -1;

        // Setting up the socket
        this.socket = socket;

        // Error flag
        error = false;

        long soTimeout = endpoint.getSoTimeout();

        boolean openSocket = true;

        while (started && !error) {

            // Parsing the request header
            try {
                // Get first message of the request
                if (!readMessage(headerMessage, true)) {
                    // This means that no data is available right now
                    // (long keepalive), so that the processor should be recycled
                    // and the method should return true
                    rp.setStage(org.apache.coyote.Constants.STAGE_ENDED);
                    break;
                }
                // Check message type, process right away and break if 
                // not regular request processing
                int type = headerMessage.getByte();
                // FIXME: Any other types which should be checked ?
                if (type == Constants.JK_AJP13_CPING_REQUEST) {
                    headerMessage.reset();
                    headerMessage.appendByte(Constants.JK_AJP13_CPONG_REPLY);
                    writeMessage(headerMessage);
                    continue;
                }

                request.setStartTime(System.currentTimeMillis());
            } catch (IOException e) {
                error = true;
                break;
            } catch (Throwable t) {
                log.debug("Error parsing HTTP request", t);
                // 400 - Bad Request
                response.setStatus(400);
                error = true;
            }

            // Setting up filters, and parse some request headers
            thrA.setCurrentStage(endpoint, "prepareRequest");
            rp.setStage(org.apache.coyote.Constants.STAGE_PREPARE);
            try {
                prepareRequest();
                thrA.setParam(endpoint, request.requestURI());
            } catch (Throwable t) {
                log.debug("Error preparing request", t);
                // 400 - Internal Server Error
                response.setStatus(400);
                error = true;
            }

            // Process the request in the adapter
            if (!error) {
                try {
                    thrA.setCurrentStage(endpoint, "service");
                    rp.setStage(org.apache.coyote.Constants.STAGE_SERVICE);
                    adapter.service(request, response);
                } catch (InterruptedIOException e) {
                    error = true;
                } catch (Throwable t) {
                    t.printStackTrace();
                    log.error("Error processing request", t);
                    // 500 - Internal Server Error
                    response.setStatus(500);
                    error = true;
                }
            }

            // If there was an error, make sure the request is counted as
            // and error, and update the statistics counter
            if (error) {
                response.setStatus(500);
            }
            request.updateCounters();

            thrA.setCurrentStage(endpoint, "ended");
            rp.setStage(org.apache.coyote.Constants.STAGE_KEEPALIVE);
            recycle();
            
        }

        // Add the socket to the poller
        if (!error) {
            endpoint.getPoller().add(socket, pool);
        } else {
            openSocket = false;
        }

        rp.setStage(org.apache.coyote.Constants.STAGE_ENDED);
        recycle();

        return openSocket;
        
    }


    // ----------------------------------------------------- ActionHook Methods


    /**
     * Send an action to the connector.
     *
     * @param actionCode Type of the action
     * @param param Action parameter
     */
    public void action(ActionCode actionCode, Object param) {

        if (actionCode == ActionCode.ACTION_COMMIT) {

            if (response.isCommitted())
                return;

            // Validate and write response headers
            try {
                prepareResponse();
            } catch (IOException e) {
                // Set error flag
                error = true;
            }

        } else if (actionCode == ActionCode.ACTION_ACK) {

            // Acknowlege request

            // Send a 100 status back if it makes sense (response not committed
            // yet, and client specified an expectation for 100-continue)

            if ((response.isCommitted()) || !expectation)
                return;

            // FIXME: No way to reply to an expectation in AJP ?

        } else if (actionCode == ActionCode.ACTION_CLIENT_FLUSH) {

            if (response.isCommitted())
                return;

            // Validate and write response headers
            try {
                prepareResponse();
            } catch (IOException e) {
                // Set error flag
                error = true;
            }

        } else if (actionCode == ActionCode.ACTION_CLOSE) {
            // Close

            // End the processing of the current request, and stop any further
            // transactions with the client

            try {
                endRequest();
            } catch (IOException e) {
                // Set error flag
                error = true;
            }

        } else if (actionCode == ActionCode.ACTION_RESET) {

            // Reset response

            // Note: This must be called before the response is committed

        } else if (actionCode == ActionCode.ACTION_CUSTOM) {

            // Do nothing

        } else if (actionCode == ActionCode.ACTION_START) {

            started = true;

        } else if (actionCode == ActionCode.ACTION_STOP) {

            started = false;

        } else if (actionCode == ActionCode.ACTION_REQ_SSL_ATTRIBUTE ) {

            // FIXME: SSL implementation
            
        } else if (actionCode == ActionCode.ACTION_REQ_HOST_ADDR_ATTRIBUTE) {

            // Get remote host address
            if (remoteAddr == null) {
                try {
                    long sa = Address.get(Socket.APR_REMOTE, socket);
                    remoteAddr = Address.getip(sa);
                } catch (Exception e) {
                    log.warn("Exception getting socket information " ,e);
                }
            }
            request.remoteAddr().setString(remoteAddr);

        } else if (actionCode == ActionCode.ACTION_REQ_LOCAL_NAME_ATTRIBUTE) {

            // Get local host name
            if (localName == null) {
                try {
                    long sa = Address.get(Socket.APR_LOCAL, socket);
                    localName = Address.getnameinfo(sa, 0);
                } catch (Exception e) {
                    log.warn("Exception getting socket information " ,e);
                }
            }
            request.localName().setString(localName);

        } else if (actionCode == ActionCode.ACTION_REQ_HOST_ATTRIBUTE) {

            // Get remote host name
            if (remoteHost == null) {
                try {
                    long sa = Address.get(Socket.APR_REMOTE, socket);
                    remoteHost = Address.getnameinfo(sa, 0);
                } catch (Exception e) {
                    log.warn("Exception getting socket information " ,e);
                }
            }
            request.remoteHost().setString(remoteHost);

        } else if (actionCode == ActionCode.ACTION_REQ_LOCAL_ADDR_ATTRIBUTE) {

            // Get local host address
            if (localAddr == null) {
                try {
                    long sa = Address.get(Socket.APR_LOCAL, socket);
                    Sockaddr addr = new Sockaddr();
                    if (Address.fill(addr, sa)) {
                        localAddr = addr.hostname;
                        localPort = addr.port;
                    }
                } catch (Exception e) {
                    log.warn("Exception getting socket information " ,e);
                }
            }

            request.localAddr().setString(localAddr);

        } else if (actionCode == ActionCode.ACTION_REQ_REMOTEPORT_ATTRIBUTE) {

            // Get remote port
            if (remotePort == -1) {
                try {
                    long sa = Address.get(Socket.APR_REMOTE, socket);
                    Sockaddr addr = Address.getInfo(sa);
                    remotePort = addr.port;
                } catch (Exception e) {
                    log.warn("Exception getting socket information " ,e);
                }
            }
            request.setRemotePort(remotePort);

        } else if (actionCode == ActionCode.ACTION_REQ_LOCALPORT_ATTRIBUTE) {

            // Get local port
            if (localPort == -1) {
                try {
                    long sa = Address.get(Socket.APR_LOCAL, socket);
                    Sockaddr addr = new Sockaddr();
                    if (Address.fill(addr, sa)) {
                        localAddr = addr.hostname;
                        localPort = addr.port;
                    }
                } catch (Exception e) {
                    log.warn("Exception getting socket information " ,e);
                }
            }
            request.setLocalPort(localPort);

        } else if (actionCode == ActionCode.ACTION_REQ_SSL_CERTIFICATE) {
            // FIXME: SSL implementation
        }

    }


    // ------------------------------------------------------ Connector Methods


    /**
     * Set the associated adapter.
     *
     * @param adapter the new adapter
     */
    public void setAdapter(Adapter adapter) {
        this.adapter = adapter;
    }


    /**
     * Get the associated adapter.
     *
     * @return the associated adapter
     */
    public Adapter getAdapter() {
        return adapter;
    }


    // ------------------------------------------------------ Protected Methods

    
    /**
     * After reading the request headers, we have to setup the request filters.
     */
    protected void prepareRequest() {

        // Translate the HTTP method code to a String.
        byte methodCode = headerMessage.getByte();
        if (methodCode != Constants.SC_M_JK_STORED) {
            String methodName = Constants.methodTransArray[(int)methodCode - 1];
            request.method().setString(methodName);
        }

        headerMessage.getBytes(request.protocol()); 
        headerMessage.getBytes(request.requestURI());

        headerMessage.getBytes(request.remoteAddr());
        headerMessage.getBytes(request.remoteHost());
        headerMessage.getBytes(request.localName());
        request.setLocalPort(headerMessage.getInt());

        boolean isSSL = headerMessage.getByte() != 0;
        if( isSSL ) {
            // XXX req.setSecure( true );
            request.scheme().setString("https");
        }

        // Decode headers
        MimeHeaders headers = request.getMimeHeaders();

        int hCount = headerMessage.getInt();
        for(int i = 0 ; i < hCount ; i++) {
            String hName = null;
            
            // Header names are encoded as either an integer code starting
            // with 0xA0, or as a normal string (in which case the first
            // two bytes are the length).
            int isc = headerMessage.peekInt();
            int hId = isc & 0xFF;
            
            MessageBytes vMB = null;
            isc &= 0xFF00;
            if(0xA000 == isc) {
                headerMessage.getInt(); // To advance the read position
                hName = Constants.headerTransArray[hId - 1];
                vMB = headers.addValue( hName );
            } else {
                // reset hId -- if the header currently being read
                // happens to be 7 or 8 bytes long, the code below
                // will think it's the content-type header or the
                // content-length header - SC_REQ_CONTENT_TYPE=7,
                // SC_REQ_CONTENT_LENGTH=8 - leading to unexpected
                // behaviour.  see bug 5861 for more information.
                hId = -1;
                headerMessage.getBytes(tmpMB);
                ByteChunk bc = tmpMB.getByteChunk();
                vMB = headers.addValue(bc.getBuffer(),
                        bc.getStart(), bc.getLength());
            }
            
            headerMessage.getBytes(vMB);
            
            if (hId == Constants.SC_REQ_CONTENT_LENGTH ||
                    (hId == -1 && tmpMB.equalsIgnoreCase("Content-Length"))) {
                // just read the content-length header, so set it
                request.setContentLength( vMB.getInt() );
            } else if (hId == Constants.SC_REQ_CONTENT_TYPE ||
                    (hId == -1 && tmpMB.equalsIgnoreCase("Content-Type"))) {
                // just read the content-type header, so set it
                ByteChunk bchunk = vMB.getByteChunk();
                request.contentType().setBytes(bchunk.getBytes(),
                        bchunk.getOffset(),
                        bchunk.getLength());
            }
        }

        // Decode extra attributes
        boolean moreAttr = true;

        while (moreAttr) {
            byte attributeCode = headerMessage.getByte();
            if (attributeCode == Constants.SC_A_ARE_DONE)
                break;

            /* Special case ( XXX in future API make it separate type !)
             */
            if (attributeCode == Constants.SC_A_SSL_KEY_SIZE) {
                // Bug 1326: it's an Integer.
                request.setAttribute(SSLSupport.KEY_SIZE_KEY,
                                 new Integer(headerMessage.getInt()));
               //Integer.toString(msg.getInt()));
            }

            if (attributeCode == Constants.SC_A_REQ_ATTRIBUTE ) {
                // 2 strings ???...
                headerMessage.getBytes(tmpMB);
                String n = tmpMB.toString();
                headerMessage.getBytes(tmpMB);
                String v = tmpMB.toString();
                request.setAttribute(n, v);
                if (log.isTraceEnabled())
                    log.trace("jk Attribute set " + n + "=" + v);
            }

            // 1 string attributes
            switch (attributeCode) {
            case Constants.SC_A_CONTEXT :
                headerMessage.getBytes(tmpMB);
                // nothing
                break;
                
            case Constants.SC_A_SERVLET_PATH :
                headerMessage.getBytes(tmpMB);
                // nothing 
                break;
                
            case Constants.SC_A_REMOTE_USER :
                if (tomcatAuthentication) {
                    // ignore server
                    headerMessage.getBytes(tmpMB);
                } else {
                    headerMessage.getBytes(request.getRemoteUser());
                }
                break;
                
            case Constants.SC_A_AUTH_TYPE :
                if (tomcatAuthentication) {
                    // ignore server
                    headerMessage.getBytes(tmpMB);
                } else {
                    headerMessage.getBytes(request.getAuthType());
                }
                break;
                
            case Constants.SC_A_QUERY_STRING :
                headerMessage.getBytes(request.queryString());
                break;
                
            case Constants.SC_A_JVM_ROUTE :
                headerMessage.getBytes(request.instanceId());
                break;
                
            case Constants.SC_A_SSL_CERT :
                request.scheme().setString("https");
                // SSL certificate extraction is costy, moved to JkCoyoteHandler
                headerMessage.getBytes(certificates);
                break;
                
            case Constants.SC_A_SSL_CIPHER   :
                request.scheme().setString( "https" );
                headerMessage.getBytes(tmpMB);
                request.setAttribute(SSLSupport.CIPHER_SUITE_KEY,
                                 tmpMB.toString());
                break;
                
            case Constants.SC_A_SSL_SESSION  :
                request.scheme().setString( "https" );
                headerMessage.getBytes(tmpMB);
                request.setAttribute(SSLSupport.SESSION_ID_KEY, 
                                  tmpMB.toString());
                break;
                
            case Constants.SC_A_SECRET  :
                headerMessage.getBytes(tmpMB);
                String secret = tmpMB.toString();
                if(log.isInfoEnabled())
                    log.info("Secret: " + secret);
                // FIXME: endpoint note - what's that ?
                // endpoint.setNote(secretNote, secret);
                break;
                
            case Constants.SC_A_STORED_METHOD:
                headerMessage.getBytes(request.method()); 
                break;
                
            default:
                break; // ignore, we don't know about it - backward compat
            }
            
        }

        // Check for a full URI (including protocol://host:port/)
        ByteChunk uriBC = request.requestURI().getByteChunk();
        if (uriBC.startsWithIgnoreCase("http", 0)) {

            int pos = uriBC.indexOf("://", 0, 3, 4);
            int uriBCStart = uriBC.getStart();
            int slashPos = -1;
            if (pos != -1) {
                byte[] uriB = uriBC.getBytes();
                slashPos = uriBC.indexOf('/', pos + 3);
                if (slashPos == -1) {
                    slashPos = uriBC.getLength();
                    // Set URI as "/"
                    request.requestURI().setBytes
                        (uriB, uriBCStart + pos + 1, 1);
                } else {
                    request.requestURI().setBytes
                        (uriB, uriBCStart + slashPos,
                         uriBC.getLength() - slashPos);
                }
                MessageBytes hostMB = headers.setValue("host");
                hostMB.setBytes(uriB, uriBCStart + pos + 3,
                                slashPos - pos - 3);
            }

        }

        MessageBytes valueMB = request.getMimeHeaders().getValue("host");
        parseHost(valueMB);

    }


    /**
     * Parse host.
     */
    public void parseHost(MessageBytes valueMB) {

        if (valueMB == null || valueMB.isNull()) {
            // HTTP/1.0
            // Default is what the socket tells us. Overriden if a host is
            // found/parsed
            request.setServerPort(endpoint.getPort()/*socket.getLocalPort()*/);
            InetAddress localAddress = endpoint.getAddress()/*socket.getLocalAddress()*/;
            // Setting the socket-related fields. The adapter doesn't know
            // about socket.
            request.setLocalHost(localAddress.getHostName());
            request.serverName().setString(localAddress.getHostName());
            return;
        }

        ByteChunk valueBC = valueMB.getByteChunk();
        byte[] valueB = valueBC.getBytes();
        int valueL = valueBC.getLength();
        int valueS = valueBC.getStart();
        int colonPos = -1;
        if (hostNameC.length < valueL) {
            hostNameC = new char[valueL];
        }

        boolean ipv6 = (valueB[valueS] == '[');
        boolean bracketClosed = false;
        for (int i = 0; i < valueL; i++) {
            char b = (char) valueB[i + valueS];
            hostNameC[i] = b;
            if (b == ']') {
                bracketClosed = true;
            } else if (b == ':') {
                if (!ipv6 || bracketClosed) {
                    colonPos = i;
                    break;
                }
            }
        }

        if (colonPos < 0) {
            if (request.scheme().equalsIgnoreCase("https")) {
                // 443 - Default HTTPS port
                request.setServerPort(443);
            } else {
                // 80 - Default HTTTP port
                request.setServerPort(80);
            }
            request.serverName().setChars(hostNameC, 0, valueL);
        } else {

            request.serverName().setChars(hostNameC, 0, colonPos);

            int port = 0;
            int mult = 1;
            for (int i = valueL - 1; i > colonPos; i--) {
                int charValue = HexUtils.DEC[(int) valueB[i + valueS]];
                if (charValue == -1) {
                    // Invalid character
                    error = true;
                    // 400 - Bad request
                    response.setStatus(400);
                    break;
                }
                port = port + (charValue * mult);
                mult = 10 * mult;
            }
            request.setServerPort(port);

        }

    }


    /**
     * When committing the response, we have to validate the set of headers, as
     * well as setup the response filters.
     */
    protected void prepareResponse()
        throws IOException {

        response.setCommitted(true);
        
        outputMessage.reset();
        outputMessage.appendByte(Constants.JK_AJP13_SEND_HEADERS);
        outputMessage.appendInt(response.getStatus());
        
        String message = response.getMessage();
        if (message == null){
            message= HttpMessages.getMessage(response.getStatus());
        } else {
            message = message.replace('\n', ' ').replace('\r', ' ');
        }
        tmpMB.setString(message);
        outputMessage.appendBytes(tmpMB);

        // XXX add headers
        
        MimeHeaders headers = response.getMimeHeaders();
        String contentType = response.getContentType();
        if (contentType != null) {
            headers.setValue("Content-Type").setString(contentType);
        }
        String contentLanguage = response.getContentLanguage();
        if (contentLanguage != null) {
            headers.setValue("Content-Language").setString(contentLanguage);
        }
        int contentLength = response.getContentLength();
        if (contentLength >= 0) {
            headers.setValue("Content-Length").setInt(contentLength);
        }
        int numHeaders = headers.size();
        outputMessage.appendInt(numHeaders);
        for (int i = 0; i < numHeaders; i++) {
            MessageBytes hN = headers.getName(i);
            outputMessage.appendBytes(hN);
            MessageBytes hV=headers.getValue(i);
            outputMessage.appendBytes(hV);
        }
        writeMessage(outputMessage);

    }

    
    /**
     * Finish AJP response.
     */
    protected void endRequest()
        throws IOException {

        if (!response.isCommitted()) {
            // Validate and write response headers
            try {
                prepareResponse();
            } catch (IOException e) {
                // Set error flag
                error = true;
            }
        }

        outputMessage.reset();
        outputMessage.appendByte(Constants.JK_AJP13_END_RESPONSE);
        outputMessage.appendByte(1);
        writeMessage(outputMessage);
    }
    
    
    /** Receive a chunk of data. Called to implement the
     *  'special' packet in ajp13 and to receive the data
     *  after we send a GET_BODY packet
     */
    public boolean receive() throws IOException {
        first = false;
        bodyMessage.reset();
        readMessage(bodyMessage, false);
        if( log.isDebugEnabled() )
            log.info( "Receiving: getting request body chunk " + bodyMessage.getLen() );
        
        // No data received.
        if( bodyMessage.getLen() == 0 ) { // just the header
            // Don't mark 'end of stream' for the first chunk.
            // end_of_stream = true;
            return false;
        }
        int blen = bodyMessage.peekInt();

        if( blen == 0 ) {
            return false;
        }

        if( log.isTraceEnabled() ) {
            bodyMessage.dump("Body buffer");
        }
        
        bodyMessage.getBytes(bodyBytes);
        if( log.isTraceEnabled() )
            log.trace( "Data:\n" + bodyBytes);
        empty = false;
        return true;
    }
    
    /**
     * Get more request body data from the web server and store it in the 
     * internal buffer.
     *
     * @return true if there is more data, false if not.    
     */
    private boolean refillReadBuffer() throws IOException 
    {
        // If the server returns an empty packet, assume that that end of
        // the stream has been reached (yuck -- fix protocol??).
        // FIXME: FORM support here ?
        //if(replay) {
        //    endOfStream = true; // we've read everything there is
        //}
        if (endOfStream) {
            if( log.isDebugEnabled() ) 
                log.debug("refillReadBuffer: end of stream " );
            return false;
        }

        // Why not use outBuf??
        readBodyMessage.reset();
        readBodyMessage.appendByte(Constants.JK_AJP13_GET_BODY_CHUNK);
        readBodyMessage.appendInt(Constants.MAX_READ_SIZE);
        writeMessage(readBodyMessage);

        // In JNI mode, response will be in bodyMsg. In TCP mode, response need to be
        // read

        boolean moreData=receive();
        if( !moreData ) {
            endOfStream = true;
        }
        return moreData;
    }
    

    /**
     * Read an AJP message.
     * 
     * @param first is true if the message is the first in the request, which
     *        will cause a short duration blocking read
     * @return true if the message has been read, false if the short read 
     *         didn't return anything
     * @throws IOException any other failure, including incomplete reads
     */
    protected boolean readMessage(AjpMessage message, boolean first)
        throws IOException {
        
        byte[] buf = message.getBuffer();
        int headerLength = message.getHeaderLength();

        // Read the message header
        // FIXME: better buffering to avoid doing two reads !!!!
        if (first) {
            int nRead = Socket.recvt
                (socket, buf, 0, headerLength, readTimeout);
            // Note: Unlike before, I assume it is not acceptable to do more than
            // one read for the first four bytes
            if (nRead == headerLength) {
                message.processHeader();
            } else {
                if ((-nRead) == Status.ETIMEDOUT || (-nRead) == Status.TIMEUP) {
                    return false;
                } else {
                    throw new IOException(sm.getString("iib.failedread"));
                }
            }
        } else {
            int nRead = Socket.recv(socket, buf, 0, headerLength);
            if (nRead == headerLength) {
                message.processHeader();
            } else {
                throw new IOException(sm.getString("iib.failedread"));
            }
        }
        
        // Read the full message body; incomplete reads is a protocol error
        int messageLength = message.getLen();
        int pos = headerLength;
        while (pos < headerLength + messageLength) {
            int nRead = Socket.recv(socket, buf, pos, headerLength + messageLength - pos);
            if (nRead > 0) {
                pos += nRead;
            } else {
                throw new IOException(sm.getString("iib.failedread"));
            }
        }
        return true;
    }
    
    
    /**
     * Send the specified AJP message.
     *   
     * @param message to send
     * @throws IOException IO error when writing the message
     */
    protected void writeMessage(AjpMessage message) 
        throws IOException {
        message.end();
        if (Socket.send(socket, message.getBuffer(), 0, message.getLen()) < 0)
            throw new IOException(sm.getString("iib.failedwrite"));
    }


    /**
     * Recycle the processor.
     */
    public void recycle() {

        // Recycle Request object
        first = true;
        endOfStream = false;
        empty = true;
        request.recycle();
        response.recycle();
        headerMessage.reset();

    }


    // ------------------------------------- InputStreamInputBuffer Inner Class


    /**
     * This class is an input buffer which will read its data from an input
     * stream.
     */
    protected class SocketInputBuffer 
        implements InputBuffer {


        /**
         * Read bytes into the specified chunk.
         */
        public int doRead(ByteChunk chunk, Request req ) 
            throws IOException {

            if (endOfStream) {
                return -1;
            }
            if (first) {
                // Handle special first-body-chunk
                if (!receive()) {
                    return 0;
                }
            } else if (empty) {
                if (!refillReadBuffer()) {
                    return -1;
                }
            }
            ByteChunk bc = bodyBytes.getByteChunk();
            chunk.setBytes( bc.getBuffer(), bc.getStart(), bc.getLength() );
            empty = true;
            return chunk.getLength();

        }


    }


    // ----------------------------------- OutputStreamOutputBuffer Inner Class


    /**
     * This class is an output buffer which will write data to an output
     * stream.
     */
    protected class SocketOutputBuffer 
        implements OutputBuffer {


        /**
         * Write chunk.
         */
        public int doWrite(ByteChunk chunk, Response res) 
            throws IOException {

            if (!response.isCommitted()) {
                // Send the connector a request for commit. The connector should
                // then validate the headers, send them (using sendHeader) and 
                // set the filters accordingly.
                response.action(ActionCode.ACTION_COMMIT, null);
            }

            int len = chunk.getLength();
            byte buf[] = bodyMessage.getBuffer();
            // 4 - hardcoded, byte[] marshalling overhead 
            int chunkSize=buf.length - bodyMessage.getHeaderLength() - 4;
            int off=0;
            while( len > 0 ) {
                int thisTime=len;
                if( thisTime > chunkSize ) {
                    thisTime=chunkSize;
                }
                len -= thisTime;
                
                // FIXME: Don't use a temp buffer
                bodyMessage.reset();
                bodyMessage.appendByte( AjpConstants.JK_AJP13_SEND_BODY_CHUNK);
                if (log.isTraceEnabled()) 
                    log.trace("doWrite " + off + " " + thisTime + " " + len);
                bodyMessage.appendBytes(chunk.getBytes(), chunk.getOffset() + off, thisTime);
                off += thisTime;
                writeMessage(bodyMessage);
            }
            
            return chunk.getLength();

        }


    }


}
