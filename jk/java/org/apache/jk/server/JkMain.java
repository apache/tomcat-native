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

/** Main class used for testing jk core and common code and tunning.
 *
 *  It'll just start/init jk and use a dummy endpoint ( i.e. no servlet
 *  container ).
 */
public class JkMain
{
    WorkerEnv wEnv=new WorkerEnv();
    String propFile;
    Properties props=new Properties();

    Worker defaultWorker;
    String jkHome;

    public JkMain()
    {
    }

    public void setPropFile( String p  ) {
        propFile=p;
        try {
            props.load( new FileInputStream(propFile) );
        } catch(IOException ex ){
            ex.printStackTrace();
        }
    }

    public void setProperty( String n, String v ) {
        props.put( n, v );
    }
    
    /**
     * Set the <code>channelClassName</code> that will used to connect to
     * httpd.
     */
    public void setChannelClassName(String name) {
        props.put( "channel.default.className",name);
    }

    /**
     * Set the <code>channelClassName</code> that will used to connect to
     * httpd.
     */
    public void setWorkerClassName(String name) {
        props.put( "worker.default.className",name);
    }

    public void setDefaultWorker( Worker w ) {
        defaultWorker=w;
    }

    public void setJkHome( String s ) {
        jkHome=s;
    }

    private Object newInstance( String type, String name, String def )
        throws IOException
    {
        try {
            String classN=props.getProperty( type + "." + name + ".className",
                                             def );
            Class channelclass = Class.forName(classN);
            return channelclass.newInstance();
        } catch (Exception ex) {
            ex.printStackTrace();
            throw new IOException("Cannot create channel class");
        }
    }
    
    public void start() throws IOException
    {
        String workers=props.getProperty( "worker.list", "default" );
        Vector workerNamesV= split( workers, ",");

        for( int i=0; i<workerNamesV.size(); i++ ) {
            String name= (String)workerNamesV.elementAt( i );
            Worker w=(Worker)newInstance( "worker", name,
                                          "org.apache.jk.common.WorkerDummy");
            
            processProperties( w, "worker."+ name + "." );

            wEnv.addWorker( name, w );
        }
        
        defaultWorker = wEnv.getWorker( "default" );

        // XXX alternatives, setters, etc
        String channels=props.getProperty( "channel.list", "default" );
        Vector channelNamesV= split( channels, ",");

        for( int i=0; i<channelNamesV.size(); i++ ) {
            String name= (String)channelNamesV.elementAt( i );
            Channel ch=(Channel)newInstance( "channel", name, 
                                      "org.apache.jk.common.ChannelSocket");
            processProperties( ch, "channel."+ name + "." );

            if( jkHome != null )
                this.setProperty( ch, "jkHome", jkHome );
            
            wEnv.addChannel( name, ch );
            ch.setWorker( defaultWorker );
        }

        // channel and handler should _pull_ the worker from we

        HandlerRequest hReq=new HandlerRequest();
        hReq.setWorker( defaultWorker );
        wEnv.addHandler( hReq );
        
        wEnv.start();
    }

    /* A bit of magic to support workers.properties without giving
       up the clean get/set
    */
    public void setProperty( Object target, String name, String val ) {
        /* XXX we need an improved IntrospectionUtils, to report error
           without d() */
        d( "setProperty " + target + " " + name + "=" + val );
        IntrospectionUtils.setProperty( target, name, val );
    }

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

    private Vector split(String s, String delim ) {
         Vector v=new Vector();
        StringTokenizer st=new StringTokenizer(s, delim );
        while( st.hasMoreTokens() ) {
            v.addElement( st.nextToken());
        }
        return v;
    }

    public static void main(String args[]) {
        try {
            if( args.length == 0 ) {
                System.out.println("Usage: ");
                System.out.println("  JkMain workers.properties workerId");
                return;
            }
            
            String propFile=args[0];
            
            JkMain jkMain=new JkMain();
            jkMain.setPropFile(propFile);
            jkMain.start();
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }

    private static final int dL=0;
    private static void d(String s ) {
        System.err.println( "JkMain: " + s );
    }


}
