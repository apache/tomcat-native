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
package org.apache.tomcat.util.http.mapper;

import javax.naming.NamingException;
import javax.naming.directory.DirContext;

import org.apache.tomcat.util.buf.CharChunk;
import org.apache.tomcat.util.buf.MessageBytes;

/**
 * Mapper, which implements the servlet API mapping rules (which are derived
 * from the HTTP rules).
 *
 * @author Remy Maucherat
 */
public final class Mapper {


    private static org.apache.commons.logging.Log logger = 
        org.apache.commons.logging.LogFactory.getLog(Mapper.class);
    // ----------------------------------------------------- Instance Variables


    /**
     * Array containing the virtual hosts definitions.
     */
    protected Host[] hosts = new Host[0];


    /**
     * Default host name.
     */
    protected String defaultHostName = null;


    /**
     * Flag indicating if physical welcome files are to be processed by this
     * mapper. Some default servlets may not want this, so this may be disabled
     * here.
     */
    protected boolean processWelcomeResources = true;

    /**
     * Flag indicating that we should redirect to a directory when the URL
     * doesn't end in a '/'.  It overrided the Authenticator, so the redirect 
     * done before authentication.
     */
    protected boolean redirectDirectories = false;

    // --------------------------------------------------------- Public Methods


    /**
     * Get default host.
     * 
     * @return Default host name
     */
    public String getDefaultHostName() {
        return defaultHostName;
    }


    /**
     * Set default host.
     * 
     * @param name Default host name
     */
    public void setDefaultHostName(String defaultHostName) {
        this.defaultHostName = defaultHostName;
    }


    /**
     * Get flag value indicating whether or not welcome files are processed by 
     * the mapper.
     * 
     * @return Value of the processWelcomeResources flag
     */
    public boolean getProcessWelcomeResources() {
        return processWelcomeResources;
    }


    /**
     * Set the process flag indicating if welcome resources should be processed
     * by the mapper. The default is true.
     * 
     * @param processWelcomeResources True if welcome files should be 
     *        fully processed by this mapper.
     */
    public void setProcessWelcomeResources(boolean processWelcomeResources) {
        this.processWelcomeResources = processWelcomeResources;
    }

    /**
     * Get the flag that indicates if we redirect to directories that don't
     * end a '/'.
     */
    public boolean getRedirectDirectories() {
        return redirectDirectories;
    }

    /**
     * Set the flag that indicates if we redirect to directories that don't
     * end in a '/'.
     */
    public void setRedirectDirectories(boolean rd) {
        redirectDirectories = rd;
    }

    /**
     * Add a new host to the mapper.
     * 
     * @param name Virtual host name
     * @param host Host object
     */
    public synchronized void addHost(String name, Object host) {
        Host[] newHosts = new Host[hosts.length + 1];
        Host newHost = new Host();
        newHost.name = name;
        newHost.object = host;
        if (insertMap(hosts, newHosts, newHost)) {
            hosts = newHosts;
        }
    }


    /**
     * Remove a host from the mapper.
     * 
     * @param name Virtual host name
     */
    public synchronized void removeHost(String name) {
        Host[] newHosts = new Host[hosts.length - 1];
        if (removeMap(hosts, newHosts, name)) {
            hosts = newHosts;
        }
    }


    /**
     * Add a new Context to an existing Host.
     * 
     * @param hostName Virtual host name this context belongs to
     * @param path Context path
     * @param context Context object
     * @param welcomeResources Welcome files defined for this context
     * @param resources Static resources of the context
     */
    public void addContext
        (String hostName, String path, Object context, 
         String[] welcomeResources, javax.naming.Context resources) {

        Host[] hosts = this.hosts;
        int pos = find(hosts, hostName);
        if (pos < 0) {
            return;
        }
        Host host = hosts[pos];
        if (host.name.equals(hostName)) {
            Context[] contexts = host.contexts;
            synchronized (host) {
                Context[] newContexts = new Context[contexts.length + 1];
                Context newContext = new Context();
                newContext.name = path;
                newContext.object = context;
                newContext.welcomeResources = welcomeResources;
                newContext.resources = resources;
                if (insertMap(contexts, newContexts, newContext)) {
                    host.contexts = newContexts;
                }
            }
        }

    }


