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

/*
 *
 * This class was originally written by Jason Hunter <jhunter@acm.org>
 * as part of the book "Java Servlet Programming" (O'Reilly).  
 * See http://www.servlets.com/book for more information.
 * Used by Sun Microsystems with permission.
 *
 */

package org.apache.tomcat.util.http;

import java.util.*;

/** 
 * A mapping to determine the (somewhat arbitrarily) preferred charset for 
 * a given locale.  Supports all locales recognized in JDK 1.1.
 * This class was originally written by Jason Hunter [jhunter@acm.org]
 * as part of the book "Java Servlet Programming" (O'Reilly).
 * See <a href="http://www.servlets.com/book">
 * http://www.servlets.com/book</a> for more information.
 * Used by Sun Microsystems with permission.
 */
public class LocaleToCharsetMap {

  private static Hashtable map;

  static {
    map = new Hashtable();

    map.put("ar", "ISO-8859-6");
    map.put("be", "ISO-8859-5");
    map.put("bg", "ISO-8859-5");
    map.put("ca", "ISO-8859-1");
    map.put("cs", "ISO-8859-2");
    map.put("da", "ISO-8859-1");
    map.put("de", "ISO-8859-1");
    map.put("el", "ISO-8859-7");
    map.put("en", "ISO-8859-1");
    map.put("es", "ISO-8859-1");
    map.put("et", "ISO-8859-1");
    map.put("fi", "ISO-8859-1");
    map.put("fr", "ISO-8859-1");
    map.put("hr", "ISO-8859-2");
    map.put("hu", "ISO-8859-2");
    map.put("is", "ISO-8859-1");
    map.put("it", "ISO-8859-1");
    map.put("iw", "ISO-8859-8");
    map.put("ja", "Shift_JIS");
    map.put("ko", "EUC-KR");     // Requires JDK 1.1.6
    map.put("lt", "ISO-8859-2");
    map.put("lv", "ISO-8859-2");
    map.put("mk", "ISO-8859-5");
    map.put("nl", "ISO-8859-1");
    map.put("no", "ISO-8859-1");
    map.put("pl", "ISO-8859-2");
    map.put("pt", "ISO-8859-1");
    map.put("ro", "ISO-8859-2");
    map.put("ru", "ISO-8859-5");
    map.put("sh", "ISO-8859-5");
    map.put("sk", "ISO-8859-2");
    map.put("sl", "ISO-8859-2");
    map.put("sq", "ISO-8859-2");
    map.put("sr", "ISO-8859-5");
    map.put("sv", "ISO-8859-1");
    map.put("tr", "ISO-8859-9");
    map.put("uk", "ISO-8859-5");
    map.put("zh", "GB2312");
    map.put("zh_TW", "Big5");

  }

  /**
   * Gets the preferred charset for the given locale, or null if the locale
   * is not recognized.
   *
   * @param loc the locale
   * @return the preferred charset
   */
  public static String getCharset(Locale loc) {
    String charset;

    // Try for an full name match (may include country)
    charset = (String) map.get(loc.toString());
    if (charset != null) return charset;

    // If a full name didn't match, try just the language
    charset = (String) map.get(loc.getLanguage());
    return charset;  // may be null
  }
}
