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

package org.apache.ajp.tomcat4;

import java.io.IOException;

import javax.servlet.ServletInputStream;

import org.apache.ajp.Ajp13;

public class Ajp13InputStream extends ServletInputStream {

    private Ajp13 ajp13;

    Ajp13InputStream(Ajp13 ajp13) {
        this.ajp13 = ajp13;
    }

    public int available() throws IOException {
        return ajp13.available();
    }

    public void close() throws IOException {
    }

    public void mark(int readLimit) {
    }

    public boolean markSupported() {
        return false;
    }

    public void reset() throws IOException {
        throw new IOException("reset() not supported");
    }

    public int read() throws IOException {
        return ajp13.doRead();
    }

    public int read(byte[] b, int off, int len) throws IOException {
        return ajp13.doRead(b, off, len);
    }

    public long skip(long n) throws IOException {
        if (n > Integer.MAX_VALUE) {
            throw new IOException("can't skip than many:  " + n);
        }
        byte[] b = new byte[(int)n];
        return ajp13.doRead(b, 0, b.length);
    }
}
