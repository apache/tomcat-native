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

package org.apache.tomcat.util.buf;

import java.text.DateFormat;
import java.text.FieldPosition;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;

import org.apache.tomcat.util.res.StringManager;

/**
 *  Common place for date utils.
 *
 * @deprecated Will be replaced with a more efficient impl, based on
 * FastDateFormat, with an API using less objects.
 * @author dac@eng.sun.com
 * @author Jason Hunter [jch@eng.sun.com]
 * @author James Todd [gonzo@eng.sun.com]
 * @author Costin Manolache
 */
public class DateTool {

    /** US locale - all HTTP dates are in english
     */
    private final static Locale LOCALE_US = Locale.US;

    /** GMT timezone - all HTTP dates are on GMT
     */
    public final static TimeZone GMT_ZONE = TimeZone.getTimeZone("GMT");

    /** format for RFC 1123 date string -- "Sun, 06 Nov 1994 08:49:37 GMT"
     */
    public final static String RFC1123_PATTERN =
        "EEE, dd MMM yyyy HH:mm:ss z";

    // format for RFC 1036 date string -- "Sunday, 06-Nov-94 08:49:37 GMT"
    public final static String rfc1036Pattern =
        "EEEEEEEEE, dd-MMM-yy HH:mm:ss z";

    // format for C asctime() date string -- "Sun Nov  6 08:49:37 1994"
    public final static String asctimePattern =
        "EEE MMM d HH:mm:ss yyyy";

    /** Pattern used for old cookies
     */
    private final static String OLD_COOKIE_PATTERN = "EEE, dd-MMM-yyyy HH:mm:ss z";

    /** DateFormat to be used to format dates. Called from MessageBytes
     */
    private final static DateFormat rfc1123Format =
	new SimpleDateFormat(RFC1123_PATTERN, LOCALE_US);
    
    /** DateFormat to be used to format old netscape cookies
	Called from ServerCookie
     */
    private final static DateFormat oldCookieFormat =
	new SimpleDateFormat(OLD_COOKIE_PATTERN, LOCALE_US);
    
    private final static DateFormat rfc1036Format =
	new SimpleDateFormat(rfc1036Pattern, LOCALE_US);
    
    private final static DateFormat asctimeFormat =
	new SimpleDateFormat(asctimePattern, LOCALE_US);
    
    static {
	rfc1123Format.setTimeZone(GMT_ZONE);
	oldCookieFormat.setTimeZone(GMT_ZONE);
	rfc1036Format.setTimeZone(GMT_ZONE);
	asctimeFormat.setTimeZone(GMT_ZONE);
    }
 
    private static String rfc1123DS;
    private static long   rfc1123Sec;

    private static StringManager sm =
        StringManager.getManager("org.apache.tomcat.util.buf.res");

    // Called from MessageBytes.getTime()
    static long parseDate( MessageBytes value ) {
     	return parseDate( value.toString());
    }

    // Called from MessageBytes.setTime
    /** 
     */
    public static String format1123( Date d ) {
	String dstr=null;
	synchronized(rfc1123Format) {
	    dstr = format1123(d, rfc1123Format);
	}
	return dstr;
    } 

    public static String format1123( Date d,DateFormat df ) {
        long dt = d.getTime() / 1000;
        if ((rfc1123DS != null) && (dt == rfc1123Sec))
            return rfc1123DS;
        rfc1123DS  = df.format( d );
        rfc1123Sec = dt;
        return rfc1123DS;
    } 


    // Called from ServerCookie
    /** 
     */
    public static void formatOldCookie( Date d, StringBuffer sb,
					  FieldPosition fp )
    {
	synchronized(oldCookieFormat) {
	    oldCookieFormat.format( d, sb, fp );
	}
    }

    // Called from ServerCookie
    public static String formatOldCookie( Date d )
    {
	String ocf=null;
	synchronized(oldCookieFormat) {
	    ocf= oldCookieFormat.format( d );
	}
	return ocf;
    }

    
    /** Called from HttpServletRequest.getDateHeader().
	Not efficient - but not very used.
     */
    public static long parseDate( String dateString ) {
	DateFormat [] format = {rfc1123Format,rfc1036Format,asctimeFormat};
	return parseDate(dateString,format);
    }
    public static long parseDate( String dateString, DateFormat []format ) {
	Date date=null;
	for(int i=0; i < format.length; i++) {
	    try {
		date = format[i].parse(dateString);
		return date.getTime();
	    } catch (ParseException e) { }
	    catch (StringIndexOutOfBoundsException e) { }
	}
	String msg = sm.getString("httpDate.pe", dateString);
	throw new IllegalArgumentException(msg);
    }

}
