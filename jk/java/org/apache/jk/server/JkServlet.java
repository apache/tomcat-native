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

import javax.servlet.*;
import javax.servlet.http.*;

import org.apache.jk.core.*;
import org.apache.jk.common.*;

import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.http.*;

/** Main class used for testing jk core and common code and tunning.
 *
 *  It'll just start/init jk and use a dummy endpoint ( i.e. no servlet
 *  container ).
 */
public class JkServlet extends HttpServlet
{
    String password;
    String user;
    /* Parameters for the ajp channel */
    String port;
    String host; /* If it starts with '/' we'll use ud */
    protected ServletContext sctx;
    protected ServletConfig sconf;
    
    public JkServlet()
    {
    }
    
    public void init(ServletConfig conf) throws ServletException {
        try {
            super.init(conf);
            sconf=conf;
            sctx=conf.getServletContext();
            getServletAdapter();
        } catch( Throwable t ) {
            t.printStackTrace();
        }
    }

    /* Ok, this is a bit hacky - there is ( or I couldn't find ) any clean
       way to access tomcat40 internals without implementing the interface,
       and that will brake 3.3 ( and probably other things ).

       It does seem to work for 4.0, and in future we can add a tomcat40
       valve/whatever
       that will provide an Attribute for 'trusted' apps with pointer to
       the internals.
    */
    private void getServletAdapter() {
        //        try40();
        try33();
    }
            
    private void try33() {
        // 33 ?
        try {
            Object o=newInstance( "org.apache.tomcat.core.Context" );
            if( o==null ) {
                d("3.3 not detected or untrusted app");
                return;
            }
            JkServlet t33=
     (JkServlet)newInstance( "org.apache.jk.server.tomcat33.JkServlet33" );
            if( t33 == null ) {
                d("3.3 not detected or untrusted app");
                return;
            }
            t33.initializeContainer( getServletConfig());
        } catch( Throwable ex ) {
            d("3.3 intialization failed " + ex.toString());
        }
    }

    private void try40() {
        // 4.x ? 
        try {
            HttpServletRequest req=(HttpServletRequest)
                newInstance( "org.apache.catalina.connector.HttpRequestBase");
            if( req==null ) {
                d("4.0 not detected or untrusted app");
                return;
            }
            HttpServletResponse res=(HttpServletResponse)
                newInstance( "org.apache.catalina.connector.HttpResponseBase");

            RequestDispatcher rd=
                sctx.getNamedDispatcher( "JkServlet40" );
            if( rd==null ) return;
            
            try {
                rd.include( req, res );
            } catch( Exception ex ) {
                ex.printStackTrace();
                // ignore it - what would you expect, we pass dummy objects
            }
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }

    protected JkMain jkMain=new JkMain();
    
    protected void initJkMain(ServletConfig cfg, JkHandler defaultWorker) {
        servletConfig2properties( jkMain, cfg );

        jkMain.getWorkerEnv().addHandler("container", defaultWorker );

        String jkHome=cfg.getServletContext().getRealPath("/");
        d("Setting jkHome " + jkHome );
        jkMain.setJkHome( jkHome + "/WEB-INF" );
                        
        try {
            jkMain.init();
            jkMain.start();
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }

    private Object newInstance( String s ) {
        try {
            Class c=Class.forName( s );
            return c.newInstance();
        } catch( Exception ex ) {
            // ex.printStackTrace();
            return null;
        }
    }

    public void initializeContainer(ServletConfig cfg) {
    }


    /** Set jk main properties using the servlet config file
     */
    private void servletConfig2properties(JkMain jk, ServletConfig conf )
    {
        if( conf==null ) {
            d("No servlet config ");
            return;
        }
        Enumeration paramNE=conf.getInitParameterNames();
        while( paramNE.hasMoreElements() ){
            String s=(String)paramNE.nextElement();
            String v=conf.getInitParameter(s);

            jk.setProperty( s, v );
        }
    }

    private static final int dL=10;
    private static void d(String s ) {
        System.err.println( "JkServlet: " + s );
    }
}


