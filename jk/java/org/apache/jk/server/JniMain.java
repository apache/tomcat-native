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

import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.http.*;

import org.apache.tomcat.util.threads.*;

import org.apache.jk.core.*;
import org.apache.jk.common.*;
import org.apache.jk.apr.*;


/**
 * Start jk inprocess. This will do the initial setup and
 * load tomcat and everything else. It replaces the 1/2 of the old 
 * jni connector in 3.3 ( the other half is in channelJni ).
 * 
 * @author Costin Manolache
 */
public class JniMain {
    
    static WorkerEnv wEnv=null;
    static int epDataNote=-1;

    static class EpData {
        public long env;
        public long ep;
        public long r;
    }
    
    public static int startup(String cmdLine,
                              String stdout,
                              String stderr)
    {
        System.out.println("In startup");
        System.err.println("In startup err");
        if( wEnv!=null ) {
            d("Second call, ignored ");
            return 1;
        }

        try {
            if(null != stdout) {
                PrintStream out=new PrintStream(new FileOutputStream(stdout));
                System.setOut(out);
                if( stderr==null ) 
                    System.setErr(out);
            }
            if(null != stderr) {
                PrintStream err=new PrintStream(new FileOutputStream(stderr));
                System.setErr(err);
                if( stdout==null )
                    System.setOut(err);
            }
            if( stdout==null && stderr==null ) {
                // no problem, use stderr - it'll go to error.log of the server.
                System.setOut( System.err );
            }

            wEnv=new WorkerEnv();
            ChannelJni cjni=new ChannelJni();
            wEnv.addChannel( "jni", cjni );
            Worker defaultWorker=new WorkerDummy();
            
            HandlerRequest hReq=new HandlerRequest();
            wEnv.addHandler( hReq );
            hReq.setWorker( defaultWorker );
            
            wEnv.start();

            System.load("/ws/jtc/jk/build/WEB-INF/jk2/jni/jni_connect.so");
        } catch(Throwable t) {
            t.printStackTrace();
        }
        System.out.println("New stream");
        System.err.println("New err stream");

        return 1;
    }

    public static void shutdown() {
        System.out.println("In shutdown");
    }

    private static final int dL=10;
    private static void d(String s ) {
        System.err.println( "JniMain: " + s );
    }


}
