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
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.StringTokenizer;
import java.util.Vector;


/**
 * Boostrap loader for Catalina.  This application constructs a class loader
 * for use in loading the Catalina internal classes (by accumulating all of the
 * JAR files found in the "server" directory under "catalina.home"), and
 * starts the regular execution of the container.  The purpose of this
 * roundabout approach is to keep the Catalina internal classes (and any
 * other classes they depend on, such as an XML parser) out of the system
 * class path and therefore not visible to application level classes.
 *
 * @author Craig R. McClanahan
 * @author Remy Maucherat
 * @author Costin Manolache
 */ 
public final class Loader  {

    private static final boolean DEBUG=true; //LoaderProperties.getProperty("loader.Loader.debug") != null;
    private static final boolean FLAT=false;//LoaderProperties.getProperty("loader.Loader.flat") != null;
    
    // -------------------------------------------------------------- Constants


    protected static final String CATALINA_HOME_TOKEN = "${catalina.home}";
    protected static final String CATALINA_BASE_TOKEN = "${catalina.base}";


    // ------------------------------------------------------- Static Variables


    /**
     * Daemon object used by main.
     */
    private static Loader daemon = null;

    // one should be enough
    ModuleListener listener;


    // -------------------------------------------------------------- Variables

    protected Repository commonRepository = null;
    protected Repository catalinaRepository = null;
    protected Repository sharedRepository = null;

    protected ClassLoader catalinaLoader = null;
    private String[] args;
    private Hashtable repositories=new Hashtable();
    


    // -------------------------------------------------------- Private Methods


    private void initClassLoaders() {
        try {
            commonRepository = initRepository("common", null);
            catalinaRepository = initRepository("server", commonRepository);
            catalinaLoader = catalinaRepository.getClassLoader();
            sharedRepository = initRepository("shared", commonRepository);
        } catch (Throwable t) {
            log("Class loader creation threw exception", t);
            System.exit(1);
        }
    }

    public Repository createRepository(String name, Repository parent) {
        Repository lg=new Repository(this);

        lg.setName(name);
       
        lg.setParent( parent );
        
        repositories.put(name, lg);
        
        if( listener != null ) {
            listener.repositoryAdd(lg);
        }
        return lg;
    }

    public Enumeration getRepositoryNames() {
        return repositories.keys();
    }
    
    /** Create a module using the NAME.loader property to construct the 
     *  classpath.
     * 
     * @param name
     * @param parent
     * @return
     * @throws Exception
     */
    private Repository initRepository(String name, Repository parent)
        throws Exception 
    {
        String value = LoaderProperties.getProperty(name + ".loader");

        Repository lg=createRepository(name, parent );
        if( DEBUG ) log( "Creating loading group " + name + " - " + value);
        
        if ((value == null) || (value.equals("")))
            return lg;

        ArrayList unpackedList = new ArrayList();
        ArrayList packedList = new ArrayList();
        ArrayList urlList = new ArrayList();

        Vector repo=split( value );
        Enumeration elems=repo.elements();
        while (elems.hasMoreElements()) {
            String repository = (String)elems.nextElement();

            // Local repository
            boolean packed = false;
            if (repository.startsWith(CATALINA_HOME_TOKEN)) {
                repository = LoaderProperties.getCatalinaHome()
                    + repository.substring(CATALINA_HOME_TOKEN.length());
            } else if (repository.startsWith(CATALINA_BASE_TOKEN)) {
                repository = LoaderProperties.getCatalinaBase()
                    + repository.substring(CATALINA_BASE_TOKEN.length());
            }

            // Check for a JAR URL repository
            try {
                urlList.add(new URL(repository));
                continue;
            } catch (MalformedURLException e) {
                // Ignore
            }

            if (repository.endsWith("*.jar")) {
                packed = true;
                repository = repository.substring
                    (0, repository.length() - "*.jar".length());
            }
            if (packed) {
                packedList.add(new File(repository));
            } else {
                unpackedList.add(new File(repository));
            }
        }

        File[] unpacked = (File[]) unpackedList.toArray(new File[0]);
        File[] packed = (File[]) packedList.toArray(new File[0]);
        URL[] urls = (URL[]) urlList.toArray(new URL[0]);

        // previously: ClassLoaderFactory.createClassLoader
        initRepository(lg, unpacked, packed, urls, parent); //new ModuleGroup();
        
        
        // TODO: JMX registration for the group loader 
        /*
        // Register the server classloader
        ObjectName objectName =
            new ObjectName("Catalina:type=ServerClassLoader,name=" + name);
        mBeanServer.registerMBean(classLoader, objectName);
        */
        
        return lg; // classLoader;

    }

