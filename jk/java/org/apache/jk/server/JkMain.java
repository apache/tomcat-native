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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Properties;
import java.util.StringTokenizer;
import java.util.Vector;

import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.apache.commons.modeler.Registry;
import org.apache.jk.core.JkHandler;
import org.apache.jk.core.WorkerEnv;
import org.apache.tomcat.util.IntrospectionUtils;

/** Main class used to startup and configure jk. It manages the conf/jk2.properties file
 *  and is the target of JMX proxy.
 *
 *  It implements a policy of save-on-change - whenever a property is changed at
 *  runtime the jk2.properties file will be overriden. 
 *
 *  You can edit the config file when tomcat is stoped ( or if you don't use JMX or
 *  other admin tools ).
 *
 *  The format of jk2.properties:
 *  <dl>
 *   <dt>TYPE[.LOCALNAME].PROPERTY_NAME=VALUE
 *   <dd>Set a property on the associated component. TYPE will be used to
 *   find the class name and instantiate the component. LOCALNAME allows
 *   multiple instances. In JMX mode, TYPE and LOCALNAME will form the
 *   JMX name ( eventually combined with a 'jk2' component )
 *
 *   <dt>NAME=VALUE
 *   <dd>Define global properties to be used in ${} substitutions
 *
 *   <dt>class.COMPONENT_TYPE=JAVA_CLASS_NAME
 *   <dd>Adds a new 'type' of component. We predefine all known types.
 * </dl>
 *
 * Instances are created the first time a component name is found. In addition,
 * 'handler.list' property will override the list of 'default' components that are
 * loaded automatically.
 *
 *  Note that the properties file is just one (simplistic) way to configure jk. We hope
 *  to see configs based on registry, LDAP, db, etc. ( XML is not necesarily better )
 * 
 * @author Costin Manolache
 * @deprecated Will be replaced with JMX operations
 */
public class JkMain implements MBeanRegistration
{
    WorkerEnv wEnv;
    String propFile;
    Properties props=new Properties();

    Properties modules=new Properties();
    boolean modified=false;
    boolean started=false;
    boolean saveProperties=false;

    public JkMain()
    {
        JkMain.jkMain=this;
        modules.put("channelSocket", "org.apache.jk.common.ChannelSocket");
        modules.put("channelUnix", "org.apache.jk.common.ChannelUn");
        modules.put("channelJni", "org.apache.jk.common.ChannelJni");
        modules.put("apr", "org.apache.jk.apr.AprImpl");
        modules.put("mx", "org.apache.jk.common.JkMX");
        modules.put("modeler", "org.apache.jk.common.JkModeler");
        modules.put("shm", "org.apache.jk.common.Shm");
        modules.put("request","org.apache.jk.common.HandlerRequest");
        modules.put("container","org.apache.jk.common.HandlerRequest");
        modules.put("modjk","org.apache.jk.common.ModJkMX");

    }

    public static JkMain getJkMain() {
        return jkMain;
    }

    private static String DEFAULT_HTTPS="com.sun.net.ssl.internal.www.protocol";
    private void initHTTPSUrls() {
        try {
            // 11657: if only ajp is used, https: redirects need to work ( at least for 1.3+)
            String value = System.getProperty("java.protocol.handler.pkgs");
            if (value == null) {
                value = DEFAULT_HTTPS;
            } else if (value.indexOf(DEFAULT_HTTPS) >= 0  ) {
                return; // already set
            } else {
                value += "|" + DEFAULT_HTTPS;
            }
            System.setProperty("java.protocol.handler.pkgs", value);
        } catch(Exception ex ) {
            ex.printStackTrace();
        }
    }

    // -------------------- Setting --------------------
    
    /** Load a .properties file into and set the values
     *  into jk2 configuration.
     */
    public void setPropertiesFile( String p  ) {
        propFile=p;
        try {
            props.load( new FileInputStream(propFile) );
        } catch(IOException ex ){
            ex.printStackTrace();
        }
    }

    public String getPropertiesFile() {
        return propFile;
    }

    public void setSaveProperties( boolean b ) {
        saveProperties=b;
    }

    /** Set a name/value as a jk2 property
     */
    public void setProperty( String n, String v ) {
        if( "jkHome".equals( n ) ) {
            setJkHome( v );
        } 
        props.put( n, v );
        if( started ) {
            processProperty( n, v );
            saveProperties();
        }
    }
    /**
     * Retrieve a property.
     */
    public Object getProperty(String name) {
        String alias = (String)replacements.get(name);
        Object result = null;
        if(alias != null) {
            result = props.get(alias);
        }
        if(result == null) {
            result = props.get(name);
        }
        return result;
    }
    /**
     * Set the <code>channelClassName</code> that will used to connect to
     * httpd.
     */
    public void setChannelClassName(String name) {
        props.put( "handler.channel.className",name);
    }

