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


package org.apache.coyote.tomcat5;

import java.io.BufferedReader;
import java.io.IOException;


/**
 * Coyote implementation of the buffred reader.
 * 
 * @author Remy Maucherat
 */
final class CoyoteReader
    extends BufferedReader {


    // -------------------------------------------------------------- Constants


    private static final char[] LINE_SEP = { '\r', '\n' };
    private static final int MAX_LINE_LENGTH = 4096;


    // ----------------------------------------------------- Instance Variables


    protected InputBuffer ib;


    protected final char[] lineBuffer = new char[MAX_LINE_LENGTH];


    // ----------------------------------------------------------- Constructors


    public CoyoteReader(InputBuffer ib) {
        super(ib);
        this.ib = ib;
    }


    // --------------------------------------------------------- Reader Methods


    public void close()
        throws IOException {
        ib.close();
    }


    public int read()
        throws IOException {
        return ib.read();
    }


    public int read(char[] cbuf)
        throws IOException {
        return ib.read(cbuf, 0, cbuf.length);
    }


    public int read(char[] cbuf, int off, int len)
        throws IOException {
        return ib.read(cbuf, off, len);
    }


    public long skip(long n)
        throws IOException {
        return ib.skip(n);
    }


    public boolean ready()
        throws IOException {
        return ib.ready();
    }


    public boolean markSupported() {
        return true;
    }


    public void mark(int readAheadLimit)
        throws IOException {
        ib.mark(readAheadLimit);
    }


    public void reset()
        throws IOException {
        ib.reset();
    }


    public String readLine()
        throws IOException {

        String result = null;

        int pos = 0;
        int end = -1;
        int skip = -1;
        StringBuffer aggregator = null;
        while (end < 0) {
            mark(MAX_LINE_LENGTH);
            while ((pos < MAX_LINE_LENGTH) && (end < 0)) {
                int nRead = read(lineBuffer, pos, MAX_LINE_LENGTH - pos);
                if (nRead < 0) {
                    if (pos == 0) {
                        return null;
                    }
                    end = pos;
                    skip = pos;
                }
                for (int i = pos; (i < (pos + nRead)) && (end < 0); i++) {
                    if (lineBuffer[i] == LINE_SEP[0]) {
                        end = i;
                        skip = i + 1;
                        char nextchar;
                        if (i == (pos + nRead - 1)) {
                            nextchar = (char) read();
                        } else {
                            nextchar = lineBuffer[i+1];
                        }
                        if (nextchar == LINE_SEP[1]) {
                            skip++;
                        }
                    } else if (lineBuffer[i] == LINE_SEP[1]) {
                        end = i;
                        skip = i + 1;
                    }
                }
                if (nRead > 0) {
                    pos += nRead;
                }
            }
            if (end < 0) {
                if (aggregator == null) {
                    aggregator = new StringBuffer();
                }
                aggregator.append(lineBuffer);
                reset();
                skip(MAX_LINE_LENGTH);
            } else {
                reset();
                skip(skip);
            }
        }

        if (aggregator == null) {
            result = new String(lineBuffer, 0, end);
        } else {
            aggregator.append(lineBuffer, 0, end);
            result = aggregator.toString();
        }

        return result;

    }


}