    /** Small hack to allow a loader.properties file to be passed in CLI, to
     * allow a more convenient method to start with different params from 
     * explorer/kde/etc.
     * 
     * If the first arg ends with ".loader", it is used as loader.properties
     * file and removed from args[].  
     */
    private void processCLI() {
        if( args.length > 0 &&
                (args[0].toLowerCase().endsWith(".tomcat") ||
                        args[0].toLowerCase().endsWith("loader.properties") )) {
            String props=args[0];
            String args2[]=new String[args.length-1];
            System.arraycopy(args, 1, args2, 0, args2.length);
            args=args2;
            LoaderProperties.setPropertiesFile(props);
        }
    }
    
    /**
     * Initialize:
     *  - detect the home/base directories
     *  - init the loaders / modules
     *  - instantiate the "startup" class(es)
     * 
     */
    public void main()
        throws Exception
    {
        processCLI();
        
        // Set Catalina path
        LoaderProperties.setCatalinaHome();
        LoaderProperties.setCatalinaBase();

        initClassLoaders();
        
        Thread.currentThread().setContextClassLoader(catalinaLoader);

        securityPreload(catalinaLoader);

        // Load our startup classes and call its process() method
        /* Why multiple classes ? 
         * - maybe you want to start more "servers" ( tomcat ,something else )
         * - easy hook for other load on startup modules ( like a jmx hook )
         * - maybe split the loader-specific code from catalina 
         */
        String startupClasses=LoaderProperties.getProperty("loader.auto-startup",
                "org.apache.catalina.startup.CatalinaModuleListener");
        Vector v=split( startupClasses ); 
        
        for( int i=0; i<v.size(); i++ ) {
            String startupCls=(String)v.elementAt(i);
        
            if (DEBUG)
                log("Loading startup class " + startupCls);
            
            Class startupClass =
                catalinaLoader.loadClass(startupCls);
            
            Object startupInstance = startupClass.newInstance();
            
            if( startupInstance instanceof ModuleListener ) {
                addModuleListener((ModuleListener)startupInstance);

                // it can get args[] and properties from Loader
                listener.setLoader(this);
                
                // all arg processing moved there. Maybe we can make it consistent
                // for all startup schemes
                listener.start();
            } else {
                Class paramTypes[] = new Class[0];
                Object paramValues[] = new Object[0];
                Method method =
                    startupInstance.getClass().getMethod("execute", paramTypes);
                if( method==null ) 
                    method = startupInstance.getClass().getMethod("start", paramTypes);
                method.invoke(startupInstance, paramValues);
            }

        }
    }

    public Repository getRepository( String name ) {
        return (Repository)repositories.get(name);
    }
    
    private  static void securityPreload(ClassLoader loader)
        throws Exception {

        if( System.getSecurityManager() == null ){
            return;
        }

        String value=LoaderProperties.getProperty("security.preload");
        Vector repo=split( value );
        Enumeration elems=repo.elements();
        while (elems.hasMoreElements()) {
            String classN = (String)elems.nextElement();
            try {
                loader.loadClass( classN);
            } catch( Throwable t ) {
                // ignore
            }
        }
    }

    // ----------------------------------------------------------- Main Program

    public String[] getArgs() {
        return args;
    }

    /**
     * Main method.
     *
     * @param args Command line arguments to be processed
     */
    public static void main(String args[]) {
        
        try {
            if (daemon == null) {
                daemon = new Loader();
                daemon.args=args;

                try {
                    daemon.main();
                } catch (Throwable t) {
                    t.printStackTrace();
                    return;
                }
            }
        } catch (Throwable t) {
            t.printStackTrace();
        }

    }

    public void setCatalinaHome(String s) {
        System.setProperty( "catalina.home", s );
    }

    public void setCatalinaBase(String s) {
        System.setProperty( "catalina.base", s );
    }

    /**
     * Public as ModuleClassLoader. 
     * 
     * @param cl
     * @return
     */
    public Module getModule(ClassLoader cl ) {
        if( cl instanceof ModuleClassLoader ) {
            return ((ModuleClassLoader)cl).getModule();
        }
        return null;
    }
    
