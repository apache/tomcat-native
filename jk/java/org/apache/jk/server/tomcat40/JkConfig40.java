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
package org.apache.jk.server.tomcat40;

import org.apache.catalina.*;
import java.io.*;

import org.apache.jk.server.*;
import org.apache.jk.core.*;


/**
 * Extract config information out of tomcat4.x.
 *
 *   @author Bill Barker
 *   @author Costin Manolache
 *   @author Larry Isaacs
 */
public class JkConfig40 implements LifecycleListener {

    JkMain jkMain;

    // -------------------- Tomcat callbacks --------------------

    // Auto-config should be able to react to dynamic config changes,
    // and regenerate the config.

    /** Generate the configuration - only when the server is
     *  completely initialized ( before starting )
     */
    public void lifecycleEvent(LifecycleEvent evt)
    {
        d( "Event " + evt );
	if(Lifecycle.START_EVENT.equals(evt.getType())) {

            Lifecycle who = evt.getLifecycle();
            if( who instanceof Server ) {
                executeServer((Server)who);
            } else if ( who instanceof Host ) {
                executeHost((Host)who);
            } else if( who instanceof Context ) {
                executeContext((Context)who);
            }
        }
    }

    public void loadExisting(Container ct) {
        d("loadExisting");
        while( !(ct instanceof Engine ) &&
               ct!=null ) {
            d("Ct==" + ct );
            ct=ct.getParent();
        }
        if( ct==null ) {
            d("Can't find the Server");
            return;
        }
        executeEngine( ( Engine)ct );
    }
    
    // Deal with the various 4.0 Containers. We care about the Contexts,
    // everything else is needed to get to the context events
    
    public void executeServer(Server svr) {
	Service [] services = svr.findServices();
	for(int ii=0; ii < services.length; ii++) {
	    Container  con = services[ii].getContainer();
	    if( con instanceof Engine ) {
		executeEngine((Engine)con);
	    }
	}
    }

    protected void executeEngine(Engine egn) {
	Container [] children = egn.findChildren();

	for(int ii=0; ii < children.length; ii++) {
	    if( children[ii] instanceof Host ) {
		executeHost((Host)children[ii]);
	    } else if( children[ii] instanceof Context ) {
		executeContext((Context)children[ii]);
	    }
	}
    }

    protected void executeHost(Host hst) {
	Container [] children = hst.findChildren();
	for(int ii=0; ii < children.length; ii++) {
	    if(children[ii] instanceof Context) {
		executeContext((Context)children[ii]);
	    }
	}
    }

    public void executeContext(Context context){

        String docRoot = context.getServletContext().getRealPath("/");
        d( "Context " + context.getPath() + "=" + docRoot );
        d( "Real Path " + getAbsoluteDocBase( context ));
        d( "Host " + getHost( context ));

    }

    /** Get the host associated with this Container (if any).
     */
    protected Host getHost(Container child) {
	while(child != null && ! (child instanceof Host) ) {
	    child = child.getParent();
	}
	return (Host)child;
    }

    // -------------------- Config Utils  --------------------


    /** Add an extension mapping. Override with method to generate
        web server specific configuration
     */
    protected boolean addExtensionMapping( String ctxPath, String ext,
					 PrintWriter pw )
    {
	return true;
    }
    
    
    /** Add a fulling specified mapping.  Override with method to generate
        web server specific configuration
     */
    protected boolean addMapping( String fullPath, PrintWriter pw ) {
	return true;
    }

    // -------------------- General Utils --------------------

    protected String tomcatHome= System.getProperty("catalina.home");;
    
    
    protected String getAbsoluteDocBase(Context context)
    {
	// Calculate the absolute path of the document base
	String docBase = context.getServletContext().getRealPath("/");
	docBase = docBase.substring(0,docBase.length()-1);
	if (!isAbsolute(docBase)){
	    docBase = tomcatHome + "/" + docBase;
	}
	docBase = patch(docBase);
        return docBase;
    }

    public static File getConfigFile( File base, File configDir, String defaultF )
    {
        if( base==null )
            base=new File( defaultF );
        if( ! base.isAbsolute() ) {
            if( configDir != null )
                base=new File( configDir, base.getPath());
            else
                base=new File( base.getAbsolutePath()); //??
        }
        File parent=new File(base.getParent());
        if(!parent.exists()){
            if(!parent.mkdirs()){
                throw new RuntimeException(
                    "Unable to create path to config file :"+
                    base.getAbsolutePath());
            }
        }
        return base;
    }
    public static String patch(String path) {
        String patchPath = path;

        // Move drive spec to the front of the path
        if (patchPath.length() >= 3 &&
            patchPath.charAt(0) == '/'  &&
            Character.isLetter(patchPath.charAt(1)) &&
            patchPath.charAt(2) == ':') {
            patchPath=patchPath.substring(1,3)+"/"+patchPath.substring(3);
        }

        // Eliminate consecutive slashes after the drive spec
	if (patchPath.length() >= 2 &&
            Character.isLetter(patchPath.charAt(0)) &&
            patchPath.charAt(1) == ':') {
            char[] ca = patchPath.replace('/', '\\').toCharArray();
            char c;
            StringBuffer sb = new StringBuffer();

            for (int i = 0; i < ca.length; i++) {
                if ((ca[i] != '\\') ||
                    (ca[i] == '\\' &&
                        i > 0 &&
                        ca[i - 1] != '\\')) {
                    if (i == 0 &&
                        Character.isLetter(ca[i]) &&
                        i < ca.length - 1 &&
                        ca[i + 1] == ':') {
                        c = Character.toUpperCase(ca[i]);
                    } else {
                        c = ca[i];
                    }

                    sb.append(c);
                }
            }

            patchPath = sb.toString();
        }

	// fix path on NetWare - all '/' become '\\' and remove duplicate '\\'
	if (System.getProperty("os.name").startsWith("NetWare") &&
	    path.length() >=3 &&
	    path.indexOf(':') > 0) {
	    char[] ca = patchPath.replace('/', '\\').toCharArray();
	    StringBuffer sb = new StringBuffer();

	    for (int i = 0; i < ca.length; i++) {
		if ((ca[i] != '\\') ||
		    (ca[i] == '\\' && i > 0 && ca[i - 1] != '\\')) {
		    sb.append(ca[i]);
		}
	    }
	    patchPath = sb.toString();
	}

        return patchPath;
    }

    public static boolean isAbsolute( String path ) {
	// normal file
	if( path.startsWith("/" ) ) return true;

	if( path.startsWith(File.separator ) ) return true;

	// win c:
	if (path.length() >= 3 &&
            Character.isLetter(path.charAt(0)) &&
            path.charAt(1) == ':')
	    return true;

	// NetWare volume:
	if (System.getProperty("os.name").startsWith("NetWare") &&
	    path.length() >=3 &&
	    path.indexOf(':') > 0)
	    return true;

	return false;
    }

    private static final int dL=10;
    private static void d(String s ) {
        System.err.println( "JkConfig40: " + s );
    }

}
