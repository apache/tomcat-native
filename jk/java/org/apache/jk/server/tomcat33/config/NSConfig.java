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
package org.apache.jk.server.tomcat33.config;

import org.apache.tomcat.core.*;
import org.apache.tomcat.util.io.FileUtil;
import org.apache.tomcat.util.log.*;
import java.io.*;
import java.util.*;


/**
    Generates automatic Netscape nsapi_redirect configurations based on
    the Tomcat server.xml settings and the war contexts
    initialized during startup.
    <p>
    This config interceptor is enabled by inserting an NSConfig
    element in the <b>&lt;ContextManager&gt;</b> tag body inside
    the server.xml file like so:
    <pre>
    * < ContextManager ... >
    *   ...
    *   <<b>NSConfig</b> <i>options</i> />
    *   ...
    * < /ContextManager >
    </pre>
    where <i>options</i> can include any of the following attributes:
    <ul>
     <li><b>configHome</b> - default parent directory for the following paths.
                            If not set, this defaults to TOMCAT_HOME. Ignored
                            whenever any of the following paths is absolute.
                             </li>
     <li><b>objConfig</b> - path to use for writing Netscape obj.conf
                            file. If not set, defaults to
                            "conf/auto/obj.conf".</li>
     <li><b>objectName</b> - Name of the Object to execute the requests.
                             Defaults to "servlet".</li>
     <li><b>workersConfig</b> - path to workers.properties file used by 
                                nsapi_redirect. If not set, defaults to
                                "conf/jk/workers.properties".</li>
     <li><b>nsapiJk</b> - path to Netscape mod_jk plugin file.  If not set,
                        defaults to "bin/nsapi_redirect.dll" on windows,
                        "bin/nsapi_rd.nlm" on netware, and
                        "bin/nsapi_redirector.so" everywhere else.</li>
     <li><b>jkLog</b> - path to log file to be used by nsapi_redirect.</li>
     <li><b>jkDebug</b> - Loglevel setting.  May be debug, info, error, or emerg.
                          If not set, defaults to emerg.</li>
     <li><b>jkWorker</b> The desired worker.  Must be set to one of the workers
                         defined in the workers.properties file. "ajp12", "ajp13"
                         or "inprocess" are the workers found in the default
                         workers.properties file. If not specified, defaults
                         to "ajp13" if an Ajp13Interceptor is in use, otherwise
                         it defaults to "ajp12".</li>
     <li><b>forwardAll</b> - If true, forward all requests to Tomcat. This helps
                             insure that all the behavior configured in the web.xml
                             file functions correctly.  If false, let Netscape serve
                             static resources assuming it has been configured
                             to do so. The default is true.
                             Warning: When false, some configuration in
                             the web.xml may not be duplicated in Netscape.
                             Review the uriworkermap file to see what
                             configuration is actually being set in Netscape.</li>
     <li><b>noRoot</b> - If true, the root context is not mapped to
                         Tomcat.  If false and forwardAll is true, all requests
                         to the root context are mapped to Tomcat. If false and
                         forwardAll is false, only JSP and servlets requests to
                         the root context are mapped to Tomcat. When false,
                         to correctly serve Tomcat's root context you must also
                         modify the Home Directory setting in Netscape
                         to point to Tomcat's root context directory.
                         Otherwise some content, such as the root index.html,
                         will be served by Netscape before nsapi_redirect gets a chance
                         to claim the request and pass it to Tomcat.
                         The default is true.</li>
    </ul>
  <p>
    @author Costin Manolache
    @author Larry Isaacs
    @author Gal Shachor
	@version $Revision$
 */
public class NSConfig  extends BaseJkConfig { 

    public static final String WORKERS_CONFIG = "conf/jk/workers.properties";
    public static final String NS_CONFIG = "conf/auto/obj.conf";
    public static final String NSAPI_LOG_LOCATION = "logs/nsapi_redirect.log";
    /** default location of nsapi plug-in. */
    public static String NSAPI_REDIRECTOR;
    
    //set up some defaults based on OS type
    static{
        String os = System.getProperty("os.name").toLowerCase();
        if(os.indexOf("windows")>=0){
           NSAPI_REDIRECTOR = "bin/nsapi_redirect.dll";
        }else if(os.indexOf("netware")>=0){
           NSAPI_REDIRECTOR = "bin/nsapi_rd.nlm";
        }else{
           NSAPI_REDIRECTOR = "bin/nsapi_redirector.so";
        }
    }

