/*
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2001 The Apache Software Foundation.  All rights
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
import org.apache.tools.ant.BuildException;
import org.apache.jk.ant.*;
import org.apache.tools.ant.taskdefs.*;
import org.apache.tools.ant.*;

import java.util.*;
import java.io.*;

/* Modeled after javac
 */

/**
 * s/javac/C compiler/
 *
 * The interface that all compiler adapters must adher to.  
 *
 * <p>A compiler adapter is an adapter that interprets the javac's
 * parameters in preperation to be passed off to the compier this
 * adapter represents.  As all the necessary values are stored in the
 * Javac task itself, the only thing all adapters need is the javac
 * task, the execute command and a parameterless constructor (for
 * reflection).</p>
 *
 * @author Jay Dickon Glanville <a href="mailto:jayglanville@home.com">jayglanville@home.com</a>
 * @author Costin Manolache
 */
public abstract class CompilerAdapter extends SoTask {
    SoTask so;
    Vector compileList;

    public CompilerAdapter() {
	so=this;
    };

    public void setSoTask(SoTask so ) {
	this.so=so;
	so.duplicateTo( this );
    }

//     /** @deprecated
//      */
//     public GlobPatternMapper getOMapper() {
//         return null;
//     }

    /** Return the files that depend on a src file.
     *  The first item should be the .o file used for linking.
     */
    public abstract String[] getTargetFiles( Source src );

    public void execute() throws BuildException {
        super.findSourceFiles();
        Vector compileList=findCompileList(srcList);
	compile( compileList );
    }

    /** Verify if a .c file needs compilation.
     *	As with javac, we assume a fixed build structure, where all .o
     *	files are in a separate directory from the sources ( no mess ).
     *
     *  XXX Hack makedepend somehow into this.
     */
    public boolean needCompile( Source source ) {
	// For each .c file we'll have a .o file in the build dir,
	// with the same name.
	File srcF = source.getFile();
	if( !srcF.exists() ) {
            if( debug > 0 )
                log("No source file " + srcF ); 
            return false;
        }

	String targetNames[]= getTargetFiles( source );
	if( targetNames==null || targetNames.length==0 ) {
            if( debug > 0 )
                log("No target files " + srcF ); 
	    return true; // strange, probably different extension ?
        }
        String targetName=targetNames[0];

        String targetDir=source.getPackage();
        File f1=new File( buildDir, targetDir );
        File target=new File( f1, targetName );
	//	System.out.println("XXX " + target );
	if( ! target.exists() ) {
            if( debug > 0 )
                log("Target doesn't exist " + target ); 
	    return true;
        }
	if( oldestO > target.lastModified() ) {
	    oldestO=target.lastModified();
	    oldestOFile=target;
	}
	if( srcF.lastModified() > target.lastModified() ) 
	    return true;

	if( debug > 0 )
	    log("No need to compile " + srcF + " target " + target ); 
	return false;
    }

    /** Remove all generated files, cleanup
     */
    public void removeOFiles( Vector srcList ) {
        for (int i = 0; i < srcList.size(); i++) {
            //            log( "Checking " + (Source)srcList.elementAt(i));
            Source source=(Source)srcList.elementAt(i);
	    String targetNA[]=getTargetFiles(source);
	    if( targetNA==null )
		continue;
            String targetDir=source.getPackage();
            File f1=new File( buildDir, targetDir );
            for( int j=0; j<targetNA.length; j++ ) {
                File target=new File( f1, targetNA[j] );
                // Check the dependency
                if( target.exists() ) {
                    // Remove it - we'll do a full build
                    target.delete();
                    log("Removing " + target );
                }
            }
        }
    }

    // XXX modified as side-effect of checking files with needCompile()
    long oldestO=System.currentTimeMillis();
    File oldestOFile=null;

