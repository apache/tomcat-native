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

import java.util.logging.*;
import java.util.*;
import java.io.IOException;
import java.io.InputStream;
import java.net.URLClassLoader;
import java.security.AccessController;
import java.security.PrivilegedAction;


/**
 * Per classloader LogManager implementation.
 */
public final class ClassLoaderLogManager extends LogManager {

    private static void doSetParentLogger(final Logger logger,
            final Logger parent) {
        if (System.getSecurityManager() != null) {
            AccessController.doPrivileged(new PrivilegedAction() {
                public Object run() {
                    logger.setParent(parent);
                    return null;
                }
            });
        } else {
            logger.setParent(parent);
        }
    }

    private final Map classLoaderLoggers = new WeakHashMap();

    private Logger rootLogger;

    private Logger globalLogger;
    
    private ThreadLocal prefix = new ThreadLocal();

    public synchronized boolean addLogger(final Logger logger) {
        final String loggerName = logger.getName();
        if ("".equals(loggerName)) {
            final boolean unset = rootLogger == null;
            if (unset) {
                rootLogger = logger;
                if (globalLogger != null) {
                    doSetParentLogger(globalLogger, rootLogger);
                }
            }
            return unset;
        }
        if ("global".equals(loggerName)) {
            final boolean unset = globalLogger == null;
            if (unset) {
                globalLogger = logger;
                if (rootLogger != null) {
                    doSetParentLogger(globalLogger, rootLogger);
                }
            }
            return unset;
        }
        final ClassLoader classLoader = Thread.currentThread()
                .getContextClassLoader();
        final ClassLoaderLogInfo info = getClassLoaderInfo(classLoader);
        if (info.loggers.containsKey(loggerName)) {
            return false;
        }

        info.loggers.put(loggerName, logger);

        // apply initial level for new logger
        final String levelString = getProperty(loggerName + ".level");
        final Level level;
        if (levelString != null) {
            Level parsedLevel = null;
            try {
                parsedLevel = Level.parse(levelString.trim());
            } catch (IllegalArgumentException e) {
                // leave level set to null
            }
            level = parsedLevel;
        } else {
            level = null;
        }
        if (level != null) {
            if (System.getSecurityManager() == null) {
                logger.setLevel(level);
            } else {
                AccessController.doPrivileged(new PrivilegedAction() {
                    public Object run() {
                        logger.setLevel(level);
                        return null;
                    }
                });
            }
        }

        // if any parent loggers have levels definied, make sure they are
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

        // find node
        final LogNode node = info.rootNode.findNode(loggerName);
        node.logger = logger;

        // set parent logger
        final Logger parentLogger = node.findParentLogger();
        if (parentLogger != null) {
            doSetParentLogger(logger, parentLogger);
        }

        // tell children we are their new parent
        node.setParentLogger(logger);

        // Add associated handlers, if any are defined using the .handlers property.
        // In this case, handlers of the parent logger(s) will not be used
        final String handlers = getProperty(loggerName + ".handlers");
        if (handlers != null) {
            logger.setUseParentHandlers(false);
            StringTokenizer tok = new StringTokenizer(handlers);
            while (tok.hasMoreTokens()) {
                String handlerName = (tok.nextToken());
                Handler handler = (Handler) info.handlers.get(handlerName);
                if (handler != null) {
                    logger.addHandler(handler);
                }
            }
        }
        
        return true;
    }

    public synchronized Logger getLogger(final String name) {
        if (rootLogger == null && globalLogger == null) {
            // this ends up being called during initialization, we don't
            // want do anything unless the root logger has been set up.
            return null;
        }
        if (name == null || name.length() == 0) {
            return rootLogger;
        }
        if ("global".equals(name)) {
            return globalLogger;
        }
        final ClassLoader classLoader = Thread.currentThread()
                .getContextClassLoader();
        final Map loggers = getClassLoaderInfo(classLoader).loggers;
        /*
         * Debug: display handler list
        Logger logger = (Logger) loggers.get(name);
        System.out.println(name);
        if (logger != null) {
            Handler[] handlers =  logger.getHandlers();
            for (int i = 0; i < handlers.length; i++) {
                System.out.println("H" + i + ":" + handlers[i]);
            }
        }
        */
        return (Logger) loggers.get(name);
    }
    
    public synchronized Enumeration getLoggerNames() {
        if (rootLogger == null && globalLogger == null) {
            // this ends up being called during initialization, we don't
            // want do anything unless the root logger has been set up.
            return Collections.enumeration(Collections.EMPTY_LIST);
        }
        final ClassLoader classLoader = Thread.currentThread()
                .getContextClassLoader();
        final Map loggers = getClassLoaderInfo(classLoader).loggers;
        return Collections.enumeration(loggers.keySet());
    }

    
    /**
     * Get the value of the specified property in the current classloader
     * context.
     */    
    public String getProperty(String name) {
        String prefix = (String) this.prefix.get();
        if (prefix != null) {
            name = prefix + name;
        }
        final ClassLoader classLoader = Thread.currentThread()
                .getContextClassLoader();
        String result = 
            getClassLoaderInfo(classLoader).props.getProperty(name);
        if (result == null) {
            // FIXME: Look in parent classloader ? Probably not.
            result = super.getProperty(name);
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
    
    private ClassLoaderLogInfo getClassLoaderInfo(final ClassLoader classLoader) {
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
                    localRootLogger = rootLogger;
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
                    StringTokenizer tok = new StringTokenizer(handlers);
                    while (tok.hasMoreTokens()) {
                        String handlerName = (tok.nextToken());
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
                            this.prefix.set(null);
                            String handlerLevel = 
                                info.props.getProperty(handlerName + ".level");
                            if (handlerLevel != null) {
                                handler.setLevel(Level.parse(handlerLevel.trim()));
                            }
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
                        StringTokenizer tok2 = new StringTokenizer(rootHandlers);
                        while (tok2.hasMoreTokens()) {
                            String handlerName = (tok2.nextToken());
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

    private static final class LogNode {
        Logger logger;

        private final Map children = new HashMap();

        private final LogNode parent;

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

    private static final class ClassLoaderLogInfo {
        final LogNode rootNode;
        final Map loggers = new HashMap();
        final Map handlers = new HashMap();
        final Properties props = new Properties();

        ClassLoaderLogInfo(final LogNode rootNode) {
            this.rootNode = rootNode;
        }

    }
    
    /**
     * This is needed to instantiate the root of each per classloader hierarchy.
     */
    private class RootLogger extends Logger {
        public RootLogger() {
            super("", null);
        }
    }

}
