/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *         Copyright (c) 1999, 2000  The Apache Software Foundation.         *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Tomcat",  and  "Apache  Software *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */
package org.apache.catalina.connector.warp;

import java.io.IOException;
import javax.servlet.ServletException;
import javax.servlet.ServletRequest;
import javax.servlet.http.HttpServletResponse;
import org.apache.catalina.Container;
import org.apache.catalina.Engine;
import org.apache.catalina.Globals;
import org.apache.catalina.Host;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.Request;
import org.apache.catalina.Response;
import org.apache.catalina.core.StandardEngine;

/**
 *
 *
 * @author <a href="mailto:pier.fumagalli@eng.sun.com">Pier Fumagalli</a>
 * @author Copyright &copy; 1999, 2000 <a href="http://www.apache.org">The
 *         Apache Software Foundation.
 * @version CVS $Id$
 */
public class WarpEngine extends StandardEngine {

    // -------------------------------------------------------------- CONSTANTS

    /** Our debug flag status (Used to compile out debugging information). */
    private static final boolean DEBUG=WarpDebug.DEBUG;
    /** The descriptive information related to this implementation. */
    private static final String info="WarpEngine/"+WarpConstants.VERSION;

    // -------------------------------------------------------- LOCAL VARIABLES

    /** The Java class name of the default Mapper class for this Container. */
    private String mapper="org.apache.catalina.connector.warp.WarpEngineMapper";
    /** The root path for web applications. */
    private String appbase="";
    /** The Host ID to use for the next dynamically configured host. */
    private int hostid=0;

    // ------------------------------------------------------------ CONSTRUCTOR

    /**
     * Create a new WarpEngine component with the default basic Valve.
     */
    public WarpEngine() {
        super();
        if (DEBUG) this.debug("New instance created");
    }

    // --------------------------------------------------------- PUBLIC METHODS

    /**
     * Return descriptive information about this implementation.
     */
    public String getInfo() {
        return(this.info);
    }

    /**
     * Create a new WarpHost with the specified host name, setup the appropriate
     * values and add it to the list of children.
     */
    public synchronized WarpHost setupChild(String name) {
        WarpHost host=(WarpHost)this.findChild(name);
        if (host==null) {
            this.debug("Creating new host "+name);
            host=new WarpHost();
            host.setName(name);
            host.setHostID(this.hostid++);
            host.setAppBase(this.appbase);
            this.addChild(host);
        }
        return(host);
    }

    /**
     * Add a child WarpHost to the current WarpEngine.
     */
    public void addChild(Container child) {
        if (child instanceof WarpHost) {
            WarpHost byid=this.findChild(((WarpHost)child).getHostID());
            if (byid!=null) {
                throw new IllegalArgumentException("Host "+byid.getName()+
                          " already configured with ID="+byid.getHostID());
            } else {
                super.addChild(child);
            }
        } else throw new IllegalArgumentException("Child is not a WarpHost");
    }

    /**
     * Find a child WarpHost associated with the specified Host ID.
     */
    public WarpHost findChild(int id) {
        Container children[]=this.findChildren();
        for (int x=0; x<children.length; x++) {
            WarpHost curr=(WarpHost)children[x];
            if (curr.getHostID()==id) return(curr);
        }
        return(null);
    }

    // ----------------------------------------------------------- BEAN METHODS

    /**
     * Return the application root for this Connector. This can be an absolute
     * pathname, a relative pathname, or a URL.
     */
    public String getAppBase() {
        return (this.appbase);
    }

    /**
     * Set the application root for this Connector. This can be an absolute
     * pathname, a relative pathname, or a URL.
     */
    public void setAppBase(String appbase) {
        if (appbase==null) return;
        if (DEBUG) this.debug("Setting application root to "+appbase);
        String old=this.appbase;
        this.appbase=appbase;
    }

    // ------------------------------------------------------ DEBUGGING METHODS

    /**
     * Dump a debug message.
     */
    private void debug(String msg) {
        if (DEBUG) WarpDebug.debug(this,msg);
    }

    /**
     * Dump information for an Exception.
     */
    private void debug(Exception exc) {
        if (DEBUG) WarpDebug.debug(this,exc);
    }
}
