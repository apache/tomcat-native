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

/**
 * Base Directory Context implementation. All j-t-c/naming contexts should
 * extend it. 
 *
 *
 * @author Remy Maucherat
 * @author Costin Manolache
 */
public class BaseNamingImpl {

    /**
     * Builds a base directory context.
     */
    public BaseNamingImpl() {
    }


    /** Configuration - called for all entries in the 'env'.
     *  The base implementation will call IntrospectionUtil and
     *  call explicit setter methods.
     */
    public void setEnv(String name, Object value ) {

    }

    /** The context facade that calls us.
     */
    public void setContext( Context ctx ) {

    }
    
    /**
     * 
     */
    public void recycle() {
    }

    // -------------------- Abstract methods -------------------- 
    // This is what a subclass should override
    
    /** The lookup method.
     */
    public Object lookup(Name name, boolean resolveLinks, Object o)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }

    public void bind(Name name, Object obj, Attributes attrs, boolean rebind )
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }
    
    public void unbind(Name name, boolean isContext)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }

    public void size() throws NamingException
    {
        throw new OperationNotSupportedException("size");
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
        // XXX Copy attributes
        bind(newName, value, null, true);
        unbind(oldName, false);
    }

    public Attributes getAttributes(Name name, String[] attrIds)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }    
}

