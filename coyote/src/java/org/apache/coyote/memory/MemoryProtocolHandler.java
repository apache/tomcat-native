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
package org.apache.coyote.memory;

import java.io.IOException;

import org.apache.tomcat.util.buf.ByteChunk;

import org.apache.coyote.Adapter;
import org.apache.coyote.InputBuffer;
import org.apache.coyote.OutputBuffer;
import org.apache.coyote.ProtocolHandler;
import org.apache.coyote.Request;
import org.apache.coyote.Response;


/**
 * Abstract the protocol implementation, including threading, etc.
 * Processor is single threaded and specific to stream-based protocols,
 * will not fit Jk protocols like JNI.
 *
 * @author Remy Maucherat
 */
public class MemoryProtocolHandler
    implements ProtocolHandler {


    // ------------------------------------------------------------- Properties


    /**
     * Pass config info.
     */
    public void setAttribute(String name, Object value) {
    }

    public Object getAttribute(String name) {
        return null;
    }


    /**
     * Associated adapter.
     */
    protected Adapter adapter = null;

    /**
     * The adapter, used to call the connector.
     */
    public void setAdapter(Adapter adapter) {
        this.adapter = adapter;
    }

    public Adapter getAdapter() {
        return (adapter);
    }


    /**
     * Hook to easily retrieve the protocol handler.
     */
    protected static MemoryProtocolHandler protocolHandler = null;

    public static MemoryProtocolHandler getProtocolHandler() {
        return protocolHandler;
    }


    // ------------------------------------------------ ProtocolHandler Methods


    /**
     * Init the protocol.
     */
    public void init()
        throws Exception {
        protocolHandler = this;
    }


    /**
     * Start the protocol.
     */
    public void start()
        throws Exception {
    }


    public void pause() 
        throws Exception {
    }

    public void resume() 
        throws Exception {
    }

    public void destroy()
        throws Exception {
        protocolHandler = null;
    }


    // --------------------------------------------------------- Public Methods


    /**
     * Process specified request.
     */
    public void process(Request request, ByteChunk input,
                        Response response, ByteChunk output)
        throws Exception {

        InputBuffer inputBuffer = new ByteChunkInputBuffer(input);
        OutputBuffer outputBuffer = new ByteChunkOutputBuffer(output);
        request.setInputBuffer(inputBuffer);
        response.setOutputBuffer(outputBuffer);

        adapter.service(request, response);

    }


    // --------------------------------------------- ByteChunkInputBuffer Class


    protected class ByteChunkInputBuffer
        implements InputBuffer {

        protected ByteChunk input = null;

        public ByteChunkInputBuffer(ByteChunk input) {
            this.input = input;
        }

        public int doRead(ByteChunk chunk, Request request) 
            throws IOException {
            return input.substract(chunk);
        }

    }


    // -------------------------------------------- ByteChunkOuptutBuffer Class


    protected class ByteChunkOutputBuffer
        implements OutputBuffer {

        protected ByteChunk output = null;

        public ByteChunkOutputBuffer(ByteChunk output) {
            this.output = output;
        }

        public int doWrite(ByteChunk chunk, Response response) 
            throws IOException {
            output.append(chunk);
            return chunk.getLength();
        }

    }


}
