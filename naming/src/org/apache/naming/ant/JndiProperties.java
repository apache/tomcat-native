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

import javax.naming.InitialContext;

import org.apache.tools.ant.PropertyHelper;
import org.apache.tools.ant.Task;


/**
 *  Dynamic properties from a JNDI context. Use ${jndi:NAME} syntax.
 *  You may need to use <jndiEnv> to set up jndi properties and drivers,
 *  and eventually different context-specific tasks.
 *
 * @author Costin Manolache
 */
public class JndiProperties extends Task {
    public static String PREFIX="jndi:";
    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( JndiProperties.class );
    private JndiHelper helper=new JndiHelper();

    public JndiProperties() {
        initNaming();
    }

    static boolean initialized=false;
    static void initNaming() {
        if( initialized ) return;
        initialized=true;
        Thread.currentThread().setContextClassLoader( JndiProperties.class.getClassLoader() );
//         System.setProperty( "java.naming.factory.initial", "org.apache.naming.memory.MemoryInitialContextFactory" );
    }
    
    class JndiHelper extends PropertyHelper {
        public boolean setPropertyHook( String ns, String name, Object v, boolean inh,
                                        boolean user, boolean isNew)
        {
            if( ! name.startsWith(PREFIX) ) {
                // pass to next
                return super.setPropertyHook(ns, name, v, inh, user, isNew);
            }
            name=name.substring( PREFIX.length() );

            // XXX later

            return true;
        }

        public Object getPropertyHook( String ns, String name , boolean user) {
            if( ! name.startsWith(PREFIX) ) {
                // pass to next
                return super.getPropertyHook(ns, name, user);
            }

            Object o=null;
            name=name.substring( PREFIX.length() );
            try {
                InitialContext ic=new InitialContext();
                // XXX lookup attribute in DirContext ?
                o=ic.lookup( name );
                if( log.isDebugEnabled() ) log.debug( "getProperty jndi: " + name +  " " + o);
            } catch( Exception ex ) {
                log.error("getProperty " + name , ex);
                o=null;
            }
            return o;
        }

    }


    public void execute() {
        PropertyHelper phelper=PropertyHelper.getPropertyHelper( project );
        helper.setProject( project );
        helper.setNext( phelper.getNext() );
        phelper.setNext( helper );

        project.addReference( "jndiProperties", this );
    }
    
}
