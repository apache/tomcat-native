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

/**
 *  Compile using MetroWerks.
 * 
 *  It extends SoTask so we can debug it or use it independently of <so>.
 *  For normal use you should use the generic task, and system-specific
 *  properties to choose the right compiler plugin ( like we select
 *  jikes ).
 *
 * @author Mike Anderson
 */
public class MwccCompiler extends CcCompiler {
    
    public MwccCompiler() {
        super();
    };

    public void setSoTask(SoTask so ) {
        this.so=so;
        so.setExtension(".nlm");
        so.duplicateTo( this );
        project.setProperty("netware", "true");
    }

    /** Compile  using mwccnlm.
     */
    public void compileSingleFile(Source sourceObj) throws BuildException {
	File f=sourceObj.getFile();
	String source=f.toString();
        String [] includeList = ( includes==null ) ?
            new String[] {} : includes.getIncludePatterns(project);

        Commandline cmd = new Commandline();

        String cc=project.getProperty("build.compiler.cc");
        if(cc==null) cc="mwccnlm";
        
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
        if (null == project.getProperty("save.optionFiles"))
        {
            File ccOpt = new File(buildDir, "cc.opt");
            ccOpt.delete();
        }
        closeStreamHandler();

    }

    /** common compiler args
     */
    private void addCCArgs(Commandline cmd, String source, String includeList[]) {
        String extra_cflags=project.getProperty("build.native.extra_cflags");
        String localCflags=cflags;
        File ccOpt = new File(buildDir, "cc.opt");
        boolean useLibC = false;

        // create a cc.opt file 
        PrintWriter ccpw = null;
        try
        {
            ccpw = new PrintWriter(new FileWriter(ccOpt));

            for( int i=0; i<includeList.length; i++ ) {
                ccpw.print("-I");
                ccpw.println(includeList[i] );
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

                    // check to see if we are building using LibC
                    if (name.equals("__NOVELL_LIBC__"))
                        useLibC = true;
                }
            }

            // finalize the cflags
            if( localCflags==null ) {
                localCflags=new String("-nosyspath -c -w nocmdline -bool on");
                if (useLibC)
                    localCflags += " -align 4";
                else
                    localCflags += " -align 1";

                if( extra_cflags!=null )
                    localCflags+=" " + extra_cflags;
            }

            if (optG)
                localCflags += " -g";

            // write the compilation flags out
            ccpw.println(localCflags);
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

