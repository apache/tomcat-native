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

package org.apache.naming.core;

import javax.naming.directory.DirContext;

/**
 * Utility class providing additional operations on DirContexts.
 * Instead of extending DirContext we use a helper who will work
 * on any context, including 'foreign' ones ( like JNDI, etc ).
 *
 * Typical methods - conversions ( int, boolean), etc.
 *
 * This code should check if the context extend our BaseDirContext
 * and use specific features ( like notes - to cache the conversion
 * results for example ), but fallback for external contexts.
 *
 * @author Costin Manolache
 */
public class DirContextHelper  {
    static DirContextHelper instance=new DirContextHelper();
    
    public static DirContextHelper getInstance() {
        return instance;
    }
    
    
    /** Debugging string - the context, imediate childs and attributes
     */
    public String toString(DirContext ctx) {
        return "";
    }

    public int getIntAttribute( DirContext ctx, String name ) {
        return 0;
    }

    public long getLongAttribute( DirContext ctx, String name ) {
        return 0;
    }

    public String getStringAttribute( DirContext ctx, String name ) {
        return null;
    }

    public boolean getBooleanAttribute( DirContext ctx, String name ) {
        return false;
    }
}