    /**
     * Remove a context from an existing host.
     * 
     * @param hostName Virtual host name this context belongs to
     * @param path Context path
     */
    public void removeContext(String hostName, String path) {
        Host[] hosts = this.hosts;
        int pos = find(hosts, hostName);
        if (pos < 0) {
            return;
        }
        Host host = hosts[pos];
        if (host.name.equals(hostName)) {
            Context[] contexts = host.contexts;
            synchronized (host) {
                Context[] newContexts = new Context[contexts.length - 1];
                if (removeMap(contexts, newContexts, path)) {
                    host.contexts = newContexts;
                }
            }
        }
    }


    /**
     * Add a new Wrapper to an existing Context.
     * 
     * @param hostName Virtual host name this wrapper belongs to
     * @param contextPath Context path this wrapper belongs to
     * @param path Wrapper mapping
     * @param wrapper Wrapper object
     */
    public void addWrapper
        (String hostName, String contextPath, String path, Object wrapper) {
        Host[] hosts = this.hosts;
        int pos = find(hosts, hostName);
        if (pos < 0) {
            return;
        }
        Host host = hosts[pos];
        if (host.name.equals(hostName)) {
            Context[] contexts = host.contexts;
            int pos2 = find(contexts, contextPath);
            if (pos2 < 0) {
                return;
            }
            Context context = contexts[pos2];
            if (context.name.equals(contextPath)) {
                synchronized (context) {
                    Wrapper newWrapper = new Wrapper();
                    newWrapper.object = wrapper;
                    if (path.endsWith("/*")) {
                        // Wildcard wrapper
                        newWrapper.name = path.substring(0, path.length() - 2);
                        Wrapper[] oldWrappers = context.wildcardWrappers;
                        Wrapper[] newWrappers = 
                            new Wrapper[oldWrappers.length + 1];
                        if (insertMap(oldWrappers, newWrappers, newWrapper)) {
                            context.wildcardWrappers = newWrappers;
                        }
                    } else if (path.startsWith("*.")) {
                        // Extension wrapper
                        newWrapper.name = path.substring(2);
                        Wrapper[] oldWrappers = context.extensionWrappers;
                        Wrapper[] newWrappers = 
                            new Wrapper[oldWrappers.length + 1];
                        if (insertMap(oldWrappers, newWrappers, newWrapper)) {
                            context.extensionWrappers = newWrappers;
                        }
                    } else if (path.equals("/")) {
                        // Default wrapper
                        newWrapper.name = "";
                        context.defaultWrapper = newWrapper;
                    } else {
                        // Exact wrapper
                        newWrapper.name = path;
                        Wrapper[] oldWrappers = context.exactWrappers;
                        Wrapper[] newWrappers = 
                            new Wrapper[oldWrappers.length + 1];
                        if (insertMap(oldWrappers, newWrappers, newWrapper)) {
                            context.exactWrappers = newWrappers;
                        }
                    }
                }
            }
        }
    }


    /**
     * Remove a wrapper from an existing context.
     * 
     * @param hostName Virtual host name this wrapper belongs to
     * @param contextPath Context path this wrapper belongs to
     * @param path Wrapper mapping
     */
    public void removeWrapper
        (String hostName, String contextPath, String path) {
        Host[] hosts = this.hosts;
        int pos = find(hosts, hostName);
        if (pos < 0) {
            return;
        }
        Host host = hosts[pos];
        if (host.name.equals(hostName)) {
            Context[] contexts = host.contexts;
            int pos2 = find(contexts, contextPath);
            if (pos2 < 0) {
                return;
            }
            Context context = contexts[pos2];
            if (context.name.equals(contextPath)) {
                synchronized (context) {
                    if (path.endsWith("/*")) {
                        // Wildcard wrapper
                        String name = path.substring(0, path.length() - 2);
                        Wrapper[] oldWrappers = context.wildcardWrappers;
                        Wrapper[] newWrappers = 
                            new Wrapper[oldWrappers.length - 1];
                        if (removeMap(oldWrappers, newWrappers, name)) {
                            context.wildcardWrappers = newWrappers;
                        }
                    } else if (path.startsWith("*.")) {
                        // Extension wrapper
                        String name = path.substring(2);
                        Wrapper[] oldWrappers = context.extensionWrappers;
                        Wrapper[] newWrappers = 
                            new Wrapper[oldWrappers.length - 1];
                        if (removeMap(oldWrappers, newWrappers, name)) {
                            context.extensionWrappers = newWrappers;
                        }
                    } else if (path.equals("/")) {
                        // Default wrapper
                        context.defaultWrapper = null;
                    } else {
                        // Exact wrapper
                        String name = path;
                        Wrapper[] oldWrappers = context.exactWrappers;
                        Wrapper[] newWrappers = 
                            new Wrapper[oldWrappers.length - 1];
                        if (removeMap(oldWrappers, newWrappers, name)) {
                            context.exactWrappers = newWrappers;
                        }
                    }
                }
            }
        }
    }


