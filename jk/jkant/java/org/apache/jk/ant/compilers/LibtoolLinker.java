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
 * Link using libtool.
 * 
 * @author Costin Manolache
 */
public class LibtoolLinker extends SoTask implements LinkerAdapter {
    SoTask so;
    protected static GlobPatternMapper lo_mapper=new GlobPatternMapper();
    static {
	lo_mapper.setFrom("*.c");
	lo_mapper.setTo("*.lo");
    }
    public LibtoolLinker() {
	so=this;
    };

    public void setSoTask(SoTask so ) {
	this.so=so;
    }

    public void execute() throws BuildException {
	findSourceFiles();
	link(this.srcList);
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
	if(cc==null) cc="gcc";

	cmd.createArgument().setValue( cc );
	
	cmd.createArgument().setValue("-module");
	cmd.createArgument().setValue("-avoid-version");
	cmd.createArgument().setValue("-rpath");
	cmd.createArgument().setValue( buildDir.getAbsolutePath());

	cmd.createArgument().setValue( "-o" );
	cmd.createArgument().setValue( soFile + ".la" );

	if( profile )
	    cmd.createArgument().setValue("-pg" );

	// All .o files must be included
	project.log( "Linking " + buildDir + "/" + soFile + ".so");

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

