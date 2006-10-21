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
import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Properties;
import java.util.StringTokenizer;
import java.util.Vector;


/**
 * Boostrap loader for Catalina or other java apps. 
 * 
 * This application constructs a class loader
 * for use in loading the Catalina internal classes (by accumulating all of the
 * JAR files found in the "server" directory under "catalina.home"), and
 * starts the regular execution of the container.  The purpose of this
 * roundabout approach is to keep the Catalina internal classes (and any
 * other classes they depend on, such as an XML parser) out of the system
 * class path and therefore not visible to application level classes.
 *
 *
 * Merged with CatalinaProperties:
 * Load a properties file describing the modules and startup sequence.
 * This is responsible for configuration of the loader. 
 * TODO: support jmx-style configuration, including persistence.
 * TODO: better separate legacy config and the new style
 * 
 * The properties file will be named "loader.properties" or 
 * "catalina.properties" ( for backwad compatibility ) and
 * will be searched in:
 *  - TODO 
 * 
 * Properties used:
 *  - TODO
 *
 * loader.* and *.loader properties are used internally by the loader ( 
 *  *.loader is for backward compat with catalina ).
 * All other properties in the config file are set as System properties.
 * 
 * Based on o.a.catalina.bootstrap.CatalinaProperties - utility class to read 
 * the bootstrap Catalina configuration.

 *
 * @author Craig R. McClanahan
 * @author Remy Maucherat
 * @author Costin Manolache
 */ 
public final class Loader  {

    private static final boolean DEBUG=true; //LoaderProperties.getProperty("loader.debug.Loader") != null;
    // If flat, only one loader is created. If false - one loader per jar/dir
    private static final boolean FLAT=false;//LoaderProperties.getProperty("loader.Loader.flat") != null;
    
    // -------------------------------------------------------------- Constants


    private static final String CATALINA_HOME_TOKEN = "${catalina.home}";
    private static final String CATALINA_BASE_TOKEN = "${catalina.base}";


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
    private ClassLoader parentClassLoader;
    
    private static Properties properties = null;


    private static String propFile;

    // -------------------------------------------------------- Private Methods
    
    /** Set the parent class loader - can be used instead of setParent, 
     * in case this is the top loader and needs to delagate to embedding app.
     * The common loader will delegate to this loader
     * 
     * @param myL
     */
    public void setParentClassLoader(ClassLoader myL) {
        this.parentClassLoader=myL;
    }



    /** Initialize the loader, creating all repositories.
     *  Will create common, server, shared. 
     *  
     *  TODO: create additional repos.
     *
     */
    public void init() {
        try {
            commonRepository = initRepository("common", null, parentClassLoader);
            catalinaRepository = initRepository("server", commonRepository,null);
            catalinaLoader = catalinaRepository.getClassLoader();
            sharedRepository = initRepository("shared", commonRepository,null);
        } catch (Throwable t) {
            log("Class loader creation threw exception", t);
            System.exit(1);
        }
    }

    /** Create a new repository. 
     *  No Module is added ( currently )
     *  TODO: use props to prepopulate, if any is present.
     * 
     * @param name
     * @param parent
     * @return
     */
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
    private Repository initRepository(String name, Repository parent, ClassLoader pcl)
        throws Exception 
    {
        String value = getProperty(name + ".loader");

        Repository lg=createRepository(name, parent );
        if( pcl != null )
            lg.setParentClassLoader( pcl );
        if( DEBUG ) log( "Creating loading group " + name + " - " + value + " " + pcl);
        
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
                repository = getCatalinaHome()
                    + repository.substring(CATALINA_HOME_TOKEN.length());
            } else if (repository.startsWith(CATALINA_BASE_TOKEN)) {
                repository = getCatalinaBase()
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
        if( args!=null && args.length > 0 &&
                (args[0].toLowerCase().endsWith(".tomcat") ||
                        args[0].toLowerCase().endsWith(".loader") ||
                        args[0].toLowerCase().endsWith("loader.properties") )) {
            String props=args[0];
            String args2[]=new String[args.length-1];
            System.arraycopy(args, 1, args2, 0, args2.length);
            args=args2;
            setPropertiesFile(props);
        } else {
            loadProperties();
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
        setCatalinaHome();
        setCatalinaBase();

        init();
        
        Thread.currentThread().setContextClassLoader(catalinaLoader);

        securityPreload(catalinaLoader);

        autostart();
    }

    private void autostart() throws ClassNotFoundException, InstantiationException, IllegalAccessException, NoSuchMethodException, InvocationTargetException {
        // Load our startup classes and call its process() method
        /* Why multiple classes ? 
         * - maybe you want to start more "servers" ( tomcat ,something else )
         * - easy hook for other load on startup modules ( like a jmx hook )
         * - maybe split the loader-specific code from catalina 
         */
        String startupClasses=getProperty("loader.auto-startup",
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
            } else if ( startupInstance instanceof Runnable ) {
                ((Runnable)startupInstance).run();
            } else {
                Class paramTypes[] = new Class[0];
                Object paramValues[] = new Object[0];
                Method method =
                    startupInstance.getClass().getMethod("execute", paramTypes);
                if( method==null ) 
                    method = startupInstance.getClass().getMethod("start", paramTypes);
                if( method!=null )
                    method.invoke(startupInstance, paramValues);
            }

        }
    }

