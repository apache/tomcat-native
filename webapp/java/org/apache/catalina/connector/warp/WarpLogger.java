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
            if (Constants.DEBUG) dump(msg,exc);
            return;
        }

        Logger logg=this.container.getLogger();
        if (logg==null) {
            if (Constants.DEBUG) dump(msg,exc);
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