    public String getChannelClassName() {
        return (String)props.get( "handler.channel.className");
    }

    /**
     * Set the <code>workerClassName</code> that will handle the request.
     * ( sort of 'pivot' in axis :-)
     */
    public void setWorkerClassName(String name) {
        props.put( "handler.container.className",name);
    }

    public String getWorkerClassName() {
        return (String)props.get( "handler.container.className");
    }

    /** Set the base dir of jk2. ( including WEB-INF if in a webapp ).
     *  We'll try to guess it from classpath if none is set ( for
     *  example on command line ), but if in a servlet environment
     *  you need to use Context.getRealPath or a system property or
     *  set it expliciltey.
     */
    public void setJkHome( String s ) {
        getWorkerEnv().setJkHome(s);
    }

    public String getJkHome() {
        return getWorkerEnv().getJkHome();
    }

    String out;
    String err;
    File propsF;
    
    public void setOut( String s ) {
        this.out=s;
    }

    public String getOut() {
        return this.out;
    }

    public void setErr( String s ) {
        this.err=s;
    }
    
    public String getErr() {
        return this.err;
    }
    
    // -------------------- Initialization --------------------
    
    public void init() throws IOException
    {
        long t1=System.currentTimeMillis();
        if(null != out) {
            PrintStream outS=new PrintStream(new FileOutputStream(out));
            System.setOut(outS);
        }
        if(null != err) {
            PrintStream errS=new PrintStream(new FileOutputStream(err));
            System.setErr(errS);
        }

        String home=getWorkerEnv().getJkHome();
        if( home==null ) {
            // XXX use IntrospectionUtil to find myself
            this.guessHome();
        }
        home=getWorkerEnv().getJkHome();
        if( home==null ) {
            log.info( "Can't find home, jk2.properties not loaded");
        }
        if( home != null ) {
            File hF=new File(home);
            File conf=new File( home, "conf" );
            if( ! conf.exists() )
                conf=new File( home, "etc" );

            propsF=new File( conf, "jk2.properties" );
            
            if( propsF.exists() ) {
                log.debug("Starting Jk2, base dir= " + home + " conf=" + propsF );
                setPropertiesFile( propsF.getAbsolutePath());
            } else {
                log.debug("Starting Jk2, base dir= " + home );
                if( log.isDebugEnabled() )
                    log.debug( "No properties file found " + propsF );
            }
        }
        String initHTTPS = (String)props.get("class.initHTTPS");
        if("true".equalsIgnoreCase(initHTTPS)) {
            initHTTPSUrls();
        }

        long t2=System.currentTimeMillis();
        initTime=t2-t1;
    }
    
    static String defaultHandlers[]= { "request",
                                       "container",
                                       "channelSocket"};
    /*
     static String defaultHandlers[]= { "apr",
                                       "shm",
                                       "request",
                                       "container",
                                       "channelSocket",
                                       "channelJni",
                                       "channelUnix"};
    */
    
    public void stop() 
    {
        for( int i=0; i<wEnv.getHandlerCount(); i++ ) {
            if( wEnv.getHandler(i) != null ) {
                try {
                    wEnv.getHandler(i).destroy();
                } catch( IOException ex) {
                    log.error("Error stoping " + wEnv.getHandler(i).getName(), ex);
                }
            }
        }

        started=false;
    }
    
    public void start() throws IOException
    {
        long t1=System.currentTimeMillis();
        // We must have at least 3 handlers:
        // channel is the 'transport'
        // request is the request processor or 'global' chain
        // container is the 'provider'
        // Additional handlers may exist and be used internally
        // or be chained to create one of the standard handlers 

        String handlers[]=defaultHandlers;
        // backward compat
        String workers=props.getProperty( "handler.list", null );
        if( workers!=null ) {
            handlers= split( workers, ",");
        }

        // Load additional component declarations
        processModules();
        
        for( int i=0; i<handlers.length; i++ ) {
            String name= handlers[i];
            JkHandler w=getWorkerEnv().getHandler( name );
            if( w==null ) {
                newHandler( name, "", name );
            }
        }

        // Process properties - and add aditional handlers.
        processProperties();

        for( int i=0; i<wEnv.getHandlerCount(); i++ ) {
            if( wEnv.getHandler(i) != null ) {
                try {
                    wEnv.getHandler(i).init();
                } catch( IOException ex) {
                    if( "apr".equals(wEnv.getHandler(i).getName() )) {
                        log.info( "APR not loaded, disabling jni components: " + ex.toString());
                    } else {
                        log.error( "error initializing " + wEnv.getHandler(i).getName(), ex );
                    }
                }
            }
        }

        started=true;
        long t2=System.currentTimeMillis();
        startTime=t2-t1;

        this.saveProperties();
        log.info("Jk running ID=" + wEnv.getLocalId() + " time=" + initTime + "/" + startTime +
                 "  config=" + propFile);
    }