    /**
     * Map the specified host name and URI, mutating the given mapping data.
     * 
     * @param host Virtual host name
     * @param uri URI
     * @param mappingData This structure will contain the result of the mapping
     *                    operation
     */
    public void map(MessageBytes host, MessageBytes uri, 
                    MappingData mappingData)
        throws Exception {

        if (host != null) {
            host.toChars();
        }
        uri.toChars();
        internalMap(host.getCharChunk(), uri.getCharChunk(), mappingData);

    }


    // -------------------------------------------------------- Private Methods


    /**
     * Map the specified URI.
     */
    private final void internalMap(CharChunk host, CharChunk uri, 
                                   MappingData mappingData)
        throws Exception {

        uri.setLimit(-1);

        Context[] contexts = null;
        Context context = null;

        // Virtual host mapping
        if (mappingData.host == null) {
            Host[] hosts = this.hosts;
            int pos = find(hosts, host);
            if ((pos != -1) && (host.equals(hosts[pos].name))) {
                mappingData.host = hosts[pos].object;
                contexts = hosts[pos].contexts;
            } else {
                pos = find(hosts, defaultHostName);
                if ((pos != -1) && (defaultHostName.equals(hosts[pos].name))) {
                    mappingData.host = hosts[pos].object;
                    contexts = hosts[pos].contexts;
                } else {
                    return;
                }
            }
        }

        // Context mapping
        if (mappingData.context == null) {
            int pos = find(contexts, uri);
            if (pos == -1) {
                return;
            }
            boolean found = false;
            while (uri.startsWith(contexts[pos].name)) {
                int length = contexts[pos].name.length();
                if (uri.getLength() == length) {
                    found = true;
                    break;
                } else if (uri.startsWithIgnoreCase("/", length)) {
                    found = true;
                    break;
                }
                pos--;
                if (pos < 0) {
                    break;
                }
            }
            if (!found) {
                if (contexts[0].name.equals("")) {
                    context = contexts[0];
                }
            } else {
                context = contexts[pos];
            }
            if (context != null) {
                mappingData.context = context.object;
                mappingData.contextPath.setString(context.name);
            }
        }

        // Wrapper mapping
        if ((context != null) && (mappingData.wrapper == null)) {
            internalMapWrapper(context, uri, mappingData);
        }

    }


