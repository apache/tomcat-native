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
