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

package org.apache.juli;

import java.io.IOException;
import java.io.InputStream;
import java.net.URLClassLoader;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Properties;
import java.util.StringTokenizer;
import java.util.WeakHashMap;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.LogManager;
import java.util.logging.Logger;


/**
 * Per classloader LogManager implementation.
 */
public class ClassLoaderLogManager extends LogManager {


    // -------------------------------------------------------------- Variables


    /**
     * Map containing the classloader information, keyed per classloader. A
     * weak hashmap is used to ensure no classloader reference is leaked from 
     * application redeployment.
     */
    protected final Map classLoaderLoggers = new WeakHashMap();

    
    /**
     * This prefix is used to allow using prefixes for the properties names
     * of handlers and their subcomponents.
     */
    protected ThreadLocal prefix = new ThreadLocal();

    
    // --------------------------------------------------------- Public Methods


    /**
     * Add the specified logger to the classloader local configuration.
     * 
     * @param logger The logger to be added
     */
    public synchronized boolean addLogger(final Logger logger) {

        final String loggerName = logger.getName();

        ClassLoader classLoader = 
            Thread.currentThread().getContextClassLoader();
        if (classLoader == null || ("global".equals(loggerName))) {
            return super.addLogger(logger);
        }
        ClassLoaderLogInfo info = getClassLoaderInfo(classLoader);
        if (info.loggers.containsKey(loggerName)) {
            return false;
        }

        info.loggers.put(loggerName, logger);

        // Apply initial level for new logger
        final String levelString = getProperty(loggerName + ".level");
        if (levelString != null) {
            try {
                AccessController.doPrivileged(new PrivilegedAction() {
                    public Object run() {
                        logger.setLevel(Level.parse(levelString.trim()));
                        return null;
                    }
                });
            } catch (IllegalArgumentException e) {
                // Leave level set to null
            }
        }

        // If any parent loggers have levels definied, make sure they are
        // instantiated
        int dotIndex = loggerName.lastIndexOf('.');
        while (dotIndex >= 0) {
            final String parentName = loggerName.substring(0, dotIndex);
            if (getProperty(parentName + ".level") != null) {
                Logger.getLogger(parentName);
                break;
            }
            dotIndex = loggerName.lastIndexOf('.', dotIndex - 1);
        }

        // Find associated node
        LogNode node = info.rootNode.findNode(loggerName);
        node.logger = logger;

        // Set parent logger
        Logger parentLogger = node.findParentLogger();
        if (parentLogger != null) {
            doSetParentLogger(logger, parentLogger);
        }

        // Tell children we are their new parent
        node.setParentLogger(logger);

        // Add associated handlers, if any are defined using the .handlers property.
        // In this case, handlers of the parent logger(s) will not be used
        String handlers = getProperty(loggerName + ".handlers");
        if (handlers != null) {
            logger.setUseParentHandlers(false);
            StringTokenizer tok = new StringTokenizer(handlers, ",");
            while (tok.hasMoreTokens()) {
                String handlerName = (tok.nextToken().trim());
                Handler handler = null;
                ClassLoader current = classLoader;
                while (current != null) {
                    info = (ClassLoaderLogInfo) classLoaderLoggers.get(current);
                    if (info != null) {
                        handler = (Handler) info.handlers.get(handlerName);
                        if (handler != null) {
                            break;
                        }
                    }
                    current = current.getParent();
                }
                if (handler != null) {
                    logger.addHandler(handler);
                }
            }
        }

        // Parse useParentHandlers to set if the logger should delegate to its parent.
        // Unlike java.util.logging, the default is to not delegate if a list of handlers
        // has been specified for the logger.
        String useParentHandlersString = getProperty(loggerName + ".useParentHandlers");
        if (Boolean.valueOf(useParentHandlersString).booleanValue()) {
            logger.setUseParentHandlers(true);
        }
        
        return true;
    }

    
    /**
     * Get the logger associated with the specified name inside 
     * the classloader local configuration. If this returns null,
     * and the call originated for Logger.getLogger, a new
     * logger with the specified name will be instantiated and
     * added using addLogger.
     * 
     * @param name The name of the logger to retrieve
     */
    public synchronized Logger getLogger(final String name) {
        final ClassLoader classLoader = Thread.currentThread()
                .getContextClassLoader();
        if (classLoader == null || ("global".equals(name))) {
            return super.getLogger(name);
        }
        final Map loggers = getClassLoaderInfo(classLoader).loggers;
        return (Logger) loggers.get(name);
    }
    
    
    /**
     * Get an enumeration of the logger names currently defined in the 
     * classloader local configuration.
     */
    public synchronized Enumeration getLoggerNames() {
        final ClassLoader classLoader = Thread.currentThread()
                .getContextClassLoader();
        if (classLoader == null) {
            return super.getLoggerNames();
        }
        final Map loggers = getClassLoaderInfo(classLoader).loggers;
        return Collections.enumeration(loggers.keySet());
    }

    
    /**
     * Get the value of the specified property in the classloader local
     * configuration.
     * 
     * @param name The property name
     */    
    public String getProperty(String name) {
        final ClassLoader classLoader = Thread.currentThread()
            .getContextClassLoader();
        if (classLoader == null) {
            return super.getProperty(name);
        }
        String prefix = (String) this.prefix.get();
        if (prefix != null) {
            name = prefix + name;
        }
        ClassLoaderLogInfo info = getClassLoaderInfo(classLoader);
        String result = info.props.getProperty(name);
        // If the property was not found, and the current classloader had no 
        // configuration (property list is empty), look for the parent classloader
        // properties.
        if ((result == null) && (info.props.isEmpty())) {
            ClassLoader current = classLoader.getParent();
            while (current != null) {
                info = (ClassLoaderLogInfo) classLoaderLoggers.get(current);
                if (info != null) {
                    result = info.props.getProperty(name);
                    if ((result != null) || (!info.props.isEmpty())) {
                        break;
                    }
                }
                current = current.getParent();
            }
            if (result == null) {
                result = super.getProperty(name);
            }
        }
        // Simple property replacement (mostly for folder names)
        if (result != null) {
            result = result.trim();
            if (result.startsWith("${")) {
                int pos = result.indexOf('}');
                if (pos != -1) {
                    String propName = result.substring(2, pos);
                    String replacement = System.getProperty(propName);
                    if (replacement != null) {
                        result = replacement + result.substring(pos + 1);
                    }
                }
            }
        }
        return result;
    }
    

