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

//import org.apache.naming.NameParserImpl;

// Based on a merge of various catalina naming contexts
// Name is used - it provide better oportunities for reuse and optimizations

/**
 * Base Directory Context implementation. All j-t-c/naming contexts should
 * extend it. 
 *
 * Implements all JNDI methods - if you just extend it you'll get UnsuportedOperation.
 * XXX Should it also act as introspector proxy or should we use a separate context ?
 * The intention is to allow use 'introspection magic' and bean-like DirContexts.
 *
 * IMPORTANT: all contexts should use setters/getters for configuration, instead
 * of the Hashtable. The default constructor will use introspection to configure
 * and may provide ( via a hook ? ) JMX management on all contexts.
 *
 * You must extend and override few methods. Of course, you can also override any other
 * method and provide a more optimal implementation, but in most cases you only
 * need the minimal set.
 *
 * All methods use Name variant. They should expect an arbitrary implementation, but
 * it's recommended to check if ServerName is used - and take advantage of the
 * specific features ( MessageBytes, etc ).
 *
 * <ul>
 *   <li>
 * </ul>
 *
 * @author Remy Maucherat
 * @author Costin Manolache
 */
public class BaseDirContext extends BaseContext implements DirContext {

    public BaseDirContext() {
        super();
    }

    public BaseDirContext(Hashtable env) {
        super();
    }

    // -------------------- Abstract methods -------------------- 
    // This is what a subclass should implement.
    
    /** The lookup method to implement
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
        Object value = lookup(oldName);
        bind(newName, value);
        unbind(oldName);
    }

    public Attributes getAttributes(Name name, String[] attrIds)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }
    
    // ----------------------------------------------------- DirContext Methods


    /**
     * Retrieves all of the attributes associated with a named object. 
     * 
     * @return the set of attributes associated with name. 
     * Returns an empty attribute set if name has no attributes; never null.
     * @param name the name of the object from which to retrieve attributes
     * @exception NamingException if a naming exception is encountered
     */
    public Attributes getAttributes(Name name)
        throws NamingException
    {
        return getAttributes(name, null);
    }


    /**
     * Retrieves all of the attributes associated with a named object.
     * 
     * @return the set of attributes associated with name
     * @param name the name of the object from which to retrieve attributes
     * @exception NamingException if a naming exception is encountered
     */
    public Attributes getAttributes(String name)
        throws NamingException
    {
        return getAttributes(string2Name(name));
    }

    /**
     * Retrieves selected attributes associated with a named object. 
     * See the class description regarding attribute models, attribute type 
     * names, and operational attributes.
     * 
     * @return the requested attributes; never null
     * @param name the name of the object from which to retrieve attributes
     * @param attrIds the identifiers of the attributes to retrieve. null 
     * indicates that all attributes should be retrieved; an empty array 
     * indicates that none should be retrieved
     * @exception NamingException if a naming exception is encountered
     */
    public Attributes getAttributes(String name, String[] attrIds)
        throws NamingException
    {
        return getAttributes(string2Name(name), attrIds);
    }


