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