    // ------------------------------------------------------ Protected Methods


    /**
     * Retrieve the configuration associated with the specified classloader. If
     * it does not exist, it will be created.
     * 
     * @param classLoader The classloader for which we will retrieve or build the 
     *                    configuration
     */
    protected ClassLoaderLogInfo getClassLoaderInfo(final ClassLoader classLoader) {
        
        ClassLoaderLogInfo info = (ClassLoaderLogInfo) classLoaderLoggers
                .get(classLoader);
        if (info == null) {
            InputStream is = null;
            // Special case for URL classloaders which are used in containers: 
            // only look in the local repositories to avoid redefining loggers 20 times
            if ((classLoader instanceof URLClassLoader) 
                    && (((URLClassLoader) classLoader).findResource("logging.properties") != null)) {
                is = classLoader.getResourceAsStream("logging.properties");
            }
            
            Logger localRootLogger = null;
            if (is != null) {
                localRootLogger = new RootLogger();
            } else {
                // Retrieve the root logger of the parent classloader instead
                if (classLoader.getParent() != null) {
                    localRootLogger = 
                        getClassLoaderInfo(classLoader.getParent()).rootNode.logger;
                } else {
                    // Retrieve the system root logger
                    localRootLogger = super.getLogger("");
                }
            }
            info = new ClassLoaderLogInfo(new LogNode(null, localRootLogger));
            info.loggers.put("", localRootLogger);
            classLoaderLoggers.put(classLoader, info);
            
            if (is != null) {
                try {
                    info.props.load(is);
                } catch (IOException e) {
                    // FIXME: Report this using the main logger ?
                    e.printStackTrace();
                } finally {
                    try {
                        is.close();
                    } catch (Throwable t) {}
                }
                
                // Create handlers for the root logger of this classloader
                String rootHandlers = info.props.getProperty(".handlers");
                String handlers = info.props.getProperty("handlers");
                if (handlers != null) {
                    StringTokenizer tok = new StringTokenizer(handlers, ",");
                    while (tok.hasMoreTokens()) {
                        String handlerName = (tok.nextToken().trim());
                        String handlerClassName = handlerName;
                        String prefix = "";
                        if (handlerClassName.length() <= 0) {
                            continue;
                        }
                        // Parse and remove a prefix (prefix start with a digit, such as 
                        // "10WebappFooHanlder.")
                        if (Character.isDigit(handlerClassName.charAt(0))) {
                            int pos = handlerClassName.indexOf('.');
                            if (pos >= 0) {
                                prefix = handlerClassName.substring(0, pos + 1);
                                handlerClassName = handlerClassName.substring(pos + 1);
                            }
                        }
                        try {
                            this.prefix.set(prefix);
                            Handler handler = 
                                (Handler) classLoader.loadClass(handlerClassName).newInstance();
                            // The specification strongly implies all configuration should be done 
                            // during the creation of the handler object.
                            // This includes setting level, filter, formatter and encoding.
                            this.prefix.set(null);
                            info.handlers.put(handlerName, handler);
                            if (rootHandlers == null) {
                                localRootLogger.addHandler(handler);
                            }
                        } catch (Exception e) {
                            // FIXME: Report this using the main logger ?
                            e.printStackTrace();
                        }
                    }
                    
                    // Add handlers to the root logger, if any are defined using the .handlers property.
                    if (rootHandlers != null) {
                        StringTokenizer tok2 = new StringTokenizer(rootHandlers, ",");
                        while (tok2.hasMoreTokens()) {
                            String handlerName = (tok2.nextToken().trim());
                            Handler handler = (Handler) info.handlers.get(handlerName);
                            if (handler != null) {
                                localRootLogger.addHandler(handler);
                            }
                        }
                    }
                    
                }
                
            }
        }
        return info;
    }

    
    /**
     * Set parent child relationship between the two specified loggers.
     * 
     * @param logger
     * @param parent
     */
    protected static void doSetParentLogger(final Logger logger,
            final Logger parent) {
        AccessController.doPrivileged(new PrivilegedAction() {
            public Object run() {
                logger.setParent(parent);
                return null;
            }
        });
    }

    
    // ---------------------------------------------------- LogNode Inner Class


