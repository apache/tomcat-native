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

package org.apache.naming.handler.jndi;

import java.io.ByteArrayInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.net.URLConnection;
import java.security.Permission;
import java.util.Enumeration;
import java.util.Vector;

import javax.naming.NameClassPair;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;
import javax.naming.directory.Attribute;
import javax.naming.directory.Attributes;
import javax.naming.directory.DirContext;

import org.apache.naming.core.JndiPermission;
import org.apache.naming.util.AttributeHelper;
// import org.apache.naming.resources.Resource;
// import org.apache.naming.resources.ResourceAttributes;

/**
 * Connection to a JNDI directory context.
 * <p/>
 * Note: All the object attribute names are the WebDAV names, not the HTTP 
 * names, so this class overrides some methods from URLConnection to do the
 * queries using the right names. Content handler is also not used; the 
 * content is directly returned.
 * 
 * @author <a href="mailto:remm@apache.org">Remy Maucherat</a>
 * @author Costin Manolache
 */
public class DirContextURLConnection 
    extends URLConnection
{
    
    
    // ----------------------------------------------------------- Constructors
    
    /**
     * @param context The base context for the dynamic resources.
     *        For regular webapps, it should be a thread-bound context, probably
     *        a branch under java:
     *        For top-level apps, it can be either the InitialContext or a branch
     *        that is used for all content.
     *
     * The choice of the context affects the base of the URLs.
     */
    public DirContextURLConnection(DirContext context, URL url) {
        super(url);
        if (context == null)
            throw new IllegalArgumentException
                ("Directory context can't be null");
        if (System.getSecurityManager() != null) {
            this.permission = new JndiPermission(url.toString());
	}
        this.context = context;
    }
    
    
    // ----------------------------------------------------- Instance Variables
    
    
    /**
     * Directory context.
     */
    protected DirContext context;
    
    
    /**
     * Associated resource.
     */
    protected Object resource;
    
    
    /**
     * Associated DirContext.
     */
    protected DirContext collection;
    
    
    /**
     * Other unknown object.
     */
    protected Object object;
    
    
    /**
     * Attributes.
     */
    protected Attributes attributes;
    
    
    /**
     * Date.
     */
    protected long date;
    
    
    /**
     * Permission
     */
    protected Permission permission;


    // ------------------------------------------------------------- Properties
    
    
    /**
     * Connect to the DirContext, and retrive the bound object, as well as
     * its attributes. If no object is bound with the name specified in the
     * URL, then an IOException is thrown.
     * 
     * @throws IOException Object not found
     */
    public void connect()
        throws IOException {
        
        if (!connected) {
            
            try {
                // TODO: What is this ???  (costin)
                date = System.currentTimeMillis();
                String path = getURL().getFile();

                /* This deals with a strange case, where the
                   name is prefixed by hostname and contextname.

                   A webapp should never use this - all resources
                   are local ( or a different mean should be used to
                   locate external res ).

                   The top-level handler must use the hostname + context to
                   locate files in a particular context.
                   
                  if (context instanceof ProxyDirContext) {
                    ProxyDirContext proxyDirContext = 
                        (ProxyDirContext) context;
                    String hostName = proxyDirContext.getHostName();
                    String contextName = proxyDirContext.getContextName();
                    if (hostName != null) {
                        if (!path.startsWith("/" + hostName + "/"))
                            return;
                        path = path.substring(hostName.length()+ 1);
                    }
                    if (contextName != null) {
                        if (!path.startsWith(contextName + "/")) {
                            return;
                        } else {
                            path = path.substring(contextName.length());
                        }
                    }
                }
                */
                object = context.lookup(path);
                attributes = context.getAttributes(path);
                //                if (object instanceof Resource)
                //  resource = (Resource) object;
                if (object instanceof DirContext)
                    collection = (DirContext) object;
                else
                    resource=object;
            } catch (NamingException e) {
                // Object not found
            }
            
            connected = true;
            
        }
        
    }
    
    
    /**
     * Return the content length value.
     */
    public int getContentLength() {
        if (!connected) {
            // Try to connect (silently)
            try {
                connect();
            } catch (IOException e) {
            }
        }
        
        if (attributes == null)
            return (-1);

        return (int)AttributeHelper.getContentLength( attributes );
    }

    /**
     * Return the content type value.
     */
    public String getContentType() {
        if (!connected) {
            // Try to connect (silently)
            try {
                connect();
            } catch (IOException e) {
            }
        }
        
        if (attributes == null)
            return null;
        
        return AttributeHelper.getContentType(attributes);
    }
    
    
    /**
     * Return the last modified date.
     * TODO: it's broken
     */
    public long getDate() {
        return date;
    }
    
    
    /**
     * Return the last modified date.
     */
    public long getLastModified() {

        if (!connected) {
            // Try to connect (silently)
            try {
                connect();
            } catch (IOException e) {
            }
        }

        if (attributes == null)
            return 0;

        return AttributeHelper.getLastModified( attributes );
    }
    
    
    /**
     * Returns the name of the specified header field.
     */
    public String getHeaderField(String name) {

        if (!connected) {
            // Try to connect (silently)
            try {
                connect();
            } catch (IOException e) {
            }
        }
        
        if (attributes == null)
            return (null);

        Attribute attribute = attributes.get(name);
        try {
            return attribute.get().toString();
        } catch (Exception e) {
            // Shouldn't happen, unless the attribute has no value
        }

        return (null);
        
    }
    
    
    /**
     * Get object content.
     */
    public Object getContent()
        throws IOException {
        
        if (!connected)
            connect();
        
        if (resource != null)
            return getInputStream();
        if (collection != null)
            return collection;
        if (object != null)
            return object;
        
        throw new FileNotFoundException();
        
    }
    
    
    /**
     * Get object content.
     */
    public Object getContent(Class[] classes)
        throws IOException {
        
        Object object = getContent();
        
        for (int i = 0; i < classes.length; i++) {
            if (classes[i].isInstance(object))
                return object;
        }
        
        return null;
        
    }
    
    
    /**
     * Get input stream.
     */
    public InputStream getInputStream() 
        throws IOException {
        
        if (!connected)
            connect();
        
        if (resource == null) {
            throw new FileNotFoundException();
        } else {
            // Reopen resource
            try {
                resource =  context.lookup(getURL().getFile());
            } catch (NamingException e) {
            }
        }
        
        //        return (resource.streamContent());
        return getInputStream(resource);
        
    }

    /** Try to extract content from a resource found in the directory
     *  Code from Resource and ProxyContext.
     */
    public static InputStream getInputStream( Object resource ) {
        if( resource instanceof InputStream )
            return (InputStream) resource;

        // Found in: ProxyDirContext.lookup ( strange, only in one ).
        return new ByteArrayInputStream(resource.toString().getBytes());
    }
    
    /**
     * Get the Permission for this URL
     */
    public Permission getPermission() {

        return permission;
    }


    // --------------------------------------------------------- Public Methods
    
    
    /**
     * List children of this collection. The names given are relative to this
     * URI's path. The full uri of the children is then : path + "/" + name.
     */
    public Enumeration list()
        throws IOException {
        
        if (!connected) {
            connect();
        }
        
        if ((resource == null) && (collection == null)) {
            throw new FileNotFoundException();
        }
        
        Vector result = new Vector();
        
        if (collection != null) {
            try {
                NamingEnumeration enum = context.list(getURL().getFile());
                while (enum.hasMoreElements()) {
                    NameClassPair ncp = (NameClassPair) enum.nextElement();
                    result.addElement(ncp.getName());
                }
            } catch (NamingException e) {
                // Unexpected exception
                throw new FileNotFoundException();
            }
        }
        
        return result.elements();
        
    }
    
    
}
