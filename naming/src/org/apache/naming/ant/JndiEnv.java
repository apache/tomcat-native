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
 */ 

package org.apache.naming.ant;

import java.io.*;
import java.util.*;

import org.apache.tools.ant.*;

import javax.naming.*;

/**
 *  Task to set up JNDI properties ( the hashtable that is passed to InitialContext )
 *  It has explicit attributes for common properties, and supports generic name/value
 *  elements.
 * 
 * @author  Costin Manolache
 */
public class JndiEnv extends Task  {
 
    Hashtable env = new Hashtable();
    String url;
    boolean topLevel=true;
    
    public JndiEnv() {
    }
    
    public JndiEnv(boolean child) {
        topLevel=false;
    }
    
    public String getProviderUrl() {
        return (String) env.get(Context.PROVIDER_URL);
    }
    
    public void setProviderUrl(String providerUrl) {
        env.put(Context.PROVIDER_URL, providerUrl);
    }

    public String getUrl() {
        return url;
    }

    public void setUrl(String url) {
        this.url = url;
    }

    public String getInitialFactory() {
        return (String) env.get(Context.INITIAL_CONTEXT_FACTORY);
    }

    public void setInitialFactory(String initialFactory) {
        env.put(Context.INITIAL_CONTEXT_FACTORY, initialFactory);
    }

    public String getAuthoritative() {
        return (String) env.get(Context.AUTHORITATIVE);
    }

    public void setAuthoritative(String authoritative) {
        env.put(Context.AUTHORITATIVE, authoritative);
    }

    public String getObjectFactories() {
        return (String) env.get(Context.OBJECT_FACTORIES);
    }

    public void setObjectFactories(String objectFactories) {
        env.put(Context.OBJECT_FACTORIES, objectFactories);
    }

    public String getUrlPkgPrefixes() {
        return (String) env.get(Context.URL_PKG_PREFIXES);
    }

    public void setUrlPkgPrefixes(String urlPkgPrefixes) {
        env.put(Context.URL_PKG_PREFIXES, urlPkgPrefixes);
    }

    public void execute() {
        if( nvEntries!=null ) {
            for( int i=0; i<nvEntries.size(); i++ ) {
                NameValue nv=(NameValue)nvEntries.elementAt(i);
                env.put( nv.name, nv.value);
            }
        }
        // If this is a standalone task - add a ref in the project.
        if(topLevel)
            project.addReference( "globalJndiEnv", this );
    }

    Vector nvEntries;
    
    public NameValue addEnv() {
        if( nvEntries==null ) nvEntries=new Vector();
        NameValue nv=new NameValue();
        nvEntries.addElement( nv );
        return nv;
    }
    
    public static class NameValue {
        String name;
        String value;
        
        public void setName(String name) {
            this.name=name;
        }
        public void setValue(String value) {
            this.value=value;
        }
    }
}
