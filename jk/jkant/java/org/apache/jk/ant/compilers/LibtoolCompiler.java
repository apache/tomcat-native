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

package org.apache.jk.ant.compilers;

import java.io.File;
import java.util.Enumeration;
import java.util.Vector;

import org.apache.jk.ant.Source;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.types.Commandline;

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

