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

package org.apache.naming.modules.memory;

import java.util.Hashtable;
import java.util.Enumeration;
import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.spi.NamingManager;

import org.apache.naming.core.*;
import org.apache.tomcat.util.res.*;

/**
 * In-memory context.
 *
 * IMPLEMENTATION. We use NamingEntry stored in a tree of hashtables. 
 *
 * @author Remy Maucherat
 * @author Costin Manolache
 */
public class MemoryNamingContext extends BaseDirContext {

    public MemoryNamingContext()
        throws NamingException
    {
        super();
    }

    /**
     * Builds a naming context using the given environment.
     */
    public MemoryNamingContext(Hashtable env) 
        throws NamingException
    {
        super( env );
        this.bindings = new Hashtable();
    }

    // ----------------------------------------------------- Instance Variables

    /**
     * The string manager for this package.
     */
    protected static StringManager sm =
        StringManager.getManager("org.apache.naming.res");

    /**
     * Bindings in this Context.
     */
    protected Hashtable bindings;

    public void setBindings( Hashtable bindings ) {
        this.bindings = bindings;
    }

    // -------------------------------------------------------- Context Methods

    /**
     * Unbinds the named object. Removes the terminal atomic name in name 
     * from the target context--that named by all but the terminal atomic 
     * part of name.
     * <p>
     * This method is idempotent. It succeeds even if the terminal atomic 
     * name is not bound in the target context, but throws 
     * NameNotFoundException if any of the intermediate contexts do not exist. 
     * 
     * @param name the name to bind; may not be empty
     * @exception NameNotFoundException if an intermediate context does not 
     * exist
     * @exception NamingException if a naming exception is encountered
     */
    public void unbind(Name name, boolean isContext)
        throws NamingException
    {
        checkWritable(name);
        
	while ((!name.isEmpty()) && (name.get(0).length() == 0))
	    name = name.getSuffix(1);
        
        if (name.isEmpty())
            throw new NamingException
                (sm.getString("namingContext.invalidName"));
        
        NamingEntry entry = (NamingEntry) bindings.get(name.get(0));
        
        if (entry == null) {
            throw new NameNotFoundException
                (sm.getString("namingContext.nameNotBound", name.get(0)));
        }
        
        if (name.size() > 1) {
            if (entry.type == NamingEntry.CONTEXT) {
                ((Context) entry.value).unbind(name.getSuffix(1));
            } else {
                throw new NamingException
                    (sm.getString("namingContext.contextExpected"));
            }
        } else {
            if (entry.type == NamingEntry.CONTEXT) {
                ((Context) entry.value).close();
                bindings.remove(name.get(0));
            } else {
                if( isContext ) {
                    throw new NotContextException
                        (sm.getString("namingContext.contextExpected"));
                } else {
                    bindings.remove(name.get(0));
                }
            }
        }
    }

    /**
     * Enumerates the names bound in the named context, along with the class 
     * names of objects bound to them. The contents of any subcontexts are 
     * not included.
     * <p>
     * If a binding is added to or removed from this context, its effect on 
     * an enumeration previously returned is undefined.
     * 
     * @param name the name of the context to list
     * @return an enumeration of the names and class names of the bindings in 
     * this context. Each element of the enumeration is of type NameClassPair.
     * @exception NamingException if a naming exception is encountered
     */
    public NamingEnumeration list(Name name)
        throws NamingException {
        // Removing empty parts
        while ((!name.isEmpty()) && (name.get(0).length() == 0))
            name = name.getSuffix(1);

        if (name.isEmpty()) {
            return new NamingContextEnumeration(bindings.elements(), this, false);
        }
        
        NamingEntry entry = (NamingEntry) bindings.get(name.get(0));
        
        if (entry == null) {
            throw new NameNotFoundException
                (sm.getString("namingContext.nameNotBound", name.get(0)));
        }
        
        if (entry.type != NamingEntry.CONTEXT) {
            throw new NamingException
                (sm.getString("namingContext.contextExpected"));
        }
        return ((Context) entry.value).list(name.getSuffix(1));
    }

    private Name removeEmptyPrefix(Name name ) {
        while ((!name.isEmpty()) && (name.get(0).length() == 0))
            name = name.getSuffix(1);
        return name;
    }
    
    /**
     * Enumerates the names bound in the named context, along with the 
     * objects bound to them. The contents of any subcontexts are not 
     * included.
     * <p>
     * If a binding is added to or removed from this context, its effect on 
     * an enumeration previously returned is undefined.
     * 
     * @param name the name of the context to list
     * @return an enumeration of the bindings in this context. 
     * Each element of the enumeration is of type Binding.
     * @exception NamingException if a naming exception is encountered
     */
    public NamingEnumeration listBindings(Name name)
        throws NamingException {
        // Removing empty parts
        while ((!name.isEmpty()) && (name.get(0).length() == 0))
            name = name.getSuffix(1);
        
        if (name.isEmpty()) {
            return new NamingContextEnumeration(bindings.elements(), this, true);
        }
        
        NamingEntry entry = (NamingEntry) bindings.get(name.get(0));
        
        if (entry == null) {
            throw new NameNotFoundException
                (sm.getString("namingContext.nameNotBound", name.get(0)));
        }
        
        if (entry.type != NamingEntry.CONTEXT) {
            throw new NamingException
                (sm.getString("namingContext.contextExpected"));
        }
        return ((Context) entry.value).listBindings(name.getSuffix(1));
    }