    /**
     * Wrapper mapping.
     */
    private final void internalMapWrapper(Context context, CharChunk path, 
                                          MappingData mappingData)
        throws Exception {

        int pathOffset = path.getOffset();
        int pathEnd = path.getEnd();
        int servletPath = pathOffset;

        int length = context.name.length();
        if (length != (pathEnd - pathOffset)) {
            servletPath = pathOffset + length;
        } else {
            // The path is empty, redirect to "/"
            path.append('/');
            mappingData.redirectPath.setChars
                (path.getBuffer(), path.getOffset(), path.getEnd());
            path.setEnd(path.getEnd() - 1);
            return;
        }

        path.setOffset(servletPath);

        // Rule 1 -- Exact Match
        Wrapper[] exactWrappers = context.exactWrappers;
        internalMapExactWrapper(exactWrappers, path, mappingData);

        // Rule 2 -- Prefix Match
        Wrapper[] wildcardWrappers = context.wildcardWrappers;
        if (mappingData.wrapper == null) {
            internalMapWildcardWrapper(wildcardWrappers, path, mappingData);
        }

        // Rule 3 -- Extension Match
        Wrapper[] extensionWrappers = context.extensionWrappers;
        if (mappingData.wrapper == null) {
            internalMapExtensionWrapper(extensionWrappers, path, mappingData);
        }
        
        // Rule 4 -- Welcome resources processing for servlets
        if (mappingData.wrapper == null) {
            char[] buf = path.getBuffer();
            if (buf[pathEnd - 1] == '/') {
                for (int i = 0; (i < context.welcomeResources.length)
                         && (mappingData.wrapper == null); i++) {
                    path.setOffset(pathOffset);
                    path.setEnd(pathEnd);
                    path.append(context.welcomeResources[i], 0, 
                                context.welcomeResources[i].length());
                    path.setOffset(servletPath);

                    // Rule 4a -- Welcome resources processing for exact macth
                    internalMapExactWrapper(exactWrappers, path, mappingData);

                    // Rule 4b -- Welcome resources processing for prefix match
                    if (mappingData.wrapper == null) {
                        internalMapWildcardWrapper
                            (wildcardWrappers, path, mappingData);
                    }

                }
                path.setOffset(servletPath);
                path.setEnd(pathEnd);
            }
        }

        // Rule 6 -- Welcome resources processing for physical folder
        if (processWelcomeResources && mappingData.wrapper == null) {
            char[] buf = path.getBuffer();
            if( context.resources != null  && buf[pathEnd - 1] == '/') {
                for (int i = 0; (i < context.welcomeResources.length)
                         && (mappingData.wrapper == null); i++) {
                    path.setOffset(pathOffset);
                    path.setEnd(pathEnd);
                    path.append(context.welcomeResources[i], 0, 
                                context.welcomeResources[i].length());
                    path.setOffset(servletPath);
                    Object file = null;
                    try {
                        file = context.resources.lookup(path.toString());
                    } catch(NamingException nex) {
                        // Swallow not found, since this is normal
                    }
                    if (file != null && !(file instanceof DirContext) ) {
                        if(logger.isTraceEnabled())
                            logger.trace("Found welcome-file: " + path);
                        internalMapExtensionWrapper(extensionWrappers,
                                                    path, mappingData);
                        if(mappingData.wrapper == null) {
                            mappingData.wrapper = context.defaultWrapper.object;
                            mappingData.requestPath.setChars
                                (path.getBuffer(), path.getStart(), path.getLength());
                            mappingData.wrapperPath.setChars
                                (path.getBuffer(), path.getStart(), path.getLength());
                        }
                    }
                }
                path.setOffset(servletPath);
                path.setEnd(pathEnd);
            }
        }

        // Rule 7 -- Default servlet
        if (mappingData.wrapper == null) {
            if (context.defaultWrapper != null) {
                mappingData.wrapper = context.defaultWrapper.object;
                mappingData.requestPath.setChars
                    (path.getBuffer(), path.getStart(), path.getLength());
                mappingData.wrapperPath.setChars
                    (path.getBuffer(), path.getStart(), path.getLength());
            }
            if( redirectDirectories ) {
                char[] buf = path.getBuffer();
                if( context.resources != null && buf[pathEnd -1 ] != '/') {
                    Object file = null;
                    try {
                        file = context.resources.lookup(path.toString());
                    } catch(NamingException nex) {
                        // Swallow, since someone else handles the 404
                    }
                    if(file != null && file instanceof DirContext ) {
                        CharChunk dirPath = path.getClone();
                        dirPath.append('/');
                        mappingData.redirectPath.setChars
                            (dirPath.getBuffer(), dirPath.getStart(), 
                             dirPath.getLength());
                    }
                }
            }
        }

        path.setOffset(pathOffset);
        path.setEnd(pathEnd);

    }


    /**
     * Exact mapping.
     */
    private final void internalMapExactWrapper
        (Wrapper[] wrappers, CharChunk path, MappingData mappingData) {
        int pos = find(wrappers, path);
        if ((pos != -1) && (path.equals(wrappers[pos].name))) {
            mappingData.requestPath.setString(wrappers[pos].name);
            mappingData.wrapperPath.setString(wrappers[pos].name);
            mappingData.wrapper = wrappers[pos].object;
        }
    }


