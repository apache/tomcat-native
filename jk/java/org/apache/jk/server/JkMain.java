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

import org.apache.jk.core.*;
import org.apache.jk.common.*;

import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.http.*;
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
 */
public class JkMain
{
    WorkerEnv wEnv=new WorkerEnv();
    String propFile;
    Properties props=new Properties();

    Properties modules=new Properties();
    boolean modified=false;
    boolean started=false;
    
    public JkMain()
    {
        modules.put("channelSocket", "org.apache.jk.common.ChannelSocket");
        modules.put("channelUnix", "org.apache.jk.common.ChannelUn");
        modules.put("channelJni", "org.apache.jk.common.ChannelJni");
        modules.put("apr", "org.apache.jk.apr.AprImpl");
        modules.put("mx", "org.apache.jk.common.JkMX");
        modules.put("shm", "org.apache.jk.common.Shm");
        modules.put("request","org.apache.jk.common.HandlerRequest");
        modules.put("container","org.apache.jk.common.HandlerRequest");
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
     * Set the <code>channelClassName</code> that will used to connect to
     * httpd.
     */
    public void setChannelClassName(String name) {
        props.put( "handler.channel.className",name);
    }

    /**
     * Set the <code>workerClassName</code> that will handle the request.
     * ( sort of 'pivot' in axis :-)
     */
    public void setWorkerClassName(String name) {
        props.put( "handler.container.className",name);
    }

    /** Set the base dir of jk2. ( including WEB-INF if in a webapp ).
     *  We'll try to guess it from classpath if none is set ( for
     *  example on command line ), but if in a servlet environment
     *  you need to use Context.getRealPath or a system property or
     *  set it expliciltey.
     */
    public void setJkHome( String s ) {
        wEnv.setJkHome(s);
    }
    
    String out;
    String err;
    
    public void setOut( String s ) {
        this.out=s;
    }

    public void setErr( String s ) {
        this.err=s;
    }
    
    // -------------------- Initialization --------------------
    
    public void init() throws IOException
    {
        if(null != out) {
            PrintStream outS=new PrintStream(new FileOutputStream(out));
            System.setOut(outS);
//             if( stderr==null ) 
//                 System.setErr(out);
        }
        if(null != err) {
            PrintStream errS=new PrintStream(new FileOutputStream(err));
            System.setErr(errS);
        }

        String home=wEnv.getJkHome();
        if( home==null ) {
            // XXX use IntrospectionUtil to find myself
            this.guessHome();
        }
        home=wEnv.getJkHome();
        if( home != null ) {
            File hF=new File(home);
            File conf=new File( home, "conf" );
            if( ! conf.exists() )
                conf=new File( home, "etc" );

            File propsF=new File( conf, "jk2.properties" );
            
            if( propsF.exists() ) {
                log.info("Starting Jk2, base dir= " + home + " conf=" + propsF );
                setPropertiesFile( propsF.getAbsolutePath());
            } else {
                log.info("Starting Jk2, base dir= " + home );
                if( log.isWarnEnabled() )
                    log.warn( "No properties file found " + propsF );
            }
        }
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
        // We must have at least 3 handlers:
        // channel is the 'transport'
        // request is the request processor or 'global' chain
        // container is the 'provider'
        // Additional handlers may exist and be used internally
        // or be chained to create one of the standard handlers 

        String handlers[]=defaultHandlers;
        String workers=props.getProperty( "handler.list", null );
        if( workers!=null ) {
            handlers= split( workers, ",");
        }

        // Load additional component declarations
        processModules();
        
        for( int i=0; i<handlers.length; i++ ) {
            String name= handlers[i];
            JkHandler w=wEnv.getHandler( name );
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
        long initTime=System.currentTimeMillis() - start_time;

        this.saveProperties();
        log.info("Jk running ID=" + wEnv.getLocalId() + " ... init time=" + initTime + " ms");
    }

    // -------------------- Usefull methods --------------------
    
    public WorkerEnv getWorkerEnv() {
        return wEnv;
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
        Object target=wEnv.getHandler( handlerN );

        setBeanProperty( target, name, val );
        if( started ) {
            saveProperties();
        }

    }

    public long getStartTime() {
        return start_time;
    }
    
    // -------------------- Main --------------------

    static long start_time=System.currentTimeMillis();
    
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

            JkMain jkMain=new JkMain();

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
        // Temp - to check if it works
        String outFile=propFile + ".save";
        log.debug("Saving properties " + outFile );
        try {
            props.save( new FileOutputStream(outFile), "AUTOMATICALLY GENERATED" );
        } catch(IOException ex ){
            ex.printStackTrace();
        }
    }
    
    private void processProperties() {
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
            // No . -> global property, will be used in substitutions
            return;
        }
        
        if( log.isDebugEnabled() )
            log.debug( "Processing " + type + ":" + localName + ":" + fullName + " " + propName );
        if( "class".equals( type ) || "handler".equals( type ) ) {
            return;
        }
        
        JkHandler comp=wEnv.getHandler( fullName );
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
}
