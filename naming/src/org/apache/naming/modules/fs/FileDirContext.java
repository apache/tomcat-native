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
 * 2. Rey appear in the software itself,
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


package org.apache.naming.modules.fs;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.Hashtable;
import java.util.Vector;

import javax.naming.Name;
import javax.naming.NameAlreadyBoundException;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;
import javax.naming.directory.Attributes;
import javax.naming.directory.DirContext;

import org.apache.naming.core.BaseDirContext;
import org.apache.naming.core.NamingContextEnumeration;
import org.apache.naming.core.NamingEntry;
import org.apache.tomcat.util.res.StringManager;

/**
 * DirContext for a filesystem directory.
 *
 * The 'bind' operation will accept an InputStream ( TODO: File, any
 * resource with content )
 * and create the file. ( TODO: what attributes can we support ? )
 *
 * The lookup operation will return a FileDirContext or a File.
 *
 * Supported attributes: (TODO: lastModified, size, ...)
 *
 * Note that JNDI allows memory-efficient style, without having one wrapper
 * object for each real resource.
 *
 * @author Remy Maucherat
 * @author Costin Manolache
 */
public class FileDirContext extends BaseDirContext {
    
    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( FileDirContext.class );

    // -------------------------------------------------------------- Constants

    protected StringManager sm =
        StringManager.getManager("org.apache.naming.res");

    protected static final int BUFFER_SIZE = 2048;

    // ----------------------------------------------------------- Constructors


    /**
     * Builds a file directory context using the given environment.
     */
    public FileDirContext() {
        super();
    }


    /**
     * Builds a file directory context using the given environment.
     */
    public FileDirContext(Hashtable env) {
        super(env);
    }


    // ----------------------------------------------------- Instance Variables


    /**
     * The document base directory.
     */
    protected File base = null;


    /**
     * Absolute normalized filename of the base.
     */
    protected String absoluteBase = null;


    /**
     * Case sensitivity.
     */
    protected boolean caseSensitive = true;


    /**
     * The document base path.
     */
    protected String docBase = null;

    // ------------------------------------------------------------- Properties


    /**
     * Set the document root.
     * 
     * @param docBase The new document root
     * 
     * @exception IllegalArgumentException if the specified value is not
     *  supported by this implementation
     * @exception IllegalArgumentException if this would create a
     *  malformed URL
     */
    public void setDocBase(String docBase) {

	// Validate the format of the proposed document root
	if (docBase == null)
	    throw new IllegalArgumentException
		(sm.getString("resources.null"));

	// Calculate a File object referencing this document base directory
	base = new File(docBase);
        try {
            base = base.getCanonicalFile();
        } catch (IOException e) {
            // Ignore
        }

	// Validate that the document base is an existing directory
	if (!base.exists() || !base.isDirectory() || !base.canRead())
	    throw new IllegalArgumentException
		(sm.getString("fileResources.base", docBase));
        this.absoluteBase = base.getAbsolutePath();

	// Change the document root property
	this.docBase = docBase;

    }

    /**
     * Return the document root for this component.
     */
    public String getDocBase() {
	return (this.docBase);
    }


    /**
     * Set case sensitivity.
     */
    public void setCaseSensitive(boolean caseSensitive) {
        this.caseSensitive = caseSensitive;
    }


    /**
     * Is case sensitive ?
     */
    public boolean isCaseSensitive() {
        return caseSensitive;
    }


    // --------------------------------------------------------- Public Methods


    /**
     * Release any resources allocated for this directory context.
     */
    public void release() {
        caseSensitive = true;
        absoluteBase = null;
        base = null;
        super.release();
    }

    public void setAttribute( String name, Object v ) {
        new Throwable().printStackTrace();
        System.out.println(name + " " + v );
    }

    // -------------------- BaseDirContext implementation --------------------

