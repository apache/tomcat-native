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
package org.apache.tomcat.util.http;

import java.net.*;
import java.util.*;


/**
 * A mime type map that implements the java.net.FileNameMap interface.
 *
 * @author James Duncan Davidson [duncan@eng.sun.com]
 * @author Jason Hunter [jch@eng.sun.com]
 */
public class MimeMap implements FileNameMap {

    // Defaults - all of them are "well-known" types,
    // you can add using normal web.xml.
    
    public static Hashtable defaultMap=new Hashtable(101);
    static {
        defaultMap.put("txt", "text/plain");
        defaultMap.put("html","text/html");
        defaultMap.put("htm", "text/html");
        defaultMap.put("gif", "image/gif");
        defaultMap.put("jpg", "image/jpeg");
        defaultMap.put("jpe", "image/jpeg");
        defaultMap.put("jpeg", "image/jpeg");
	defaultMap.put("java", "text/plain");
        defaultMap.put("body", "text/html");
        defaultMap.put("rtx", "text/richtext");
        defaultMap.put("tsv", "text/tab-separated-values");
        defaultMap.put("etx", "text/x-setext");
        defaultMap.put("ps", "application/x-postscript");
        defaultMap.put("class", "application/java");
        defaultMap.put("csh", "application/x-csh");
        defaultMap.put("sh", "application/x-sh");
        defaultMap.put("tcl", "application/x-tcl");
        defaultMap.put("tex", "application/x-tex");
        defaultMap.put("texinfo", "application/x-texinfo");
        defaultMap.put("texi", "application/x-texinfo");
        defaultMap.put("t", "application/x-troff");
        defaultMap.put("tr", "application/x-troff");
        defaultMap.put("roff", "application/x-troff");
        defaultMap.put("man", "application/x-troff-man");
        defaultMap.put("me", "application/x-troff-me");
        defaultMap.put("ms", "application/x-wais-source");
        defaultMap.put("src", "application/x-wais-source");
        defaultMap.put("zip", "application/zip");
        defaultMap.put("bcpio", "application/x-bcpio");
        defaultMap.put("cpio", "application/x-cpio");
        defaultMap.put("gtar", "application/x-gtar");
        defaultMap.put("shar", "application/x-shar");
        defaultMap.put("sv4cpio", "application/x-sv4cpio");
        defaultMap.put("sv4crc", "application/x-sv4crc");
        defaultMap.put("tar", "application/x-tar");
        defaultMap.put("ustar", "application/x-ustar");
        defaultMap.put("dvi", "application/x-dvi");
        defaultMap.put("hdf", "application/x-hdf");
        defaultMap.put("latex", "application/x-latex");
        defaultMap.put("bin", "application/octet-stream");
        defaultMap.put("oda", "application/oda");
        defaultMap.put("pdf", "application/pdf");
        defaultMap.put("ps", "application/postscript");
        defaultMap.put("eps", "application/postscript");
        defaultMap.put("ai", "application/postscript");
        defaultMap.put("rtf", "application/rtf");
        defaultMap.put("nc", "application/x-netcdf");
        defaultMap.put("cdf", "application/x-netcdf");
        defaultMap.put("cer", "application/x-x509-ca-cert");
        defaultMap.put("exe", "application/octet-stream");
        defaultMap.put("gz", "application/x-gzip");
        defaultMap.put("Z", "application/x-compress");
        defaultMap.put("z", "application/x-compress");
        defaultMap.put("hqx", "application/mac-binhex40");
        defaultMap.put("mif", "application/x-mif");
        defaultMap.put("ief", "image/ief");
        defaultMap.put("tiff", "image/tiff");
        defaultMap.put("tif", "image/tiff");
        defaultMap.put("ras", "image/x-cmu-raster");
        defaultMap.put("pnm", "image/x-portable-anymap");
        defaultMap.put("pbm", "image/x-portable-bitmap");
        defaultMap.put("pgm", "image/x-portable-graymap");
        defaultMap.put("ppm", "image/x-portable-pixmap");
        defaultMap.put("rgb", "image/x-rgb");
        defaultMap.put("xbm", "image/x-xbitmap");
        defaultMap.put("xpm", "image/x-xpixmap");
        defaultMap.put("xwd", "image/x-xwindowdump");
        defaultMap.put("au", "audio/basic");
        defaultMap.put("snd", "audio/basic");
        defaultMap.put("aif", "audio/x-aiff");
        defaultMap.put("aiff", "audio/x-aiff");
        defaultMap.put("aifc", "audio/x-aiff");
        defaultMap.put("wav", "audio/x-wav");
        defaultMap.put("mpeg", "video/mpeg");
        defaultMap.put("mpg", "video/mpeg");
        defaultMap.put("mpe", "video/mpeg");
        defaultMap.put("qt", "video/quicktime");
        defaultMap.put("mov", "video/quicktime");
        defaultMap.put("avi", "video/x-msvideo");
        defaultMap.put("movie", "video/x-sgi-movie");
        defaultMap.put("avx", "video/x-rad-screenplay");
        defaultMap.put("wrl", "x-world/x-vrml");
        defaultMap.put("mpv2", "video/mpeg2");
    }
    

    private Hashtable map = new Hashtable();

    public void addContentType(String extn, String type) {
        map.put(extn, type.toLowerCase());
    }

    public Enumeration getExtensions() {
        return map.keys();
    }

    public String getContentType(String extn) {
        String type = (String)map.get(extn.toLowerCase());
	if( type == null ) type=(String)defaultMap.get( extn );
	return type;
    }

    public void removeContentType(String extn) {
        map.remove(extn.toLowerCase());
    }

    /** Get extension of file, without fragment id
     */
    public static String getExtension( String fileName ) {
        // play it safe and get rid of any fragment id
        // that might be there
	int length=fileName.length();
	
        int newEnd = fileName.lastIndexOf('#');
	if( newEnd== -1 ) newEnd=length;
	// Instead of creating a new string.
	//         if (i != -1) {
	//             fileName = fileName.substring(0, i);
	//         }
        int i = fileName.lastIndexOf('.', newEnd );
        if (i != -1) {
             return  fileName.substring(i + 1, newEnd );
        } else {
            // no extension, no content type
            return null;
        }
    }
    
    public String getContentTypeFor(String fileName) {
	String extn=getExtension( fileName );
        if (extn!=null) {
            return getContentType(extn);
        } else {
            // no extension, no content type
            return null;
        }
    }

}
