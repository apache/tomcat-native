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

import org.apache.jk.ant.Def;
import org.apache.jk.ant.SoTask;
import org.apache.jk.ant.Source;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.types.Commandline;

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
