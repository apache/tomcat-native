/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
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

/**
 * Class loader associated with a repository ( common, shared, server, etc ). 
 *
 * This class loader will never load any class by itself ( since it has no repository ),
 * it will just delegate to modules. 
 * 
 * Refactored as a separate class to make the code cleaner.
 * Based on catalina loader.
 *   
 * @author Costin Manolache
 * @author Remy Maucherat
 * @author Craig R. McClanahan
 */
public class RepositoryClassLoader
    extends URLClassLoader
{
    private static final boolean DEBUG=false; //LoaderProperties.getProperty("loader.debug.ModuleClassLoader") != null;
    private static final boolean DEBUGNF=false;//LoaderProperties.getProperty("loader.debug.ModuleClassLoaderNF") != null;
    
    // ----------------------------------------------------------- Constructors

    public RepositoryClassLoader(URL repositories[], ClassLoader parent, Repository lg) {
        super(repositories, parent);
        this.repository=lg;
    }
    

    public RepositoryClassLoader(URL repositories[], Repository lg) {
        super(repositories);
        this.repository=lg;
    }


    // ----------------------------------------------------- Instance Variables

    private Repository repository;

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
        
        Enumeration modulesE=repository.getModules();
        while( modulesE.hasMoreElements() ) {
            try {
                Module m=(Module)modulesE.nextElement();
                return ((ModuleClassLoader)m.getClassLoader()).findClass2(name, false);
            } catch( ClassNotFoundException ex ) {
                // ignore
            }
        }
        throw new ClassNotFoundException( name );

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
        Enumeration modulesE=repository.getModules();
        while( modulesE.hasMoreElements() ) {
            try {
                Module m=(Module)modulesE.nextElement();
                return ((ModuleClassLoader)m.getClassLoader()).findLocalClass(name);
            } catch( ClassNotFoundException ex ) {
                // ignore
            }
        }
        throw new ClassNotFoundException( name );
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
        Enumeration modulesE=repository.getModules();
        while( modulesE.hasMoreElements() ) {
                Module m=(Module)modulesE.nextElement();
                url=((ModuleClassLoader)m.getClassLoader()).findResource2(name, false);
                if( url!= null ) {
                    return url;
                }
        }
        
        if (url==null && DEBUG) {
            if (DEBUGNF) log("findResource() NOTFOUND " + name );
        }

        return null;
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
        
        Enumeration modulesE=repository.getModules();
        while( modulesE.hasMoreElements() ) {
                Module m=(Module)modulesE.nextElement();
                Enumeration myRes=((ModuleClassLoader)m.getClassLoader()).findResources2(name,false);
                if( myRes!=null ) {
                    while( myRes.hasMoreElements() ) {
                        result.addElement(myRes.nextElement());
                    }
                }
        }
        
        return result.elements();

    }

    // Next methods implement the search alghoritm - parent, repo, delegation, etc 

    /** getResource() - modified to implement the search alghoritm 
     * 
     */
    public URL getResource(String name) {
        URL url = null;
        Enumeration modulesE=repository.getModules();
        while( modulesE.hasMoreElements() ) {
                Module m=(Module)modulesE.nextElement();
                url=((ModuleClassLoader)m.getClassLoader()).getResource2(name, null, false);
                if( url!= null ) {
                    return url;
                }
        }
        
        if (url==null && DEBUG) {
            if (DEBUGNF) log("findResource() NOTFOUND " + name );
        }

        return null;
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
        Enumeration modulesE=repository.getModules();
        while( modulesE.hasMoreElements() ) {
            try {
                Module m=(Module)modulesE.nextElement();
                return ((ModuleClassLoader)m.getClassLoader()).loadClass2(name, resolve, false);
            } catch( ClassNotFoundException ex ) {
                // ignore
            }
        }
        throw new ClassNotFoundException( name );

    }


    // ------------------ Local methods ------------------------

    private void log(String s ) {
        System.err.println("RepositoryClassLoader: " + s);
    }
    private void log(String s, Throwable t ) {
        System.err.println("RepositoryClassLoader: " + s);
        t.printStackTrace();
    }

}

