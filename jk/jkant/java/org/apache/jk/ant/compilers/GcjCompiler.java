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

import org.apache.jk.ant.Source;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.types.Commandline;

/**
 *  Compile using Gcj. This is ( even more ) experimental.
 * 
 * @author Costin Manolache
 */
public class GcjCompiler extends CcCompiler {
    
    public GcjCompiler() {
	super();
	co_mapper.setFrom("*.java");
	co_mapper.setTo("*.o");
    }

    public String[] getTargetFiles( Source src ) {
        File srcFile = src.getFile();
        String name=srcFile.getName();
        if( name.endsWith( ".java" ) ) {
            return co_mapper.mapFileName( name );
        } else {
            return new String[]
            { name + ".o" };
        }
    }

    
    /** Compile using libtool.
     */
    public void compileSingleFile(Source sourceObj) throws BuildException {
	File f=sourceObj.getFile();
	String source=f.toString();
	Commandline cmd = new Commandline();

	cmd.setExecutable( "gcj" );

	cmd.createArgument().setValue("-c" );
	
	if( optG ) {
            //	    cmd.createArgument().setValue("-g" );
            cmd.createArgument().setValue("-ggdb3" );
            //            cmd.createArgument().setValue("-save-temps" );
	    //  cmd.createArgument().setValue("-Wall");
	}
	addOptimize( cmd );
	addExtraFlags( cmd );
	cmd.createArgument().setValue("-fPIC" );
	addIncludes( cmd );
        String targetDir=sourceObj.getPackage();
	try {
	    File f1=new File( buildDir, targetDir );
	    f1.mkdirs();
            cmd.createArgument().setValue( "-o" );
            String targetO[]=getTargetFiles( sourceObj );
            if( targetO==null ) {
                log("no target for " + sourceObj.getFile() );
                return;
            }
            File ff=new File( f1, targetO[0]);
            cmd.createArgument().setValue( ff.toString() );
	} catch( Exception ex ) {
	    ex.printStackTrace();
	}
	
        if( ! source.endsWith(".java") ) {
            cmd.createArgument().setValue("-R" );
            cmd.createArgument().setValue( targetDir + "/" + f.getName() );
            //System.out.println("XXX resource " + targetDir + "/" + f.getName() );
        } else {
            //            cmd.createArgument().setValue("-fno-bounds-check" );
        }
        cmd.createArgument().setValue( source );
	project.log( "Compiling " + source);

	if( debug > 0 )
	    project.log( "Command: " + cmd ); 
	int result=execute( cmd );
	if( result!=0 ) {
	    displayError( result, source, cmd );
	}
	closeStreamHandler();
    }

    protected void addIncludes(Commandline cmd) {
	String [] includeList = ( includes==null ) ?
	    new String[] {} : includes.getIncludePatterns(project); 
	for( int i=0; i<includeList.length; i++ ) {
	    cmd.createArgument().setValue("-I" + includeList[i] );
	}
    }


}

