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
 * Add "mx.enabled=true" in jk2.properties to enable it.
 * You could also select http and/or jrmp protocol, 
 * with mx.httpPort, mx.httpHost, mxjrmpPort and mx.jrmpPort
 *
 */
public class JkMX extends JkHandler
{
    MBeanServer mserver;
	private boolean enabled=false;
	private int httpport=-1;
	private String httphost="localhost";
	private int jrmpport=-1;
	private String jrmphost="localhost";

    public JkMX()
    {
    }

    /* -------------------- Public methods -------------------- */

	/** Enable the MX4J adapters (new way)
	 */
	public void setEnabled(boolean b) {
		enabled=b;
	}
	
	public boolean getEnabled() {
		return enabled;
	}
	
	/** Enable the MX4J adapters (old way, compatible)
	 */
	public void setPort(int i) {
		enabled=(i != -1);
	}
	
	public int getPort() {
		return ((httpport != -1) ? httpport : jrmpport);
	}

	/** Enable the MX4J HTTP internal adapter
	 */	
	public void setHttpPort( int i ) {
		httpport=i;
	}

	public int getHttpPort() {
		return httpport;
	}

	public void setHttpHost(String host ) {
		this.httphost=host;
	}

	public String getHttpHost() {
		return httphost;
	}

	/** Enable the MX4J JRMP internal adapter
	 */
	public void setJrmpPort( int i ) {
		jrmpport=i;
	}

	public int getJrmpPort() {
		return jrmpport;
	}

	public void setJrmpHost(String host ) {
		this.jrmphost=host;
	}

	public String getJrmpHost() {
		return jrmphost;
	}

    /* ==================== Start/stop ==================== */
	ObjectName httpServerName=null;
	ObjectName jrmpServerName=null;

    /** Initialize the worker. After this call the worker will be
     *  ready to accept new requests.
     */
    public void loadAdapter() throws IOException {
		boolean httpAdapterLoaded = false;
		boolean jrmpAdapterLoaded = false;

        if ((httpport != -1) && classExists("mx4j.adaptor.http.HttpAdaptor")) {
            try {
				httpServerName = new ObjectName("Http:name=HttpAdaptor");
                mserver.createMBean("mx4j.adaptor.http.HttpAdaptor", httpServerName, null);
                if( httphost!=null )
                    mserver.setAttribute(httpServerName, new Attribute("Host", httphost));
                mserver.setAttribute(httpServerName, new Attribute("Port", new Integer(httpport)));

                ObjectName processorName = new ObjectName("Http:name=XSLTProcessor");
                mserver.createMBean("mx4j.adaptor.http.XSLTProcessor", processorName, null);

                //mserver.setAttribute(processorName, new Attribute("File", "/opt/41/server/lib/openjmx-tools.jar"));
                //mserver.setAttribute(processorName, new Attribute("UseCache", new Boolean(false)));
                //mserver.setAttribute(processorName, new Attribute("PathInJar", "/openjmx/adaptor/http/xsl"));
                mserver.setAttribute(httpServerName, new Attribute("ProcessorName", processorName));

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
                mserver.invoke(httpServerName, "start", null, null);

                log.info( "Started MX4J console on host " + httphost + " at port " + httpport);
                //return;

				httpAdapterLoaded = true;

            } catch( Throwable t ) {
				httpServerName=null;
                log.error( "Can't load the MX4J http adapter " + t.toString()  );
            }
        }

        if ((jrmpport != -1) && classExists("mx4j.tools.naming.NamingService")) {
            try {
                jrmpServerName = new ObjectName("Naming:name=rmiregistry");
                mserver.createMBean("mx4j.tools.naming.NamingService", jrmpServerName, null);
                mserver.invoke(jrmpServerName, "start", null, null);
                log.info( "Creating " + jrmpServerName );

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

				String jrpmurl = "rmi://" + jrmphost + ":" + Integer.toString(jrmpport) ;
					
                mserver.invoke( adaptor, "putNamingProperty",
                        new Object[] {
                            javax.naming.Context.PROVIDER_URL,
                            jrpmurl},
                        new String[] { "java.lang.Object", "java.lang.Object" });

                //mbean.putNamingProperty(javax.naming.Context.INITIAL_CONTEXT_FACTORY, "com.sun.jndi.rmi.registry.RegistryContextFactory");
                //mbean.putNamingProperty(javax.naming.Context.PROVIDER_URL, "rmi://localhost:1099");
                // Registers the JRMP adaptor in JNDI and starts it
                mserver.invoke(adaptor, "start", null, null);
                //   mbean.start();
                log.info( "Creating " + adaptor + " on host " + jrmphost + " at port " + jrmpport);

                jrmpAdapterLoaded = true;

            } catch( Exception ex ) {
				jrmpServerName = null;
                log.error( "MX4j RMI adapter not loaded: " + ex.toString());
            }
        }

        if ((httpport != -1) && (! httpAdapterLoaded) && classExists("com.sun.jdmk.comm.HtmlAdaptorServer")) {
            try {
                Class c=Class.forName( "com.sun.jdmk.comm.HtmlAdaptorServer" );
                Object o=c.newInstance();
                httpServerName=new ObjectName("Adaptor:name=html,port=" + httpport);
                log.info("Registering the JMX_RI html adapter " + httpServerName + " at port " + httpport);
                mserver.registerMBean(o,  httpServerName);

                mserver.setAttribute(httpServerName,
                                     new Attribute("Port", new Integer(httpport)));

                mserver.invoke(httpServerName, "start", null, null);

				httpAdapterLoaded = true;
            } catch( Throwable t ) {
				httpServerName = null;
                log.error( "Can't load the JMX_RI http adapter " + t.toString()  );
            }
        }

        if ((!httpAdapterLoaded) && (!jrmpAdapterLoaded))
            log.warn( "No adaptors were loaded but mx.enabled was defined.");

    }

    public void destroy() {
        try {
            log.info("Stoping JMX ");

			if( httpServerName!=null ) {
				mserver.invoke(httpServerName, "stop", null, null);
			}
			if( jrmpServerName!=null ) {
				mserver.invoke(jrmpServerName, "stop", null, null);
			}
        } catch( Throwable t ) {
            log.error( "Destroy error" + t );
        }
    }

    public void init() throws IOException {
        try {
            mserver = getMBeanServer();

            if( enabled ) {
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


    private static boolean classExists(String className) {
        try {
            Thread.currentThread().getContextClassLoader().loadClass(className);
            return true;
        } catch(Throwable e) {
            if (log.isInfoEnabled())
                log.info( "className [" + className + "] does not exist");
            return false;
        }
    }


    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( JkMX.class );


}