    /** Find the subset of the source list that needs compilation.
     */
    protected Vector findCompileList(Vector srcList) throws BuildException {
        Vector compileList=new Vector();

        for (int i = 0; i < srcList.size(); i++) {
	    Source source=(Source)srcList.elementAt(i);
	    File srcFile=source.getFile();
	   
            if (!srcFile.exists()) {
                throw new BuildException("Source \"" + srcFile.getPath() +
                                         "\" does not exist!", location);
            }

	    // Check the dependency
	    if( needCompile( source ) ) 
		compileList.addElement( source );
	}

	if( checkDepend(oldestO, oldestOFile) ) {
	    log("Dependency expired, removing "
                + srcList.size() + " .o files and doing a full build ");
	    removeOFiles(srcList);
            compileList=new Vector();
	    for(int i=0; i<srcList.size(); i++ ) {
		Source source=(Source)srcList.elementAt(i);
		compileList.addElement( source );
	    }
            return compileList;
	}


        return compileList;
    }
    
    /** Return the files that were actually compiled
     */
    public Vector getCompiledFiles() {
        return compileList;
    }

    /** Compile the source files. The compiler adapter can override either this method
     *  ( if it can compile multiple files at once ) or compileSingleFile().
     *  Note that the list includes _all_ sources, findCompileList() can be used to
     *  avoid compiling files that are up-to-date.
     */
    public void compile(Vector sourceFiles ) throws BuildException {
        compileList=findCompileList(sourceFiles);

        log("Compiling " + compileList.size() + " out of " + sourceFiles.size());
	Enumeration en=compileList.elements();
	while( en.hasMoreElements() ) {
	    Source source=(Source)en.nextElement();
	    compileSingleFile(source);
	}
    }
    
    /** Compile single file
     */
    public void compileSingleFile(Source sourceObj) throws BuildException {
    }


    protected void displayError( int result, String source, Commandline cmd )
	throws BuildException
    {
        if( result == 0 ) {
            String err=errorstream.toString();
            if(err==null ) return;
            if( err.indexOf( "warning" ) <= 0 )
                return;
            log("Warnings: ");
            log( err );
            return;
        }
        
	log("Compile failed " + result + " " +  source );
	log("Command:" + cmd.toString());
	log("Output:" );
	if( outputstream!=null ) 
	    log( outputstream.toString());
	log("StdErr:" );
	if( errorstream!=null ) 
	    log( errorstream.toString());
	
	throw new BuildException("Compile failed " + source);
    }

    protected void addIncludes(Commandline cmd) {
	String [] includeList = ( includes==null ) ?
	    new String[] {} : includes.getIncludePatterns(project); 
	for( int i=0; i<includeList.length; i++ ) {
	    cmd.createArgument().setValue("-I" + includeList[i] );
	}
    }

    /** Common cc parameters
     */
    protected void addExtraFlags(Commandline cmd )  {
	String extra_cflags=project.getProperty("build.native.extra_cflags");
	String localCflags=cflags;
	if( localCflags==null ) {
	    localCflags=extra_cflags;
	} else {
	    if( extra_cflags!=null ) {
		localCflags+=" " + extra_cflags;
	    }
 	}
	if( localCflags != null )
	    cmd.createArgument().setLine( localCflags );
    }

    protected void addDefines( Commandline cmd ) {
        // Define by default the OS ( as known to java )
        String os=System.getProperty("java.os");

        if( defines.size() > 0 ) {
	    Enumeration defs=defines.elements();
	    while( defs.hasMoreElements() ) {
		Def d=(Def)defs.nextElement();
		String name=d.getName();
		String val=d.getValue();
		if( name==null ) continue;
		String arg="-D" + name;
		if( val!=null )
		    arg+= "=" + val;
		cmd.createArgument().setValue( arg );
            }
        }
    }

    protected void addDebug(Commandline cmd) {
    }

    protected void addOptimize( Commandline cmd ) {
    }

    protected void addProfile( Commandline cmd ) {
    }
}
