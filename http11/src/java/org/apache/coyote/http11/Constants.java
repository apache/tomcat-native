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
package org.apache.coyote.http11;


/**
 * Constants.
 *
 * @author Remy Maucherat
 */
public final class Constants {


    // -------------------------------------------------------------- Constants


    /**
     * Package name.
     */
    public static final String Package = "org.apache.coyote.http11";

    public static final int DEFAULT_CONNECTION_LINGER = -1;
    public static final int DEFAULT_CONNECTION_TIMEOUT = 60000;
    public static final int DEFAULT_CONNECTION_UPLOAD_TIMEOUT = 300000;
    public static final int DEFAULT_SERVER_SOCKET_TIMEOUT = 0;
    
    /**
     * Server string.
     */
    public static final String SERVER = "Apache-Coyote/1.1";


    /**
     * CR.
     */
    public static final byte CR = (byte) '\r';


    /**
     * LF.
     */
    public static final byte LF = (byte) '\n';


    /**
     * SP.
     */
    public static final byte SP = (byte) ' ';


    /**
     * HT.
     */
    public static final byte HT = (byte) '\t';


    /**
     * COLON.
     */
    public static final byte COLON = (byte) ':';


    /**
     * 'A'.
     */
    public static final byte A = (byte) 'A';


    /**
     * 'a'.
     */
    public static final byte a = (byte) 'a';


    /**
     * 'Z'.
     */
    public static final byte Z = (byte) 'Z';


    /**
     * '?'.
     */
    public static final byte QUESTION = (byte) '?';


    /**
     * Lower case offset.
     */
    public static final byte LC_OFFSET = A - a;


    /**
     * Default HTTP header buffer size.
     */
    public static final int DEFAULT_HTTP_HEADER_BUFFER_SIZE = 48 * 1024;


    /**
     * CRLF.
     */
    public static final String CRLF = "\r\n";


    /**
     * CRLF bytes.
     */
    public static final byte[] CRLF_BYTES = {(byte) '\r', (byte) '\n'};


    /**
     * Colon bytes.
     */
    public static final byte[] COLON_BYTES = {(byte) ':', (byte) ' '};


    /**
     * Close bytes.
     */
    public static final byte[] CLOSE_BYTES = {
        (byte) 'c',
        (byte) 'l',
        (byte) 'o',
        (byte) 's',
        (byte) 'e'
    };


    /**
     * Keep-alive bytes.
     */
    public static final byte[] KEEPALIVE_BYTES = {
        (byte) 'k',
        (byte) 'e',
        (byte) 'e',
        (byte) 'p',
        (byte) '-',
        (byte) 'a',
        (byte) 'l',
        (byte) 'i',
        (byte) 'v',
        (byte) 'e'
    };


    /**
     * Indetity filters (input and output).
     */
    public static final int IDENTITY_FILTER = 0;


    /**
     * Chunked filters (input and output).
     */
    public static final int CHUNKED_FILTER = 1;


    /**
     * Void filters (input and output).
     */
    public static final int VOID_FILTER = 2;


    /**
     * GZIP filters (input and output).
     */
    public static final int GZIP_FILTER = 3;


    /**
     * HTTP/1.0.
     */
    public static final String HTTP_10 = "HTTP/1.0";


    /**
     * HTTP/1.1.
     */
    public static final String HTTP_11 = "HTTP/1.1";


    /**
     * GET.
     */
    public static final String GET = "GET";


    /**
     * HEAD.
     */
    public static final String HEAD = "HEAD";


    /**
     * POST.
     */
    public static final String POST = "POST";


    /**
     * Ack string when pipelining HTTP requests.
     */
    public static final byte[] ACK_BYTES = {
        (byte) 'H',
        (byte) 'T',
        (byte) 'T',
        (byte) 'P',
        (byte) '/',
        (byte) '1',
        (byte) '.',
        (byte) '1',
        (byte) ' ',
        (byte) '1',
        (byte) '0',
        (byte) '0',
        (byte) ' ',
        (byte) 'C',
        (byte) 'o',
        (byte) 'n',
        (byte) 't',
        (byte) 'i',
        (byte) 'n',
        (byte) 'u',
        (byte) 'e',
        (byte) '\r',
        (byte) '\n',
        (byte) '\r',
        (byte) '\n'
    };


}
