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
import java.io.OutputStream;

import org.apache.ajp.Ajp13;

public class Ajp13OutputStream extends OutputStream {

    private Ajp13 ajp13;
    
    Ajp13OutputStream(Ajp13 ajp13) {
        this.ajp13 = ajp13;
    }

    public void write(int b) throws IOException {
        byte[] bb = new byte[]{(byte)b};
        ajp13.doWrite(bb, 0, 1);
    }

    public void write(byte[] b, int off, int len) throws IOException {
        ajp13.doWrite(b, off, len);
    }

    public void close() throws IOException {
    }

    public void flush() throws IOException {
    }
}
