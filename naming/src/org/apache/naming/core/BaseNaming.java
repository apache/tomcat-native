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


package org.apache.naming.core;

import java.util.*;
import javax.naming.*;
import javax.naming.directory.DirContext;
import javax.naming.directory.Attributes;
import javax.naming.directory.Attribute;
import javax.naming.directory.ModificationItem;
import javax.naming.directory.SearchControls;
import org.apache.tomcat.util.res.StringManager;

// Based on a merge of various catalina naming contexts
// Name is used - it provide better oportunities for reuse and optimizations

/**
 *  This is the base class for our naming operations, for easy reading.
 *
 *  <p>Creating a new context:
 *  <ul>
 *  <li>Create a new class, extending BaseContext or BaseDirContext ( second
 *      if you want to support attributes ).
 *  <li>Add setters for configuration. The setters will be called autmatically,
 *      like in ant, from the initial env settings.
 *  <li>Override methods that are defined in BaseNaming. Default behavior
 *      is provided for all.
 *  <li>If performance is a concern or have special behavior - override Context and
 *      DirContext methods. That shouldn't be needed in most cases.
 *  </ul>
 *
 *  This class is designed to minimize the ammount of code that is required to
 * create a new context. The usual DirContext interface has way too many methods,
 *  so implementation requires a lot of typing.
 *
 *  Our contexts are mostly wrappers for files or in memory structures. That means
 *  some operations are cheaper, and we're far from the features that would be
 *  exposed for an LDAP or real Directory server.
 *
 * @author Remy Maucherat
 * @author Costin Manolache
 */
public class BaseNaming {

    /**
     * Builds a base directory context.
     */
    public BaseNaming() {
        this.env=new Hashtable();
    }

    /**
     * Builds a base directory context using the given environment.
     */
    public BaseNaming(Hashtable env) {
        this.env=new Hashtable();
        if (env != null ) {
            Enumeration envEntries = env.keys();
            while (envEntries.hasMoreElements()) {
                String entryName = (String) envEntries.nextElement();
                Object entryValue=env.get(entryName);
                this.env.put(entryName, entryValue);
                try {
                    // XXX We need a mechanism to select properties for
                    // this task. Maybe all contexts should use as property prefix the
                    // class name ? Or base class name ? 
                    IntrospectionUtil.setProperty( this, entryName, entryValue );
                } catch(Exception ex ) {
                    System.out.println("Unsuported property " + entryName + " " + ex.getMassage());
                }
            }
        }
    }

    // ----------------------------------------------------- Instance Variables

    /**
     * Environment. All context config info.
     */
    protected Hashtable env;

    /**
     * Default name parser for this context.
     * XXX This should be combined with the Tomcat mapper, and
     * optimized for MessageBytes.
     */
    protected final NameParser nameParser = new NameParserImpl();

    /**
     * Cached.
     * deprecated ? Should be implemented via notes or other mechanism.
     * Or via config.
     */
    protected boolean cached = true;
    protected int cacheTTL = 5000; // 5s
    protected int cacheObjectMaxSize = 32768; // 32 KB

    
    /** Prefix used for URL-based namming lookup. It must be removed
     *  from all names.
     *  Deprecated ? Do we need it ?
     */
    protected String urlPrefix="";

    // ------------------------------------------------------------- Properties
    // Common properties, apply to all jtc naming contexts.
    
    // XXX Introspection should be used to turn the Hashtable attributes
    // into setters.
    public void setURLPrefix( String s ) {
        urlPrefix=s;
    }
    
    /**
     * Set cached attribute. If false, this context will be skipped from caching
     */
    public void setCached(boolean cached) {
        this.cached = cached;
    }

    /**
     * Is cached ?
     */
    public boolean isCached() {
        return cached;
    }
    public boolean getCached() {
        return cached;
    }


    /**
     * Set cache TTL.
     */
    public void setCacheTTL(int cacheTTL) {
        this.cacheTTL = cacheTTL;
    }