    private File objConfig = null;
    private File nsapiJk = null;
    private String objectName = "servlet";

    Log loghelper = Log.getLog("tc_log", this);
    
    public NSConfig() 
    {
    }

    //-------------------- Properties --------------------
    
    /**
        set the path to the output file for the auto-generated
        isapi_redirect registry file.  If this path is relative
        then getRegConfig() will resolve it absolutely against
        the getConfigHome() path.
        <p>
        @param <b>path</b> String path to a file
    */
    public void setObjConfig(String path) {
	objConfig= (path==null)?null:new File(path);
    }

    /**
        set the path to the nsapi plugin module
        @param <b>path</b> String path to a file
    */
    public void setNsapiJk(String path) {
        nsapiJk=( path==null?null:new File(path));
    }

    /**
        Set the name for the Object that implements the
        jk_service call.
        @param <b>name</b> Name of the obj.conf Object
    */
    public void setObjectName(String name) {
        objectName = name;
    }

    // -------------------- Initialize/guess defaults --------------------

    /** Initialize defaults for properties that are not set
	explicitely
    */
    protected void initProperties(ContextManager cm) {
        super.initProperties(cm);

	objConfig=FileUtil.getConfigFile( objConfig, configHome, NS_CONFIG);
	workersConfig=FileUtil.getConfigFile( workersConfig, configHome, WORKERS_CONFIG);

	if( nsapiJk == null )
	    nsapiJk=new File(NSAPI_REDIRECTOR);
	else
	    nsapiJk =FileUtil.getConfigFile( nsapiJk, configHome, NSAPI_REDIRECTOR );
	jkLog=FileUtil.getConfigFile( jkLog, configHome, NSAPI_LOG_LOCATION);
    }

    // -------------------- Generate config --------------------

    /**
        executes the NSConfig interceptor. This method generates Netscape
        configuration files for use with nsapi_redirect.  If not
        already set, this method will setConfigHome() to the value returned
        from <i>cm.getHome()</i>.
        <p>
        @param <b>cm</b> a ContextManager object.
    */
    public void execute(ContextManager cm) throws TomcatException 
    {
        try {
	    initProperties(cm);
	    initWorker(cm);

            PrintWriter objfile = new PrintWriter(new FileWriter(objConfig));
    	    log("Generating netscape web server config = "+objConfig );

	    generateNsapiHead( objfile );

            objfile.println("<Object name=default>");

            // Set up contexts
            // XXX deal with Virtual host configuration !!!!
            Enumeration enum = cm.getContexts();
            while (enum.hasMoreElements()) {
                Context context = (Context)enum.nextElement();

                String vhost = context.getHost();
                if(vhost != null) {
                    // Vhosts are not supported yet for Netscape
                    continue;
                }

		if( forwardAll )
		    generateStupidMappings( context, objfile );
		else
		    generateContextMappings( context, objfile );
            }

            generateNsapiTail(objfile);

            objfile.close();
        } catch(Exception ex) {
            loghelper.log("Error generating automatic Netscape configuration", ex);
        }
    }

    private void generateNsapiHead(PrintWriter objfile)
	throws TomcatException
    {
        objfile.println("###################################################################");		    
        objfile.println("# Auto generated configuration. Dated: " +  new Date());
        objfile.println("###################################################################");		    
        objfile.println();

        objfile.println("#");        
        objfile.println("# You will need to merge the content of this file with your ");
        objfile.println("# regular obj.conf and then restart (=stop + start) your Netscape server. ");
        objfile.println("#");        
        objfile.println();
            
        objfile.println("#");                    
        objfile.println("# Loading the redirector into your server");
        objfile.println("#");        
        objfile.println();            
        objfile.println("Init fn=\"load-modules\" funcs=\"jk_init,jk_service\" shlib=\"<put full path to the redirector here>\"");
        objfile.println("Init fn=\"jk_init\" worker_file=\"" + 
                        workersConfig.toString().replace('\\', '/') +  
                        "\" log_level=\"" + jkDebug + "\" log_file=\"" + 
                        jkLog.toString().replace('\\', '/') + 
                        "\"");
        objfile.println();
    }

