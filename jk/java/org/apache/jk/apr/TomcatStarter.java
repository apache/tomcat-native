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
 
 package org.apache.jk.apr;

import java.lang.reflect.Method;

// Hack for Catalina 4.1 who hungs the calling thread.
// Also avoids delays in apache initialization ( tomcat can take a while )

/** 
 * Start some tomcat.
 * 
 */
public class TomcatStarter implements Runnable {
    Class c;
    String args[];
    AprImpl apr = new AprImpl();
    
    public static String mainClasses[]={ "org.apache.tomcat.startup.Main",
                                         "org.apache.catalina.startup.BootstrapService",
                                         "org.apache.catalina.startup.Bootstrap"};

    // If someone has time - we can also guess the classpath and do other
    // fancy guessings.
    
    public static void main( String args[] ) {
        System.err.println("TomcatStarter: main()");
        int nClasses = 0;
        
        try {
            AprImpl.jniMode();            
            // Find the class
            Class c=null;
            for( int i=0; i<mainClasses.length; i++ ) {
                try {
                    System.err.println("Try  " + mainClasses[i]);
                    c=Class.forName( mainClasses[i] );
                } catch( ClassNotFoundException ex  ) {
                    continue;
                }
                if( c!= null ) {
                    ++nClasses;
                    Thread startThread=new Thread( new TomcatStarter(c, args));
                    c=null;
                    startThread.start();
                    break;
                }
            }
            if (nClasses==0)
                System.err.println("No class found  ");

        } catch (Throwable t ) {
            t.printStackTrace(System.err);
        }
    }

    public TomcatStarter( Class c, String args[] ) {
        this.c=c;
        this.args=args;
    }
    
    public void run() {
        System.err.println("Starting " + c.getName());
        try {
            Class argClass=args.getClass();
            Method m=c.getMethod( "main", new Class[] {argClass} );
            m.invoke( c, new Object[] { args } );
            System.out.println("TomcatStarter: Done");
            if (apr.isLoaded())
                apr.jkSetAttribute(0, 0, "channel:jni", "done");
            if (args[0].equals("stop")) {
                  Thread.sleep(5000);
                  Runtime.getRuntime().exit(0);
            }
        } catch( Throwable t ) {
            t.printStackTrace(System.err);
        }
    }
}
