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

package org.apache.coyote.tomcat3;

import java.util.Enumeration;
import java.util.Hashtable;

import org.apache.coyote.ActionCode;
import org.apache.coyote.ProtocolHandler;
import org.apache.tomcat.core.BaseInterceptor;
import org.apache.tomcat.core.Context;
import org.apache.tomcat.core.ContextManager;
import org.apache.tomcat.core.TomcatException;
import org.apache.tomcat.util.IntrospectionUtils;
import org.apache.tomcat.util.net.SSLSupport;

/** Standalone http.
 *
 *  Connector properties:
 *  <ul>
 *  <li> secure - will load a SSL socket factory and act as https server</li>
 *  </ul>
 *
 *  Properties passed to the net layer:
 *  <ul>
 *  <li>timeout</li>
 *  <li>backlog</li>
 *  <li>address</li>
 *  <li>port</li>
 *  </ul>
 * Thread pool properties:
 *  <ul>
 *  <li>minSpareThreads</li>
 *  <li>maxSpareThreads</li>
 *  <li>maxThreads</li>
 *  <li>poolOn</li>
 *  </ul>
 * Properties for HTTPS:
 *  <ul>
 *  <li>keystore - certificates - default to ~/.keystore</li>
 *  <li>keypass - password</li>
 *  <li>clientauth - true if the server should authenticate the client using certs</li>
 *  </ul>
 * Properties for HTTP:
 *  <ul>
 *  <li>reportedname - name of server sent back to browser (security purposes)</li>
 *  </ul>
 *  <ul>
 *  <li>compression - use gzip compression in HTTP 1.1 (on/off) - def off</li>
 *  </ul>
 *  <ul>
 *  <li>compressionMinSize - minimum size content to use gzip compression in HTTP 1.1 - def 2048</li>
 *  </ul>
 *  <ul>
 *  <li>noCompressionUserAgents - comma separated list of userAgents who didn't support gzip</li>
 *  </ul>
 *  <ul>
 *  <li>restrictedUserAgents - comma separated list of userAgents who didn't support HTTP 1.1 (use HTTP 1.0)</li>
 *  </ul>
 *  <ul>
 *  <li>compressableMimeTypes - comma separated list of mime types supported for compression - def text/html,text/xml,text/plain</li>
 *  </ul>
 */
public class CoyoteInterceptor2 extends BaseInterceptor
{
    private String processorClassName="org.apache.coyote.http11.Http11Protocol";
    Tomcat3Adapter adapter;
    ProtocolHandler proto;
    int protocolNote;
    
    public CoyoteInterceptor2() {
	super();
	// defaults:
        this.setAttribute( "port", "8080" );
        this.setAttribute( "soLinger", "-1" );
    }

    // -------------------- PoolTcpConnector --------------------

    /** Set the class of the processor to use.
     */
    public void setProcessorClassName(String pcn) {
	processorClassName = pcn;
    }

    // -------------------- Start/stop --------------------
    Hashtable attributes=new Hashtable();
    
    public void setAttribute( String prop, Object value) {
	attributes.put( prop, value );
    }


    public void setProperty( String prop, String value ) {
        setAttribute( prop, value );
    }

