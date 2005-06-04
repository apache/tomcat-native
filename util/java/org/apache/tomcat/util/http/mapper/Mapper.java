/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

package org.apache.tomcat.util.http.mapper;

import java.util.List;
import java.util.ArrayList;

import javax.naming.directory.DirContext;
import javax.naming.NamingException;

import org.apache.tomcat.util.buf.Ascii;
import org.apache.tomcat.util.buf.CharChunk;
import org.apache.tomcat.util.buf.MessageBytes;

/**
 * Mapper, which implements the servlet API mapping rules (which are derived
 * from the HTTP rules).
 *
 * @author Remy Maucherat
 * @author George Sexton
 * @author Peter Rossbach
 */
public final class Mapper {


    private static org.apache.commons.logging.Log logger =
    org.apache.commons.logging.LogFactory.getLog(Mapper.class);
    // ----------------------------------------------------- Instance Variables


    HostMap hostMap=new HostMap();

    /**
     * Default number of alias matches for each host.
     */
    private int defaultAliasMatches=0;


    /**
     * Default host name.
     */
    protected String defaultHostName = null;

    /**
     * Default host, already looked up cached to 
     * eliminate lookup.
     */
    protected Host defaultHost=null;

    /**
     * Context associated with this wrapper, used for wrapper mapping.
     */
    protected Context context = new Context();


    // --------------------------------------------------------- Public Methods

    /**
     * Set the number of allowed alias matches. If this number is
     * non-zero, then wild-card host matching is enabled. This 
     * number limits the number of wild-card host matches in
     * order to prevent a denial of service attack.
     */
    public void setAllowedAliasMatches(final int matches){
        defaultAliasMatches=matches;
        hostMap.setEnableWildCardMatching(matches>0);
    }

    /**
     * 
     */
    public int getAllowedAliasMatches(){
        return defaultAliasMatches;
    }

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
     * @param defaultHostName Default host name
     */
    public void setDefaultHostName(String defaultHostName) {
        this.defaultHostName = defaultHostName;
        defaultHost=null;
    }

    /**
     * Return the default host, or null if the host
     * has not been set.
     */
    private Host getDefaultHost(){
        if (defaultHost==null && defaultHostName!=null) {
            defaultHost=hostMap.getHost(defaultHostName);
        }
        return defaultHost;
    }
    
    /**
     * Add a new host to the mapper.
     *
     * @param name Virtual host name
     * @param host Host object
     */
    public synchronized void addHost(String name, String[] aliases,
                                     Object host) {
        HostMap hm=hostMap;
        if (hm.contains(name)) {
            return;
        }
        hm=new HostMap(hm);
        
        Host newHost=new Host();
        ContextList contextList=new ContextList();
        newHost.name=name;
        newHost.contextList=contextList;
        newHost.object=host;
        newHost.aliasMatchesRemaining=defaultAliasMatches;
        hm.addHost(newHost);
        for (int i=0; i < aliases.length; i++) {
            if (!hm.contains(aliases[i])) {
                newHost=new Host();
                newHost.aliasMatchesRemaining=defaultAliasMatches;
                newHost.name=aliases[i];
                newHost.contextList=contextList;
                newHost.object=host;
                hm.addHost(newHost);
            }
        }
        hostMap=hm;
    }


    /**
     * Remove a host from the mapper.
     *
     * @param name Virtual host name
     */
    public synchronized void removeHost(String name) {

        if (!hostMap.contains(name)) {
            return;
        }
        HostMap hm=new HostMap(hostMap);
        hm.removeHost(name);
        hostMap=hm;
    }
 
    /**
     * Get current host matching map
     */
    public String[] getHosts() {
        return hostMap.getHosts();
    }


