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

package org.apache.coyote.tomcat4;

import java.io.IOException;

import javax.servlet.ServletOutputStream;

/**
 * Coyote implementation of the servlet output stream.
 * 
 * @author Costin Manolache
 * @author Remy Maucherat
 */
final class CoyoteOutputStream 
    extends ServletOutputStream {


    // ----------------------------------------------------- Instance Variables


    protected OutputBuffer ob;


    // ----------------------------------------------------------- Constructors


    protected CoyoteOutputStream(OutputBuffer ob) {
        this.ob = ob;
    }


    // --------------------------------------------------- OutputStream Methods


    public void write(int i)
        throws IOException {
        ob.writeByte(i);
    }


    public void write(byte[] b)
        throws IOException {
        write(b, 0, b.length);
    }


    public void write(byte[] b, int off, int len)
        throws IOException {
        ob.write(b, off, len);
    }


    /**
     * Will send the buffer to the client.
     */
    public void flush()
        throws IOException {
        ob.flush();
    }


    public void close()
        throws IOException {
        ob.close();
    }


    // -------------------------------------------- ServletOutputStream Methods


    public void print(String s)
        throws IOException {
        ob.write(s);
    }


}

