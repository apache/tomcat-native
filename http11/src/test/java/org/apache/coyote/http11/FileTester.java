/*
 * Copyright 1999,2004 The Apache Software Foundation.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */ 
package org.apache.coyote.http11;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.Socket;
import java.util.Locale;

import org.apache.coyote.Adapter;
import org.apache.coyote.ActionCode;
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
        Http11Processor http11Processor = new Http11Processor();
        http11Processor.setSocket(new Socket("127.0.0.1", 8080));
        http11Processor.action(ActionCode.ACTION_START, null);

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
