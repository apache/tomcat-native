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
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Enumeration;

import org.apache.jk.ant.Def;
import org.apache.jk.ant.SoTask;
import org.apache.jk.ant.Source;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.types.Commandline;
import org.apache.tools.ant.util.GlobPatternMapper;

/**
 *  Compile using Microsoft Visual C++ v6.0
 * 
 * @author Costin Manolache
 * @author Ignacio J. Ortega
 * @author Mike Anderson
 * @author Larry Isaacs
 */
public class MsvcCompiler extends CompilerAdapter {
    GlobPatternMapper co_mapperS=new GlobPatternMapper();
    
    public MsvcCompiler() {
        super();
	co_mapperS.setFrom("*.c");
	co_mapperS.setTo("*.obj");
    }

    public String[] getTargetFiles( Source src ) {
        File srcFile = src.getFile();
        String name=srcFile.getName();
        
        return co_mapperS.mapFileName( name );
    }

    public void setSoTask(SoTask so ) {
        this.so=so;
        so.setExtension(".dll");
        so.duplicateTo( this );
        project.setProperty("win32", "true");
        if (optG)
            project.setProperty("win32.debug", "true");
        else
            project.setProperty("win32.release", "true");
    }

    /** Compile using msvc 
     */
    public void compileSingleFile(Source sourceObj) throws BuildException {
	File f=sourceObj.getFile();
	String source=f.toString();
        String [] includeList = ( includes==null ) ?
            new String[] {} : includes.getIncludePatterns(project);

        Commandline cmd = new Commandline();

        String cc=project.getProperty("build.compiler.cc");
        if(cc==null) cc="cl";
        
        cmd.setExecutable( cc );
        addCCArgs( cmd, source, includeList );

        int result=execute( cmd );
        if( result!=0 ) {
            log("Compile failed " + result + " " +  source );
            log("Output:" );
            if( outputstream!=null ) 
                log( outputstream.toString());
            log("StdErr:" );
            if( errorstream!=null ) 
                log( errorstream.toString());
            
            throw new BuildException("Compile failed " + source);
        }
        File ccOpt = new File(buildDir, "cc.opt");
        ccOpt.delete();
        closeStreamHandler();

    }

    /** common compiler args
     */
    private void addCCArgs(Commandline cmd, String source, String includeList[]) {
        String extra_cflags=project.getProperty("build.native.extra_cflags");
        String localCflags=cflags;
        File ccOpt = new File(buildDir, "cc.opt");
        if( localCflags==null ) {
            localCflags=new String("-nologo -W3 -GX -O2 -c");
            if( extra_cflags!=null ) {
                localCflags+=" " + extra_cflags;
            }
        }

        if (optG)
            localCflags += " -MTd -Zi";
        else
            localCflags += " -MT";

        // create a cc.opt file 
        PrintWriter ccpw = null;
        try
        {
            ccpw = new PrintWriter(new FileWriter(ccOpt));
            // write the compilation flags out
            ccpw.println(localCflags);
            for( int i=0; i<includeList.length; i++ ) {
                ccpw.print("-I");
                if (!includeList[i].startsWith("\"")) {
                    ccpw.print("\"");
                }
                ccpw.print(includeList[i] );
                if (!includeList[i].endsWith("\"")) {
                    ccpw.print("\"");
                }
                ccpw.println();
            }

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
                    ccpw.println(arg);
                }
            }
        }
        catch (IOException ioe)
        {
            log("Caught IOException");
        }
        finally
        {
            if (ccpw != null)
            {
                ccpw.close();
            }
        }

        project.log( "Compiling " + source);
        cmd.createArgument().setValue( source );
        cmd.createArgument().setValue( "@cc.opt" );
    }
}

