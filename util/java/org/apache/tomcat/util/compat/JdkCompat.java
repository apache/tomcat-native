/*
 * $Header$
 * $Revision$
 * $Date$
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

package org.apache.tomcat.util.compat;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.net.MalformedURLException;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

/**
 *  General-purpose utility to provide backward-compatibility and JDK
 *  independence. This allow use of JDK1.3 ( or higher ) facilities if
 *  available, while maintaining the code compatible with older VMs.
 *
 *  The goal is to make backward-compatiblity reasonably easy.
 *
 *  The base class supports JDK1.3 behavior.
 *
 *  @author Tim Funk
 */
public class JdkCompat {

    // ------------------------------------------------------- Static Variables

    /**
     * class providing java2 support
     */
    static final String JAVA14_SUPPORT =
        "org.apache.tomcat.util.compat.Jdk14Compat";
    /**
     *  Commons logger wrapper
     */
    static Log logger = LogFactory.getLog(JdkCompat.class);

    /** Return java version as a string
     */
    public static String getJavaVersion() {
        return javaVersion;
    }

    public static boolean isJava2() {
        return java2;
    } 
   
    public static boolean isJava14() {
        return java14;
    }

    // -------------------- Implementation --------------------
    
    // from ant
    public static final String JAVA_1_0 = "1.0";
    public static final String JAVA_1_1 = "1.1";
    public static final String JAVA_1_2 = "1.2";
    public static final String JAVA_1_3 = "1.3";
    public static final String JAVA_1_4 = "1.4";

    static String javaVersion;
    static boolean java2=false;
    static boolean java14=false;
    static JdkCompat jdkCompat;
    
    static {
        init();
    }

    private static void init() {
        try {
            javaVersion = JAVA_1_0;
            Class.forName("java.lang.Void");
            javaVersion = JAVA_1_1;
            Class.forName("java.lang.ThreadLocal");
            java2=true;
            javaVersion = JAVA_1_2;
            Class.forName("java.lang.StrictMath");
            javaVersion = JAVA_1_3;
            Class.forName("java.lang.CharSequence");
            javaVersion = JAVA_1_4;
            java14=true;
        } catch (ClassNotFoundException cnfe) {
            // swallow as we've hit the max class version that we have
        }
        if( java14 ) {
            try {
                Class c=Class.forName(JAVA14_SUPPORT);
                jdkCompat=(JdkCompat)c.newInstance();
            } catch( Exception ex ) {
                jdkCompat=new JdkCompat();
            }
        } else {
            jdkCompat=new JdkCompat();
            // Install jar handler if none installed
        }
    }

    // ----------------------------------------------------------- Constructors
    /**
     *  Default no-arg constructor
     */
    protected JdkCompat() {
    }


    // --------------------------------------------------------- Public Methods
    /**
     * Get a compatibiliy helper class.
     */
    public static JdkCompat getJdkCompat() {
        return jdkCompat;
    }

    /**
     *  Return the URI for the given file. Originally created for
     *  o.a.c.loader.WebappClassLoader
     *
     *  @param File to wrap into URI
     *  @return A URI as a URL
     */
    public URL getURI(File file)
        throws MalformedURLException {

        File realFile = file;
        try {
            realFile = realFile.getCanonicalFile();
        } catch (IOException e) {
            // Ignore
        }

        return realFile.toURL();
    }


    /**
     *  Return the maximum amount of memory the JVM will attempt to use.
     */
    public long getMaxMemory() {
        return (-1L);
    }


 }
