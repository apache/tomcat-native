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