    /**
     * Create and return a new class loader, based on the configuration
     * defaults and the specified directory paths:
     *
     * @param unpacked Array of pathnames to unpacked directories that should
     *  be added to the repositories of the class loader, or <code>null</code> 
     * for no unpacked directories to be considered
     * @param packed Array of pathnames to directories containing JAR files
     *  that should be added to the repositories of the class loader, 
     * or <code>null</code> for no directories of JAR files to be considered
     * @param urls Array of URLs to remote repositories, designing either JAR 
     *  resources or uncompressed directories that should be added to 
     *  the repositories of the class loader, or <code>null</code> for no 
     *  directories of JAR files to be considered
     * @param parent Parent class loader for the new class loader, or
     *  <code>null</code> for the system class loader.
     *
     * @exception Exception if an error occurs constructing the class loader
     */
    private void initRepository(Repository lg, File unpacked[],
            File packed[],  URL urls[],  Repository parent)
        throws Exception 
    {
        StringBuffer sb=new StringBuffer();

        // Construct the "class path" for this class loader
        ArrayList list = new ArrayList();
        
        // Add unpacked directories
        if (unpacked != null) {
            for (int i = 0; i < unpacked.length; i++)  {
                File file = unpacked[i];
                if (!file.exists() || !file.canRead()) {
                    if (DEBUG)
                        log("  Not found:  "+ file.getAbsolutePath());
                    continue;
                }
                if (DEBUG)
                    sb.append(" "+ file.getAbsolutePath());
                String cPath=file.getCanonicalPath();
                URL url=null;
                if( cPath.toLowerCase().endsWith(".jar") ||
                        cPath.toLowerCase().endsWith(".zip") ) {
                    url = new URL("file", null, cPath);
                } else {
                    url = new URL("file", null, cPath + File.separator);
                }
                if( ! FLAT ) {
                    addLoader(lg, parent, new URL[] { url });
                } else {
                    list.add(url);
                }
            }
        }
        
        // Add packed directory JAR files
        if (packed != null) {
            for (int i = 0; i < packed.length; i++) {
                File directory = packed[i];
                if (!directory.isDirectory() || !directory.exists() ||
                        !directory.canRead()) {
                    if (DEBUG)
                        log("  Not found:  "+ directory.getAbsolutePath());
                    continue;
                }
                String filenames[] = directory.list();
                for (int j = 0; j < filenames.length; j++) {
                    String filename = filenames[j].toLowerCase();
                    if (!filename.endsWith(".jar"))
                        continue;
                    File file = new File(directory, filenames[j]);
                    if (DEBUG)
                        sb.append(" "+ file.getAbsolutePath());
                    URL url = new URL("file", null,
                            file.getCanonicalPath());
                    if( ! FLAT ) {
                        addLoader(lg, parent, new URL[] { url });
                    } else {
                        list.add(url);
                    }
                }
            }
        }
        
        // Add URLs
        if (urls != null) {
            for (int i = 0; i < urls.length; i++) {
                if( ! FLAT ) {
                    addLoader(lg, parent, new URL[] { urls[i] });
                } else {
                    list.add(urls[i]);
                }
                if (DEBUG)
                    sb.append(" "+ urls[i]);
            }
        }
        
        // Construct the class loader itself
        
        // TODO: experiment with loading each jar in a separate loader.
        if (DEBUG)
            log("Creating new class loader " + lg.getName() + " " + sb.toString());
        
        
        URL[] array = (URL[]) list.toArray(new URL[list.size()]);
        if( array.length > 0 ) {
            addLoader(lg, parent, array);
        }
    }
    
    /**
     * @param lg
     * @param parent
     * @param list
     */
    private void addLoader(Repository lg, Repository parent, URL array[]) 
        throws Exception
    {
        Module module=new Module();
        
        module.setParent( parent );
        module.setClasspath( array );
        
        lg.addModule(module);
        
    }

    private static Vector split( String value ) {
        Vector result=new Vector();
        StringTokenizer tokenizer = new StringTokenizer(value, ",");
        while (tokenizer.hasMoreElements()) {
            String repository = tokenizer.nextToken();
            repository.trim();
            if( ! "".equals(repository) )
                result.addElement(repository);
        }
        return result;
    }

    void notifyModuleStart(Module module) {
        if(listener!=null) listener.moduleStart(module);
    }

    void notifyModuleStop(Module module) {
        listener.moduleStop(module);
    }
    
    /** Add a module listener. 
     * 
     * To keep the dependencies minimal, the loader package only implements the
     * basic class loading mechanism - but any advanced feature ( management, 
     * policy, etc ) should be implemented by a module.
     * 
     * @param listener
     */
    public void addModuleListener(ModuleListener listener) {
        this.listener=listener;
    }

    private static void log(String s) {
        System.err.println("Main: " + s);
    }
    
    private static void log( String msg, Throwable t ) {
        System.err.println("Main: " + msg);
        t.printStackTrace();
    }

}
