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

package org.apache.jk.ant;

import org.apache.tools.ant.Project;

/**
 *  Set platform specific data.  Can be used to specify files (fileName="value")
 *  symbols (symbol="value") or generic values (value="value").  A platform
 *  can also be set on the object so that platform specific compilers can
 *  determine if they want to use the values.
 *  XXX will use apxs query to detect the flags.
 * 
 * @author Mike Anderson
 */
public class JkData {

    private String value;
    private boolean isfile = false;
    private String ifCond;
    String unlessCond;
    Project project;
    
    
    public JkData() {
    }

    public void setProject( Project p ) {
        project=p;
    }
    
    public void setFileName( String s ) {
        value = s;
        isfile = true;
    }
    
    public void setSymbol( String s ) {
        value = s;
        isfile = false;
    }

    public void setValue( String s ) {
        value = s;
    }

    public void setIsFile( boolean isf ) {
        isfile = isf;
    }

    public void setIf( String s ) {
        ifCond = s;
    }

    public void setUnless( String s ) {
        unlessCond = s;
    }

    public String getValue()
    {
        if( ifCond!=null && project.getProperty(ifCond) == null )
            return null;
        if (unlessCond != null && project.getProperty(unlessCond) != null) 
            return null;
        return value;
    }

    public boolean isFile()
    {
        return isfile;
    }
}
