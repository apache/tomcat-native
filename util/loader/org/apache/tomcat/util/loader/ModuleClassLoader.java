/*
 * Copyright 1999,2004 The Apache Software Foundation.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


package org.apache.tomcat.util.loader;

import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.Enumeration;
import java.util.Vector;

/*
 * Initially, I started with WebappClassLoader attempting to clean up and
 * refactor it. Because of complexity and very weird ( and likely buggy ) 
 * behavior, I moved the other way, starting with URLClassLoader and adding
 * the functionality from WebappClassLoader. 
 * 
 * The current version has a lot of unimplemented WebappClassLoader features and
 * TODOs - all of them are needed in order to support a single/consistent loader
 * for webapps and server/modules. 
 * 
 * - all ordering options and tricks
 * - local loading - in case it can be made more efficient than URLCL
 * - hook to plugin JNDI finder
 * - ability to add extra permissions to loaded classes
 * - ability to use work dir for anti-jar locking and generated classes ( incl getURLs)
 * 
 * 
 * Things better kept out:
 *  - negative cache - it'll waste space with little benefit, most not found classes
 *  will not be asked multiple times, and most will be in other loaders
 *  - binaryContent cache - it's unlikely same resource will be loaded multiple
 * times, and some may be large  
 * 
 */

/**
 * Simple module class loader. Will search the repository if the class is not
 * found locally.
 * 
 * TODO: findResources() - merge all responses from the repo and parent. 
 *
 * Based on StandardClassLoader and WebappClassLoader.
 *   
 * @author Costin Manolache
 * @author Remy Maucherat
 * @author Craig R. McClanahan
 */
