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

package org.apache.naming.modules.fs;

import java.io.File;

import javax.naming.directory.Attribute;
import javax.naming.directory.BasicAttribute;
import javax.naming.directory.BasicAttributes;

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