    /**
     * Creates and binds a new context. Creates a new context with the given 
     * name and binds it in the target context (that named by all but 
     * terminal atomic component of the name). All intermediate contexts and 
     * the target context must already exist.
     * 
     * @param name the name of the context to create; may not be empty
     * @return the newly created context
     * @exception NameAlreadyBoundException if name is already bound
     * @exception InvalidAttributesException if creation of the subcontext 
     * requires specification of mandatory attributes
     * @exception NamingException if a naming exception is encountered
     */
    public DirContext createSubcontext(Name name, Attributes attrs)
        throws NamingException
    {
        checkWritable(name);
        
        DirContext newContext = new MemoryNamingContext(env);
        bind(name, newContext);
        return newContext;
    }

    // XXX Make it iterative, less objects
    private NamingEntry findNamingEntry(Name name, boolean resolveLinks)
        throws NamingException
    {
         if (name.isEmpty()) {
//             // If name is empty, a newly allocated naming context is returned
//             MemoryNamingContext mmc= new MemoryNamingContext(env);
//             mmc.setBindings( bindings );
//             return mmc;
             return null;
         }
        
        NamingEntry entry = (NamingEntry) bindings.get(name.get(0));
        
        if (entry == null) {
            throw new NameNotFoundException
                (sm.getString("namingContext.nameNotBound", name.get(0)));
        }
        
        if (name.size() > 1) {
            // If the size of the name is greater that 1, then we go through a
            // number of subcontexts.
            if (entry.type != NamingEntry.CONTEXT) {
                throw new NamingException
                    (sm.getString("namingContext.contextExpected"));
            }
            return entry;
        } else {
            return entry;
        }
    }

    public Object lookup(Name name, boolean resolveLinks)
        throws NamingException
    {
        // Removing empty parts
        while ((!name.isEmpty()) && (name.get(0).length() == 0))
            name = name.getSuffix(1);
        
        NamingEntry entry=findNamingEntry( name, resolveLinks );

        if( entry.type == NamingEntry.CONTEXT ) {
            return ((BaseDirContext) entry.value).lookup(name.getSuffix(1), resolveLinks);
        }
        
        if ((resolveLinks) && (entry.type == NamingEntry.LINK_REF)) {
            String link = ((LinkRef) entry.value).getLinkName();
            if (link.startsWith(".")) {
                // Link relative to this context
                return lookup(link.substring(1));
            } else {
                return (new InitialContext(env)).lookup(link);
            }
        } else if (entry.type == NamingEntry.REFERENCE) {
            try {
                Object obj = NamingManager.getObjectInstance
                    (entry.value, name, this, env);
                if (obj != null) {
                    entry.value = obj;
                    entry.type = NamingEntry.ENTRY;
                }
                return obj;
            } catch (NamingException e) {
                throw e;
            } catch (Exception e) {
                throw new NamingException(e.getMessage());
            }
        } else {
            return entry.value;
        }
    }

    /**
     * Binds a name to an object. All intermediate contexts and the target 
     * context (that named by all but terminal atomic component of the name) 
     * must already exist.
     * 
     * @param name the name to bind; may not be empty
     * @param object the object to bind; possibly null
     * @param rebind if true, then perform a rebind (ie, overwrite)
     * @exception NameAlreadyBoundException if name is already bound
     * @exception InvalidAttributesException if object did not supply all 
     * mandatory attributes
     * @exception NamingException if a naming exception is encountered
     */
    public void bind(Name name, Object obj, Attributes attrs,
                     boolean rebind)
        throws NamingException
    {
        checkWritable(name);
        
	while ((!name.isEmpty()) && (name.get(0).length() == 0))
	    name = name.getSuffix(1);

        if (name.isEmpty())
            throw new NamingException
                (sm.getString("namingContext.invalidName"));
        
        NamingEntry entry = (NamingEntry) bindings.get(name.get(0));
        
        if (name.size() > 1) {
            if (entry == null) {
                throw new NameNotFoundException
                    (sm.getString("namingContext.nameNotBound", name.get(0)));
            }
            if (entry.type == NamingEntry.CONTEXT) {
                if (rebind) {
                    ((Context) entry.value).rebind(name.getSuffix(1), obj);
                } else {
                    ((Context) entry.value).bind(name.getSuffix(1), obj);
                }
            } else {
                throw new NamingException
                    (sm.getString("namingContext.contextExpected"));
            }
        } else {
            if ((!rebind) && (entry != null)) {
                throw new NamingException
                    (sm.getString("namingContext.alreadyBound", name.get(0)));
            } else {
                // Getting the type of the object and wrapping it within a new
                // NamingEntry
                Object toBind = 
                    NamingManager.getStateToBind(obj, name, this, env);
                if (toBind instanceof Context) {
                    entry = new NamingEntry(name.get(0), toBind, attrs,  
                                            NamingEntry.CONTEXT);
                } else if (toBind instanceof LinkRef) {
                    entry = new NamingEntry(name.get(0), toBind, attrs, 
                                            NamingEntry.LINK_REF);
                } else if (toBind instanceof Reference) {
                    entry = new NamingEntry(name.get(0), toBind, attrs, 
                                            NamingEntry.REFERENCE);
                } else if (toBind instanceof Referenceable) {
                    toBind = ((Referenceable) toBind).getReference();
                    entry = new NamingEntry(name.get(0), toBind, attrs, 
                                            NamingEntry.REFERENCE);
                } else {
                    entry = new NamingEntry(name.get(0), toBind, attrs, 
                                            NamingEntry.ENTRY);
                }
                bindings.put(name.get(0), entry);
            }
        }
    }
}

