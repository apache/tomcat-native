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
import java.util.Vector;

import org.apache.jk.ant.JkData;
import org.apache.jk.ant.SoTask;
import org.apache.jk.ant.Source;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.types.Commandline;
import org.apache.tools.ant.util.GlobPatternMapper;

/**
 * Link using MSVC Linker
 *
 * @author Costin Manolache
 * @author Ignacio J. Ortega
 * @author Mike Anderson
 * @author Larry Isaacs
 */
public class MsvcLinker extends LinkerAdapter {
    SoTask so;
    GlobPatternMapper co_mapper=new GlobPatternMapper();
    
    public MsvcLinker() {
        so=this;
        co_mapper.setFrom("*.c");
        co_mapper.setTo("*.obj");
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

    public void execute() throws BuildException {
        findSourceFiles();
        link(this.srcList);
    }

    public boolean link(Vector srcList) throws BuildException {
        Commandline cmd = new Commandline();
        File linkOpt = new File(buildDir, "link.opt");
        File linkDef = new File(buildDir, "link.def");

        String libtool=project.getProperty("build.compiler.ld");
        if(libtool==null) libtool="link";

        cmd.setExecutable( libtool );

        // All .obj files must be included
        project.log( "Linking " + buildDir + "/" + soFile + ".dll");

        // create a .opt file and a .def file
        PrintWriter linkOptPw = null;
        PrintWriter linkDefPw = null;
        try
        {
            linkOptPw = new PrintWriter(new FileWriter(linkOpt));
            linkDefPw = new PrintWriter(new FileWriter(linkDef));

            // write the imports to link with to the .opt file
            linkOptPw.print("  ");
            Enumeration imps = imports.elements();
            while( imps.hasMoreElements() ) {
                JkData imp = (JkData) imps.nextElement();
                String name = imp.getValue();
                if( name==null ) continue;
                linkOptPw.print(name+" ");
            }

            // write the link flags out

            linkOptPw.print("/machine:I386 ");
            linkOptPw.print("/out:" + soFile + ".dll ");
            linkOptPw.print("/nologo ");
            linkOptPw.print("/dll ");
            linkOptPw.print("/incremental:no ");

            // write out any additional link options
            Enumeration opts = linkOpts.elements();
            while( opts.hasMoreElements() ) {
                JkData opt = (JkData) opts.nextElement();
                String option = opt.getValue();
                if( option == null ) continue;
                linkOptPw.println( option );
            }

            // add debug information in if requested
            if (optG)
            {
                linkOptPw.print("/debug ");
            }
            // def file
            linkOptPw.println("/def:link.def");
            // write the objects to link with to the .opt file
            for( int i=0; i<srcList.size(); i++ ) {
                Source source=(Source)srcList.elementAt(i);
                File srcF = source.getFile();
                String name=srcF.getName();
                String targetNA[]=co_mapper.mapFileName( name );
                if( targetNA!=null )
                    linkOptPw.println( targetNA[0] );
            }
            // Write the resources to link to .opt file
            Enumeration ress = resources.elements();
            while( ress.hasMoreElements() ) {
                JkData res = (JkData) ress.nextElement();
                String name = res.getValue();
                if( name==null ) continue;
                linkOptPw.println(name);
            }
            
            // Write the library name to the def file
            linkDefPw.println("LIBRARY\t\""+soFile+"\"");

            // write the exported symbols to the .def file
            Enumeration exps = exports.elements();
            if ( exps.hasMoreElements() )
            {
                linkDefPw.println("EXPORTS");
                while( exps.hasMoreElements() ) {
                    JkData exp = (JkData) exps.nextElement();
                    String name = exp.getValue();
                    if( name==null ) continue;
                    linkDefPw.println("\t" + name);
                }
            }
        }
        catch (IOException ioe)
        {
            System.out.println("Caught IOException");
        }
        finally
        {
            if (linkOptPw != null)
            {
                linkOptPw.close();
            }

            if (linkDefPw != null)
            {
                linkDefPw.close();
            }
        }


        cmd.createArgument().setValue( "@link.opt" );
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
        //         linkOpt.delete();
        //         linkDef.delete();
        closeStreamHandler();
        return true;
    }
}

