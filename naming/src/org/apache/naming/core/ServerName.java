/*
 * ====================================================================
 *
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999 The Apache Software Foundation.  All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution, if
 *    any, must include the following acknowlegement:  
 *       "This product includes software developed by the 
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowlegement may appear in the software itself,
 *    if and wherever such third-party acknowlegements normally appear.
 *
 * 4. The names "The Jakarta Project", "Tomcat", and "Apache Software
 *    Foundation" must not be used to endorse or promote products derived
 *    from this software without prior written permission. For written 
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * [Additional notices, if required by prior licensing conditions]
 *
 */ 


package org.apache.naming.core;

import javax.naming.*;

/**
 * Extended version for Attribute. All our dirContexts should return objects
 * of this type. Methods that take attribute param should use this type
 * of objects for performance.
 *
 * This is an extension of the 'note' in tomcat 3.3. Each attribute will have an
 * 'ID' ( small int ) and an associated namespace. The attributes are recyclable.
 *
 * The attribute is designed for use in server env, where performance is important.
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
