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

package org.apache.naming.modules.fs;

import java.util.Hashtable;

import javax.naming.Context;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.spi.InitialContextFactory;
import javax.naming.spi.ObjectFactory;
//import org.apache.naming.ContextBindings;

/**
 * Context factory for the "file:" namespace.
 * <p>
 * <b>Important note</b> : This factory MUST be associated with the "java" URL
 * prefix, which can be done by either :
 * <ul>
 * <li>Adding a 
 * java.naming.factory.url.pkgs=org.apache.naming property to the JNDI properties file</li>
 * <li>Setting an environment variable named Context.URL_PKG_PREFIXES with 
 * its value including the org.apache.naming package name. 
 * More detail about this can be found in the JNDI documentation : 
 * {@link javax.naming.spi.NamingManager#getURLContext(java.lang.String, java.util.Hashtable)}.</li>
 * </ul>
 * 
 * @author Remy Maucherat
 */

public class fsURLContextFactory implements ObjectFactory,InitialContextFactory
{
    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( fsURLContextFactory.class );
    /**
     * Initial context.
     */
    protected static Context initialContext = null;


    // --------------------------------------------------------- Public Methods

    /**
     * Crete a new Context's instance.
     */
    public Object getObjectInstance(Object obj, Name name, Context nameCtx,
                                    Hashtable environment)
        throws NamingException {
        // Called with null/null/null if fs:/tmp/test
        if( log.isDebugEnabled() ) log.debug( "getObjectInstance " + obj + " " + name + " " + nameCtx + " " + environment);

        FileDirContext fc= new FileDirContext(environment);
        fc.setDocBase( "/" );
        fc.setURLPrefix("fs:");
        return fc;
    }


    /**
     * Get a new (writable) initial context.
     */
    public Context getInitialContext(Hashtable environment)
        throws NamingException
    {
        // If the thread is not bound, return a shared writable context
        if (initialContext == null) {
            FileDirContext fc= new FileDirContext(environment);
            fc.setDocBase( "/" );
            fc.setURLPrefix("fs:");
            initialContext=fc;
            if( log.isDebugEnabled() )
                log.debug("Create initial fs context "+ environment);
        }
        return initialContext;
    }
}