    /** Returns one of the repositories. 
     * 
     *  Typically at startup we create at least: "common", "shared" and "server", with 
     *  same meaning as in tomcat. 
     * 
     * @param name
     * @return
     */
    public Repository getRepository( String name ) {
        return (Repository)repositories.get(name);
    }
    
    private  static void securityPreload(ClassLoader loader)
        throws Exception {

        if( System.getSecurityManager() == null ){
            return;
        }

        String value=getProperty("security.preload");
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

    /** Access to the command line arguments, when Loader is used to launc an app.
     */
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


    /**
     * Initialize the loader properties explicitely. 
     * 
     * TODO: add setPropertiesRes
     * 
     * @param props
     */
    public void setPropertiesFile(String props) {
        propFile=props;
        loadProperties();
    }

    /**
     * Return specified property value.
     */
    static String getProperty(String name) {
        if( properties==null ) loadProperties();
        return properties.getProperty(name);
    }


    /**
     * Return specified property value.
     */
    static String getProperty(String name, String defaultValue) {
        if( properties==null ) loadProperties();
        return properties.getProperty(name, defaultValue);
    }

    /**
     * Load properties.
     * Will try: 
     * - "catalina.config" system property ( a URL )
     * - "catalina.base", "catalina.home", "user.dir" system properties +
     *    "/conf/" "../conf" "/" +  "loader.properties" or "catalina.properties"
     * - /org/apache/catalina/startup/catalina.properties
     * 
     * Properties will be loaded as system properties. 
     * 
     * loader.properties was added to allow coexistence with bootstrap.jar ( the 
     * current scheme ), since the classpaths are slightly different. 
     */
    static void loadProperties() {
        properties = new Properties();
        
        InputStream is = null;
        Throwable error = null;

        // TODO: paste the code to do ${} substitution 
        // TODO: add the code to detect where tomcat-properties is loaded from
        if( propFile != null ) {
            try {
                File properties = new File(propFile);
                is = new FileInputStream(properties);
                if( is!=null && DEBUG ) {
                    log("Loaded from loader.properties " + properties );
                }
            } catch( Throwable t) {
                System.err.println("Can't find " + propFile);
                return;
            }
        }
        
        if( is == null ) {
            try {
                // "catalina.config" system property
                String configUrl = System.getProperty("catalina.config");
                if (configUrl != null) {
                    is = (new URL(configUrl)).openStream();
                    if( is!=null && DEBUG ) {
                        log("Loaded from catalina.config " + configUrl );
                    }
                }
            } catch (Throwable t) {
                // Ignore
            }
        }

        if( is == null ) {
            try {
                // "loader.config" system property
                String configUrl = System.getProperty("loader.config");
                if (configUrl != null) {
                    is = (new URL(configUrl)).openStream();
                    if( is!=null && DEBUG ) {
                        log("Loaded from catalina.config " + configUrl );
                    }
                }
            } catch (Throwable t) {
                // Ignore
            }
        }

        if (is == null) {
            try {
                setCatalinaBase(); // use system properties, then user.dir
                File home = new File(getCatalinaBase());
                File conf = new File(home, "conf");
                
                // use conf if exists, or the base directory otherwise
                if( ! conf.exists() ) conf = new File(home, "../conf");
                if( ! conf.exists() ) conf = home;
                File propertiesF=null;
                if(  conf.exists() )  
                     propertiesF= new File(conf, "loader.properties");
                if( ! propertiesF.exists() ) {
                    propertiesF= new File( home, "loader.properties");
                }
                if( propertiesF.exists() )
                    is = new FileInputStream(propertiesF);
                if( is!=null && DEBUG ) {
                    log("Loaded from loader.properties " + properties );
                }
            } catch (Throwable t) {
                // Ignore
            }
        }

        if (is == null) {
            try {
                File home = new File(getCatalinaBase());
                File conf = new File(home, "conf");
                File properties = new File(conf, "catalina.properties");
                is = new FileInputStream(properties);
                if( is!=null && DEBUG ) {
                    log("Loaded from catalina.properties " + properties );
                }
            } catch (Throwable t) {
                // Ignore
            }
        }

        if (is == null) {
            try {
                is = Loader.class.getResourceAsStream
                    ("/org/apache/catalina/startup/catalina.properties");
                if( is!=null && DEBUG ) {
                    log("Loaded from o/a/c/startup/catalina.properties " );
                }

            } catch (Throwable t) {
                // Ignore
            }
        }

        if (is == null) {
            try {
                is = Loader.class.getResourceAsStream
                    ("loader.properties");
                if( is!=null && DEBUG ) {
                    log("Loaded from res loader.properties " );
                }
            } catch (Throwable t) {
                // Ignore
            }
        }
        
        if (is != null) {
            try {
                properties.load(is);
                is.close();
            } catch (Throwable t) {
                error = t;
            }
        }

//        if ((is == null) || (error != null)) {
//            // Do something
//            log("Error: no properties found !!!");
//        }

        // Register the _unused_ properties as system properties
        if( properties != null ) {
            Enumeration enumeration = properties.propertyNames();
            while (enumeration.hasMoreElements()) {
                String name = (String) enumeration.nextElement();
                String value = properties.getProperty(name);
                if( "security.preload".equals( name )) continue;
                if( "package.access".equals( name )) continue;
                if( "package.definition".equals( name )) continue;
                if( name.endsWith(".loader")) continue;
                if( name.startsWith("loader.")) continue;
                if (value != null) {
                    System.setProperty(name, value);
                }
            }
        }

    }

    static void setCatalinaHome(String s) {
        System.setProperty( "catalina.home", s );
    }

    static void setCatalinaBase(String s) {
        System.setProperty( "catalina.base", s );
    }


    /**
     * Get the value of the catalina.home environment variable.
     * 
     * @deprecated
     */
    static String getCatalinaHome() {
        if( properties==null ) loadProperties();
        return System.getProperty("catalina.home",
                                  System.getProperty("user.dir"));
    }
    
    
    /**
     * Get the value of the catalina.base environment variable.
     * 
     * @deprecated
     */
    static String getCatalinaBase() {
        if( properties==null ) loadProperties();
        return System.getProperty("catalina.base", getCatalinaHome());
    }

    
    /**
     * Set the <code>catalina.base</code> System property to the current
     * working directory if it has not been set.
     */
    static void setCatalinaBase() {
        if( properties==null ) loadProperties();

        if (System.getProperty("catalina.base") != null)
            return;
        if (System.getProperty("catalina.home") != null)
            System.setProperty("catalina.base",
                               System.getProperty("catalina.home"));
        else
            System.setProperty("catalina.base",
                               System.getProperty("user.dir"));

    }


    /**
     * Set the <code>catalina.home</code> System property to the current
     * working directory if it has not been set.
     */
    static void setCatalinaHome() {

        if (System.getProperty("catalina.home") != null)
            return;
        File bootstrapJar = 
            new File(System.getProperty("user.dir"), "bootstrap.jar");
        File tloaderJar = 
            new File(System.getProperty("user.dir"), "tomcat-loader.jar");
        if (bootstrapJar.exists() || tloaderJar.exists()) {
            try {
                System.setProperty
                    ("catalina.home", 
                     (new File(System.getProperty("user.dir"), ".."))
                     .getCanonicalPath());
            } catch (Exception e) {
                // Ignore
                System.setProperty("catalina.home",
                                   System.getProperty("user.dir"));
            }
        } else {
            System.setProperty("catalina.home",
                               System.getProperty("user.dir"));
        }

    }


    
    /**
     * Get the module from the classloader. Works only for classloaders created by
     * this package - or extending ModuleClassLoader.
     * 
     * This shold be the only public method that allows this - Loader acts as a 
     * guard, only if you have the loader instance you can access the internals.
     * 
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
//                String cPath=file.getCanonicalPath();
//                URL url=null;
//                
//                if( cPath.toLowerCase().endsWith(".jar") ||
//                        cPath.toLowerCase().endsWith(".zip") ) {
//                    url = new URL("file", null, cPath);
//                } else {
//                    url = new URL("file", null, cPath + File.separator);
//                }
                URL url=file.toURL();
                if (DEBUG)
                    sb.append(" : "+ url);
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
//                    if (DEBUG)
//                        sb.append(" [pak]="+ file.getCanonicalPath());
//                    URL url = new URL("file", null,
//                            file.getCanonicalPath());
                    URL url=file.toURL();
                    if (DEBUG)
                        sb.append(" pk="+ url);

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
        if( listener!=null ) listener.moduleStop(module);
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