    /**
     * Set context, used for wrapper mapping (request dispatcher).
     *
     * @param welcomeResources Welcome files defined for this context
     * @param resources Static resources of the context
     */
    public void setContext(String path, String[] welcomeResources,
                           javax.naming.Context resources) {
        context.name = path;
        context.welcomeResources = welcomeResources;
        context.resources = resources;
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

        Host host=hostMap.getHost(hostName);
        if (host==null) {
            addHost(hostName, new String[0], "");
            host=hostMap.getHost(hostName);
        }
        int slashCount = slashCount(path);
        synchronized (host) {
            Context[] contexts = host.contextList.contexts;
            // Update nesting
            if (slashCount > host.contextList.nesting) {
                host.contextList.nesting = slashCount;
            }
            Context[] newContexts = new Context[contexts.length + 1];
            Context newContext = new Context();
            newContext.name = path;
            newContext.object = context;
            newContext.welcomeResources = welcomeResources;
            newContext.resources = resources;
            if (insertMap(contexts, newContexts, newContext)) {
                host.contextList.contexts = newContexts;
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
        Host host=hostMap.getHost(hostName);
        if (host==null) {
            return;
        }
        synchronized (host) {
            Context[] contexts = host.contextList.contexts;
            if ( contexts.length == 0 ) {
                return;
            }
            Context[] newContexts = new Context[contexts.length - 1];
            if (removeMap(contexts, newContexts, path)) {
                host.contextList.contexts = newContexts;
                // Recalculate nesting
                host.contextList.nesting = 0;
                for (int i = 0; i < newContexts.length; i++) {
                    int slashCount = slashCount(newContexts[i].name);
                    if (slashCount > host.contextList.nesting) {
                        host.contextList.nesting = slashCount;
                    }
                }
            }
        }

    }


    /**
     * Return all contexts, in //HOST/PATH form
     *
     * @return The context names
     */
    public String[] getContextNames() {
        List list=new ArrayList();
        Host[] hosts=hostMap.toHostArray();
        for ( int i=0; i<hosts.length; i++ ) {
            for ( int j=0; j<hosts[i].contextList.contexts.length; j++ ) {
                String cname=hosts[i].contextList.contexts[j].name;
                list.add("//" + hosts[i].name +
                         (cname.startsWith("/") ? cname : "/"));
            }
        }
        String res[] = new String[list.size()];
        return(String[])list.toArray(res);
    }


    /**
     * Add a new Wrapper to an existing Context.
     *
     * @param hostName Virtual host name this wrapper belongs to
     * @param contextPath Context path this wrapper belongs to
     * @param path Wrapper mapping
     * @param wrapper Wrapper object
     */
    public void addWrapper(String hostName, String contextPath, String path,
                           Object wrapper) {
        addWrapper(hostName, contextPath, path, wrapper, false);
    }


    public void addWrapper(String hostName, String contextPath, String path,
                           Object wrapper, boolean jspWildCard) {
        Host host=hostMap.getHost(hostName);
        if (host==null) {
            return;
        }
        Context[] contexts = host.contextList.contexts;
        int pos2 = find(contexts, contextPath);
        if ( pos2<0 ) {
            logger.error("No context found: " + contextPath );
            return;
        }
        Context context = contexts[pos2];
        if (context.name.equals(contextPath)) {
            addWrapper(context, path, wrapper, jspWildCard);
        }
    }


    /**
     * Add a wrapper to the context associated with this wrapper.
     *
     * @param path Wrapper mapping
     * @param wrapper The Wrapper object
     */
    public void addWrapper(String path, Object wrapper) {
        addWrapper(context, path, wrapper);
    }


    public void addWrapper(String path, Object wrapper, boolean jspWildCard) {
        addWrapper(context, path, wrapper, jspWildCard);
    }


    protected void addWrapper(Context context, String path, Object wrapper) {
        addWrapper(context, path, wrapper, false);
    }


    /**
     * Adds a wrapper to the given context.
     *
     * @param context The context to which to add the wrapper
     * @param path Wrapper mapping
     * @param wrapper The Wrapper object
     * @param jspWildCard true if the wrapper corresponds to the JspServlet
     * and the mapping path contains a wildcard; false otherwise
     */
    protected void addWrapper(Context context, String path, Object wrapper,
                              boolean jspWildCard) {

        synchronized (context) {
            Wrapper newWrapper = new Wrapper();
            newWrapper.object = wrapper;
            newWrapper.jspWildCard = jspWildCard;
            if (path.endsWith("/*")) {
                // Wildcard wrapper
                newWrapper.name = path.substring(0, path.length() - 2);
                Wrapper[] oldWrappers = context.wildcardWrappers;
                Wrapper[] newWrappers =
                new Wrapper[oldWrappers.length + 1];
                if (insertMap(oldWrappers, newWrappers, newWrapper)) {
                    context.wildcardWrappers = newWrappers;
                    int slashCount = slashCount(newWrapper.name);
                    if (slashCount > context.nesting) {
                        context.nesting = slashCount;
                    }
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


    /**
     * Remove a wrapper from the context associated with this wrapper.
     *
     * @param path Wrapper mapping
     */
    public void removeWrapper(String path) {
        removeWrapper(context, path);
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
        Host host=hostMap.getHost(hostName);
        if (host==null) {
            return;
        }
        Context[] contexts = host.contextList.contexts;
        int pos2 = find(contexts, contextPath);
        if (pos2 < 0) {
            return;
        }
        Context context = contexts[pos2];
        if (context.name.equals(contextPath)) {
            removeWrapper(context, path);
        }
    }

    protected void removeWrapper(Context context, String path) {
        synchronized (context) {
            if (path.endsWith("/*")) {
                // Wildcard wrapper
                String name = path.substring(0, path.length() - 2);
                Wrapper[] oldWrappers = context.wildcardWrappers;
                Wrapper[] newWrappers =
                new Wrapper[oldWrappers.length - 1];
                if (removeMap(oldWrappers, newWrappers, name)) {
                    // Recalculate nesting
                    context.nesting = 0;
                    for (int i = 0; i < newWrappers.length; i++) {
                        int slashCount = slashCount(newWrappers[i].name);
                        if (slashCount > context.nesting) {
                            context.nesting = slashCount;
                        }
                    }
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

    public String getWrappersString( String host, String context ) {
        String names[]=getWrapperNames(host, context);
        StringBuffer sb=new StringBuffer();
        for ( int i=0; i<names.length; i++ ) {
            sb.append(names[i]).append(":");
        }
        return sb.toString();
    }

    public String[] getWrapperNames( String hostName, String context ) {
        List list=new ArrayList();
        if ( hostName==null ) hostName="";
        if ( context==null ) context="";
        Host host=hostMap.getHost(hostName);
        if (host!=null) {

            for ( int j=0; j<host.contextList.contexts.length; j++ ) {

                if ( ! context.equals( host.contextList.contexts[j].name))
                    continue;
                // found the context
                Context ctx=host.contextList.contexts[j];
                list.add( ctx.defaultWrapper.path);
                for ( int k=0; k<ctx.exactWrappers.length; k++ ) {
                    list.add( ctx.exactWrappers[k].path);
                }
                for ( int k=0; k<ctx.wildcardWrappers.length; k++ ) {
                    list.add( ctx.wildcardWrappers[k].path + "*");
                }
                for ( int k=0; k<ctx.extensionWrappers.length; k++ ) {
                    list.add( "*." + ctx.extensionWrappers[k].path);
                }
            }
        }
        String res[]=new String[list.size()];
        return(String[])list.toArray(res);
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

        host.toChars();
        uri.toChars();
        internalMap(host.getCharChunk(), uri.getCharChunk(), mappingData);

    }


    /**
     * Map the specified URI relative to the context,
     * mutating the given mapping data.
     *
     * @param uri URI
     * @param mappingData This structure will contain the result of the mapping
     *                    operation
     */
    public void map(MessageBytes uri, MappingData mappingData)
    throws Exception {

        uri.toChars();
        CharChunk uricc = uri.getCharChunk();
        uricc.setLimit(-1);
        internalMapWrapper(context, uricc, mappingData);

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
        int nesting = 0;

        // Virtual host mapping
        if (mappingData.host == null) {
            Host hFound=hostMap.getHost(host);
            if (hFound==null) {
                hFound=getDefaultHost();
                if (hFound==null) {
                    return;
                }
            }
            mappingData.host=hFound.object;
            contexts = hFound.contextList.contexts;
            nesting = hFound.contextList.nesting;
        }

        // Context mapping
        if (mappingData.context == null) {
            int pos = find(contexts, uri);
            if (pos == -1) {
                return;
            }

            int lastSlash = -1;
            int uriEnd = uri.getEnd();
            int length = -1;
            boolean found = false;
            while (pos >= 0) {
                if (uri.startsWith(contexts[pos].name)) {
                    length = contexts[pos].name.length();
                    if (uri.getLength() == length) {
                        found = true;
                        break;
                    } else if (uri.startsWithIgnoreCase("/", length)) {
                        found = true;
                        break;
                    }
                }
                if (lastSlash == -1) {
                    lastSlash = nthSlash(uri, nesting + 1);
                } else {
                    lastSlash = lastSlash(uri);
                }
                uri.setEnd(lastSlash);
                pos = find(contexts, uri);
            }
            uri.setEnd(uriEnd);

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
        boolean noServletPath = false;

        int length = context.name.length();
        if (length != (pathEnd - pathOffset)) {
            servletPath = pathOffset + length;
        } else {
            noServletPath = true;
            path.append('/');
            pathOffset = path.getOffset();
            pathEnd = path.getEnd();
            servletPath = pathOffset+length;
        }

        path.setOffset(servletPath);

        // Rule 1 -- Exact Match
        Wrapper[] exactWrappers = context.exactWrappers;
        internalMapExactWrapper(exactWrappers, path, mappingData);

        // Rule 2 -- Prefix Match
        boolean checkJspWelcomeFiles = false;
        Wrapper[] wildcardWrappers = context.wildcardWrappers;
        if (mappingData.wrapper == null) {
            internalMapWildcardWrapper(wildcardWrappers, context.nesting, 
                                       path, mappingData);
            if (mappingData.wrapper != null && mappingData.jspWildCard) {
                char[] buf = path.getBuffer();
                if (buf[pathEnd - 1] == '/') {
                    /*
                     * Path ending in '/' was mapped to JSP servlet based on
                     * wildcard match (e.g., as specified in url-pattern of a
                     * jsp-property-group.
                     * Force the context's welcome files, which are interpreted
                     * as JSP files (since they match the url-pattern), to be
                     * considered. See Bugzilla 27664.
                     */ 
                    mappingData.wrapper = null;
                    checkJspWelcomeFiles = true;
                } else {
                    // See Bugzilla 27704
                    mappingData.wrapperPath.setChars(buf, path.getStart(),
                                                     path.getLength());
                    mappingData.pathInfo.recycle();
                }
            }
        }

        if (mappingData.wrapper == null && noServletPath) {
            // The path is empty, redirect to "/"
            mappingData.redirectPath.setChars
            (path.getBuffer(), pathOffset, pathEnd);
            path.setEnd(pathEnd - 1);
            return;
        }

        // Rule 3 -- Extension Match
        Wrapper[] extensionWrappers = context.extensionWrappers;
        if (mappingData.wrapper == null && !checkJspWelcomeFiles) {
            internalMapExtensionWrapper(extensionWrappers, path, mappingData);
        }

        // Rule 4 -- Welcome resources processing for servlets
        if (mappingData.wrapper == null) {
            boolean checkWelcomeFiles = checkJspWelcomeFiles;
            if (!checkWelcomeFiles) {
                char[] buf = path.getBuffer();
                checkWelcomeFiles = (buf[pathEnd - 1] == '/');
            }
            if (checkWelcomeFiles) {
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
                        (wildcardWrappers, context.nesting, 
                         path, mappingData);
                    }

                    // Rule 4c -- Welcome resources processing
                    //            for physical folder
                    if (mappingData.wrapper == null
                        && context.resources != null) {
                        Object file = null;
                        String pathStr = path.toString();
                        try {
                            file = context.resources.lookup(pathStr);
                        } catch (NamingException nex) {
                            // Swallow not found, since this is normal
                        }
                        if (file != null && !(file instanceof DirContext) ) {
                            internalMapExtensionWrapper(extensionWrappers,
                                                        path, mappingData);
                            if (mappingData.wrapper == null
                                && context.defaultWrapper != null) {
                                mappingData.wrapper =
                                context.defaultWrapper.object;
                                mappingData.requestPath.setChars
                                (path.getBuffer(), path.getStart(), 
                                 path.getLength());
                                mappingData.wrapperPath.setChars
                                (path.getBuffer(), path.getStart(), 
                                 path.getLength());
                                mappingData.requestPath.setString(pathStr);
                                mappingData.wrapperPath.setString(pathStr);
                            }
                        }
                    }
                }

                path.setOffset(servletPath);
                path.setEnd(pathEnd);
            }

        }


        // Rule 7 -- Default servlet
        if (mappingData.wrapper == null && !checkJspWelcomeFiles) {
            if (context.defaultWrapper != null) {
                mappingData.wrapper = context.defaultWrapper.object;
                mappingData.requestPath.setChars
                (path.getBuffer(), path.getStart(), path.getLength());
                mappingData.wrapperPath.setChars
                (path.getBuffer(), path.getStart(), path.getLength());
            }
            // Redirection to a folder
            char[] buf = path.getBuffer();
            if (context.resources != null && buf[pathEnd -1 ] != '/') {
                Object file = null;
                String pathStr = path.toString();
                try {
                    file = context.resources.lookup(pathStr);
                } catch (NamingException nex) {
                    // Swallow, since someone else handles the 404
                }
                if (file != null && file instanceof DirContext) {
                    // Note: this mutates the path: do not do any processing 
                    // after this (since we set the redirectPath, there 
                    // shouldn't be any)
                    path.setOffset(pathOffset);
                    path.append('/');
                    mappingData.redirectPath.setChars
                    (path.getBuffer(), path.getStart(), path.getLength());
                } else {
                    mappingData.requestPath.setString(pathStr);
                    mappingData.wrapperPath.setString(pathStr);
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
    (Wrapper[] wrappers, int nesting, CharChunk path, 
     MappingData mappingData) {

        int pathEnd = path.getEnd();
        int pathOffset = path.getOffset();

        int lastSlash = -1;
        int length = -1;
        int pos = find(wrappers, path);
        if (pos != -1) {
            boolean found = false;
            while (pos >= 0) {
                if (path.startsWith(wrappers[pos].name)) {
                    length = wrappers[pos].name.length();
                    if (path.getLength() == length) {
                        found = true;
                        break;
                    } else if (path.startsWithIgnoreCase("/", length)) {
                        found = true;
                        break;
                    }
                }
                if (lastSlash == -1) {
                    lastSlash = nthSlash(path, nesting + 1);
                } else {
                    lastSlash = lastSlash(path);
                }
                path.setEnd(lastSlash);
                pos = find(wrappers, path);
            }
            path.setEnd(pathEnd);
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
                mappingData.jspWildCard = wrappers[pos].jspWildCard;
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

        if (compare(name, start, end, map[0].name) < 0 ) {
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
    private static final int findIgnoreCase(MapElement[] map, CharChunk name) {
        return findIgnoreCase(map, name, name.getStart(), name.getEnd());
    }


    /**
     * Find a map elemnt given its name in a sorted array of map elements.
     * This will return the index for the closest inferior or equal item in the
     * given array.
     */
    private static final int findIgnoreCase(MapElement[] map, CharChunk name,
                                            int start, int end) {

        int a = 0;
        int b = map.length - 1;

        // Special cases: -1 and 0
        if (b == -1) {
            return -1;
        }
        if (compareIgnoreCase(name, start, end, map[0].name) < 0 ) {
            return -1;
        }
        if (b == 0) {
            return 0;
        }

        int i = 0;
        while (true) {
            i = (b + a) / 2;
            int result = compareIgnoreCase(name, start, end, map[i].name);
            if (result == 1) {
                a = i;
            } else if (result == 0) {
                return i;
            } else {
                b = i;
            }
            if ((b - a) == 1) {
                int result2 = compareIgnoreCase(name, start, end, map[b].name);
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
        final int compareLen=compareTo.length();
        int len = compareLen;
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
            if (compareLen > (end - start)) {
                result = -1;
            } else if (compareLen < (end - start)) {
                result = 1;
            }
        }
        return result;
    }


    /**
     * Compare given char chunk with String ignoring case.
     * Return -1, 0 or +1 if inferior, equal, or superior to the String.
     */
    private static final int compareIgnoreCase(CharChunk name, int start, int end,
                                               String compareTo) {
        int result = 0;
        char[] c = name.getBuffer();
        final int compareLen=compareTo.length();
        int len = compareLen;
        if ((end - start) < len) {
            len = end - start;
        }
        for (int i = 0; (i < len) && (result == 0); i++) {
            if (Ascii.toLower(c[i + start]) > Ascii.toLower(compareTo.charAt(i))) {
                result = 1;
            } else if (Ascii.toLower(c[i + start]) < Ascii.toLower(compareTo.charAt(i))) {
                result = -1;
            }
        }
        if (result == 0) {
            if (compareLen > (end - start)) {
                result = -1;
            } else if (compareLen < (end - start)) {
                result = 1;
            }
        }
        return result;
    }


    /**
     * Find the position of the last slash in the given char chunk.
     */
    private static final int lastSlash(CharChunk name) {

        char[] c = name.getBuffer();
        int end = name.getEnd();
        int start = name.getStart();
        int pos = end;

        while (pos > start) {
            if (c[--pos] == '/') {
                break;
            }
        }

        return(pos);

    }


    /**
     * Find the position of the nth slash, in the given char chunk.
     */
    private static final int nthSlash(CharChunk name, int n) {

        char[] c = name.getBuffer();
        int end = name.getEnd();
        int start = name.getStart();
        int pos = start;
        int count = 0;

        while (pos < end) {
            if ((c[pos++] == '/') && ((++count) == n)) {
                pos--;
                break;
            }
        }

        return(pos);

    }


    /**
     * Return the slash count in a given string.
     */
    private static final int slashCount(String name) {
        int pos = -1;
        int count = 0;
        while ((pos = name.indexOf('/', pos + 1)) != -1) {
            count++;
        }
        return count;
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


    protected static abstract class MapElement {

        public String name = null;
        public Object object = null;

    }


    // ------------------------------------------------------- Host Inner Class


    protected static final class Host
    extends MapElement implements Comparable {

        public ContextList contextList = null;
        public int hash;
        /** 
         * Limiter for host alias matching to prevent DOS. The default
         * of 0, disables alias matching.
         */
        public int aliasMatchesRemaining=0;

        public int compareTo(Object o){
            int iResult=0;
            if (o==null) {
                iResult=1;
            } else {
                Host oCompare=(Host)o;
                if (this.hash>oCompare.hash) {
                    iResult=1;
                } else if (this.hash<oCompare.hash) {
                    iResult=-1;
                } else {
                    // iResult is already 0
                }
            }
            return iResult;
        }

    }


    // ------------------------------------------------ ContextList Inner Class


    protected static final class ContextList {

        public Context[] contexts = new Context[0];
        public int nesting = 0;

    }


    // ---------------------------------------------------- Context Inner Class


    protected static final class Context
    extends MapElement {

        public String path = null;
        public String[] welcomeResources = new String[0];
        public javax.naming.Context resources = null;
        public Wrapper defaultWrapper = null;
        public Wrapper[] exactWrappers = new Wrapper[0];
        public Wrapper[] wildcardWrappers = new Wrapper[0];
        public Wrapper[] extensionWrappers = new Wrapper[0];
        public int nesting = 0;

    }


    // ---------------------------------------------------- Wrapper Inner Class


    protected static class Wrapper
    extends MapElement {

        public String path = null;
        public boolean jspWildCard = false;
    }


    // -------------------------------------------------------- Testing Methods

    // FIXME: Externalize this

    public static void main(String args[]) {

        try {

            Mapper mapper = new Mapper();
            System.out.println("Start");

            final int TEST_HOST=3;

            String[][][] testHosts={
                {{"fewohfoweoih"},new String[0], {"blah9"}},
                {{"fwehoihoihwfeo"},new String[0], {"blah5"}},
                {{"iohgeoihro"},new String[0], {"blah4"}},
                {{"iowejoiejfoiew"},{"testalias","*.testdomain.com"}, {"blah7"}},
                {{"iowejoiejfoiew"},new String[0], {"blah17"}},
                {{"lkwefjwojweffewoih"},new String[0], {"blah11"}},
                {{"ohewoihfewoih"},new String[0], {"blah8"}},
                {{"owefojiwefoi"},new String[0], {"blah6"}},
                {{"qwigqwiwoih"},new String[0], {"blah14"}},
                {{"sjbjdvwsbvhr/"},new String[0], {"blah1"}},
                {{"sjbjdvwsbvhrb"},new String[0], {"blah1"}},
                {{"ttthtiuhwoih"},new String[0], {"blah10"}},
                {{"wekhfewuifweuibf"},new String[0], {"blah2"}},
                {{"xxxxgqwiwoih"},new String[0], {"blah13"}},
                {{"ylwrehirkuewh"},new String[0], {"blah3"}},
                {{"zzzuyopjvewpovewjhfewoih"},new String[0], {"blah12"}}
            };

            for (int i=0; i < testHosts.length; i++) {
                mapper.addHost(testHosts[i][0][0],testHosts[i][1],testHosts[i][2][0]);
            }

            System.out.println("Map:");
            String[] hNames=mapper.getHosts();

            if (hNames.length!=(testHosts.length+1)) {
                // One of the entries was a duplicate, but it also has
                // two aliases, so the expected
                // count of hosts is +1
                System.out.println("***************** FAIL ********** hostCount="+hNames.length+" Expected="+(testHosts.length+1));
            }

            for (int i=0; i < hNames.length; i++ ) {
                System.out.println(hNames[i]);
                if (!mapper.hostMap.contains(hNames[i])) {
                    System.out.println("BIG FAILURE!!!!! Host - "+hNames[i]+" doesnt appear in the hostMap!");
                }
            }

            for (int i=0; i < testHosts.length; i++) {
                if (!mapper.hostMap.contains(testHosts[i][0][0])) {
                    System.out.println("BIG FAILURE!!!!! HostMapper doesnt appear to contain "+testHosts[i][0][0]+" in the hostMap!");
                }
            }

            final String sDefault=testHosts[testHosts.length-2][0][0];
            mapper.setDefaultHostName(sDefault);
            if (!sDefault.equals(mapper.getDefaultHostName())) {
                System.out.println("***************** FAIL ********** getDefaultHostName() returned incorrect value");
            }

            String[] welcomes = new String[2];
            welcomes[0] = "boo/baba";
            welcomes[1] = "bobou";


            /*
            Columns are

            0   host name
            1   context path
            2   Context Name
            3   Welcome File List
            */
            final int CONTEXT_NAME_COL=2;
            final int CONTEXT_PATH_COL=1;
            String[][][] contextTestMap={
                {{testHosts[TEST_HOST][0][0]}, {""}, {"context0"}, new String[0]},
                {{testHosts[TEST_HOST][0][0]}, {"/foo"}, {"context1"}, new String[0]},
                {{testHosts[TEST_HOST][0][0]}, {"/foo/bar"}, {"context2"}, welcomes},
                {{testHosts[TEST_HOST][0][0]}, {"/foo/bar/ble"}, {"context3"}, new String[0]}
            };

            for (int i=0; i < contextTestMap.length; i++) {
                mapper.addContext(contextTestMap[i][0][0],
                                  contextTestMap[i][1][0],
                                  contextTestMap[i][2][0],
                                  contextTestMap[i][3],
                                  null);
            }

            /*
            Columns are

                0   java.lang.Integer   Mapping into ContextTestMap
                1   java.lang.String    URL Mapping
                2   java.lang.String    Wrapper ID
                3   java.lang.String    Test URL
            */

            final int TEST_URI_COL=3;
            final int WRAPPER_NAME_COL=2;
            final int WRAPPER_PATH_COL=1;
            Object[][] testWrappers={
                {new Integer(0),"","wrapper99","/index.html"},      
                {new Integer(2),"/fo/*","wrapper0","/foo/bar/fo/xxx.html"},
                {new Integer(2),"/fuzz/*","wrapper1","/foo/bar/fuzz/test.pdf"},
                {new Integer(2),"/blh","wrapper2","/foo/bar/blh"},
                {new Integer(2),"*.jsp","wrapper3","/foo/bar/baz.jsp"},
                {new Integer(2),"/blah/bou/*","wrapper4","/foo/bar/blah/bou/blech.html"},
                {new Integer(2),"/bla/blah/bou/*","wrapper4a","/foo/bar/bla/blah/bou/blech.html"},
                {new Integer(2),"/blah/bobou/*","wrapper5","/foo/bar/blah/bobou/banana.html"},
                {new Integer(2),"*.html","wrapper6","/foo/bar/farley.html"}

            };
            for (int i=0; i < testWrappers.length; i++) {
                mapper.addWrapper(
                                 contextTestMap[((Integer)testWrappers[i][0]).intValue()][0][0],   // Server
                                 contextTestMap[((Integer)testWrappers[i][0]).intValue()][1][0],   // Context,
                                 (String)testWrappers[i][1],
                                 (String)testWrappers[i][2]
                                 );
            }
            String[] sHost=null;
            MappingData mappingData=new MappingData();
            MessageBytes uri=null ,host=null;

            for (int iOuter=0; iOuter < 2; iOuter++) {
                if (iOuter==0) {
                    // First Pass is against the test host
                    sHost=testHosts[TEST_HOST][0];
                } else {
                    // 2nd pass is against an alias
                    sHost=testHosts[TEST_HOST][1];
                }
                host = MessageBytes.newInstance();
                host.setString(sHost[0]);

                for (int iInner=0; iInner < testWrappers.length; iInner++) {

                    uri = MessageBytes.newInstance();

                    uri.setString((String)testWrappers[iInner][TEST_URI_COL]);
                    uri.toChars();
                    uri.getCharChunk().setLimit(-1);

                    mapper.map(host, uri, mappingData);
                    //  OK, now let's check that the mapper worked.
                    System.out.println("Host = "+sHost[0]+" URI="+uri.toString());
                    System.out.println("MD Host:" + mappingData.host);
                    System.out.println("MD Context:" + mappingData.context);
                    System.out.println("MD Wrapper:" + mappingData.wrapper);

                    System.out.println("contextPath:" + mappingData.contextPath);
                    System.out.println("wrapperPath:" + mappingData.wrapperPath);
                    System.out.println("pathInfo:" + mappingData.pathInfo);
                    System.out.println("redirectPath:" + mappingData.redirectPath);

                    // Check the host name - Aliases will come back with the parent host
                    // name.
                    if (!testHosts[TEST_HOST][2][0].equals(mappingData.host)) {
                        System.out.println("******* FAIL host.name="+mappingData.host +" Expected="+testHosts[TEST_HOST][2][0]);
                    }

                    // Check the context
                    if (mappingData.context!=null) {
                        if (!mappingData.context.equals(
                                                       contextTestMap[((Integer)testWrappers[iInner][0]).intValue()][CONTEXT_NAME_COL][0]
                                                       )
                           ) {
                        }
                    }

                    if (mappingData.wrapper!=null) {
                        if (!mappingData.wrapper.equals(testWrappers[iInner][2])
                           ) {
                            System.out.println(" Wrappers not equal as expected - mappingData.wrapper="+mappingData.wrapper+" expected="+
                                               testWrappers[iInner][2]);
                        }
                    }

                    if (mappingData.contextPath!=null) {
                        if (!contextTestMap[((Integer)testWrappers[iInner][0]).intValue()][CONTEXT_PATH_COL][0].equals(mappingData.contextPath.toString()

                                                                                                                      )) {
                            System.out.println(" Context Path not as expected - mappingData.contextPath="+mappingData.contextPath.toString()+" expected="+
                                               contextTestMap[((Integer)testWrappers[iInner][0]).intValue()][CONTEXT_PATH_COL][0]);
                        }
                    }

                    if (mappingData.wrapperPath.toString()!=null) {
                        if (!((String)testWrappers[iInner][WRAPPER_PATH_COL]).startsWith(mappingData.wrapperPath.toString())
                            && 
                            !((String)testWrappers[iInner][WRAPPER_PATH_COL]).startsWith("*")
                           ) {
                            System.out.println("Wrapper Path="+mappingData.wrapperPath + "  Expected="+testWrappers[iInner][WRAPPER_PATH_COL]);
                        }
                    }

                    if (mappingData.pathInfo.toString()!=null) {
                        if (!
                            ((String)testWrappers[iInner][TEST_URI_COL]).endsWith(mappingData.pathInfo.toString())
                           ) {
                            System.out.println(" Mismatch between pathInfo ("+mappingData.pathInfo+" and Test URI ("+
                                               testWrappers[iInner][TEST_URI_COL]);
                        }
                    }

                    // TODO: RedirectPath                           redirectPath:null

                    mappingData.recycle();

                }
            }

            host = MessageBytes.newInstance();
            host.setString(testHosts[TEST_HOST][0][0]);

            uri=MessageBytes.newInstance();
            uri.setString((String)testWrappers[6][TEST_URI_COL]);
            uri.toChars();
            uri.getCharChunk().setLimit(-1);


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
            System.out.println("Host = "+host.toString()+" URI="+uri.toString());
            System.out.println("MD Host:" + mappingData.host);
            System.out.println("MD Context:" + mappingData.context);
            System.out.println("MD Wrapper:" + mappingData.wrapper);

            System.out.println("contextPath:" + mappingData.contextPath);
            System.out.println("wrapperPath:" + mappingData.wrapperPath);
            System.out.println("pathInfo:" + mappingData.pathInfo);
            System.out.println("redirectPath:" + mappingData.redirectPath);

            System.out.println("Testing virtual host name.");

            host = MessageBytes.newInstance();
            host.setString("host99.testdomain.com");

            uri=MessageBytes.newInstance();
            uri.setString((String)testWrappers[6][TEST_URI_COL]);
            uri.toChars();
            uri.getCharChunk().setLimit(-1);

            System.out.println("Host = "+host.toString()+" URI="+uri.toString());

            mappingData.recycle();
            mapper.map(host, uri, mappingData);

            System.out.println("MD Host:" + mappingData.host);
            System.out.println("MD Context:" + mappingData.context);
            System.out.println("MD Wrapper:" + mappingData.wrapper);

            System.out.println("contextPath:" + mappingData.contextPath);
            System.out.println("wrapperPath:" + mappingData.wrapperPath);
            System.out.println("pathInfo:" + mappingData.pathInfo);
            System.out.println("redirectPath:" + mappingData.redirectPath);

        } catch (Exception e) {
            e.printStackTrace();
        }

    }
}
