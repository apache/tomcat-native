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

package org.apache.naming.ant;

import java.util.Hashtable;
import java.util.Vector;

import javax.naming.Context;

import org.apache.tools.ant.Task;

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