    /**
     * Wildcard mapping.
     */
    private final void internalMapWildcardWrapper
        (Wrapper[] wrappers, CharChunk path, MappingData mappingData) {
        int length = -1;
        int pos = find(wrappers, path);
        if (pos != -1) {
            boolean found = false;
            while (path.startsWith(wrappers[pos].name)) {
                length = wrappers[pos].name.length();
                if (path.getLength() == length) {
                    found = true;
                    break;
                } else if (path.startsWithIgnoreCase("/", length)) {
                    found = true;
                    break;
                }
                pos--;
                if (pos < 0) {
                    break;
                }
            }
            if (found) {
                mappingData.wrapperPath.setString(wrappers[pos].name);
                if (path.getLength() > length) {
                    mappingData.pathInfo.setChars
                        (path.getBuffer(), 
                         path.getOffset() + length, 
                         path.getLength() - length);
                }
                mappingData.requestPath.setChars
                    (path.getBuffer(), path.getOffset(), path.getLength());
                mappingData.wrapper = wrappers[pos].object;
            }
        }
    }

    /**
     * Extension mappings.
     */
    private final void internalMapExtensionWrapper
        (Wrapper[] wrappers, CharChunk path, MappingData mappingData) {
        char[] buf = path.getBuffer();
        int pathEnd = path.getEnd();
        int servletPath = path.getOffset();
        int slash = -1;
        for (int i = pathEnd - 1; i >= servletPath; i--) {
            if (buf[i] == '/') {
                slash = i;
                break;
            }
        }
        if (slash >= 0) {
            int period = -1;
            for (int i = pathEnd - 1; i > slash; i--) {
                if (buf[i] == '.') {
                    period = i;
                    break;
                }
            }
            if (period >= 0) {
                path.setOffset(period + 1);
                path.setEnd(pathEnd);
                int pos = find(wrappers, path);
                if ((pos != -1) 
                    && (path.equals(wrappers[pos].name))) {
                    mappingData.wrapperPath.setChars
                        (buf, servletPath, pathEnd - servletPath);
                    mappingData.requestPath.setChars
                        (buf, servletPath, pathEnd - servletPath);
                    mappingData.wrapper = wrappers[pos].object;
                }
                path.setOffset(servletPath);
                path.setEnd(pathEnd);
            }
        }
    }


    /**
     * Find a map elemnt given its name in a sorted array of map elements.
     * This will return the index for the closest inferior or equal item in the
     * given array.
     */
    private static final int find(MapElement[] map, CharChunk name) {
        return find(map, name, name.getStart(), name.getEnd());
    }


    /**
     * Find a map elemnt given its name in a sorted array of map elements.
     * This will return the index for the closest inferior or equal item in the
     * given array.
     */
    private static final int find(MapElement[] map, CharChunk name, 
                                  int start, int end) {

        int a = 0;
        int b = map.length - 1;

        // Special cases: -1 and 0
        if (b == -1) {
            return -1;
        }
        if (compare(name, start, end, map[0].name) < 0) {
            return -1;
        }
        if (b == 0) {
            return 0;
        }

        int i = 0;
        while (true) {
            i = (b + a) / 2;
            int result = compare(name, start, end, map[i].name);
            if (result == 1) {
                a = i;
            } else if (result == 0) {
                return i;
            } else {
                b = i;
            }
            if ((b - a) == 1) {
                int result2 = compare(name, start, end, map[b].name);
                if (result2 < 0) {
                    return a;
                } else {
                    return b;
                }
            }
        }

    }


    /**
     * Find a map elemnt given its name in a sorted array of map elements.
     * This will return the index for the closest inferior or equal item in the
     * given array.
     */
    private static final int find(MapElement[] map, String name) {
        
        int a = 0;
        int b = map.length - 1;

        // Special cases: -1 and 0
        if (b == -1) {
            return -1;
        }
        if (name.compareTo(map[0].name) < 0) {
            return -1;
        }
        if (b == 0) {
            return 0;
        }

        int i = 0;
        while (true) {
            i = (b + a) / 2;
            int result = name.compareTo(map[i].name);
            if (result > 0) {
                a = i;
            } else if (result == 0) {
                return i;
            } else {
                b = i;
            }
            if ((b - a) == 1) {
                int result2 = name.compareTo(map[b].name);
                if (result2 < 0) {
                    return a;
                } else {
                    return b;
                }
            }
        }

    }