    // -------------------- Usefull methods --------------------
    
    public WorkerEnv getWorkerEnv() {
        if( wEnv==null ) { 
            wEnv=new WorkerEnv();
        }
        return wEnv;
    }

    public void setWorkerEnv(WorkerEnv wEnv) {
        this.wEnv = wEnv;
    }

    /* A bit of magic to support workers.properties without giving
       up the clean get/set
    */
    public void setBeanProperty( Object target, String name, String val ) {
        if( val!=null )
            val=IntrospectionUtils.replaceProperties( val, props );
        if( log.isDebugEnabled())
            log.debug( "setProperty " + target + " " + name + "=" + val );
        
        IntrospectionUtils.setProperty( target, name, val );
    }

    /* 
     * Set a handler property
     */
    public void setPropertyString( String handlerN, String name, String val ) {
        if( log.isDebugEnabled() )
            log.debug( "setProperty " + handlerN + " " + name + "=" + val );
        Object target=getWorkerEnv().getHandler( handlerN );

        setBeanProperty( target, name, val );
        if( started ) {
            saveProperties();
        }

    }

    /** The time it took to initialize jk ( ms)
     */
    public long getInitTime() {
        return initTime;
    }

    /** The time it took to start jk ( ms )
     */
    public long getStartTime() {
        return startTime;
    }
    
    // -------------------- Main --------------------

    long initTime;
    long startTime;
    static JkMain jkMain=null;

