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
 */ 
package org.apache.tomcat.util.log;

import java.io.*;
import java.lang.reflect.*;
import java.util.*;


/**
 * This is the main class seen by objects that need to log. 
 * 
 * It has a log channel to write to; if it can't find a log with that name,
 * it outputs to the default
 * sink.  Also prepends a descriptive name to each message
 * (usually the toString() of the calling object), so it's easier
 * to identify the source.<p>
 *
 * Intended for use by client classes to make it easy to do
 * reliable, consistent logging behavior, even if you don't
 * necessarily have a context, or if you haven't registered any
 * log files yet, or if you're in a non-Tomcat application.
 * <p>
 * Usage: <pre>
 * class Foo {
 *   Log log = Log.getLog("tc_log", "Foo"); // or...
 *   Log log = Log.getLog("tc_log", this); // fills in "Foo" for you
 *   ...
 *     log.log("Something happened");
 *     ...
 *     log.log("Starting something", Log.DEBUG);
 *     ...
 *     catch (IOException e) {
 *       log.log("While doing something", e);
 *     }
 * </pre>
 * 
 *  As a special feature ( required in tomcat operation ) the
 *  Log can be modified at run-time, by changing and configuring the logging
 *  implementation ( without requiring any special change in the user code ).
 *  That means that the  user can do a Log.getLog() at any time,
 *  even before the logging system is fully configured. The
 *  embeding application can then set up or change the logging details,
 *  without any action from the log user.
 *
 * @deprecated Commons-logging should be used instead.
 * @author Alex Chaffee [alex@jguru.com]
 * @author Costin Manolache
 **/
public class Log implements org.apache.commons.logging.Log {

    /**
     * Verbosity level codes.
     */
    public static final int FATAL = Integer.MIN_VALUE;
    public static final int ERROR = 1;
    public static final int WARNING = 2;
    public static final int INFORMATION = 3;
    public static final int DEBUG = 4;
    

    // name of the logger ( each logger has a unique name,
    // used as a key internally )
    protected String logname;

    // string displayed at the beginning of each log line,
    // to identify the source
    protected String prefix;

    /* The "real" logger. This allows the manager to change the
       sink/logger at runtime, and without requiring the log
       user to do any special action or be aware of the changes
    */
    private LogHandler proxy; // the default

    // Used to get access to other logging channels.
    // Can be replaced with an application-specific impl.
    private static LogManager logManager=new LogManager();


    // -------------------- Various constructors --------------------

    protected Log(String channel, String prefix, LogHandler proxy, Object owner) {
	this.logname=channel;
	this.prefix=prefix;
	this.proxy=proxy;
    }

    /**
     * @param logname name of log to use
     * @param owner object whose class name to use as prefix
     **/
    public static Log getLog( String channel, String prefix ) {
	return logManager.getLog( channel, prefix, null );
    }
    
    /**
     * @param logname name of log to use
     * @param prefix string to prepend to each message
     **/
    public static Log getLog( String channel, Object owner ) {
	return logManager.getLog( channel, null, owner );
    }
    
    // -------------------- Log messages. --------------------
    
    /**
     * Logs the message with level INFORMATION
     **/
    public void log(String msg) 
    {
	log(msg, null, INFORMATION);
    }
    
    /**
     * Logs the Throwable with level ERROR (assumes an exception is
     * trouble; if it's not, use log(msg, t, level))
     **/
    public void log(String msg, Throwable t) 
    {
	log(msg, t, ERROR);
    }
    
    /**
     * Logs the message with given level
     **/
    public void log(String msg, int level) 
    {
	log(msg, null, level);
    }
    
    /**
     * Logs the message and Throwable to its logger or, if logger
     * not found, to the default logger, which writes to the
     * default sink, which is usually System.err
     **/
    public void log(String msg, Throwable t, int level)
    {
	log( prefix, msg, t, level );
    }

    /** 
     */
    public void log( String prefix, String msg, Throwable t, int level ) {
	proxy.log( prefix, msg, t, level );
    }
    