    /** Called when the ContextManger is started
     */
    public void engineInit(ContextManager cm) throws TomcatException {
	super.engineInit( cm );

        protocolNote = cm.getNoteId(ContextManager.MODULE_NOTE,
				    "coyote.protocol");
        adapter=new Tomcat3Adapter(cm);
        try {
            Class c=Class.forName(processorClassName);
            proto=(ProtocolHandler)c.newInstance();
	    setNote(protocolNote, proto);
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
        
        this.setAttribute("jkHome", cm.getHome());

        proto.setAdapter( adapter );
        try {
            Enumeration keys=attributes.keys();
            while( keys.hasMoreElements() ) {
                String k=(String)keys.nextElement();
                Object o=attributes.get(k);
                if( o instanceof String )
                    IntrospectionUtils.setProperty( proto, k, (String)o );
                else
                    IntrospectionUtils.setAttribute( proto, k, o );
            }
	    proto.init();
        } catch( Exception ex ) {
            throw new TomcatException( "Error setting protocol properties ", ex );
        }
    }

    /** Called when the ContextManger is started
     */
    public void engineStart(ContextManager cm) throws TomcatException {
	try {
            proto.start();
	} catch( Exception ex ) {
            ex.printStackTrace();
	    throw new TomcatException( ex );
	}
    }

    public void engineShutdown(ContextManager cm) throws TomcatException {
	try {
	    proto.destroy();	
        } catch( Exception ex ) {
	    throw new TomcatException( ex );
	}
    }

    // -------------------- Handler implementation --------------------

    /** Handle HTTP expectations.
     */
    public int preService(org.apache.tomcat.core.Request request,
                          org.apache.tomcat.core.Response response) {
	if(response instanceof Tomcat3Response) {
	    try {
		((Tomcat3Response)response).sendAcknowledgement();
	    } catch(Exception ex) {
		log("Can't send ACK", ex);
	    }
	}
	return 0;
    }

    public int postRequest(org.apache.tomcat.core.Request request,
                           org.apache.tomcat.core.Response response) {
	if(request instanceof Tomcat3Request) {
	    try {
                Tomcat3Request httpReq=(Tomcat3Request)request;
                org.apache.coyote.Request cReq = httpReq.getCoyoteRequest();
                cReq.action( ActionCode.ACTION_POST_REQUEST , null);
	    } catch(Exception ex) {
		log("Can't send ACK", ex);
	    }
	}
        return 0;
    }
    
    /**
       getInfo calls for SSL data
       
       @return the requested data
    */
    public Object getInfo( Context ctx, org.apache.tomcat.core.Request request,
                           int id, String key ) {
        if( ! ( request instanceof Tomcat3Request ) )
            return null;

        Tomcat3Request httpReq=(Tomcat3Request)request;
        
        if(key!=null && httpReq!=null ){
            org.apache.coyote.Request cReq = httpReq.getCoyoteRequest();
            Object info = cReq.getAttribute(key);
            if( info != null)
                return info;
            // XXX Should use MsgContext, pass the attribute we need.
            // This will extract both 
            if(isSSLAttribute(key)) {
                cReq.action(ActionCode.ACTION_REQ_SSL_ATTRIBUTE,
                            httpReq.getCoyoteRequest() );
		// Only allowed a single cert under the 2.2 Spec.
		Object [] value = (Object []) cReq.getAttribute(SSLSupport.CERTIFICATE_KEY);
		if( value != null ) {
		    cReq.setAttribute(SSLSupport.CERTIFICATE_KEY, value[0]);
		}
		
                return cReq.getAttribute(key);
            }

            return cReq.getAttribute( key );
        }
        return super.getInfo(ctx,request,id,key);
    }

    public int setInfo( Context ctx, org.apache.tomcat.core.Request request,
                         int id, String key, String object ) {
        if( ! ( request instanceof Tomcat3Request ) )
            return DECLINED;
        
        Tomcat3Request httpReq=(Tomcat3Request)request;
        
        if(key!=null && httpReq!=null ){
            org.apache.coyote.Request cReq = httpReq.getCoyoteRequest();
            cReq.setAttribute(key, object);
	    return OK;
        }
	return super.setInfo(ctx, request, id, key, object);
    }

    /**
     * Check if a string is a reserved SSL attribute key.
     */
    public static boolean isSSLAttribute(String key) {
	return SSLSupport.CIPHER_SUITE_KEY.equals(key) ||
	    SSLSupport.KEY_SIZE_KEY.equals(key)        ||
	    SSLSupport.CERTIFICATE_KEY.equals(key)     ||
	    SSLSupport.SESSION_ID_KEY.equals(key);
    }
}

