/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
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
import org.apache.catalina.Logger;

public class WarpLogger {

    /* ==================================================================== */
    /* Variables                                                            */
    /* ==================================================================== */

    /* -------------------------------------------------------------------- */
    /* Bean variables */

    /** The <code>Container</code> instance processing requests. */
    private Container container=null;
    /** The source of log messages for this logger. */
    private Object source=null;

    /* ==================================================================== */
    /* Constructor                                                          */
    /* ==================================================================== */

    /** Deny empty construction. */
    private WarpLogger() {
        super();
    }

    /**
     * Construct a new instance of a <code>WarpConnector</code>.
     */
    public WarpLogger(Object source) {
        super();
        this.source=source;
    }

    /* ==================================================================== */
    /* Bean methods                                                         */
    /* ==================================================================== */

    /**
     * Return the <code>Container</code> instance which will process all
     * requests received by this <code>Connector</code>.
     */
    public Container getContainer() {
        return(this.container);
    }

    /**
     * Set the <code>Container</code> instance which will process all requests
     * received by this <code>Connector</code>.
     *
     * @param container The new Container to use
     */
    public void setContainer(Container container) {
        this.container=container;
    }

    /* ==================================================================== */
    /* Logging and debugging methods                                        */
    /* ==================================================================== */

    /** Log to the container logger with the specified level or to stderr */
    private void log(String msg, Exception exc, int lev) {
        if (this.container==null) {
            dump(msg,exc);
            return;
        }

        Logger logg=this.container.getLogger();
        if (logg==null) {
            dump(msg,exc);
            return;
        }

        String cls="["+this.source.getClass().getName()+"] ";
        if (msg==null) msg=cls;
        else msg=cls.concat(msg);

        if (exc==null) logg.log(msg,lev);
        else logg.log(msg,exc,lev);
    }

    /** Invoked when we can't get a hold on the logger, dump to stderr */
    private void dump(String message, Exception exception) {
        String cls="["+this.source.getClass().getName()+"] ";

        if (message!=null) {
            System.err.print(cls);
            System.err.println(message);
        }
        if (exception!=null) {
            System.err.print(cls);
            exception.printStackTrace(System.err);
        }
    }

    /**
     * If Constants.DEBUG was set true at compilation time, dump a debug
     * message to Standard Error.
     *
     * @param message The message to dump.
     */
    protected void debug(String message) {
        if (Constants.DEBUG) this.log(message,null,Logger.DEBUG);
    }

    /**
     * If Constants.DEBUG was set true at compilation time, dump an exception
     * stack trace to Standard Error.
     *
     * @param exception The exception to dump.
     */
    protected void debug(Exception exception) {
        if (Constants.DEBUG) this.log(null,exception,Logger.DEBUG);
    }

    /**
     * If Constants.DEBUG was set true at compilation time, dump a debug
     * message and a related exception stack trace to Standard Error.
     *
     * @param exception The exception to dump.
     * @param message The message to dump.
     */
    protected void debug(String message, Exception exception) {
        if (Constants.DEBUG) this.log(message,exception,Logger.DEBUG);
    }

    /**
     * Log a message.
     *
     * @param message The message to log.
     */
    protected void log(String message) {
        this.log(message,null,Logger.ERROR);
    }

    /**
     * Log an exception.
     *
     * @param exception The exception to log.
     */
    protected void log(Exception exception) {
        this.log(null,exception,Logger.ERROR);
    }

    /**
     * Log an exception and related message.
     *
     * @param exception The exception to log.
     * @param message The message to log.
     */
    protected void log(String message, Exception exception) {
        this.log(message,exception,Logger.ERROR);
    }
}
