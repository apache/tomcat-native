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
import java.io.FileInputStream;
import java.io.InputStream;
import java.net.URL;
import java.util.Enumeration;
import java.util.Properties;


/**
 * Load a properties file describing the modules and startup sequence.
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
 *  
 * Based on o.a.catalina.bootstrap.CatalinaProperties - utility class to read 
 * the bootstrap Catalina configuration.
 *
 * @author Remy Maucherat
 * @author Costin Manolache
 */
public class LoaderProperties {

    private static Properties properties = null;

    static {

        loadProperties();

    }

    private static boolean DEBUG= getProperty("debug.LoaderProperties")!=null;
    private static String propFile;

    // --------------------------------------------------------- Public Methods


    /**
     * Return specified property value.
     */
    public static String getProperty(String name) {
        return properties.getProperty(name);
    }


    /**
     * Return specified property value.
     */
    public static String getProperty(String name, String defaultValue) {
        return properties.getProperty(name, defaultValue);
    }


    // --------------------------------------------------------- Public Methods


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
    private static void loadProperties() {

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

        if (is == null) {
            try {
                File home = new File(getCatalinaBase());
                File conf = new File(home, "conf");
                // use conf if exists, or the base directory otherwise
                if( ! conf.exists() ) conf = new File(home, "../conf");
                if( ! conf.exists() ) conf=home;
                File properties = new File(conf, "loader.properties");
                is = new FileInputStream(properties);
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
                is = LoaderProperties.class.getResourceAsStream
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
                is = LoaderProperties.class.getResourceAsStream
                    ("/org/apache/tomcat/util/loader/loader.properties");
                if( is!=null && DEBUG ) {
                    log("Loaded from o/a/t/u/loader/loader.properties " );
                }
            } catch (Throwable t) {
                // Ignore
            }
        }
        
        properties = new Properties();
        
        if (is != null) {
            try {
                properties.load(is);
                is.close();
            } catch (Throwable t) {
                error = t;
            }
        }

        if ((is == null) || (error != null)) {
            // Do something
            log("Error: no properties found !!!");
        }

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


    /**
     * Get the value of the catalina.home environment variable.
     */
    static String getCatalinaHome() {
        return System.getProperty("catalina.home",
                                  System.getProperty("user.dir"));
    }
    
    
    /**
     * Get the value of the catalina.base environment variable.
     */
    static String getCatalinaBase() {
        return System.getProperty("catalina.base", getCatalinaHome());
    }

    
    /**
     * Set the <code>catalina.base</code> System property to the current
     * working directory if it has not been set.
     */
    static void setCatalinaBase() {

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


    private static void log(String s ) { 
        System.err.println("LoaderProperties: "+ s);
    }


    /**
     * @param props
     */
    public static void setPropertiesFile(String props) {
        propFile=props;
        loadProperties();
    }
}