    public static void main(String args[]) {
        try {
            if( args.length == 1 &&
                ( "-?".equals(args[0]) || "-h".equals( args[0])) ) {
                System.out.println("Usage: ");
                System.out.println("  JkMain [args]");
                System.out.println();
                System.out.println("  Each bean setter corresponds to an arg ( like -debug 10 )");
                System.out.println("  System properties:");
                System.out.println("    jk2.home    Base dir of jk2");
                return;
            }

            jkMain=new JkMain();

            IntrospectionUtils.processArgs( jkMain, args, new String[] {},
                                            null, new Hashtable());

            jkMain.init();
            jkMain.start();
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }

    // -------------------- Private methods --------------------

    public  void saveProperties() {
        if( !saveProperties) return;
        
        // Temp - to check if it works
        String outFile=propFile + ".save";
        log.debug("Saving properties " + outFile );
        try {
            props.save( new FileOutputStream(outFile), "AUTOMATICALLY GENERATED" );
        } catch(IOException ex ){
            ex.printStackTrace();
        }
    }

    // translate top-level keys ( from coyote or generic ) into component keys
    static Hashtable replacements=new Hashtable();
    static {
        replacements.put("port","channelSocket.port");
        replacements.put("maxThreads", "channelSocket.maxThreads");   
        replacements.put("backlog", "channelSocket.backlog");   
        replacements.put("tcpNoDelay", "channelSocket.tcpNoDelay");
        replacements.put("soTimeout", "channelSocket.soTimeout");
        replacements.put("timeout", "channelSocket.timeout");
        replacements.put("address", "channelSocket.address");            
        replacements.put("tomcatAuthentication", "request.tomcatAuthentication");            
    }

    private void preProcessProperties() {
        Enumeration keys=props.keys();
        Vector v=new Vector();
        
        while( keys.hasMoreElements() ) {
            String key=(String)keys.nextElement();          
            Object newName=replacements.get(key);
            if( newName !=null ) {
                v.addElement(key);
            }
        }
        keys=v.elements();
        while( keys.hasMoreElements() ) {
            String key=(String)keys.nextElement();
            Object propValue=props.getProperty( key );
            String replacement=(String)replacements.get(key);
            props.put(replacement, propValue);
            if( log.isDebugEnabled()) 
                log.debug("Substituting " + key + " " + replacement + " " + 
                    propValue);
        }
    }
    
    private void processProperties() {
        preProcessProperties();
        Enumeration keys=props.keys();

        while( keys.hasMoreElements() ) {
            String name=(String)keys.nextElement();
            String propValue=props.getProperty( name );

            processProperty( name, propValue );
        }
    }

    private void processProperty(String name, String propValue) {
        String type=name;
        String fullName=name;
        String localName="";
        String propName="";
        // ignore
        if( name.startsWith("key.")) return;

        int dot=name.indexOf(".");
        int lastDot=name.lastIndexOf(".");
        if( dot > 0 ) {
            type=name.substring(0, dot );
            if( dot != lastDot ) {
                localName=name.substring( dot + 1, lastDot );
                fullName=type + "." + localName;
            } else {
                fullName=type;
            }
            propName=name.substring( lastDot+1);
        } else {
            return;
        }
        
        if( log.isDebugEnabled() )
            log.debug( "Processing " + type + ":" + localName + ":" + fullName + " " + propName );
        if( "class".equals( type ) || "handler".equals( type ) ) {
            return;
        }
        
        JkHandler comp=getWorkerEnv().getHandler( fullName );
        if( comp==null ) {
            comp=newHandler( type, localName, fullName );
        }
        if( comp==null )
            return;
        
        if( log.isDebugEnabled() ) 
            log.debug("Setting " + propName + " on " + fullName + " " + comp);
        this.setBeanProperty( comp, propName, propValue );
    }

    private JkHandler newHandler( String type, String localName, String fullName )
    {
        JkHandler handler;
        String classN=modules.getProperty(type);
        if( classN == null ) {
            log.error("No class name for " + fullName + " " + type );
            return null;
        }
        try {
            Class channelclass = Class.forName(classN);
            handler=(JkHandler)channelclass.newInstance();
        } catch (Throwable ex) {
            handler=null;
            log.error( "Can't create " + fullName, ex );
            return null;
        }
        if( this.domain != null ) {
            try {
                Registry.getRegistry().registerComponent(handler, this.domain, classN,
                        "type=JkHandler,name=" + fullName);
            } catch (Exception e) {
                log.error( "Error registering " + fullName, e );
            }

        }
        wEnv.addHandler( fullName, handler );
        return handler;
    }

    private void processModules() {
        Enumeration keys=props.keys();
        int plen=6;
        
        while( keys.hasMoreElements() ) {
            String k=(String)keys.nextElement();
            if( ! k.startsWith( "class." ) )
                continue;

            String name= k.substring( plen );
            String propValue=props.getProperty( k );

            if( log.isDebugEnabled()) log.debug("Register " + name + " " + propValue );
            modules.put( name, propValue );
        }
    }

    private String[] split(String s, String delim ) {
         Vector v=new Vector();
        StringTokenizer st=new StringTokenizer(s, delim );
        while( st.hasMoreTokens() ) {
            v.addElement( st.nextToken());
        }
        String res[]=new String[ v.size() ];
        for( int i=0; i<res.length; i++ ) {
            res[i]=(String)v.elementAt(i);
        }
        return res;
    }

    // guessing home
    private static String CNAME="org/apache/jk/server/JkMain.class";

    private void guessHome() {
        String home= wEnv.getJkHome();
        if( home != null )
            return;
        home=IntrospectionUtils.guessInstall( "jk2.home","jk2.home",
                                              "tomcat-jk2.jar", CNAME );
        if( home != null ) {
            log.info("Guessed home " + home );
            wEnv.setJkHome( home );
        }
    }

    static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( JkMain.class );

    protected String domain;
    protected ObjectName oname;
    protected MBeanServer mserver;

    public ObjectName getObjectName() {
        return oname;
    }

    public String getDomain() {
        return domain;
    }

    public ObjectName preRegister(MBeanServer server,
                                  ObjectName name) throws Exception {
        oname=name;
        mserver=server;
        domain=name.getDomain();
        return name;
    }

    public void postRegister(Boolean registrationDone) {
    }

    public void preDeregister() throws Exception {
    }

    public void postDeregister() {
    }

    public void pause() throws Exception {
        for( int i=0; i<wEnv.getHandlerCount(); i++ ) {
            if( wEnv.getHandler(i) != null ) {
                wEnv.getHandler(i).pause();
            }
        }
    }

    public void resume() throws Exception {
        for( int i=0; i<wEnv.getHandlerCount(); i++ ) {
            if( wEnv.getHandler(i) != null ) {
                wEnv.getHandler(i).resume();
            }
        }
    }


}
