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

package org.apache.naming.util;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import javax.naming.NamingException;
import javax.naming.directory.Attribute;
import javax.naming.directory.Attributes;
// import org.apache.naming.resources.Resource;
// import org.apache.naming.resources.ResourceAttributes;

/**
 * @author <a href="mailto:remm@apache.org">Remy Maucherat</a>
 * @author Costin Manolache
 */
public class AttributeHelper
{
    /**
     * Content length.
     */
    public static final String CONTENT_LENGTH = "getcontentlength";
    public static final String ALTERNATE_CONTENT_LENGTH = "content-length";

    /**
     * MIME type of the content.
     */
    public static final String CONTENT_TYPE = "getcontenttype";
    public static final String ALTERNATE_TYPE = "content-type";
    

    /**
     * Last modification date. XXX Use standard LDAP att name
     */
    public static final String LAST_MODIFIED = "getlastmodified";
    public static final String ALTERNATE_LAST_MODIFIED = "last-modified";
    /**
     * Date formats using for Date parsing.
     */
    protected static final SimpleDateFormat formats[] = {
        new SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss zzz", Locale.US),
        new SimpleDateFormat("EEE MMM dd HH:mm:ss zzz yyyy", Locale.US),
        new SimpleDateFormat("EEEEEE, dd-MMM-yy HH:mm:ss zzz", Locale.US),
        new SimpleDateFormat("EEE MMMM d HH:mm:ss yyyy", Locale.US)
    };


    /**
     * Get content length.
     * 
     * @return content length value
     */
    public static long getContentLength(Attributes attributes) {
        long contentLength=-1;
        if (contentLength != -1L)
            return contentLength;
        if (attributes != null) {
            Attribute attribute = attributes.get(CONTENT_LENGTH);
            if( attribute==null )
                attribute=attributes.get(ALTERNATE_CONTENT_LENGTH );
            if (attribute != null) {
                try {
                    Object value = attribute.get();
                    if (value instanceof Long) {
                        contentLength = ((Long) value).longValue();
                    } else {
                        try {
                            contentLength = Long.parseLong(value.toString());
                        } catch (NumberFormatException e) {
                            ; // Ignore
                        }
                    }
                } catch (NamingException e) {
                    ; // No value for the attribute
                }
            }
        }
        return contentLength;
    }
    
    /**
     * Return the content type value.
     */
    public static String getContentType(Attributes attributes) {
        Attribute attribute = attributes.get(CONTENT_TYPE);
        if( attribute == null ) return null;
        
        try {
            String s= attribute.get().toString();
            return s;
        } catch (Exception e) {
            // Shouldn't happen, unless the attribute has no value
        }
        return null;
    }
    
    /** Find the last modified of an entry. It's using various common
     *  attribute names, and support Long, Date, String att values.
     */
    public static long getLastModified( Attributes attributes ) {
        long lastModified=-1;
        Date lastModifiedDate;
        
        Attribute attribute = attributes.get(LAST_MODIFIED);
        if( attribute==null )
            attribute=attributes.get(ALTERNATE_LAST_MODIFIED);
        
        if (attribute != null) {
            try {
                Object value = attribute.get();
                if (value instanceof Long) {
                    lastModified = ((Long) value).longValue();
                } else if (value instanceof Date) {
                    lastModified = ((Date) value).getTime();
                    lastModifiedDate = (Date) value;
                } else {
                    String lastModifiedDateValue = value.toString();
                    Date result = null;
                    // Parsing the HTTP Date
                    for (int i = 0; (result == null) && 
                             (i < formats.length); i++) {
                        try {
                            result = 
                                formats[i].parse(lastModifiedDateValue);
                        } catch (ParseException e) {
                            ;
                        }
                    }
                    if (result != null) {
                        lastModified = result.getTime();
                        lastModifiedDate = result;
                    }
                }
            } catch (NamingException e) {
                ; // No value for the attribute
            }
        }
        return lastModified;
    }
        
}
