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

import org.apache.tools.ant.BuildException;
import org.apache.jk.ant.*;

import java.util.*;

/* Modeled after javac
 */

/**
 * s/javac/C linker/
 *
 * The interface that all compiler adapters must adher to.  
 *
 * <p>A compiler adapter is an adapter that interprets the javac's
 * parameters in preperation to be passed off to the compier this
 * adapter represents.  As all the necessary values are stored in the
 * Javac task itself, the only thing all adapters need is the javac
 * task, the execute command and a parameterless constructor (for
 * reflection).</p>
 *
 * @author Jay Dickon Glanville <a href="mailto:jayglanville@home.com">jayglanville@home.com</a>
 * @author Costin Manolache
 */
public abstract class LinkerAdapter extends SoTask {
    protected SoTask so;

    /**
     * Sets the compiler attributes, which are stored in the Javac task.
     */
    public void setSoTask(SoTask so ) {
	this.so=so;
    }

    public void execute() throws BuildException {
	findSourceFiles();
	link(this.srcList);
    }


    /**
     * Executes the task.
     *
     * @return has the compilation been successful
     */
    public abstract boolean link(Vector srcFiles) throws BuildException;
}
