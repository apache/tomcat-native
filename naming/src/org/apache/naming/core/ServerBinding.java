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

import javax.naming.Binding;
import javax.naming.Context;
import javax.naming.NamingException;

/** This is used for both NameClass and Binding.
 * Lazy eval - so the extra methods in Binding don't affect us.
 * For most contexts we'll deal getting the class name is as expensive
 * as getting the object. In addition most operations will only use the name.
 *
 */
public class ServerBinding extends Binding {
    public ServerBinding( String name, Context ctx, boolean isRelative ) {
        super( name, null );
        this.ctx=ctx;
        this.name = name;
        this.isRel=isRelative;
    }

    public void recycle() {
    }

    private Context ctx;
    private Object boundObj;
    private String name;
    private boolean isRel = true;
    private String className;

    private void lookup() {
        try {
            boundObj=ctx.lookup(name);
        } catch( NamingException ex ) {
            ex.printStackTrace();
        }
    }

    public String getClassName() {
        if( className!=null ) return className;
        if( boundObj==null ) lookup();

        if( boundObj!=null )
            className=boundObj.getClass().getName();
        return className;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public void setClassName(String name) {
        this.className = name;
    }

    public boolean isRelative() {
        return isRel;
    }

    public void setRelative(boolean r) {
        isRel = r;
    }

    /**
     * Generates the string representation of this name/class pair.
     * The string representation consists of the name and class name separated
     * by a colon (':').
     * The contents of this string is useful
     * for debugging and is not meant to be interpreted programmatically.
     *
     * @return The string representation of this name/class pair.
     */
    public String toString() {
        return  (isRelative() ? "" : "(not relative)") + getName() + ": " +
                getClassName();
    }


    public Object getObject() {
        if( boundObj!=null )
            return boundObj;
        lookup();
        return boundObj;
    }

    public void setObject(Object obj) {
        boundObj = obj;
    }
}
