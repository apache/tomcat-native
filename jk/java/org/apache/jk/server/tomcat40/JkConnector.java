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
import java.security.*;
import java.util.*;

import org.apache.catalina.*;
import org.apache.catalina.core.*;
import org.apache.catalina.net.DefaultServerSocketFactory;
import org.apache.catalina.net.ServerSocketFactory;
import org.apache.catalina.util.LifecycleSupport;
import org.apache.catalina.util.StringManager;

import org.apache.jk.server.*;

import org.apache.catalina.jk.*;

/**
 * Implementation of an Jk connector.
 *
 * @author Kevin Seguin
 * @author Costin Manolache
 */
public final class JkConnector
    implements Connector, Lifecycle {

    /**
     * The Container used for processing requests received by this Connector.
     */
    protected Container container = null;
    protected LifecycleSupport lifecycle = new LifecycleSupport(this);
    private boolean started = false;
    private boolean stopped = false;
    private Service service = null;


    // ------------------------------------------------------------- Properties

    /**
     * Return the Container used for processing requests received by this
     * Connector.
     */
    public Container getContainer() {
	return container;
    }

    /**
     * Set the Container used for processing requests received by this
     * Connector.
     *
     * @param container The new Container to use
     */
    public void setContainer(Container container) {
	this.container = container;
    }


    /**
     * Return descriptive information about this Connector implementation.
     */
    public String getInfo() {
	return "JkConnector/2.0dev";
    }

    /**
     * Returns the <code>Service</code> with which we are associated.
     */
    public Service getService() {
	return service;
    }

    /**
     * Set the <code>Service</code> with which we are associated.
     */
    public void setService(Service service) {
	this.service = service;
    }

    // --------------------------------------------------------- Public Methods

    /**
     * Invoke a pre-startup initialization. This is used to allow connectors
     * to bind to restricted ports under Unix operating environments.
     * ServerSocket (we start as root and change user? or I miss something?).
     */
    public void initialize() throws LifecycleException {
    }

    // ------------------------------------------------------ Lifecycle Methods


    /**
     * Add a lifecycle event listener to this component.
     *
     * @param listener The listener to add
     */
    public void addLifecycleListener(LifecycleListener listener) {
	lifecycle.addLifecycleListener(listener);
    }


    /**
     * Get the lifecycle listeners associated with this lifecycle. If this
     * Lifecycle has no listeners registered, a zero-length array is returned.
     */
    public LifecycleListener[] findLifecycleListeners() {
        return null; // FIXME: lifecycle.findLifecycleListeners();
    }


    /**
     * Remove a lifecycle event listener from this component.
     *
     * @param listener The listener to add
     */
    public void removeLifecycleListener(LifecycleListener listener) {
	lifecycle.removeLifecycleListener(listener);
    }

    Properties props=new Properties();

    /**
     * Begin processing requests via this Connector.
     *
     * @exception LifecycleException if a fatal startup error occurs
     */
    public void start() throws LifecycleException {
        JkConfig40 config=new JkConfig40();
	lifecycle.fireLifecycleEvent(START_EVENT, null);
        if( dL > 0 )
            d( "Start " + container + " " + service );

        Worker40 worker=new Worker40();
        Container ct=service.getContainer();
        worker.setContainer( ct );

        ((ContainerBase)ct).addLifecycleListener(config);
        config.loadExisting( ct );

        JkMain jkMain=new JkMain();
        jkMain.setProperties( props );
        jkMain.setDefaultWorker( worker );

        String catalinaHome=System.getProperty("catalina.home");
        File f=new File( catalinaHome );
        File jkHomeF=new File( f, "webapps/jk" );
        
        d("Setting jkHome " + jkHomeF );
        jkMain.setJkHome( jkHomeF.getAbsolutePath() );
                        
        try {
            jkMain.start();
        } catch( Exception ex ) {
            ex.printStackTrace();
        }

    }


    /**
     * Terminate processing requests via this Connector.
     *
     * @exception LifecycleException if a fatal shutdown error occurs
     */
    public void stop() throws LifecycleException {
	lifecycle.fireLifecycleEvent(STOP_EVENT, null);
    }


    private static final int dL=10;
    private static void d(String s ) {
        System.err.println( "JkConnector: " + s );
    }

    // -------------------- Not used --------------------
    
    public void setConnectionTimeout(int connectionTimeout) {}
    public boolean getEnableLookups() { return false;}
    public void setEnableLookups(boolean enableLookups) {}
    public org.apache.catalina.net.ServerSocketFactory getFactory() { return null; }
    public void setFactory(org.apache.catalina.net.ServerSocketFactory s) {}
    public int getRedirectPort() { return -1; }
    public void setRedirectPort(int i ) {}
    public java.lang.String getScheme() { return null; }
    public void setScheme(java.lang.String s ) {}
    public boolean getSecure() { return false; }
    public void setSecure(boolean b) {}
    public org.apache.catalina.Request createRequest() { return null; }
    public org.apache.catalina.Response createResponse() { return null; }
    
}
