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
    public static final boolean DEFAULT_TCP_NO_DELAY = true;
    
    
    /**
     * Server string.
     */
    public static final byte[] SERVER_BYTES = convertToBytes("Apache-Coyote/1.1");

    
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

    
    /* Various constant "strings" */
    public static final byte[] CRLF_BYTES = convertToBytes(CRLF);
    public static final byte[] COLON_BYTES = convertToBytes(": ");
    public static final byte[] CONNECTION_BYTES = convertToBytes("Connection");
    public static final byte[] CLOSE_BYTES = convertToBytes("close");
    public static final byte[] KEEPALIVE_BYTES = convertToBytes("keep-alive");
    public static final byte[] CHUNKED_BYTES = convertToBytes("chunked");
    public static final byte[] ACK_BYTES = convertToBytes("HTTP/1.1 100 Continue" + CRLF + CRLF);
    public static final byte[] TRANSFERENCODING_BYTES = convertToBytes("Transfer-Encoding");
    

    /**
     * Identity filters (input and output).
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
     * GZIP filter (output).
     */
    public static final int GZIP_FILTER = 3;


    /**
     * Buffered filter (input)
     */
    public static final int BUFFERED_FILTER = 3;


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
     * Utility method.
     * 
     * @param value to convert to byte array
     * @return the byte array value
     */
    public static final byte[] convertToBytes(String value) {
        byte[] result = new byte[value.length()];
        for (int i = 0; i < value.length(); i++) {
            result[i] = (byte) value.charAt(i);
        }
        return result;
    }
    
}
