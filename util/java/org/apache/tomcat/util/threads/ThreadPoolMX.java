/*
 *
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

package org.apache.tomcat.util.threads;

import java.util.*;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.commons.modeler.Registry;

/**
 * Manageable thread pool
 *
 * @author Costin Manolache
 */
public class ThreadPoolMX extends ThreadPool {
    static Log log = LogFactory.getLog(ThreadPoolMX.class);
    Registry reg;
    protected String domain;

    protected String name;

    public ThreadPoolMX() {
        super();
    }

    public void register(String name) {
        this.name=name;
        reg=Registry.getRegistry();
        try {
            reg.registerComponent(this, domain, "ThreadPool",  name);
        } catch( Exception ex ) {
            log.error( "Error registering thread pool", ex );
        }
    }

    public synchronized void start() {
        super.start();
    }

    public void addThread( Thread t, ControlRunnable cr ) {
        threads.put( t, cr );
        for( int i=0; i<listeners.size(); i++ ) {
            ThreadPoolListener tpl=(ThreadPoolListener)listeners.elementAt(i);
            tpl.threadStart(this, t);
        }
    }

    public void removeThread( Thread t ) {
        threads.remove(t);
        for( int i=0; i<listeners.size(); i++ ) {
            ThreadPoolListener tpl=(ThreadPoolListener)listeners.elementAt(i);
            tpl.threadEnd(this, t);
        }
    }

    public void addThreadPoolListener( ThreadPoolListener tpl ) {
        listeners.addElement( tpl );
    }

    /**
     * Executes a given Runnable on a thread in the pool, block if needed.
     */
    public void runIt(ThreadPoolRunnable r) {
        super.runIt( r );
    }

    /**
     * Stop the thread pool
     */
    public synchronized void shutdown() {
        super.shutdown();
    }

    /**
     * Returns the thread to the pool.
     * Called by threads as they are becoming idel.
     */
    protected synchronized void returnController(ControlRunnable c) {
        super.returnController(c);
    }

    /**
     * Inform the pool that the specific thread finish.
     *
     * Called by the ControlRunnable.run() when the runnable
     * throws an exception.
     */
    protected synchronized void notifyThreadEnd(ControlRunnable c) {
        super.notifyThreadEnd(c);
    }

    /** Debug display of the stage of each thread. The return is html style,
     * for display in the console ( it can be easily parsed too )
     *
     * @return
     */
    public String threadStatusString() {
        StringBuffer sb=new StringBuffer();
        Iterator it=threads.keySet().iterator();
        sb.append("<ul>");
        while( it.hasNext()) {
            sb.append("<li>");
            ThreadWithAttributes twa=(ThreadWithAttributes)
                    it.next();
            sb.append(twa.getCurrentStage(this) ).append(" ");
            sb.append( twa.getParam(this));
            sb.append( "</li>\n");
        }
        sb.append("</ul>");
        return sb.toString();
    }

    /** Return an array with the status of each thread. The status
     * indicates the current request processing stage ( for tomcat ) or
     * whatever the thread is doing ( if the application using TP provide
     * this info )
     *
     * @return
     */
    public String[] threadStatus() {
        String status[]=new String[ threads.size()];
        Iterator it=threads.keySet().iterator();
        for( int i=0; ( i<status.length && it.hasNext()); i++ ) {
            ThreadWithAttributes twa=(ThreadWithAttributes)
                    it.next();
            status[i]=twa.getCurrentStage(this);
        }
        return status;
    }

    /** Return an array with the current "param" ( XXX better name ? )
     * of each thread. This is typically the last request.
     *
     * @return
     */
    public String[] threadParam() {
        String status[]=new String[ threads.size()];
        Iterator it=threads.keySet().iterator();
        for( int i=0; ( i<status.length && it.hasNext()); i++ ) {
            ThreadWithAttributes twa=(ThreadWithAttributes)
                    it.next();
            Object o=twa.getParam(this);
            status[i]=(o==null)? null : o.toString();
        }
        return status;
    }
}
