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


import java.util.Enumeration;

import javax.naming.Binding;
import javax.naming.Context;
import javax.naming.NameClassPair;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;

/**
 * Naming enumeration implementation.
 *
 * TODO: implement 'throw exceptions on close' part of the spec.
 * TODO: implement recycling ( for example on close )
 *
 * @author Remy Maucherat
 * @author Costin Manolache
 */
public class NamingContextEnumeration 
    implements NamingEnumeration
{
    /** Constructor.
     *
     * @param enum base enumeration. Elements can be Strings, NameClassPair,
     * Bindings or Entries, we'll provide the wrapping if needed. For String
     * the Class and value will be lazy-loaded.
     *
     * @param ctx The context where this enum belongs. Used to lazy-eval
     * the class and value
     *
     * @param bindings If true, we'll wrap things as Binding ( true for
     * listBindings, false for list ).
     */
    public NamingContextEnumeration( Enumeration enum, Context ctx,
                                     boolean bindings )
    {
        this.ctx = ctx;
        this.bindings=bindings;
        this.enum = enum;
    }

    // -------------------------------------------------------------- Variables

    // return bindings instead of NameClassPair
    protected boolean bindings;
    /**
     * Underlying enumeration.
     */
    protected Enumeration enum;

    protected Context ctx;

    // --------------------------------------------------------- Public Methods


    /**
     * Retrieves the next element in the enumeration.
     */
    public Object next()
        throws NamingException
    {
        return nextElement();
    }


    /**
     * Determines whether there are any more elements in the enumeration.
     */
    public boolean hasMore()
        throws NamingException
    {
        return enum.hasMoreElements();
    }


    /**
     * Closes this enumeration.
     */
    public void close()
        throws NamingException
    {
        // XXX all exceptions should be thrown on close ( AFAIK )
    }


    public boolean hasMoreElements() {
        return enum.hasMoreElements();
    }

    public Object nextElement() {
        Object next=enum.nextElement();
        if( next instanceof NamingEntry ) {
            NamingEntry entry = (NamingEntry) next;
            return new ServerBinding(entry.name, ctx, true);
        } else if( next instanceof NameClassPair ) {
            NameClassPair ncp=(NameClassPair)next;
            if( bindings )
                return new ServerBinding(ncp.getName(), ctx, true);
            return next;
        } else if( next instanceof Binding ) {
            return next;
        } else if( next instanceof String ) {
            String name=(String)next;
            return new ServerBinding( name, ctx, true );
        }
        return null;
    }

}