    /**
     * Get cache TTL.
     */
    public int getCacheTTL() {
        return cacheTTL;
    }


    /**
     * Set cacheObjectMaxSize.
     */
    public void setCacheObjectMaxSize(int cacheObjectMaxSize) {
        this.cacheObjectMaxSize = cacheObjectMaxSize;
    }


    /**
     * Get cacheObjectMaxSize.
     */
    public int getCacheObjectMaxSize() {
        return cacheObjectMaxSize;
    }

    // -------------------- Lifecycle methods ? -------------------- 

    /**
     * Allocate resources for this directory context.
     */
    public void allocate() {
        ; // No action taken by the default implementation
    }


    /**
     * Release any resources allocated for this directory context.
     */
    public void release() {
        ; // No action taken by the default implementation
    }

    public void recycle() {
        // nothing yet.
    }
    
    // -------------------- Abstract methods -------------------- 
    // This is what a subclass should implement.

    // XXX Base resolveLinks() method ?? And then use lookup without resolveLinks flag
    
    /** The lookup method. 
     *
     * @param Name
     * @param resolveLinks. If false, this is a lookupLink call. 
     */
    public Object lookup(Name name, boolean resolveLinks)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }

    /** This is the main bind operation to support.
     *
     *  
     */
    public void bind(Name name, Object obj, Attributes attrs, boolean rebind )
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }

    /** Remove a binding
     */
    public void unbind(Name name, boolean isContext)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }

    public int size() throws NamingException
    {
        return 0;
    }

    public Name childNameAt( int i ) throws NamingException
    {
        return null;
    }

    public Object childAt( int i ) throws NamingException
    {
        return null;
    }
    
    public DirContext createSubcontext(Name name, Attributes attrs)
        throws NamingException
    {
        // XXX We can implement a decent default using bind and the current class.
        throw new OperationNotSupportedException();
    }

    public void rename(Name oldName, Name newName)
        throws NamingException
    {
        // Override if needed
        Object value = lookup(oldName, false, null);
        bind(newName, value, null, false);
        unbind(oldName, true);
    }

    public Attributes getAttributes(Name name, String[] attrIds)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }
    
    // -------------------- Utils --------------------
    

    /**
     * Returns true if writing is allowed on this context.
     */
    protected boolean isWritable(Name name) {
        return ContextAccessController.isWritable(name);
    }


    /**
     * Throws a naming exception is Context is not writable.
     */
    protected void checkWritable(Name n) 
        throws NamingException
    {
        if (!isWritable(n))
            throw new NamingException("read only");
    }

    protected Name string2Name(String s ) throws InvalidNameException {
        // XXX uniq
        //        try {
            return new CompositeName( s );
//         } catch( InvalidNameException ex ) {
//             ex.printStackTrace();
//             return null;
//         }
    }
    


    /**
     * Closes this context. This method releases this context's resources 
     * immediately, instead of waiting for them to be released automatically 
     * by the garbage collector.
     * This method is idempotent: invoking it on a context that has already 
     * been closed has no effect. Invoking any other method on a closed 
     * context is not allowed, and results in undefined behaviour.
     * 
     * @exception NamingException if a naming exception is encountered
     */
    public void close()
        throws NamingException
    {
        // We don't own the env., but the clone
        env.clear();
    }

    //-------------------- Helpers -------------------- 

    /** Just a hack so that all DirContexts can be used as tasks.
     * They'll do nothing - the setters will be called ( just like
     * new Context(Hashtable) - since we use introspection ) and the
     * context can be registred as a reference in the Project ns.
     *
     * Then other tasks could manipulate it by name.
     *
     * In a future version of ant we should have the 'references'
     * pluggable and a possible impl should be JNDI.
     *
     * Alternative: there is a way to use tasks without this method,
     * but for now it's simpler.
     */
    public void execute() {
    }
    
}

