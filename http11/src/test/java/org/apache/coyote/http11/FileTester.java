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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Locale;

import org.apache.coyote.Adapter;
import org.apache.coyote.Processor;

/**
 * File tester.
 * <p>
 * This tester is initialized with an adapter (it will use the HTTP/1.1 
 * processor), and will then accept an input file (which will contain the 
 * input data), and an output file, to which the result of the request(s)
 * will be written.
 *
 * @author Remy Maucherat
 */
public class FileTester {


    // -------------------------------------------------------------- Constants


    // ----------------------------------------------------------- Constructors


    /**
     * Construct a new file tester using the two files specified. The contents
     * of the output file will be overwritten when the test is run.
     * 
     * @param adapter Coyote adapter to use for this test
     * @param processor Coyote processor to use for this test
     * @param inputFile File containing the HTTP requests in plain text
     * @param outputFile File containing the HTTP responses
     */
    public FileTester(Adapter adapter, Processor processor,
                      File inputFile, File outputFile) {

        processor.setAdapter(adapter);

        this.adapter = adapter;
        this.processor = processor;
        this.inputFile = inputFile;
        this.outputFile = outputFile;

    }


    // --------------------------------------------------------- Static Methods


    /**
     * Utility main method, which will use the HTTP/1.1 processor with the 
     * test adapter.
     *
     * @param args Command line arguments to be processed
     */
    public static void main(String args[])
        throws Exception {

        if (args.length < 2) {
            System.out.println("Incorrect number of arguments");
            return;
        }

        // The first argument is the input file
        File inputFile = new File(args[0]);

        // The second argument is the output file
        File outputFile = new File(args[1]);

        Adapter testAdapter = new RandomAdapter();
        Processor http11Processor = new Http11Processor();

        FileTester tester = new FileTester(testAdapter, http11Processor,
                                           inputFile, outputFile);
        tester.test();

    }


    // ----------------------------------------------------- Instance Variables


    /**
     * File containing the input data.
     */
    protected File inputFile;


    /**
     * File containing the output data.
     */
    protected File outputFile;


    /**
     * Coyote adapter to use.
     */
    protected Adapter adapter;


    /**
     * Coyote processor to use.
     */
    protected Processor processor;


    // --------------------------------------------------------- Public Methods


    /**
     * Process the test.
     */
    public void test()
        throws Exception {

        InputStream is = new FileInputStream(inputFile);
        OutputStream os = new FileOutputStream(outputFile);

        processor.process(is, os);

    }


}