public class ModuleClassLoader
    extends URLClassLoader
{
    // Don't use commons logging or configs to debug loading - logging is dependent
    // on loaders and drags a lot of stuff in the classpath 
    //
    private static final boolean DEBUG=false; //LoaderProperties.getProperty("loader.ModuleClassLoader.debug") != null;
    private static final boolean DEBUGNF=false;//LoaderProperties.getProperty("loader.ModuleClassLoader.debugNF") != null;
    
    // ----------------------------------------------------------- Constructors


    public ModuleClassLoader(URL repositories[], ClassLoader parent) {
        super(repositories, parent);
    }
    

    public ModuleClassLoader(URL repositories[]) {
        super(repositories);
    }


    // ----------------------------------------------------- Instance Variables

    protected Repository repository;

    /**
     * Should this class loader delegate to the parent class loader
     * <strong>before</strong> searching its own repositories (i.e. the
     * usual Java2 delegation model)?  If set to <code>false</code>,
     * this class loader will search its own repositories first, and
     * delegate to the parent only if the class or resource is not
     * found locally.
     */
    protected boolean delegate = false;

    /**
     * Last time a JAR was accessed. 
     * TODO: change to last time the loader was accessed
     */
    protected long lastJarAccessed = 0L;

    /**
     * Has this component been started?
     */
    protected boolean started = false;

    protected Module module;

    // ------------------------------------------------------------- Properties


    /**
     * Return the "delegate first" flag for this class loader.
     */
//    boolean getDelegate() {
//
//        return (this.delegate);
//
//    }


    /**
     * Set the "delegate first" flag for this class loader.
     *
     * @param delegate The new "delegate first" flag
     */
    void setDelegate(boolean delegate) {
        this.delegate = delegate;
    }

    void setRepository(Repository lg ) {
        this.repository=lg;
    }

    void setModule(Module webappLoader) {
        this.module=webappLoader;
    }

    /** Not public - his can only be called from package.
     *  To get the module from a ClassLoader you need access to the Loader
     * instance.
     * 
     * @return
     */
    Module getModule() {
        return module;
    }

    void setWorkDir(File s) {
        // TODO
    }

    /**
     * Add a new repository to the set of places this ClassLoader can look for
     * classes to be loaded.
     *
     * @param repository Name of a source of classes to be loaded, such as a
     *  directory pathname, a JAR file pathname, or a ZIP file pathname
     *
     * @exception IllegalArgumentException if the specified repository is
     *  invalid or does not exist
     */
    void addRepository(String repository) {
        // Add this repository to our underlying class loader
        try {
            URL url = new URL(repository);
            super.addURL(url);
        } catch (MalformedURLException e) {
            IllegalArgumentException iae = new IllegalArgumentException
                ("Invalid repository: " + repository); 
            iae.initCause(e);
            //jdkCompat.chainException(iae, e);
            throw iae;
        }
    }

    /**
     * Have one or more classes or resources been modified so that a reload
     * is appropriate?
     * 
     * Not public - call it via Module
     */
    boolean modified() {
        if (DEBUG)
            log("modified() false");

        // TODO - check at least the jars 
        return (false);
    }

    // ---------------------------------------------------- ClassLoader Methods


    /**
     * Find the specified class in our local repositories, if possible.  If
     * not found, throw <code>ClassNotFoundException</code>.
     *
     * @param name Name of the class to be loaded
     *
     * @exception ClassNotFoundException if the class was not found
     */
    public Class findClass(String name) throws ClassNotFoundException {

        Class clazz = null;
            
        try {
            clazz = super.findClass(name);
        } catch (RuntimeException e) {
            if (DEBUG)
                log("findClass() -->RuntimeException " + name, e);
            throw e;
        } catch( ClassNotFoundException ex ) {
            if (DEBUGNF)
                log("findClass() NOTFOUND  " + name);
            throw ex;
        }
            
        if (clazz == null) { // does it ever happen ? 
            if (DEBUGNF)
                log("findClass() NOTFOUND throw CNFE " + name);
            throw new ClassNotFoundException(name);
        }

        // Return the class we have located
        if (DEBUG) {
            if( clazz.getClassLoader() != this ) 
                log("findClass() FOUND " + clazz + " Loaded by " + clazz.getClassLoader());
            else 
                log("findClass() FOUND " + clazz );
        }
        return (clazz);
    }
    
    /** Same as findClass, but also checks if the class has been previously 
     * loaded.
     * 
     * In most implementations, findClass() doesn't check with findLoadedClass().
     * In order to implement repository, we need to ask each loader in the group
     * to load only from it's local resources - however this will lead to errors
     * ( duplicated definition ) if findClass() is used.
     *
     * @param name
     * @return
     * @throws ClassNotFoundException
     */
    public Class findLocalClass(String name) throws ClassNotFoundException
    {
        Class clazz = findLoadedClass(name);
        if (clazz != null) {
            if (DEBUG)
                log("findLocalClass() - FOUND " + name);
            return (clazz);
        }
        
        return findClass(name);
    }



    
    /**
     * Find the specified resource in our local repository, and return a
     * <code>URL</code> refering to it, or <code>null</code> if this resource
     * cannot be found.
     *
     * @param name Name of the resource to be found
     */
    public URL findResource(final String name) {

        URL url = null;

        url = super.findResource(name);
        
        if(url==null) {
            // try the repository
            // TODO
        }
        
        if (url==null && DEBUG) {
            if (DEBUGNF) log("findResource() NOTFOUND " + name );
            return null;
        }

        if (DEBUG) log("findResource() found " + name + " " + url );
        return (url);
    }


    /**
     * Return an enumeration of <code>URLs</code> representing all of the
     * resources with the given name.  If no resources with this name are
     * found, return an empty enumeration.
     *
     * @param name Name of the resources to be found
     *
     * @exception IOException if an input/output error occurs
     */
    public Enumeration findResources(String name) throws IOException {
        Vector result=new Vector();
        
        Enumeration myRes=super.findResources(name);
        if( myRes!=null ) {
            while( myRes.hasMoreElements() ) {
                result.addElement(myRes.nextElement());
            }
        }
        // TODO: same in repository ( parent ? ) 
        
        return result.elements();

    }

    // Next methods implement the search alghoritm - parent, repo, delegation, etc 

    /** getResource() - modified to implement the search alghoritm 
     * 
     */
    public URL getResource(String name) {

        URL url = null;

        // (1) Delegate to parent if requested
        if (delegate) {
            url=getResourceParentDelegate(name);
            if(url!=null ) return url;
        }

        // (2) Search local repositories
        url = findResource(name);
        if (url != null) {
            // TODO: antijar locking - WebappClassLoader is making a copy ( is it ??)
            if (DEBUG)
                log("getResource() found locally " + delegate + " " + name + " " + url);
            return (url);
        }

        // Finally, try the group loaders ( via super() in StandardClassLoader ).
        // not found using normal loading mechanism. load from one of the classes in the group
        if( repository!=null ) {
            url=repository.findResource(this, name);
            if(url!=null ) {
                if( DEBUG )
                    log("getResource() FOUND from group " + repository.getName() + " " + name + " " + url);
                return url;
            }
        }

        // (3) Delegate to parent unconditionally if not already attempted
        if( !delegate ) {
            url=getResourceParentDelegate(name);
            if(url!=null ) return url;
        }

        
        // (4) Resource was not found
        if (DEBUGNF)
            log("getResource() NOTFOUND  " + delegate + " " + name + " " + url);
        return (null);

    }

    // to avoid duplication
    private URL getResourceParentDelegate(String name) {
        URL url=null;
        ClassLoader loader = getParent();
        
        if (loader == null) {
            loader = getSystemClassLoader();
            if (url != null) {
                if (DEBUG)
                    log("getResource() found by system " +  delegate + " " + name + " " + url);
                return (url);
            }
        } else {
            url = loader.getResource(name);
            if (url != null) {
                if (DEBUG)
                    log("getResource() found by parent " +  delegate + " " + name + " " + url);
                return (url);
            }
        }

        return url;
    }
    
    /**
     * Load the class with the specified name, searching using the following
     * algorithm until it finds and returns the class.  If the class cannot
     * be found, returns <code>ClassNotFoundException</code>.
     * <ul>
     * <li>Call <code>findLoadedClass(String)</code> to check if the
     *     class has already been loaded.  If it has, the same
     *     <code>Class</code> object is returned.</li>
     * <li>If the <code>delegate</code> property is set to <code>true</code>,
     *     call the <code>loadClass()</code> method of the parent class
     *     loader, if any.</li>
     * <li>Call <code>findClass()</code> to find this class in our locally
     *     defined repositories.</li>
     * <li>Call the <code>loadClass()</code> method of our parent
     *     class loader, if any.</li>
     * </ul>
     * If the class was found using the above steps, and the
     * <code>resolve</code> flag is <code>true</code>, this method will then
     * call <code>resolveClass(Class)</code> on the resulting Class object.
     *
     * @param name Name of the class to be loaded
     * @param resolve If <code>true</code> then resolve the class
     *
     * @exception ClassNotFoundException if the class was not found
     */
    public Class loadClass(String name, boolean resolve)
        throws ClassNotFoundException
    {

        Class clazz = null;

        // Don't load classes if class loader is stopped
        if (!started) {
            //log("Not started " + this + " " + module);
            throw new ThreadDeath();
        }

        // (0) Check our previously loaded local class cache
        clazz = findLoadedClass(name);
        if (clazz != null) {
            if (DEBUG)
                log("loadClass() FOUND findLoadedClass " + name + " , " + resolve);
            if (resolve) resolveClass(clazz);
            return (clazz);
        }

        // (0.2) Try loading the class with the system class loader, to prevent
        //       the webapp from overriding J2SE classes
        try {
            clazz = getSystemClassLoader().loadClass(name);
            if (clazz != null) {
                // enabling this can result in ClassCircularityException
//                if (DEBUG)
//                    log("loadClass() FOUND system " + name + " , " + resolve);
                if (resolve) resolveClass(clazz);
                return (clazz);
            }
        } catch (ClassNotFoundException e) {
            // Ignore
        }

        // TODO: delegate based on filter
        boolean delegateLoad = delegate;// || filter(name);

        // (1) Delegate to our parent if requested
        if (delegateLoad) {

            ClassLoader loader = getParent();
            if( loader != null ) {
                try {
                    clazz = loader.loadClass(name);
                    if (clazz != null) {
                        if (DEBUG)
                            log("loadClass() FOUND by parent " + delegate + " " + name + " , " + resolve);
                        if (resolve)
                            resolveClass(clazz);
                        return (clazz);
                    }
                } catch (ClassNotFoundException e) {
                    ;
                }
            }
        }

        // (2) Search local repositories
        try {
            clazz = findClass(name);
            if (clazz != null) {
//                if (DEBUG)
//                    log("loadClass - FOUND findClass " + delegate + " " + name + " , " + resolve);
                if (resolve) resolveClass(clazz);
                return (clazz);
            }
        } catch (ClassNotFoundException e) {
            ;
        }

        // Finally, try the group loaders ( via super() in StandardClassLoader ).
        // not found using normal loading mechanism. load from one of the classes in the group
        if( repository!=null ) {
            Class cls=repository.findClass(this, name);
            if(cls!=null ) {
                if( DEBUG )
                    log("loadClass(): FOUND from group " + repository.getName() + " " + name);
                if (resolve) resolveClass(clazz);
                return cls;
            }
        }

        // (3) Delegate to parent unconditionally
        if (!delegateLoad) {
            ClassLoader loader = getParent();
            if( loader != null ) {
                try {
                    clazz = loader.loadClass(name);
                    if (clazz != null) {
                        if (DEBUG)
                            log("loadClass() FOUND parent " + delegate + " " + name + " , " + resolve);
                        if (resolve) resolveClass(clazz);
                        return (clazz);
                    }
                } catch (ClassNotFoundException e) {
                    ;
                }
            }
        }

        if( DEBUGNF ) log("loadClass(): NOTFOUND " + name );
        throw new ClassNotFoundException(name);
    }


    // ------------------------------------------------------ Lifecycle Methods



    /**
     * Start the class loader.
     *
     * @exception LifecycleException if a lifecycle error occurs
     */
    void start()  {

        started = true;

    }

    /** Support for "disabled" state.
    *
    * @return
    */
    boolean isStarted() {
        return started;
    }


    /**
     * Stop the class loader.
     *
     * @exception LifecycleException if a lifecycle error occurs
     */
    void stop() {

        started = false;

    }




    /**
     * Validate a classname. As per SRV.9.7.2, we must restict loading of 
     * classes from J2SE (java.*) and classes of the servlet API 
     * (javax.servlet.*). That should enhance robustness and prevent a number
     * of user error (where an older version of servlet.jar would be present
     * in /WEB-INF/lib).
     * 
     * @param name class name
     * @return true if the name is valid
     */
    protected boolean validate(String name) {

        if (name == null)
            return false;
        if (name.startsWith("java."))
            return false;

        return true;

    }


    // ------------------ Local methods ------------------------

    private void log(String s ) {
        System.err.println("ModuleCL: " + s);
    }
    private void log(String s, Throwable t ) {
        System.err.println("ModuleCL: " + s);
        t.printStackTrace();
    }
    
    Object debugObj=new Object();

    /**
     * Render a String representation of this object.
     */
    public String toString() {

        StringBuffer sb = new StringBuffer("ModuleCL ");
        sb.append(debugObj).append(" delegate: ");
        sb.append(delegate);
        //sb.append("\r\n");
        sb.append(" cp: ");
        URL cp[]=super.getURLs();
        if (cp != null ) {
            for (int i = 0; i <cp.length; i++) {
                sb.append("  ");
                sb.append(cp[i].getFile());
            }
        }
        if (getParent() != null) {
            sb.append("\r\n----------> Parent: ");
            sb.append(getParent().toString());
            sb.append("\r\n");
        }
        return (sb.toString());
    }
}

