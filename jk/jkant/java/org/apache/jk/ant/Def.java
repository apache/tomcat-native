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

/** Define name/value, value is optional
 *  The define will take place if the condition
 *  is met.
 *
 *  A <so> task should include all the defines it supports,
 *  with the default value. This allows the builder to customize
 *  without having to look at the source.
 */
public class Def {
    String name;
    String info;
    String value;
    String ifCond;
    String unlessCond;
    Project project;

    public Def() {
    }

    public void setProject( Project p ) {
	project=p;
    }
    
    public void setName(String n) {
	name=n;
    }

    public void setValue( String v ) {
	value=v;
    }

    public void setIf( String ifCond ) {
	this.ifCond=ifCond;
    }

    public void setUnless( String unlessCond ) {
	this.unlessCond=unlessCond;
    }

    /** Informations about the option
     */
    public void setInfo(String n ) {
	info=n;
    }

    // -------------------- Getters --------------------

    /** Return the name of the define, or null if the define is not
     *  valid ( if/unless conditions )
     */
    public String getName() {
	if( ifCond!=null && project.getProperty(ifCond) == null )
	    return null;
	if (unlessCond != null && project.getProperty(unlessCond) != null) 
	    return null;
	return name;
    }

    public String getValue() {
	return value;
    }
}