    /**
     * Retrieves the named object. The result is a File relative to the docBase
     * or a FileDirContext for directories.
     * 
     * @param name the name of the object to look up
     * @return the object bound to name
     * @exception NamingException if a naming exception is encountered
     */
    public Object lookup(Name nameObj, boolean resolveLinkx)
        throws NamingException
    {
        if( log.isDebugEnabled() ) log.debug( "lookup " + nameObj );

        System.out.println("XXX " + nameObj.get(0));
        if( "fs:".equals( nameObj.get(0).toString() ))
            nameObj=nameObj.getSuffix(1);
        
        String name=nameObj.toString(); // we need to convert anyway, for File constructor
        
        Object result = null;
        File file = file(name);
        
        if (file == null)
            throw new NamingException
                (sm.getString("resources.notFound", name));
        
        if (file.isDirectory()) {
            FileDirContext tempContext = new FileDirContext(env);
            tempContext.setDocBase(file.getPath());
            result = tempContext;
        } else {
            // TODO: based on the name, return various 'styles' of
            // content
            // TODO: use lazy streams, cacheable
            result = file; //new FileResource(file);
        }
        
        return result;
    }


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
    public void unbind(Name nameObj)
        throws NamingException
    {
        if( "fs:".equals( nameObj.get(0).toString() ))
            nameObj=nameObj.getSuffix(1);
        String name=nameObj.toString();
        if( log.isDebugEnabled() ) log.debug( "unbind " + name );
        File file = file(name);

        if (file == null)
            throw new NamingException
                (sm.getString("resources.notFound", name));

        if (!file.delete())
            throw new NamingException
                (sm.getString("resources.unbindFailed", name));

    }


