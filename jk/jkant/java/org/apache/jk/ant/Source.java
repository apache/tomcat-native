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

import java.io.File;

import org.apache.tools.ant.util.GlobPatternMapper;

public class Source {
    File dir;
    String localPart;
    
    public Source( File dir, String localPart ) {
	this.dir=dir;
	this.localPart=localPart;
    }

    public String getTargetFile( GlobPatternMapper mapper ) {
	String targetNA[]=mapper.mapFileName( localPart );
	if( targetNA==null )
	    return null; // strange, probably different extension ?
	String target=targetNA[0];
	return target;
    }

    public File getFile() {
	return  new File( dir, localPart );
    }

    public String getPackage() {
	int lastSlash=localPart.lastIndexOf("/");
	if( lastSlash==-1 )
	    return "";
	return localPart.substring( 0, lastSlash );
    }
}
