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


import org.apache.jk.core.JkHandler;

import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.Attribute;
import javax.management.MBeanServerFactory;
import java.io.IOException;

/**
 * Load the HTTP or RMI adapters for MX4J and JMXRI.
 *
 * Add "mx.port=PORT" in jk2.properties to enable it.
 *
 */
public class JkMX extends JkHandler
{
    MBeanServer mserver;
    private int port=-1;
    private String host;
    
    public JkMX()
    {
    }

    /* -------------------- Public methods -------------------- */

    /** Enable the MX4J internal adapter
     */
    public void setPort( int i ) {
        port=i;
    }

    public int getPort() {
        return port;
    }

    public void setHost(String host ) {
        this.host=host;
    }

    public String getHost() {
        return host;
    }

    /* ==================== Start/stop ==================== */
    ObjectName serverName=null;
    
    /** Initialize the worker. After this call the worker will be
     *  ready to accept new requests.
     */
    public void loadAdapter() throws IOException {
        try {
            serverName = new ObjectName("Http:name=HttpAdaptor");
            mserver.createMBean("mx4j.adaptor.http.HttpAdaptor", serverName, null);
            if( host!=null ) 
                mserver.setAttribute(serverName, new Attribute("Host", host));
            mserver.setAttribute(serverName, new Attribute("Port", new Integer(port)));

            ObjectName processorName = new ObjectName("Http:name=XSLTProcessor");
            mserver.createMBean("mx4j.adaptor.http.XSLTProcessor", processorName, null);

            //mserver.setAttribute(processorName, new Attribute("File", "/opt/41/server/lib/openjmx-tools.jar"));
            //mserver.setAttribute(processorName, new Attribute("UseCache", new Boolean(false)));
            //mserver.setAttribute(processorName, new Attribute("PathInJar", "/openjmx/adaptor/http/xsl"));
            mserver.setAttribute(serverName, new Attribute("ProcessorName", processorName));
            
            //server.invoke(serverName, "addAuthorization",
            //             new Object[] {"openjmx", "openjmx"},
            //             new String[] {"java.lang.String", "java.lang.String"});
            
            // use basic authentication
            //server.setAttribute(serverName, new Attribute("AuthenticationMethod", "basic"));
             
            //  ObjectName sslFactory = new ObjectName("Adaptor:service=SSLServerSocketFactory");
            //         server.createMBean("openjmx.adaptor.ssl.SSLAdaptorServerSocketFactory", sslFactory, null);
            //        SSLAdaptorServerSocketFactoryMBean factory =
            // (SSLAdaptorServerSocketFactoryMBean)StandardMBeanProxy.create(SSLAdaptorServerSocketFactoryMBean.class, server, sslFactory);
            //             // Customize the values below
            //             factory.setKeyStoreName("certs");
            //             factory.setKeyStorePassword("openjmx");
            
            //             server.setAttribute(serverName, new Attribute("SocketFactoryName", sslFactory.toString()));

            // starts the server
            mserver.invoke(serverName, "start", null, null);

            log.info( "Started MX4J console on " + port);
            //return;
        } catch( Throwable t ) {
            serverName=null;
            log.error( "Can't load the MX4J http adapter " + t.toString()  );
        }
        try {
            ObjectName serverName2 = new ObjectName("Naming:name=rmiregistry");
            mserver.createMBean("mx4j.tools.naming.NamingService", serverName2, null);
            mserver.invoke(serverName2, "start", null, null);
            log.info( "Creating " + serverName2 );

            // Create the JRMP adaptor
            ObjectName adaptor = new ObjectName("Adaptor:protocol=jrmp");
            mserver.createMBean("mx4j.adaptor.rmi.jrmp.JRMPAdaptor", adaptor, null);

            //    mx4j.adaptor.rmi.jrmp.JRMPAdaptorMBean mbean = (mx4j.adaptor.rmi.jrmp.JRMPAdaptorMBean)mx4j.util.StandardMBeanProxy.
            //        create(mx4j.adaptor.rmi.jrmp.JRMPAdaptorMBean.class, mserver, adaptor);

            mserver.setAttribute(adaptor, new Attribute("JNDIName", "jrmp"));

            mserver.invoke( adaptor, "putNamingProperty",
                    new Object[] {
                        javax.naming.Context.INITIAL_CONTEXT_FACTORY,
                        "com.sun.jndi.rmi.registry.RegistryContextFactory"},
                    new String[] { "java.lang.Object", "java.lang.Object" });

            mserver.invoke( adaptor, "putNamingProperty",
                    new Object[] {
                        javax.naming.Context.PROVIDER_URL,
                        "rmi://localhost:1099"},
                    new String[] { "java.lang.Object", "java.lang.Object" });

            //mbean.putNamingProperty(javax.naming.Context.INITIAL_CONTEXT_FACTORY, "com.sun.jndi.rmi.registry.RegistryContextFactory");
            //mbean.putNamingProperty(javax.naming.Context.PROVIDER_URL, "rmi://localhost:1099");
            // Registers the JRMP adaptor in JNDI and starts it
            mserver.invoke(adaptor, "start", null, null);
            //   mbean.start();
            log.info( "Creating " + adaptor );
        } catch( Exception ex ) {
            log.info( "MX4j RMI adapter not loaded: " + ex.toString());
        }


        try {
            Class c=Class.forName( "com.sun.jdmk.comm.HtmlAdaptorServer" );
            Object o=c.newInstance();
            serverName=new ObjectName("Adaptor:name=html,port=" + port);
            log.info("Registering the JMX_RI html adapter " + serverName);
            mserver.registerMBean(o,  serverName);

            mserver.setAttribute(serverName,
                                 new Attribute("Port", new Integer(port)));

            mserver.invoke(serverName, "start", null, null);

        } catch( Throwable t ) {
            log.error( "Can't load the JMX_RI http adapter " + t.toString()  );
        }
    }

