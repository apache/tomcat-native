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


package org.apache.naming.modules.fs;

import java.util.Arrays;
import java.util.Hashtable;
import java.util.Vector;
import java.util.Date;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.FileOutputStream;
import java.io.FileNotFoundException;
import java.io.OutputStream;
import java.io.IOException;
import javax.naming.Context;
import javax.naming.Name;
import javax.naming.NameParser;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;
import javax.naming.CompositeName;
import javax.naming.NameParser;
import javax.naming.OperationNotSupportedException;
import javax.naming.NameAlreadyBoundException;
import javax.naming.directory.*;

import org.apache.tomcat.util.res.StringManager;

import org.apache.naming.core.*;

/**
 * This specialized resource attribute implementation does some lazy 
 * reading (to speed up simple checks, like checking the last modified 
 * date).
 */
public class FileAttributes extends BasicAttributes {
    // -------------------------------------------------------- Constructor
    
    public FileAttributes(File file) {
        this.file = file;
    }
    
    // --------------------------------------------------- Member Variables
    
    
    protected File file;
    
    
    protected boolean accessed = false;
        
        
    // ----------------------------------------- ResourceAttributes Methods
    public static String CONTENT_LENGTH="contentLength";
    
    public Attribute get(String attrId) {
        if( CONTENT_LENGTH.equalsIgnoreCase(attrId) ) {
            // XXX use our own att, with long support
            return new BasicAttribute(CONTENT_LENGTH, new Long( getContentLength() ));
        }
	return (super.get(attrId));
    }

        
    /**
     * Is collection.
     */
//     public boolean isCollection() {
//         if (!accessed) {
//             collection = file.isDirectory();
//             accessed = true;
//         }
//         return super.isCollection();
//     }

    // Those methods avoid using an Attribute and return the real value.
    // There is no caching at this level - use the higher level caching.
        
    /**
     * Get content length.
     * 
     * @return content length value
     */
    public long getContentLength() {
        long contentLength = file.length();
        return contentLength;
    }
        
        
    /**
     * Get creation time.
     * 
     * @return creation time value
     */
    public long getCreation() {
        long creation = file.lastModified();
        return creation;
    }
    
    /**
     * Get last modified time.
     * 
     * @return lastModified time value
     */
    public long getLastModified() {
        long lastModified = file.lastModified();
        return lastModified;
    }
    
    /**
     * Get resource type.
     * 
     * @return String resource type
     */
    //         public String getResourceType() {
    //             if (!accessed) {
//                 //collection = file.isDirectory();
//                 accessed = true;
//             }
//             return super.getResourceType();
//         }
}

