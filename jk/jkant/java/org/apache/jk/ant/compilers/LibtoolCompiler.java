/*
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
 * 4. The names "The Jakarta Project", "Ant", and "Apache Software
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
 */

package org.apache.jk.ant.compilers;

import org.apache.tools.ant.types.*;
import org.apache.tools.ant.util.*;
import org.apache.tools.ant.taskdefs.*;
import org.apache.tools.ant.*;
import org.apache.jk.ant.*;

import java.io.*;
import java.util.*;

/**
 *  Compile using libtool.
 * 
 *  It extends SoTask so we can debug it or use it independently of <so>.
 *  For normal use you should use the generic task, and system-specific
 *  properties to choose the right compiler plugin ( like we select
 *  jikes ).
 *
 * @author Costin Manolache
 */
public class LibtoolCompiler extends CcCompiler {

    public LibtoolCompiler() {
	super();
    };

    public void compile(Vector sourceFiles ) throws BuildException {
        compileList=findCompileList(sourceFiles);
        
        log("Compiling " + compileList.size() + " out of " + sourceFiles.size());
	Enumeration en=compileList.elements();
	while( en.hasMoreElements() ) {
	    Source source=(Source)en.nextElement();
	    compileSingleFile(source);
	}
    }
    

    /** Compile using libtool.
     */
    public void compileSingleFile(Source sourceObj) throws BuildException {
	File f=sourceObj.getFile();
	String source=f.toString();
	Commandline cmd = new Commandline();

	String libtool=project.getProperty("build.native.libtool");
	if(libtool==null) libtool="libtool";

	cmd.setExecutable( libtool );
	
	cmd.createArgument().setValue("--mode=compile");

	String cc=project.getProperty("build.native.cc");
	if(cc==null) cc="cc";

	cmd.createArgument().setValue( cc );

	cmd.createArgument().setValue( "-c" );
	
	cmd.createArgument().setValue( "-o" );
	
	File ff=new File( buildDir, sourceObj.getTargetFile(co_mapper));
	cmd.createArgument().setValue( ff.toString() );
	try {
	    String targetDir=sourceObj.getPackage();
	    File f1=new File( buildDir, targetDir );
	    f1.mkdirs();
	    // System.out.println("mkdir " + f1 );
	} catch( Exception ex ) {
	    ex.printStackTrace();
	}

	addIncludes(cmd);
	addExtraFlags( cmd );
	addDebug(cmd);
	addDefines( cmd );
	addOptimize( cmd );
	addProfile( cmd );

	project.log( "Compiling " + source);
	cmd.createArgument().setValue( source );

        if( debug > 0 ) project.log(cmd.toString());
	int result=execute( cmd );
        displayError( result, source, cmd );
	closeStreamHandler();
    }
}

