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

/* We must use package starting with o.a.catalina in order for
   ContainerServlet interface to work
*/
package org.apache.catalina.jk;

import java.io.*;
import java.net.*;
import java.util.*;

import javax.servlet.*;
import javax.servlet.http.*;

import org.apache.jk.core.*;
import org.apache.jk.common.*;
import org.apache.jk.server.*;

/* XXX we should move it to this package
 */
import org.apache.jk.server.tomcat40.*;

import org.apache.tomcat.util.net.*;
import org.apache.tomcat.util.buf.*;
import org.apache.tomcat.util.http.*;

import org.apache.catalina.*;

/**
 * Tomcat specific adapter
 */
public class JkServlet40 extends JkServlet
    implements ContainerServlet
{
    public JkServlet40()
    {
    }

    Wrapper w;
    
    /**
     * Return the Wrapper with which this Servlet is associated.
     */
    public Wrapper getWrapper() {
        return w;
    }


    /**
     * Set the Wrapper with which this Servlet is associated.
     *
     * @param wrapper The new associated Wrapper
     */
    public void setWrapper(Wrapper wrapper) {
        d("Set wrapper " + wrapper );
        w=wrapper;
    }


    public void init(ServletConfig conf) throws ServletException {
        try {
            d("init, calling initializeContainer");
            initializeContainer( conf );
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }

    public void service( HttpServletRequest req, HttpServletResponse res )
        throws ServletException
    {
        d("Dummy service() " + w);
    }

    private static final int dL=0;
    private static void d(String s ) {
        System.err.println( "JkServlet40: " + s );
    }

    Context ctx;
    Deployer deployer;
    
    public void initializeContainer(ServletConfig cfg) {
        d("Initializing 4.0 for jk requests using" + w );

        // Get control ( cut&paste from ManagerServlet )
        ctx=(Context)w.getParent();
        d("Context = " + ctx );
        deployer=(Deployer)ctx.getParent();
        d("Deployer = " + deployer );
        
        d("Parent = " + deployer.getParent() );
        
        d("Parent2 = " + deployer.getParent().getParent() );
        

        // Now we're in control, find JkServlet and get it's
        // config. The cfg param is _our_ config, not usefull..

        
        // start Jk
        JkMain jkMain=new JkMain();
        jkMain.setProperties( servletConfig2properties( cfg ));
        Worker40 worker=new Worker40();
        worker.setContainer( deployer.getParent().getParent());
        jkMain.setDefaultWorker( worker );
        
        try {
            jkMain.start();
        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }
    
}
