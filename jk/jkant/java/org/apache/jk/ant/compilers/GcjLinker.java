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

import org.apache.jk.ant.JkData;
import org.apache.jk.ant.SoTask;
import org.apache.jk.ant.Source;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.types.Commandline;
import org.apache.tools.ant.util.GlobPatternMapper;

/**
 * Link java using gcj.
 * 
 * @author Costin Manolache
 */
public class GcjLinker extends LinkerAdapter {
    SoTask so;
    protected static GlobPatternMapper co_mapper=new GlobPatternMapper();
    static {
	co_mapper.setFrom("*.java");
	co_mapper.setTo("*.o");
    }
    public GcjLinker() {
	so=this;
    }

    public String[] getTargetFiles( Source src ) {
        File srcFile = src.getFile();
        String name=srcFile.getName();
        if( name.endsWith( ".java" ) )
            return co_mapper.mapFileName( name );
        else
            return new String[]
            { name + ".o" };
    }


    
    public void setSoTask(SoTask so ) {
	this.so=so;
	so.duplicateTo( this );
    }

    public void execute() throws BuildException {
	findSourceFiles();
	link(this.srcList);
    }

    /** Link using libtool.
     */
    public boolean link(Vector srcList) throws BuildException {
//         link( srcList, false );
        link( srcList, true );
	return true;
    }
    public boolean link(Vector srcList, boolean shared) throws BuildException {
	Commandline cmd = new Commandline();

	cmd.setExecutable( "gcj" );

        if( shared )
            cmd.createArgument().setValue( "--shared" );
	cmd.createArgument().setValue( "-o" );
        if( shared )
            cmd.createArgument().setValue( soFile + ".so" );
        else
            cmd.createArgument().setValue( soFile + ".a" );

	project.log( "Linking " + buildDir + "/" + soFile + ".so");

        // write out any additional link options
        Enumeration opts = linkOpts.elements();
        while( opts.hasMoreElements() ) {
            JkData opt = (JkData) opts.nextElement();
            String option = opt.getValue();
            if( option == null ) continue;

            cmd.createArgument().setValue( option );
        }

	for( int i=0; i<srcList.size(); i++ ) {
	    Source source=(Source)srcList.elementAt(i);
	    File f1=new File(buildDir, source.getPackage());
	    File srcF=new File(f1, getTargetFiles(source)[0]);
	    cmd.createArgument().setValue( srcF.toString() );
	}
	
	int result=execute( cmd );
	if( result!=0 ) {
	    log("Link failed " + result );
	    log("Command:" + cmd.toString());
	    log("Output:" );
	    if( outputstream!=null ) 
		log( outputstream.toString());
	    log("StdErr:" );
	    if( errorstream!=null ) 
		log( errorstream.toString());
	    
	    throw new BuildException("Link failed " + soFile);
	}
	closeStreamHandler();

	return true;
    }
}