    protected static final class LogNode {
        Logger logger;

        protected final Map children = new HashMap();

        protected final LogNode parent;

        LogNode(final LogNode parent, final Logger logger) {
            this.parent = parent;
            this.logger = logger;
        }

        LogNode(final LogNode parent) {
            this(parent, null);
        }

        LogNode findNode(String name) {
            assert name != null && name.length() > 0;
            LogNode currentNode = this;
            while (name != null) {
                final int dotIndex = name.indexOf('.');
                final String nextName;
                if (dotIndex < 0) {
                    nextName = name;
                    name = null;
                } else {
                    nextName = name.substring(0, dotIndex);
                    name = name.substring(dotIndex + 1);
                }
                LogNode childNode = (LogNode) currentNode.children
                        .get(nextName);
                if (childNode == null) {
                    childNode = new LogNode(currentNode);
                    currentNode.children.put(nextName, childNode);
                }
                currentNode = childNode;
            }
            return currentNode;
        }

        Logger findParentLogger() {
            Logger logger = null;
            LogNode node = parent;
            while (node != null && logger == null) {
                logger = node.logger;
                node = node.parent;
            }
            assert logger != null;
            return logger;
        }

        void setParentLogger(final Logger parent) {
            for (final Iterator iter = children.values().iterator(); iter
                    .hasNext();) {
                final LogNode childNode = (LogNode) iter.next();
                if (childNode.logger == null) {
                    childNode.setParentLogger(parent);
                } else {
                    doSetParentLogger(childNode.logger, parent);
                }
            }
        }

    }

    // -------------------------------------------- ClassLoaderInfo Inner Class


    protected static final class ClassLoaderLogInfo {
        final LogNode rootNode;
        final Map loggers = new HashMap();
        final Map handlers = new HashMap();
        final Properties props = new Properties();

        ClassLoaderLogInfo(final LogNode rootNode) {
            this.rootNode = rootNode;
        }

    }
    

    // ------------------------------------------------- RootLogger Inner Class


    /**
     * This class is needed to instantiate the root of each per classloader 
     * hierarchy.
     */
    protected class RootLogger extends Logger {
        public RootLogger() {
            super("", null);
        }
    }

    
}
