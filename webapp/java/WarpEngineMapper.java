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



import org.apache.catalina.Container;

import org.apache.catalina.Engine;

import org.apache.catalina.Host;

import org.apache.catalina.Mapper;

import org.apache.catalina.Request;

import org.apache.catalina.core.StandardHostMapper;



/**

 *

 * @author <a href="mailto:pier.fumagalli@eng.sun.com">Pier Fumagalli</a>

 * @author Copyright &copy; 1999, 2000 <a href="http://www.apache.org">The

 *         Apache Software Foundation.

 * @version CVS $Id$

 */

public class WarpEngineMapper extends StandardHostMapper {



    // -------------------------------------------------------------- CONSTANTS



    /** Our debug flag status (Used to compile out debugging information). */

    private static final boolean DEBUG=WarpDebug.DEBUG;



    // ------------------------------------------------------------ CONSTRUCTOR



    /**

     * Create a new WarpEngineMapper.

     */

    public WarpEngineMapper() {

        super();

        if (DEBUG) this.debug("New instance created");

    }



    // --------------------------------------------------------- PUBLIC METHODS



    /**

     * Return the child Container that should be used to process this Request,

     * based upon its characteristics.  If no such child Container can be

     * identified, return <code>null</code> instead.

     *

     * @param request Request being processed

     * @param update Update the Request to reflect the mapping selection?

     */

    public Container map(Request request, boolean update) {

        if (DEBUG) this.debug("Trying to map request to host");



        if (request instanceof WarpRequest) {

            WarpRequest r=(WarpRequest)request;



            Container engine=super.getContainer();

    	    Container children[]=engine.findChildren();



    	    for (int x=0; x<children.length; x++) {

    	        if (children[x] instanceof WarpHost) {

    	            WarpHost host=(WarpHost)children[x];

    	            if (r.getRequestedHostID()==host.getHostID()) {

    	                return(children[x]);

    	            }

                }

            }

        }

        return(super.map(request,update));

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

