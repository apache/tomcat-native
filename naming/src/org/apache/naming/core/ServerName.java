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

import javax.naming.CompositeName;
import javax.naming.InvalidNameException;
import javax.naming.Name;

/**
 * Implementation of Name  with support for extra information.
 *
 * An extra feature ( not yet implemneted ) is the support for
 * MessageBytes. This allows tomcat to operate lookup operations
 * on the original message, without creating Strings.
 *
 * Another feature is support for extra information that can be cached.
 * This brakes a bit the JNDI requirements, as Contexts can modify the
 * Name and add anotations. The main benefit is that after the first
 * lookup it'll be possible to avoid some expensive operations.
 *
 * @author Costin Manolache
 */
public class ServerName extends CompositeName
{
    private int id=0;

    public ServerName( String s ) throws InvalidNameException {
        super(s);
    }

    /** ID is a small int, equivalent with a note.
        The name is reused in the server environment, and various
        components can use the id for fast access.

        The id is local for a particular context.

        /Servlet/ClassName -> ClassName will have an ID ( note id ) unique
                              for the Servlet 
    */
    public final int getId() {
        return id;
    }

    // XXX Should we allow setting the id, or use a counter for each prefix ? 
    public final void setId( int id ) {
        this.id=id;
    }

    /**
     * Factory method to create server names. 
     */
    public static Name getName( String s ) throws InvalidNameException {
        return new ServerName( s );
    }

    /**
     * @todo: optimize parsing and cache ( reuse existing instances ).
     */
//     public static Name getName( MessageBytes mb ) {
//         return new ServerName( mb.toString() );
//     }
}
