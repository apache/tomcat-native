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
import org.apache.tools.ant.util.GlobPatternMapper;

/**
 *  Compile using Gcc.
 *
 * @author Costin Manolache
 */
public class CcCompiler extends CompilerAdapter {
    GlobPatternMapper co_mapper=new GlobPatternMapper();

    public CcCompiler() {
	super();
	co_mapper.setFrom("*.c");
	co_mapper.setTo("*.o");
    }

    public String[] getTargetFiles( Source src ) {
        File srcFile = src.getFile();
        String name=srcFile.getName();
        
        return co_mapper.mapFileName( name );
    }
    
    String cc;
    
    /** Compile  using 'standard' gcc flags. This assume a 'current' gcc on
     *  a 'normal' platform - no need for libtool
     */
    public void compileSingleFile(Source sourceObj) throws BuildException {
	File f=sourceObj.getFile();
	String source=f.toString();
	Commandline cmd = new Commandline();

	cc=project.getProperty("build.native.cc");
	if(cc==null) cc="cc";
	
	cmd.setExecutable( cc );

	cmd.createArgument().setValue( "-c" );

	addIncludes(cmd);
	addExtraFlags( cmd );
	addDebug(cmd);
	addDefines( cmd );
	addOptimize( cmd );
	addProfile( cmd );

	cmd.createArgument().setValue( source );

	project.log( "Compiling " + source);

	int result=execute( cmd );
        displayError( result, source, cmd );
	closeStreamHandler();
    }
    protected void addDebug(Commandline cmd) {
	if( optG ) {
	    cmd.createArgument().setValue("-g" );
        }

        if( optWgcc ) {
	    if( ! "HP-UX".equalsIgnoreCase( System.getProperty( "os.name" )) ) {
                // HP-UX uses -W for some other things
                cmd.createArgument().setValue("-W");
            }

            if( cc!= null && cc.indexOf( "gcc" ) >= 0 ) {
                //cmd.createArgument().setValue("-Wall");
                cmd.createArgument().setValue("-Wimplicit");
                cmd.createArgument().setValue("-Wreturn-type");
                cmd.createArgument().setValue("-Wcomment");
                cmd.createArgument().setValue("-Wformat");
                cmd.createArgument().setValue("-Wchar-subscripts");
                cmd.createArgument().setValue("-O");
                cmd.createArgument().setValue("-Wuninitialized");
                
                // Non -Wall
                // 	    cmd.createArgument().setValue("-Wtraditional");
                // 	    cmd.createArgument().setValue("-Wredundant-decls");
                cmd.createArgument().setValue("-Wmissing-declarations");
                cmd.createArgument().setValue("-Wmissing-prototypes");
                //	    cmd.createArgument().setValue("-Wconversions");
                cmd.createArgument().setValue("-Wcast-align");
                // 	    cmd.createArgument().setValue("-pedantic" );
            }
	}
    }
    protected void addOptimize( Commandline cmd ) {
	if( optimize )
	    cmd.createArgument().setValue("-O3" );
    }

    protected void addProfile( Commandline cmd ) {
	if( profile ) {
	    cmd.createArgument().setValue("-pg" );
	    // bb.in 
	    // cmd.createArgument().setValue("-ax" );
	}
    }


}

