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

/** Main class used to startup jk. 
 *
 * It is also useable standalone for testing or as a minimal socket server.
 */
public class JkMain
{
    WorkerEnv wEnv=new WorkerEnv();
    String propFile;
    Properties props=new Properties();

    public JkMain()
    {
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
        } else {
            props.put( n, v );
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

    // -------------------- Initialization --------------------
    
    public void init() throws IOException
    {
        String home=wEnv.getJkHome();
        if( home==null ) {
            // XXX use IntrospectionUtil to find myself
            this.guessHome();
        }
        log.info("Jk2 home " + home );
        if( home != null ) {
            File hF=new File(home);
            File conf=new File( home, "conf" );
            if( ! conf.exists() )
                conf=new File( home, "etc" );

            File propsF=new File( conf, "jk2.properties" );
            
            if( propsF.exists() ) {
                log.info("Jk2 conf " + propsF );
                setPropertiesFile( propsF.getAbsolutePath());
            } else {
                if( log.isWarnEnabled() )
                    log.warn( "No properties file found " + propsF );
            }
        }
    }
    
    
    public void start() throws IOException
    {
        if( props.get( "handler.request.className" )==null )
            props.put( "handler.request.className",
                       "org.apache.jk.common.HandlerRequest" );
        
        if( props.get( "handler.channel.className" )==null )
            props.put( "handler.channel.className",
                       "org.apache.jk.common.ChannelSocket" );

        // We must have at least 3 handlers:
        // channel is the 'transport'
        // request is the request processor or 'global' chain
        // container is the 'provider'
        // Additional handlers may exist and be used internally
        // or be chained to create one of the standard handlers 
        
        String workers=props.getProperty( "handler.list",
                                          "channel,request,container" );
        Vector workerNamesV= split( workers, ",");
        if( log.isDebugEnabled() )
            log.debug("workers: " + workers + " " + workerNamesV.size() );
        
        for( int i=0; i<workerNamesV.size(); i++ ) {
            String name= (String)workerNamesV.elementAt( i );
            if( log.isDebugEnabled() )
                log.debug("Configuring " + name );
            JkHandler w=wEnv.getHandler( name );
            if( w==null )
                w=(JkHandler)newInstance( "handler", name, null );
            if( w==null ) {
                log.warn("Can't create handler for name " + name );
                continue;
            }

            wEnv.addHandler( name, w );
            
            processProperties( w, "handler."+ name + "." );
        }
        
        wEnv.start();
        long initTime=System.currentTimeMillis() - start_time;
        log.info("Jk running... init time=" + initTime + " ms");
    }

    // -------------------- Usefull methods --------------------
    
    public WorkerEnv getWorkerEnv() {
        return wEnv;
    }
    
    /* A bit of magic to support workers.properties without giving
       up the clean get/set
    */
    public void setProperty( Object target, String name, String val ) {
        if( log.isDebugEnabled())
            log.debug( "setProperty " + target + " " + name + "=" + val );
        IntrospectionUtils.setProperty( target, name, val );
    }

    /* 
     * Set a handler property
     */
    public void setProperty( String handlerN, String name, String val ) {
        if( log.isDebugEnabled() )
            log.debug( "setProperty " + handlerN + " " + name + "=" + val );
        Object target=wEnv.getHandler( handlerN );
        IntrospectionUtils.setProperty( target, name, val );
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

    
    private void processProperties(Object o, String prefix) {
        Enumeration keys=props.keys();
        int plen=prefix.length();
        
        while( keys.hasMoreElements() ) {
            String k=(String)keys.nextElement();
            if( ! k.startsWith( prefix ) )
                continue;

            String name= k.substring( plen );
            String propValue=props.getProperty( k );
            if( "className".equals( name ) )
                continue;
            this.setProperty( o, name, propValue );
        }
    }

    private Object newInstance( String type, String name, String def )
        throws IOException
    {
        String classN=null;
        try {
            classN=props.getProperty( type + "." + name + ".className",
                                             def );
            if( classN== null ) return null;
            Class channelclass = Class.forName(classN);
            return channelclass.newInstance();
        } catch (Throwable ex) {
            ex.printStackTrace();
            throw new IOException("Cannot create channel class " + classN);
        }
    }

    private Vector split(String s, String delim ) {
         Vector v=new Vector();
        StringTokenizer st=new StringTokenizer(s, delim );
        while( st.hasMoreTokens() ) {
            v.addElement( st.nextToken());
        }
        return v;
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