    /**
     * Compare given char chunk with String.
     * Return -1, 0 or +1 if inferior, equal, or superior to the String.
     */
    private static final int compare(CharChunk name, int start, int end, 
                                     String compareTo) {
        int result = 0;
        char[] c = name.getBuffer();
        int len = compareTo.length();
        if ((end - start) < len) {
            len = end - start;
        }
        for (int i = 0; (i < len) && (result == 0); i++) {
            if (c[i + start] > compareTo.charAt(i)) {
                result = 1;
            } else if (c[i + start] < compareTo.charAt(i)) {
                result = -1;
            }
        }
        if (result == 0) {
            if (compareTo.length() > (end - start)) {
                result = -1;
            } else if (compareTo.length() < (end - start)) {
                result = 1;
            }
        }
        return result;
    }


    /**
     * Insert into the right place in a sorted MapElement array, and prevent
     * duplicates.
     */
    private static final boolean insertMap
        (MapElement[] oldMap, MapElement[] newMap, MapElement newElement) {
        int pos = find(oldMap, newElement.name);
        if ((pos != -1) && (newElement.name.equals(oldMap[pos].name))) {
            return false;
        }
        System.arraycopy(oldMap, 0, newMap, 0, pos + 1);
        newMap[pos + 1] = newElement;
        System.arraycopy
            (oldMap, pos + 1, newMap, pos + 2, oldMap.length - pos - 1);
        return true;
    }


    /**
     * Insert into the right place in a sorted MapElement array.
     */
    private static final boolean removeMap
        (MapElement[] oldMap, MapElement[] newMap, String name) {
        int pos = find(oldMap, name);
        if ((pos != -1) && (name.equals(oldMap[pos].name))) {
            System.arraycopy(oldMap, 0, newMap, 0, pos);
            System.arraycopy(oldMap, pos + 1, newMap, pos, 
                             oldMap.length - pos - 1);
            return true;
        }
        return false;
    }


    // ------------------------------------------------- MapElement Inner Class


    protected abstract class MapElement {

        public String name = null;
        public Object object = null;

    }


    // ------------------------------------------------------- Host Inner Class


    protected final class Host
        extends MapElement {

        public Context[] contexts = new Context[0];

    }


    // ---------------------------------------------------- Context Inner Class


    protected final class Context
        extends MapElement {

        public String path = null;
        public String[] welcomeResources = new String[0];
        public javax.naming.Context resources = null;
        public Wrapper defaultWrapper = null;
        public Wrapper[] exactWrappers = new Wrapper[0];
        public Wrapper[] wildcardWrappers = new Wrapper[0];
        public Wrapper[] extensionWrappers = new Wrapper[0];

    }


    // ---------------------------------------------------- Wrapper Inner Class


    protected class Wrapper
        extends MapElement {

        public String path = null;

    }


    // -------------------------------------------------------- Testing Methods

