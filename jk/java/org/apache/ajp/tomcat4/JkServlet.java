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

package org.apache.ajp.tomcat4;


import java.io.IOException;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.catalina.Container;
import org.apache.catalina.ContainerServlet;
import org.apache.catalina.Context;
import org.apache.catalina.Wrapper;


/**
 * Module loader for JkServlet
 *
 * @author Costin Manolache
 */
public class JkServlet
    extends HttpServlet implements ContainerServlet
{
    // -------------------- ContainerServlet interface --------------------
    Wrapper wrapper;
    
    public Wrapper getWrapper() {
	if( dL > 0 ) d("getWrapper()");
	return wrapper;
    }

    public void setWrapper(Wrapper wrapper) {
	if( dL > 0 ) d("setWrapper() " + wrapper );
	this.wrapper=wrapper;
    }

    Context ctx;
    
    /**
     * Initialize this servlet.
     */
    public void init() throws ServletException {
	super.init();
        if(wrapper == null) {
	    log("No wrapper available, make sure the app is trusted");
	    //System.out.println("No wrapper available, make sure the app is trusted");
	    return;
	}

	ctx=(Context) wrapper.getParent();

	if( dL > 0 ) {
	    d("Wrapper: " + wrapper.getClass().getName() + " " + wrapper );
	    d("Ctx: " + ctx.getClass().getName() + " " + ctx );
	    Object parent=ctx.getParent();
	    d("P: " + parent.getClass().getName() + " " + parent );
	    while( parent instanceof Container ) {
		parent=((Container)parent).getParent();
		d("P: " + parent.getClass().getName() + " " + parent );
	    }
	}
	
    }

    public void service(HttpServletRequest request,
                      HttpServletResponse response)
        throws IOException, ServletException
    {
	throw new ServletException("Shouldn't be called direclty");
    }
    
    private static final int dL=10;
    private static void d(String s ) {
        log( "JkServlet: " + s );
    }


}
