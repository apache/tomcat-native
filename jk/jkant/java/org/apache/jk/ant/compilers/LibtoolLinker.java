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
 * Link using libtool.
 * 
 * @author Costin Manolache
 */
public class LibtoolLinker extends LinkerAdapter {
    SoTask so;
    GlobPatternMapper lo_mapper=new GlobPatternMapper();
    public LibtoolLinker() {
	so=this;
	lo_mapper.setFrom("*.c");
	lo_mapper.setTo("*.lo");
    }

    /** Link using libtool.
     */
    public boolean link(Vector srcList) throws BuildException {
	so.duplicateTo( this );
	Commandline cmd = new Commandline();

	String libtool=project.getProperty("build.native.libtool");
	if(libtool==null) libtool="libtool";

	cmd.setExecutable( libtool );
	
	cmd.createArgument().setValue("--mode=link");

	String cc=project.getProperty("build.native.cc");
	if(cc==null) cc="cc";

	cmd.createArgument().setValue( cc );
	
	cmd.createArgument().setValue("-module");
	cmd.createArgument().setValue("-avoid-version");
	cmd.createArgument().setValue("-rpath");
	cmd.createArgument().setValue( buildDir.getAbsolutePath());

	cmd.createArgument().setValue( "-o" );
	cmd.createArgument().setValue( soFile + ".la" );

	if( profile )
	    cmd.createArgument().setValue("-pg" );

        // write out any additional link options
        Enumeration opts = linkOpts.elements();
        while( opts.hasMoreElements() ) {
            JkData opt = (JkData) opts.nextElement();
            String option = opt.getValue();
            if( option == null ) continue;

            cmd.createArgument().setValue( option );
        }
        
	// All .o files must be included
	project.log( "Linking " + buildDir + "/" + soFile + ".so");

        if( libs!=null ) {
            String libsA[]=libs.list(); 
            for( int i=0; i< libsA.length; i++ ) {
                cmd.createArgument().setValue( "-l" + libsA[i] );
                //XXX debug
                project.log("XXX -l" + libsA[i] );
            }
        }
	
	for( int i=0; i<srcList.size(); i++ ) {
	    Source sourceObj=(Source)srcList.elementAt(i);
	    
	    File ff=new File( buildDir, sourceObj.getTargetFile(lo_mapper));
	    cmd.createArgument().setValue( ff.toString() );
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

	executeLibtoolInstall();
	return true;
    }

    /** Final step using libtool.
     */
    private void executeLibtoolInstall() throws BuildException {
	Commandline cmd = new Commandline();

	String libtool=project.getProperty("build.native.libtool");
	if(libtool==null) libtool="libtool";

	cmd.setExecutable( libtool );
	
	cmd.createArgument().setValue("--mode=install");

	cmd.createArgument().setValue( "cp" );

	File laFile=new File( buildDir, soFile + ".la" );
	cmd.createArgument().setValue( laFile.getAbsolutePath());
	
	File soFileF=new File( buildDir, soFile + ".so" );
	cmd.createArgument().setValue( soFileF.getAbsolutePath());

	int result=execute( cmd );
	if( result!=0 ) {
	    log("Link/install failed " + result );
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
    }    
}

