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

import javax.naming.directory.Attributes;

/**
 * Represents a binding in a NamingContext. All jtc contexts should
 * use this class to represent entries.
 *
 * @author Remy Maucherat
 * @author Costin Manolache
 */
public class NamingEntry {


    // -------------------------------------------------------------- Constants


    public static final int ENTRY = 0;
    public static final int LINK_REF = 1;
    public static final int REFERENCE = 2;
    
    public static final int CONTEXT = 10;


    // ----------------------------------------------------------- Constructors


    public NamingEntry(String name, Object value, Attributes atts, int type) {
        this.name = name;
        this.value = value;
        this.type = type;
        this.attributes=atts;
    }


    // ----------------------------------------------------- Instance Variables


    /**
     * The type instance variable is used to avoid unsing RTTI when doing
     * lookups.
     */
    public int type;
    public String name;
    public Object value;

    public Attributes attributes;

    // cached values
    private boolean hasIntValue=false;
    private boolean hasBoolValue=false;
    private boolean hasLongValue=false;
    
    private int intValue;
    private boolean boolValue;
    private long longValue;

    // --------------------------------------------------------- Object Methods

    public void recycle() {
    }

    public boolean equals(Object obj) {
        if ((obj != null) && (obj instanceof NamingEntry)) {
            return name.equals(((NamingEntry) obj).name);
        } else {
            return false;
        }
    }

    public int hashCode() {
        return name.hashCode();
    }

}
