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

package org.apache.naming.handler.jndi;

import java.io.IOException;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;
import java.util.Hashtable;

import javax.naming.directory.DirContext;

/**
 * Stream handler to a JNDI directory context.
 * 
 * @author <a href="mailto:remm@apache.org">Remy Maucherat</a>
 * @version $Revision$
 */
public class DirContextURLStreamHandler 
    extends URLStreamHandler {
    
    
    // ----------------------------------------------------------- Constructors
    
    
    public DirContextURLStreamHandler() {
    }
    
    
    public DirContextURLStreamHandler(DirContext context) {
        this.context = context;
    }
    
    
    // -------------------------------------------------------------- Variables
    
    
    /**
     * Bindings class loader - directory context. Keyed by CL id.
     */
    private static Hashtable clBindings = new Hashtable();
    
    
    /**
     * Bindings thread - directory context. Keyed by thread id.
     */
    private static Hashtable threadBindings = new Hashtable();
    
    
    // ----------------------------------------------------- Instance Variables
    
    
    /**
     * Directory context.
     */
    protected DirContext context = null;
    
    
    // ------------------------------------------------------------- Properties
    
    
    // ----------------------------------------------- URLStreamHandler Methods
    
    
    /**
     * Opens a connection to the object referenced by the <code>URL</code> 
     * argument.
     */
    protected URLConnection openConnection(URL u) 
        throws IOException {
        DirContext currentContext = this.context;
        if (currentContext == null)
            currentContext = get();
        return new DirContextURLConnection(currentContext, u);
    }
    
    
    // --------------------------------------------------------- Public Methods
    public static final String PROTOCOL_HANDLER_VARIABLE = 
        "java.protocol.handler.pkgs";

    public static final String PACKAGE = "org.apache.naming.handler.jndi";

    
    /**
     * Set the java.protocol.handler.pkgs system property.
     */
    public static void setProtocolHandler() {
        String value = System.getProperty(PROTOCOL_HANDLER_VARIABLE);
        if (value == null) {
            value = PACKAGE;
            System.setProperty(PROTOCOL_HANDLER_VARIABLE, value);
        } else if (value.indexOf( PACKAGE ) == -1) {
            value += "|" + PACKAGE;
            System.setProperty(PROTOCOL_HANDLER_VARIABLE, value);
        }
    }
    
    
    /**
     * Returns true if the thread or the context class loader of the current 
     * thread is bound.
     */
    public static boolean isBound() {
        return (clBindings.containsKey
                (Thread.currentThread().getContextClassLoader()))
            || (threadBindings.containsKey(Thread.currentThread()));
    }
    
    
    /**
     * Binds a directory context to a class loader.
     */
    public static void bind(DirContext dirContext) {
        ClassLoader currentCL = 
            Thread.currentThread().getContextClassLoader();
        if (currentCL != null)
            clBindings.put(currentCL, dirContext);
    }
    
    
    /**
     * Unbinds a directory context to a class loader.
     */
    public static void unbind() {
        ClassLoader currentCL = 
            Thread.currentThread().getContextClassLoader();
        if (currentCL != null)
            clBindings.remove(currentCL);
    }
    
    
    /**
     * Binds a directory context to a thread.
     */
    public static void bindThread(DirContext dirContext) {
        threadBindings.put(Thread.currentThread(), dirContext);
    }
    
    
    /**
     * Unbinds a directory context to a thread.
     */
    public static void unbindThread() {
        threadBindings.remove(Thread.currentThread());
    }
    
    
    /**
     * Get the bound context.
     */
    public static DirContext get() {

        DirContext result = null;

        Thread currentThread = Thread.currentThread();
        ClassLoader currentCL = currentThread.getContextClassLoader();

        // Checking CL binding
        result = (DirContext) clBindings.get(currentCL);
        if (result != null)
            return result;

        // Checking thread biding
        result = (DirContext) threadBindings.get(currentThread);

        // Checking parent CL binding
        currentCL = currentCL.getParent();
        while (currentCL != null) {
            result = (DirContext) clBindings.get(currentCL);
            if (result != null)
                return result;
            currentCL = currentCL.getParent();
        }

        if (result == null)
            throw new IllegalStateException("Illegal class loader binding");

        return result;

    }
    
    
    /**
     * Binds a directory context to a class loader.
     */
    public static void bind(ClassLoader cl, DirContext dirContext) {
        clBindings.put(cl, dirContext);
    }
    
    
    /**
     * Unbinds a directory context to a class loader.
     */
    public static void unbind(ClassLoader cl) {
        clBindings.remove(cl);
    }
    
    
    /**
     * Get the bound context.
     */
    public static DirContext get(ClassLoader cl) {
        return (DirContext) clBindings.get(cl);
    }
    
    
    /**
     * Get the bound context.
     */
    public static DirContext get(Thread thread) {
        return (DirContext) threadBindings.get(thread);
    }
    
    
}