    public void destroy() {
        try {
            log.info("Stoping JMX ");

            if( serverName!=null ) {
                mserver.invoke(serverName, "stop", null, null);
            }
        } catch( Throwable t ) {
            log.error( "Destroy error" + t );
        }
    }

    public void init() throws IOException {
        try {
            mserver = getMBeanServer();

            if( port > 0 ) {
                loadAdapter();
            }

            try {
                Class c=Class.forName( "org.apache.log4j.jmx.HierarchyDynamicMBean" );
                Object o=c.newInstance();
                log.info("Registering the JMX hierarchy for Log4J ");
                mserver.registerMBean(o, new ObjectName("log4j:hierarchy=default"));
            } catch( Throwable t ) {
                log.info("Can't enable log4j mx: " +  t.toString());
            }

            /*
            DynamicMBeanProxy.createMBean( JkMain.getJkMain(), "jk2", "name=JkMain" );
            
            for( int i=0; i< wEnv.getHandlerCount(); i++ ) {
                JkHandler h=wEnv.getHandler( i );
                DynamicMBeanProxy.createMBean( h, "jk2", "name=" + h.getName() );
            }
            */
        } catch( Throwable t ) {
            log.error( "Init error", t );
        }
    }

    public void addHandlerCallback( JkHandler w ) {
        /*if( w!=this ) {
            DynamicMBeanProxy.createMBean( w, "jk2", "name=" + w.getName() );
        }
        */
    }

    MBeanServer getMBeanServer() {
        MBeanServer server;
        if( MBeanServerFactory.findMBeanServer(null).size() > 0 ) {
            server=(MBeanServer)MBeanServerFactory.findMBeanServer(null).get(0);
        } else {
            server=MBeanServerFactory.createMBeanServer();
        }
        return (server);
    }


    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( JkMX.class );


}

