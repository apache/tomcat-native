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
public class MwldLinker extends LinkerAdapter {
    GlobPatternMapper lo_mapper=new GlobPatternMapper();
    
    public MwldLinker() {
        super();
        lo_mapper.setFrom("*.c");
	lo_mapper.setTo("*.o");
    }

    public void setSoTask(SoTask so ) {
        this.so=so;
        so.setExtension(".nlm");
        so.duplicateTo( this );
        project.setProperty("netware", "true");

        Enumeration e=altSoFiles.elements();
        while (e.hasMoreElements())
        {
            JkData data = (JkData) e.nextElement();
            String altSo = data.getValue();
            if (altSo == null) 
                continue;
            else
            {
                so.setTarget(altSo);    // set it on the master copy
                setTarget(altSo);       // set it on ourself
                break;
            }
        }
    }

    public void execute() throws BuildException {
        findSourceFiles();
        link(this.srcList);
    }

    /** Link using libtool.
     */
    public boolean link(Vector srcList) throws BuildException {
        Commandline cmd = new Commandline();
        File linkOpt = new File(buildDir, "link.opt");
        File linkDef = new File(buildDir, "link.def");

        String libtool=project.getProperty("build.compiler.ld");
        if(libtool==null) libtool="mwldnlm";

        cmd.setExecutable( libtool );
        
        // All .obj files must be included
        project.log( "Linking " + buildDir + "/" + soFile + ".nlm");

        // create a .opt file and a .def file
        PrintWriter linkOptPw = null;
        PrintWriter linkDefPw = null;
        try
        {
            String libBase = project.getProperty("build.compiler.base");
            if (libBase == null) libBase = "\\tools\\mw\\5.3";
            linkOptPw = new PrintWriter(new FileWriter(linkOpt));
            linkDefPw = new PrintWriter(new FileWriter(linkDef));

            // write the link flags out
            linkOptPw.println("-warnings off");
            linkOptPw.println("-zerobss");
            linkOptPw.println("-o " + soFile + ".nlm");
            linkOptPw.println("-map " + soFile + ".map");
            linkOptPw.println("-nodefaults");

            // add debug information in if requested
            if (optG)
            {
                linkOptPw.println("-g");
                linkOptPw.println("-sym internal");
                linkOptPw.println("-sym codeview4");
                linkOptPw.println("-osym " + soFile + ".NCV");
            }

            // write out any additional link options
            Enumeration opts = linkOpts.elements();
            while( opts.hasMoreElements() ) {
                JkData opt = (JkData) opts.nextElement();
                String option = opt.getValue();
                if( option == null ) continue;
                linkOptPw.println( option );
            }

            // add the default startup code to the list of objects
            if (null == project.getProperty("use.novelllibc"))
                linkOptPw.println(libBase + "\\lib\\nwpre.obj");
            else
                linkOptPw.println("-llibcpre.o");

            // write the objects to link with to the .opt file
            for( int i=0; i<srcList.size(); i++ ) {
                Source source=(Source)srcList.elementAt(i);
                File srcF = source.getFile();
                String name=srcF.getName();
                String targetNA[]=lo_mapper.mapFileName( name );
                if( targetNA!=null )
                    linkOptPw.println( targetNA[0] );
            }
            linkOptPw.println("-commandfile link.def");

            // write the dependant modules to the .def file
            Enumeration mods = modules.elements();
            while( mods.hasMoreElements() ) {
                JkData mod = (JkData) mods.nextElement();
                String name = mod.getValue();
                if( name==null ) continue;
                linkDefPw.println("module " + name);
            }

            // write the imports to link with to the .def file
            Enumeration imps = imports.elements();
            while( imps.hasMoreElements() ) {
                JkData imp = (JkData) imps.nextElement();
                String name = imp.getValue();
                if( name==null ) continue;
                if (imp.isFile())
                    linkDefPw.println("Import @" + name);
                else
                    linkDefPw.println("Import " + name);
            }

            // write the exports to link with to the .def file
            Enumeration exps = exports.elements();
            while( exps.hasMoreElements() ) {
                JkData exp = (JkData) exps.nextElement();
                String name = exp.getValue();
                if( name==null ) continue;
                if (exp.isFile())
                    linkDefPw.println("Export @" + name);
                else
                    linkDefPw.println("Export " + name);
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
        linkOpt.delete();
        linkDef.delete();
        closeStreamHandler();
        return true;
    }
}