    /** Flush any buffers.
     *  Override if needed.
     */
    public void flush() {
	proxy.flush();
    }

    public void close() {
	proxy.close();
    }

    /** The configured logging level for this channel
     */
    public int getLevel() {
	return proxy.getLevel();
    }

    // -------------------- Management --------------------

    // No getter for the log manager ( user code shouldn't be
    // able to control the logger )
    
    /** Used by the embeding application ( tomcat ) to manage
     *  the logging.
     *
     *  Initially, the Log is not managed, and the default
     *  manager is used. The application can find what loggers
     *  have been created from the default LogManager, and 
     *  provide a special manager implemetation.
     */
    public static LogManager setLogManager( LogManager lm ) {
	// can be changed only once - so that user
	// code can't change the log manager in running servers
	if( logManager.getClass() == LogManager.class ) {
	    LogManager oldLM=logManager;
	    logManager=lm;
	    return oldLM;
	}
	return null;
    }

    public String  getChannel( LogManager lm ) {
	if( lm != logManager ) return null;
	return logname;
    }

    public void setProxy( LogManager lm, LogHandler l ) {
	// only the manager can change the proxy
	if( lm!= logManager ) {
	    System.out.println("Attempt to change proxy " + lm + " " + logManager);
	    return;
	}
	proxy=l;
    }


    // -------------------- Common-logging impl --------------------

    
    // -------------------- Log implementation -------------------- 

    public void debug(Object message) {
        log(message.toString(), null, DEBUG);
    }

    public void debug(Object message, Throwable exception) {
        log(message.toString(), exception, DEBUG);
    }

    public void error(Object message) {
        log(message.toString(), null, ERROR);
    }

    public void error(Object message, Throwable exception) {
        log(message.toString(), exception, ERROR);
    }

    public void fatal(Object message) {
        log(message.toString(), null, FATAL);
    }

    public void fatal(Object message, Throwable exception) {
        log(message.toString(), exception, FATAL);
    }

    public void info(Object message) {
        log(message.toString(), null, INFORMATION);
    }

    public void info(Object message, Throwable exception) {
        log(message.toString(), exception, INFORMATION);
    }
    public void trace(Object message) {
        log(message.toString(), null, DEBUG);
    }
    public void trace(Object message, Throwable exception) {
        log(message.toString(), exception, DEBUG);
    }
    public void warn(Object message) {
        log(message.toString(), null, WARNING);
    }
    public void warn(Object message, Throwable exception) {
        log(message.toString(), exception, WARNING);
    }

    public boolean isDebugEnabled() {
        return proxy.getLevel() <= DEBUG;
    }
    public boolean isErrorEnabled() {
        return proxy.getLevel() <= ERROR;
    }
    public boolean isFatalEnabled() {
        return proxy.getLevel() <= FATAL;
    }
    public boolean isInfoEnabled() {
        return proxy.getLevel() <= INFORMATION;
    }
    public boolean isTraceEnabled() {
        return proxy.getLevel() <= DEBUG;
    }
    public boolean isWarnEnabled() {
        return proxy.getLevel() <= WARNING;
    }
    
    /** Security notes:

    Log acts as a facade to an actual logger ( which has setters, etc).
    
    The "manager" ( embeding application ) can set the handler, and
    edit/modify all the properties at run time.

    Applications can log if they get a Log instance. From Log there is
    no way to get an instance of the LogHandler or LogManager.

    LogManager controls access to the Log channels - a 1.2 implementation
    would check for LogPermissions ( or other mechanisms - like
    information about the current thread, etc ) to determine if the
    code can get a Log.
    
    The "managing application" ( tomcat for example ) can control
    any aspect of the logging and change the actual logging
    implementation behind the scenes.

    One typical usage is that various components will use Log.getLog()
    at various moments. The "manager" ( tomcat ) will initialize the
    logging system by setting a LogManager implementation ( which
    can't be changed by user code after that ). It can then set the
    actual implementation of the logger at any time. ( or provide
    access to trusted code to it ).

    Please review.
    */
    
    
}    
