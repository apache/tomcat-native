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

import org.apache.tools.ant.BuildException;

/**
 *  Set preferences for compiling Apache modules.
 *  XXX will use apxs query to detect the flags.
 * 
 * @author Costin Manolache
 */
public class ApacheConfig {
    
    
    public ApacheConfig() {
    }

    public void setApxs( String s ) {

    }
    
    /** Return include path for Apache
     */
    public String[] getIncludes() {
	return null;
    }

    /** Return extra C flags that are needed to compile
     */
    public String[] getCflags() {
	return null;
    }
    
    public void execute() throws BuildException {

    }
    
}