    /**
     * Modifies the attributes associated with a named object. The order of 
     * the modifications is not specified. Where possible, the modifications 
     * are performed atomically.
     * 
     * @param name the name of the object whose attributes will be updated
     * @param mod_op the modification operation, one of: ADD_ATTRIBUTE, 
     * REPLACE_ATTRIBUTE, REMOVE_ATTRIBUTE
     * @param attrs the attributes to be used for the modification; may not 
     * be null
     * @exception AttributeModificationException if the modification cannot be
     * completed successfully
     * @exception NamingException if a naming exception is encountered
     */
    public void modifyAttributes(Name name, int mod_op, Attributes attrs)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }


    /**
     * Modifies the attributes associated with a named object.
     * 
     * @param name the name of the object whose attributes will be updated
     * @param mod_op the modification operation, one of: ADD_ATTRIBUTE, 
     * REPLACE_ATTRIBUTE, REMOVE_ATTRIBUTE
     * @param attrs the attributes to be used for the modification; may not 
     * be null
     * @exception AttributeModificationException if the modification cannot be
     * completed successfully
     * @exception NamingException if a naming exception is encountered
     */
    public void modifyAttributes(String name, int mod_op, Attributes attrs)
        throws NamingException
    {
        modifyAttributes(string2Name(name), mod_op, attrs);
    }


    /**
     * Modifies the attributes associated with a named object using an an 
     * ordered list of modifications. The modifications are performed in the 
     * order specified. Each modification specifies a modification operation 
     * code and an attribute on which to operate. Where possible, the 
     * modifications are performed atomically.
     * 
     * @param name the name of the object whose attributes will be updated
     * @param mods an ordered sequence of modifications to be performed; may 
     * not be null
     * @exception AttributeModificationException if the modification cannot be
     * completed successfully
     * @exception NamingException if a naming exception is encountered
     */
    public void modifyAttributes(Name name, ModificationItem[] mods)
        throws NamingException {
        if( mods==null ) return;
        for( int i=0; i<mods.length; i++ ) {
            //mods[i];
        }
    }


    /**
     * Modifies the attributes associated with a named object using an an 
     * ordered list of modifications.
     * 
     * @param name the name of the object whose attributes will be updated
     * @param mods an ordered sequence of modifications to be performed; may 
     * not be null
     * @exception AttributeModificationException if the modification cannot be
     * completed successfully
     * @exception NamingException if a naming exception is encountered
     */
    public void modifyAttributes(String name, ModificationItem[] mods)
        throws NamingException
    {
        modifyAttributes(string2Name(name), mods);
    }


    /**
     * Binds a name to an object, along with associated attributes. If attrs 
     * is null, the resulting binding will have the attributes associated 
     * with obj if obj is a DirContext, and no attributes otherwise. If attrs 
     * is non-null, the resulting binding will have attrs as its attributes; 
     * any attributes associated with obj are ignored.
     * 
     * @param name the name to bind; may not be empty
     * @param obj the object to bind; possibly null
     * @param attrs the attributes to associate with the binding
     * @exception NameAlreadyBoundException if name is already bound
     * @exception InvalidAttributesException if some "mandatory" attributes 
     * of the binding are not supplied
     * @exception NamingException if a naming exception is encountered
     */
    public void bind(Name name, Object obj, Attributes attrs)
        throws NamingException
    {
        bind(name, obj, attrs, false);
    }


    /**
     * Binds a name to an object, along with associated attributes.
     * 
     * @param name the name to bind; may not be empty
     * @param obj the object to bind; possibly null
     * @param attrs the attributes to associate with the binding
     * @exception NameAlreadyBoundException if name is already bound
     * @exception InvalidAttributesException if some "mandatory" attributes 
     * of the binding are not supplied
     * @exception NamingException if a naming exception is encountered
     */
    public void bind(String name, Object obj, Attributes attrs)
        throws NamingException
    {
        bind( string2Name(name), obj, attrs, false);
    }


    /**
     * Binds a name to an object, along with associated attributes, 
     * overwriting any existing binding. If attrs is null and obj is a 
     * DirContext, the attributes from obj are used. If attrs is null and obj 
     * is not a DirContext, any existing attributes associated with the object
     * already bound in the directory remain unchanged. If attrs is non-null, 
     * any existing attributes associated with the object already bound in 
     * the directory are removed and attrs is associated with the named 
     * object. If obj is a DirContext and attrs is non-null, the attributes 
     * of obj are ignored.
     * 
     * @param name the name to bind; may not be empty
     * @param obj the object to bind; possibly null
     * @param attrs the attributes to associate with the binding
     * @exception InvalidAttributesException if some "mandatory" attributes 
     * of the binding are not supplied
     * @exception NamingException if a naming exception is encountered
     */
    public void rebind(Name name, Object obj, Attributes attrs)
        throws NamingException
    {
        bind(name, obj, attrs, true);
    }


    /**
     * Binds a name to an object, along with associated attributes, 
     * overwriting any existing binding.
     * 
     * @param name the name to bind; may not be empty
     * @param obj the object to bind; possibly null
     * @param attrs the attributes to associate with the binding
     * @exception InvalidAttributesException if some "mandatory" attributes 
     * of the binding are not supplied
     * @exception NamingException if a naming exception is encountered
     */
    public void rebind(String name, Object obj, Attributes attrs)
        throws NamingException
    {
        bind( string2Name( name ) , obj, attrs, true);
    }


    /**
     * Creates and binds a new context, along with associated attributes. 
     * This method creates a new subcontext with the given name, binds it in 
     * the target context (that named by all but terminal atomic component of 
     * the name), and associates the supplied attributes with the newly 
     * created object. All intermediate and target contexts must already 
     * exist. If attrs is null, this method is equivalent to 
     * Context.createSubcontext().
     * 
     * @param name the name of the context to create; may not be empty
     * @param attrs the attributes to associate with the newly created context
     * @return the newly created context
     * @exception NameAlreadyBoundException if the name is already bound
     * @exception InvalidAttributesException if attrs does not contain all 
     * the mandatory attributes required for creation
     * @exception NamingException if a naming exception is encountered
     */
    public DirContext createSubcontext(String name, Attributes attrs)
        throws NamingException
    {
        return createSubcontext(string2Name( name ), attrs);
    }


    /**
     * Retrieves the schema associated with the named object. The schema 
     * describes rules regarding the structure of the namespace and the 
     * attributes stored within it. The schema specifies what types of 
     * objects can be added to the directory and where they can be added; 
     * what mandatory and optional attributes an object can have. The range 
     * of support for schemas is directory-specific.
     * 
     * @param name the name of the object whose schema is to be retrieved
     * @return the schema associated with the context; never null
     * @exception OperationNotSupportedException if schema not supported
     * @exception NamingException if a naming exception is encountered
     */
    public DirContext getSchema(Name name)
        throws NamingException
    {
        return getSchema(name.toString());
    }


    /**
     * Retrieves the schema associated with the named object.
     * 
     * @param name the name of the object whose schema is to be retrieved
     * @return the schema associated with the context; never null
     * @exception OperationNotSupportedException if schema not supported
     * @exception NamingException if a naming exception is encountered
     */
    public DirContext getSchema(String name)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }


    /**
     * Retrieves a context containing the schema objects of the named 
     * object's class definitions.
     * 
     * @param name the name of the object whose object class definition is to 
     * be retrieved
     * @return the DirContext containing the named object's class 
     * definitions; never null
     * @exception OperationNotSupportedException if schema not supported
     * @exception NamingException if a naming exception is encountered
     */
    public DirContext getSchemaClassDefinition(Name name)
        throws NamingException
    {
        return getSchemaClassDefinition(name.toString());
    }


    /**
     * Retrieves a context containing the schema objects of the named 
     * object's class definitions.
     * 
     * @param name the name of the object whose object class definition is to 
     * be retrieved
     * @return the DirContext containing the named object's class 
     * definitions; never null
     * @exception OperationNotSupportedException if schema not supported
     * @exception NamingException if a naming exception is encountered
     */
    public DirContext getSchemaClassDefinition(String name)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }


    /**
     * Searches in a single context for objects that contain a specified set 
     * of attributes, and retrieves selected attributes. The search is 
     * performed using the default SearchControls settings.
     * 
     * @param name the name of the context to search
     * @param matchingAttributes the attributes to search for. If empty or 
     * null, all objects in the target context are returned.
     * @param attributesToReturn the attributes to return. null indicates 
     * that all attributes are to be returned; an empty array indicates that 
     * none are to be returned.
     * @return a non-null enumeration of SearchResult objects. Each 
     * SearchResult contains the attributes identified by attributesToReturn 
     * and the name of the corresponding object, named relative to the 
     * context named by name.
     * @exception NamingException if a naming exception is encountered
     */
    public NamingEnumeration search(Name name, Attributes matchingAttributes,
                                    String[] attributesToReturn)
        throws NamingException
    {
        return search(name.toString(), matchingAttributes, attributesToReturn);
    }


    /**
     * Searches in a single context for objects that contain a specified set 
     * of attributes, and retrieves selected attributes.
     * 
     * @param name the name of the context to search
     * @param matchingAttributes the attributes to search for. If empty or 
     * null, all objects in the target context are returned.
     * @param attributesToReturn the attributes to return. null indicates 
     * that all attributes are to be returned; an empty array indicates that 
     * none are to be returned.
     * @return a non-null enumeration of SearchResult objects. Each 
     * SearchResult contains the attributes identified by attributesToReturn 
     * and the name of the corresponding object, named relative to the 
     * context named by name.
     * @exception NamingException if a naming exception is encountered
     */
    public NamingEnumeration search(String name, Attributes matchingAttributes,
                                    String[] attributesToReturn)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }


    /**
     * Searches in a single context for objects that contain a specified set 
     * of attributes. This method returns all the attributes of such objects. 
     * It is equivalent to supplying null as the atributesToReturn parameter 
     * to the method search(Name, Attributes, String[]).
     * 
     * @param name the name of the context to search
     * @param matchingAttributes the attributes to search for. If empty or 
     * null, all objects in the target context are returned.
     * @return a non-null enumeration of SearchResult objects. Each 
     * SearchResult contains the attributes identified by attributesToReturn 
     * and the name of the corresponding object, named relative to the 
     * context named by name.
     * @exception NamingException if a naming exception is encountered
     */
    public NamingEnumeration search(Name name, Attributes matchingAttributes)
        throws NamingException {
        return search(name.toString(), matchingAttributes);
    }


    /**
     * Searches in a single context for objects that contain a specified set 
     * of attributes.
     * 
     * @param name the name of the context to search
     * @param matchingAttributes the attributes to search for. If empty or 
     * null, all objects in the target context are returned.
     * @return a non-null enumeration of SearchResult objects. Each 
     * SearchResult contains the attributes identified by attributesToReturn 
     * and the name of the corresponding object, named relative to the 
     * context named by name.
     * @exception NamingException if a naming exception is encountered
     */
    public NamingEnumeration search(String name, Attributes matchingAttributes)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }

    /**
     * Searches in the named context or object for entries that satisfy the 
     * given search filter. Performs the search as specified by the search 
     * controls.
     * 
     * @param name the name of the context or object to search
     * @param filter the filter expression to use for the search; may not be 
     * null
     * @param cons the search controls that control the search. If null, 
     * the default search controls are used (equivalent to 
     * (new SearchControls())).
     * @return an enumeration of SearchResults of the objects that satisfy 
     * the filter; never null
     * @exception InvalidSearchFilterException if the search filter specified 
     * is not supported or understood by the underlying directory
     * @exception InvalidSearchControlsException if the search controls 
     * contain invalid settings
     * @exception NamingException if a naming exception is encountered
     */
    public NamingEnumeration search
        (Name name, String filter, SearchControls cons)
        throws NamingException
    {
        return search(name.toString(), filter, cons);
    }


    /**
     * Searches in the named context or object for entries that satisfy the 
     * given search filter. Performs the search as specified by the search 
     * controls.
     * 
     * @param name the name of the context or object to search
     * @param filter the filter expression to use for the search; may not be 
     * null
     * @param cons the search controls that control the search. If null, 
     * the default search controls are used (equivalent to 
     * (new SearchControls())).
     * @return an enumeration of SearchResults of the objects that satisfy 
     * the filter; never null
     * @exception InvalidSearchFilterException if the search filter 
     * specified is not supported or understood by the underlying directory
     * @exception InvalidSearchControlsException if the search controls 
     * contain invalid settings
     * @exception NamingException if a naming exception is encountered
     */
    public NamingEnumeration search(String name, String filter, 
                                             SearchControls cons)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }


    /**
     * Searches in the named context or object for entries that satisfy the 
     * given search filter. Performs the search as specified by the search 
     * controls.
     * 
     * @param name the name of the context or object to search
     * @param filterExpr the filter expression to use for the search. 
     * The expression may contain variables of the form "{i}" where i is a 
     * nonnegative integer. May not be null.
     * @param filterArgs the array of arguments to substitute for the 
     * variables in filterExpr. The value of filterArgs[i] will replace each 
     * occurrence of "{i}". If null, equivalent to an empty array.
     * @param cons the search controls that control the search. If null, the 
     * default search controls are used (equivalent to (new SearchControls())).
     * @return an enumeration of SearchResults of the objects that satisy the 
     * filter; never null
     * @exception ArrayIndexOutOfBoundsException if filterExpr contains {i} 
     * expressions where i is outside the bounds of the array filterArgs
     * @exception InvalidSearchControlsException if cons contains invalid 
     * settings
     * @exception InvalidSearchFilterException if filterExpr with filterArgs 
     * represents an invalid search filter
     * @exception NamingException if a naming exception is encountered
     */
    public NamingEnumeration search(Name name, String filterExpr, 
                                    Object[] filterArgs, SearchControls cons)
        throws NamingException
    {
        return search(name.toString(), filterExpr, filterArgs, cons);
    }


    /**
     * Searches in the named context or object for entries that satisfy the 
     * given search filter. Performs the search as specified by the search 
     * controls.
     * 
     * @param name the name of the context or object to search
     * @param filterExpr the filter expression to use for the search. 
     * The expression may contain variables of the form "{i}" where i is a 
     * nonnegative integer. May not be null.
     * @param filterArgs the array of arguments to substitute for the 
     * variables in filterExpr. The value of filterArgs[i] will replace each 
     * occurrence of "{i}". If null, equivalent to an empty array.
     * @param cons the search controls that control the search. If null, the 
     * default search controls are used (equivalent to (new SearchControls())).
     * @return an enumeration of SearchResults of the objects that satisy the 
     * filter; never null
     * @exception ArrayIndexOutOfBoundsException if filterExpr contains {i} 
     * expressions where i is outside the bounds of the array filterArgs
     * @exception InvalidSearchControlsException if cons contains invalid 
     * settings
     * @exception InvalidSearchFilterException if filterExpr with filterArgs 
     * represents an invalid search filter
     * @exception NamingException if a naming exception is encountered
     */
    public  NamingEnumeration search(String name, String filterExpr, 
                                         Object[] filterArgs,
                                         SearchControls cons)
        throws NamingException
    {
        throw new OperationNotSupportedException();
    }

}