    /**
     * Binds a new name to the object bound to an old name, and unbinds the 
     * old name. Both names are relative to this context. Any attributes 
     * associated with the old name become associated with the new name. 
     * Intermediate contexts of the old name are not changed.
     * 
     * @param oldName the name of the existing binding; may not be empty
     * @param newName the name of the new binding; may not be empty
     * @exception NameAlreadyBoundException if newName is already bound
     * @exception NamingException if a naming exception is encountered
     */
    public void rename(Name oldNameO, Name newNameO)
        throws NamingException
    {
        String oldName=oldNameO.toString();
        String newName=newNameO.toString();
        File file = file(oldName);

        if (file == null)
            throw new NamingException
                (sm.getString("resources.notFound", oldName));

        File newFile = new File(base, newName);
        
        file.renameTo(newFile);        
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
    public NamingEnumeration list(Name nameN)
        throws NamingException
    {
        String name=nameN.toString();
        if( log.isDebugEnabled() ) log.debug( "list " + name );
        File file = file(name);

        if (file == null)
            throw new NamingException
                (sm.getString("resources.notFound", name));

        Vector entries = list(file);

        return new NamingContextEnumeration(entries.elements(), this, false);

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
    public NamingEnumeration listBindings(Name nameN)
        throws NamingException
    {
        String name=nameN.toString();
        if( log.isDebugEnabled() ) log.debug( "listBindings " + name );

        File file = file(name);

        if (file == null)
            throw new NamingException
                (sm.getString("resources.notFound", name));

        Vector entries = list(file);

        return new NamingContextEnumeration(entries.elements(), this, true);

    }


    /**
     * Destroys the named context and removes it from the namespace. Any 
     * attributes associated with the name are also removed. Intermediate 
     * contexts are not destroyed.
     * <p>
     * This method is idempotent. It succeeds even if the terminal atomic 
     * name is not bound in the target context, but throws 
     * NameNotFoundException if any of the intermediate contexts do not exist. 
     * 
     * In a federated naming system, a context from one naming system may be 
     * bound to a name in another. One can subsequently look up and perform 
     * operations on the foreign context using a composite name. However, an 
     * attempt destroy the context using this composite name will fail with 
     * NotContextException, because the foreign context is not a "subcontext" 
     * of the context in which it is bound. Instead, use unbind() to remove 
     * the binding of the foreign context. Destroying the foreign context 
     * requires that the destroySubcontext() be performed on a context from 
     * the foreign context's "native" naming system.
     * 
     * @param name the name of the context to be destroyed; may not be empty
     * @exception NameNotFoundException if an intermediate context does not 
     * exist
     * @exception NotContextException if the name is bound but does not name 
     * a context, or does not name a context of the appropriate type
     */
    public void destroySubcontext(Name name)
        throws NamingException
    {
        unbind(name);
    }


    /**
     * Retrieves the full name of this context within its own namespace.
     * <p>
     * Many naming services have a notion of a "full name" for objects in 
     * their respective namespaces. For example, an LDAP entry has a 
     * distinguished name, and a DNS record has a fully qualified name. This 
     * method allows the client application to retrieve this name. The string 
     * returned by this method is not a JNDI composite name and should not be 
     * passed directly to context methods. In naming systems for which the 
     * notion of full name does not make sense, 
     * OperationNotSupportedException is thrown.
     * 
     * @return this context's name in its own namespace; never null
     * @exception OperationNotSupportedException if the naming system does 
     * not have the notion of a full name
     * @exception NamingException if a naming exception is encountered
     */
    public String getNameInNamespace()
        throws NamingException {
        return docBase;
    }


    // ----------------------------------------------------- DirContext Methods


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
    public Attributes getAttributes(Name nameN, String[] attrIds)
        throws NamingException
    {
        String name=nameN.toString();
        if( log.isDebugEnabled() ) log.debug( "getAttributes " + name );

        // Building attribute list
        File file = file(name);

        if (file == null)
            throw new NamingException
                (sm.getString("resources.notFound", name));

        return new FileAttributes(file);

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
    public void bind(Name nameN, Object obj, Attributes attrs)
        throws NamingException {

        String name=nameN.toString();
        // Note: No custom attributes allowed
        
        File file = new File(base, name);
        if (file.exists())
            throw new NameAlreadyBoundException
                (sm.getString("resources.alreadyBound", name));
        
        rebind(name, obj, attrs);
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
    public void rebind(Name nameN, Object obj, Attributes attrs)
        throws NamingException {
        String name=nameN.toString();

        // Note: No custom attributes allowed
        // Check obj type

        File file = new File(base, name);

        InputStream is = null;
//         if (obj instanceof Resource) {
//             try {
//                 is = ((Resource) obj).streamContent();
//             } catch (IOException e) {
//             }
//         } else

        // TODO support File, byte[], String
        if (obj instanceof InputStream) {
            is = (InputStream) obj;
        } else if (obj instanceof DirContext) {
            if (file.exists()) {
                if (!file.delete())
                    throw new NamingException
                        (sm.getString("resources.bindFailed", name));
            }
            if (!file.mkdir())
                throw new NamingException
                    (sm.getString("resources.bindFailed", name));
        }
        if (is == null)
            throw new NamingException
                (sm.getString("resources.bindFailed", name));

        // Open os

        try {
            FileOutputStream os = null;
            byte buffer[] = new byte[BUFFER_SIZE];
            int len = -1;
            try {
                os = new FileOutputStream(file);
                while (true) {
                    len = is.read(buffer);
                    if (len == -1)
                        break;
                    os.write(buffer, 0, len);
                }
            } finally {
                if (os != null)
                    os.close();
                is.close();
            }
        } catch (IOException e) {
            throw new NamingException
                (sm.getString("resources.bindFailed", e));
        }
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
    public DirContext createSubcontext(Name nameN, Attributes attrs)
        throws NamingException
    {
        String name=nameN.toString();
        File file = new File(base, name);
        if (file.exists())
            throw new NameAlreadyBoundException
                (sm.getString("resources.alreadyBound", name));
        if (!file.mkdir())
            throw new NamingException
                (sm.getString("resources.bindFailed", name));
        return (DirContext) lookup(name);
    }

    // ------------------------------------------------------ Protected Methods


    /**
     * Return a context-relative path, beginning with a "/", that represents
     * the canonical version of the specified path after ".." and "." elements
     * are resolved out.  If the specified path attempts to go outside the
     * boundaries of the current context (i.e. too many ".." path elements
     * are present), return <code>null</code> instead.
     *
     * @param path Path to be normalized
     */
    protected String normalize(String path) {

	String normalized = path;

	// Normalize the slashes and add leading slash if necessary
	if (normalized.indexOf('\\') >= 0)
	    normalized = normalized.replace('\\', '/');
	if (!normalized.startsWith("/"))
	    normalized = "/" + normalized;

	// Resolve occurrences of "//" in the normalized path
	while (true) {
	    int index = normalized.indexOf("//");
	    if (index < 0)
		break;
	    normalized = normalized.substring(0, index) +
		normalized.substring(index + 1);
	}

	// Resolve occurrences of "/./" in the normalized path
	while (true) {
	    int index = normalized.indexOf("/./");
	    if (index < 0)
		break;
	    normalized = normalized.substring(0, index) +
		normalized.substring(index + 2);
	}

	// Resolve occurrences of "/../" in the normalized path
	while (true) {
	    int index = normalized.indexOf("/../");
	    if (index < 0)
		break;
	    if (index == 0)
		return (null);	// Trying to go outside our context
	    int index2 = normalized.lastIndexOf('/', index - 1);
	    normalized = normalized.substring(0, index2) +
		normalized.substring(index + 3);
	}

	// Return the normalized path that we have completed
	return (normalized);

    }


    /**
     * Return a File object representing the specified normalized
     * context-relative path if it exists and is readable.  Otherwise,
     * return <code>null</code>.
     *
     * @param name Normalized context-relative path (with leading '/')
     */
    protected File file(String name) {

        File file = new File(base, name);
        if (file.exists() && file.canRead()) {

            // Check that this file belongs to our root path
            String canPath = null;
            try {
                canPath = file.getCanonicalPath();
            } catch (IOException e) {
            }
            if (canPath == null)
                return null;

            if (!canPath.startsWith(absoluteBase)) {
                return null;
            }

            // Windows only check
            if ((caseSensitive) && (File.separatorChar  == '\\')) {
                String fileAbsPath = file.getAbsolutePath();
                if (fileAbsPath.endsWith("."))
                    fileAbsPath = fileAbsPath + "/";
                String absPath = normalize(fileAbsPath);
                if (canPath != null)
                    canPath = normalize(canPath);
                if ((absoluteBase.length() < absPath.length()) 
                    && (absoluteBase.length() < canPath.length())) {
                    absPath = absPath.substring(absoluteBase.length() + 1);
                    if ((canPath == null) || (absPath == null))
                        return null;
                    if (absPath.equals(""))
                        absPath = "/";
                    canPath = canPath.substring(absoluteBase.length() + 1);
                    if (canPath.equals(""))
                        canPath = "/";
                    if (!canPath.equals(absPath))
                        return null;
                }
            }

        } else {
            if( log.isDebugEnabled() ) log.debug( file + " " +
                                                  file.exists() + " " +
                                                  file.canRead() );
            return null;
        }
        return file;

    }


    /**
     * List the resources which are members of a collection.
     * 
     * @param file Collection
     * @return Vector containg NamingEntry objects
     */
    protected Vector list(File file) {

        Vector entries = new Vector();
        if (!file.isDirectory())
            return entries;
        String[] names = file.list();
        Arrays.sort(names);             // Sort alphabetically
        if (names == null)
            return entries;
        NamingEntry entry = null;

        for (int i = 0; i < names.length; i++) {

            File currentFile = new File(file, names[i]);
            Object object = null;
            if (currentFile.isDirectory()) {
                FileDirContext tempContext = new FileDirContext(env);
                tempContext.setDocBase(file.getPath());
                object = tempContext;
            } else {
                //object = new FileResource(currentFile);
                object = currentFile;
            }
            entry = new NamingEntry(names[i], object, null, NamingEntry.ENTRY);
            entries.addElement(entry);

        }

        return entries;

    }

}

