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
            localCflags=new String("-nosyspath -c -w nocmdline -bool on");
            if (null == project.getProperty("use.novelllibc"))
                localCflags += " -align 1";
            else
                localCflags += " -align 4";

            if( extra_cflags!=null )
                localCflags+=" " + extra_cflags;
        }

        if (optG)
            localCflags += " -g";

        // create a cc.opt file 
        PrintWriter ccpw = null;
        try
        {
            ccpw = new PrintWriter(new FileWriter(ccOpt));
            // write the compilation flags out
            ccpw.println(localCflags);
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
                }
            }
        }
        catch (IOException ioe)
        {
            System.out.println("Caught IOException");
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