    public static void main(String args[]) {

        try {

        Mapper mapper = new Mapper();
        System.out.println("Start");
        
        mapper.addHost("sjbjdvwsbvhrb", "blah1");
        mapper.addHost("sjbjdvwsbvhr/", "blah1");
        mapper.addHost("wekhfewuifweuibf", "blah2");
        mapper.addHost("ylwrehirkuewh", "blah3");
        mapper.addHost("iohgeoihro", "blah4");
        mapper.addHost("fwehoihoihwfeo", "blah5");
        mapper.addHost("owefojiwefoi", "blah6");
        mapper.addHost("iowejoiejfoiew", "blah7");
        mapper.addHost("iowejoiejfoiew", "blah17");
        mapper.addHost("ohewoihfewoih", "blah8");
        mapper.addHost("fewohfoweoih", "blah9");
        mapper.addHost("ttthtiuhwoih", "blah10");
        mapper.addHost("lkwefjwojweffewoih", "blah11");
        mapper.addHost("zzzuyopjvewpovewjhfewoih", "blah12");
        mapper.addHost("xxxxgqwiwoih", "blah13");
        mapper.addHost("qwigqwiwoih", "blah14");

        System.out.println("Map:");
        for (int i = 0; i < mapper.hosts.length; i++) {
            System.out.println(mapper.hosts[i].name);
        }

        mapper.setDefaultHostName("ylwrehirkuewh");

        String[] welcomes = new String[2];
        welcomes[0] = "boo/baba";
        welcomes[1] = "bobou";

        mapper.addContext("iowejoiejfoiew", "", "context0", new String[0], null);
        mapper.addContext("iowejoiejfoiew", "/foo", "context1", new String[0], null);
        mapper.addContext("iowejoiejfoiew", "/foo/bar", "context2", welcomes, null);
        mapper.addContext("iowejoiejfoiew", "/foo/bar/bla", "context3", new String[0], null);

        mapper.addWrapper("iowejoiejfoiew", "/foo/bar", "/fo/*", "wrapper0");
        mapper.addWrapper("iowejoiejfoiew", "/foo/bar", "/", "wrapper1");
        mapper.addWrapper("iowejoiejfoiew", "/foo/bar", "/blh", "wrapper2");
        mapper.addWrapper("iowejoiejfoiew", "/foo/bar", "*.jsp", "wrapper3");
        mapper.addWrapper("iowejoiejfoiew", "/foo/bar", "/blah/bou/*", "wrapper4");
        mapper.addWrapper("iowejoiejfoiew", "/foo/bar", "/blah/bobou/*", "wrapper5");
        mapper.addWrapper("iowejoiejfoiew", "/foo/bar", "*.htm", "wrapper6");

        MappingData mappingData = new MappingData();
        MessageBytes host = MessageBytes.newInstance();
        host.setString("iowejoiejfoiew");
        MessageBytes uri = MessageBytes.newInstance();
        uri.setString("/foo/bar/blah/bobou/foo");
        uri.toChars();
        uri.getCharChunk().setLimit(-1);

        mapper.map(host, uri, mappingData);
        System.out.println("MD Host:" + mappingData.host);
        System.out.println("MD Context:" + mappingData.context);
        System.out.println("MD Wrapper:" + mappingData.wrapper);

        System.out.println("contextPath:" + mappingData.contextPath);
        System.out.println("wrapperPath:" + mappingData.wrapperPath);
        System.out.println("pathInfo:" + mappingData.pathInfo);
        System.out.println("redirectPath:" + mappingData.redirectPath);

        mappingData.recycle();
        mapper.map(host, uri, mappingData);
        System.out.println("MD Host:" + mappingData.host);
        System.out.println("MD Context:" + mappingData.context);
        System.out.println("MD Wrapper:" + mappingData.wrapper);

        System.out.println("contextPath:" + mappingData.contextPath);
        System.out.println("wrapperPath:" + mappingData.wrapperPath);
        System.out.println("pathInfo:" + mappingData.pathInfo);
        System.out.println("redirectPath:" + mappingData.redirectPath);

        for (int i = 0; i < 1000000; i++) {
            mappingData.recycle();
            mapper.map(host, uri, mappingData);
        }

        long time = System.currentTimeMillis();
        for (int i = 0; i < 1000000; i++) {
            mappingData.recycle();
            mapper.map(host, uri, mappingData);
        }
        System.out.println("Elapsed:" + (System.currentTimeMillis() - time));
        
        System.out.println("MD Host:" + mappingData.host);
        System.out.println("MD Context:" + mappingData.context);
        System.out.println("MD Wrapper:" + mappingData.wrapper);

        System.out.println("contextPath:" + mappingData.contextPath);
        System.out.println("wrapperPath:" + mappingData.wrapperPath);
        System.out.println("requestPath:" + mappingData.requestPath);
        System.out.println("pathInfo:" + mappingData.pathInfo);
        System.out.println("redirectPath:" + mappingData.redirectPath);

        } catch (Exception e) {
            e.printStackTrace();
        }
        
    }


}