    private void generateNsapiTail(PrintWriter objfile)
	throws TomcatException
    {
        objfile.println();
        objfile.println("#######################################################");		    
        objfile.println("# Protecting the WEB-INF and META-INF directories.");
        objfile.println("#######################################################");		    
        objfile.println("PathCheck fn=\"deny-existence\" path=\"*/WEB-INF\""); 
        objfile.println("PathCheck fn=\"deny-existence\" path=\"*/WEB-INF/*\""); 
        objfile.println("PathCheck fn=\"deny-existence\" path=\"*/META-INF\""); 
        objfile.println("PathCheck fn=\"deny-existence\" path=\"*/META-INF/*\""); 
        objfile.println();

        objfile.println("</Object>");            
        objfile.println();

        objfile.println("#######################################################");		    
        objfile.println("# New object to execute your servlet requests.");
        objfile.println("#######################################################");		    
        objfile.println("<Object name=" + objectName + ">");
        objfile.println("ObjectType fn=force-type type=text/html");
        objfile.println("Service fn=\"jk_service\" worker=\""+ jkWorker + "\" path=\"/*\"");
        objfile.println("</Object>");
        objfile.println();

        if( !forwardAll ) {
            objfile.println("#######################################################");		    
            objfile.println("# New object to execute URLs containing \";jsessionid=\"");
            objfile.println("#######################################################");		    
            objfile.println("<Object ppath=\"*;jsessionid=*\">");
            objfile.println("ObjectType fn=force-type type=text/html");
            objfile.println("Service fn=\"jk_service\" worker=\""+ jkWorker + "\" path=\"/*\"");
            objfile.println("</Object>");
            objfile.println();
        }
    }

    // -------------------- Forward all mode --------------------
    
    /** Forward all requests for a context to tomcat.
	The default.
     */
    private void generateStupidMappings(Context context, PrintWriter objfile )
    {
        String ctxPath  = context.getPath();
	String nPath=("".equals(ctxPath)) ? "/" : ctxPath;

        if( noRoot &&  "".equals(ctxPath) ) {
            log("Ignoring root context in forward-all mode  ");
            return;
        } 

        objfile.println("NameTrans fn=\"assign-name\" from=\"" + ctxPath + "\" name=\"" + objectName + "\""); 
        objfile.println("NameTrans fn=\"assign-name\" from=\"" + ctxPath + "/*\" name=\"" + objectName + "\""); 
    }


    // -------------------- Netscape serves static mode --------------------
    // This is not going to work for all apps. We fall back to stupid mode.
    
    private void generateContextMappings(Context context, PrintWriter objfile )
    {
        String ctxPath  = context.getPath();
	String nPath=("".equals(ctxPath)) ? "/" : ctxPath;

        if( noRoot &&  "".equals(ctxPath) ) {
            log("Ignoring root context in non-forward-all mode  ");
            return;
        } 

        // Static files will be served by Netscape
        objfile.println("#########################################################");		    
        objfile.println("# Auto configuration for the " + nPath + " context starts.");
        objfile.println("#########################################################");		    
        objfile.println();

        // XXX Need to determine what if/how static mappings are done

	// InvokerInterceptor - it doesn't have a container,
	// but it's implemented using a special module.
	
	// XXX we need to better collect all mappings
	addMapping( ctxPath + "/servlet/*", objfile );
	    
	Enumeration servletMaps=context.getContainers();
	while( servletMaps.hasMoreElements() ) {
	    Container ct=(Container)servletMaps.nextElement();
	    addMapping( context, ct , objfile );
	}

        // XXX ErrorDocument
        // Security and filter mappings

    }

    /** Add a Netscape extension mapping.
     */
    protected boolean addExtensionMapping( String ctxPath, String ext,
					 PrintWriter objfile )
    {
        if( debug > 0 )
            log( "Adding extension map for " + ctxPath + "/*." + ext );
        objfile.println("NameTrans fn=\"assign-name\" from=\"" +
                    ctxPath + "/*." + ext + "\" name=\"" + objectName + "\""); 
	return true;
    }

    /** Add a fulling specified Netscape mapping.
     */
    protected boolean addMapping( String fullPath, PrintWriter objfile ) {
        if( debug > 0 )
            log( "Adding map for " + fullPath );
        if( fullPath.endsWith("/*") ) {
            objfile.println("NameTrans fn=\"assign-name\" from=\""
                    + fullPath.substring(0, fullPath.length() - 2)
                    + "\" name=\"" + objectName + "\""); 
        }
        objfile.println("NameTrans fn=\"assign-name\" from=\"" +
                        fullPath + "\" name=\"" + objectName + "\""); 
	return true;
    }

}
