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
import java.io.Serializable;
import java.lang.reflect.Constructor;
import java.net.URL;

// Based on org.apache.catalina.Loader - removed most of the catalina-specific

/**
 *  Base representation for "server extensions" ( connectors, realms, etc ), webapps,
 * libraries.   
 * 
 * 
 * @author Costin Manolache
 * 
 * @author Craig R. McClanahan
 * @author Remy Maucherat
 */
public class Module implements Serializable {

    // ----------------------------------------------------------- Constructors


    /**
     * 
     */
    public Module() {
    }


    // ----------------------------------------------------- Instance Variables

    private static final boolean DEBUG=false; //LoaderProperties.getProperty("loader.Module.debug") != null;

    /**
     * The class loader being managed by this Loader component.
     */
    private transient ModuleClassLoader classLoader = null;

    /**
     * The "follow standard delegation model" flag that will be used to
     * configure our ClassLoader.
     */
    private boolean delegate = false;

    private Class classLoaderClass;

    /**
     * The Java class name of the ClassLoader implementation to be used.
     * This class should extend ModuleClassLoader, otherwise, a different 
     * loader implementation must be used.
     */
    private String loaderClass = null;
//        "org.apache.catalina.loader.WebappClassLoader";

    /**
     * The parent class loader of the class loader we will create.
     * Use Repository if the parent is also a repository, otherwise set 
     * the ClassLoader
     */
    private transient ClassLoader parentClassLoader = null;
    private Repository parent;

    private Repository repository;

    /**
     * The set of repositories associated with this class loader.
     */
    private String repositories[] = new String[0];
    private URL classpath[] ;

    private File workDir;

    /**
     * Has this component been started?
     */
    private boolean started = false;

    boolean hasIndex=false;

    // ------------------------------------------------------------- Properties


    /**
     * Return the Java class loader to be used by this Container.
     */
    public ClassLoader getClassLoader() {
        return classLoader;
    }


    /**
     * Return the "follow standard delegation model" flag used to configure
     * our ClassLoader.
     */
    public boolean getDelegate() {
        return (this.delegate);
    }


    /**
     * Set the "follow standard delegation model" flag used to configure
     * our ClassLoader.
     *
     * @param delegate The new flag
     */
    public void setDelegate(boolean delegate) {
        boolean oldDelegate = this.delegate;
        this.delegate = delegate;
        if( classLoader != null ) classLoader.setDelegate(delegate);
    }


    // --------------------------------------------------------- Public Methods

    /**
     * Has the internal repository associated with this Loader been modified,
     * such that the loaded classes should be reloaded?
     */
    public boolean modified() {
        return (classLoader.modified());
    }
    
    public boolean isStarted() {
        return started;
    }

    public String getClasspathString() {
        if(classpath==null ) {
            return null;
        }
        StringBuffer sb=new StringBuffer();
        for( int i=0; i<classpath.length; i++ ) {
            if( i>0 ) sb.append(":");
            sb.append( classpath[i].getFile() );
        }
        return sb.toString();
    }
    
    /**
     * Start this component, initializing our associated class loader.
     *
     * @exception LifecycleException if a lifecycle error occurs
     */
    public void start()  {
        // Validate and update our current component state
        if (started)
            throw new RuntimeException
                ("Already started");
        started = true;

        // Construct a class loader based on our current repositories list
        try {

            classLoader = createClassLoader();

            //classLoader.setResources(container.getResources());
            classLoader.setDelegate(this.delegate);

            for (int i = 0; i < repositories.length; i++) {
                classLoader.addRepository(repositories[i]);
            }

            classLoader.start();

            getRepository().getLoader().notifyModuleStart(this);

        } catch (Throwable t) {
            log( "LifecycleException ", t );
            throw new RuntimeException("start: ", t);
        }

    }


    /**
     * Stop this component, finalizing our associated class loader.
     *
     * @exception LifecycleException if a lifecycle error occurs
     */
    public void stop() {
        if (!started)
            throw new RuntimeException("stop: started=false");
        
        //if (DEBUG) 
        log("stop()", null);
        
        getRepository().getLoader().notifyModuleStop(this);
        
        started = false;

        // unregister this classloader in the server group
        if( repository != null ) repository.removeClassLoader(classLoader);

        // Throw away our current class loader
        classLoader.stop();

        classLoader = null;

    }

    // ------------------------------------------------------- Private Methods

    /** Set the class used to construct the class loader.
     * 
     * The alternative is to set the context class loader to allow loaderClass
     * to be created. 
     * 
     * @param c
     */
    public void setClassLoaderClass( Class c ) {
        classLoaderClass=c;
    }

    /**
     * Create associated classLoader.
     */
    ModuleClassLoader createClassLoader()
        throws Exception 
    {

        if( classLoader != null ) return classLoader;
        if( classLoaderClass==null && loaderClass!=null) {
            classLoaderClass = Class.forName(loaderClass);
        }
        
        ModuleClassLoader classLoader = null;

        if (parentClassLoader == null) {
            if( parent != null ) {
                parentClassLoader = parent.getClassLoader();
            }
        }
        if (parentClassLoader == null) {
            parentClassLoader = Thread.currentThread().getContextClassLoader();
        }
        
        if( classLoaderClass != null ) {
            Class[] argTypes = { URL[].class, ClassLoader.class };
            Object[] args = { classpath, parentClassLoader };
            Constructor constr = classLoaderClass.getConstructor(argTypes);
            classLoader = (ModuleClassLoader) constr.newInstance(args);
        } else {
            classLoader=new ModuleClassLoader( classpath, parentClassLoader);
        }
        
        classLoader.setModule(this);
        classLoader.setDelegate( delegate );
        
        classLoader.start();
        repository.addClassLoader(classLoader);
        
        return classLoader;
    }


    /**
     * @param parent
     */
    public void setParent(Repository parent) {
        this.parent=parent;
    }

    /**
     * @param array
     */
    public void setClasspath(URL[] array) {
        this.classpath=array;
    }


    /**
     * @param lg
     */
    public void setRepository(Repository lg) {
        this.repository=lg;
    }

    /**
     * Return a String representation of this component.
     */
    public String toString() {

        StringBuffer sb = new StringBuffer("ModuleLoader[");
        sb.append(getClasspathString());
        sb.append("]");
        return (sb.toString());

    }

    private void log( String s ) {
        log(s,null);
    }
    
    private void log(String s, Throwable t ) {
        System.err.println("Module: " + s );
        if( t!=null)
            t.printStackTrace();
    }


    /**
     * @return
     */
    public Repository getRepository() {
        return repository;
    }


    /**
     * @return
     */
    public String getName() {
        return classpath[0].getFile(); // this.toString();
    }


}
