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

import org.apache.tools.ant.Task;
import org.apache.tools.ant.TaskAdapter;


/**
 * Set a JNDI attribute or context.
 *
 * @author Costin Manolache
 */
public class JndiSet extends Task  {
    private static org.apache.commons.logging.Log log=
        org.apache.commons.logging.LogFactory.getLog( JndiSet.class );
    String refId;
    String value;
    
    String contextName;
    String attributeName;
    
    public JndiSet() {
    }

    /** Will bind the referenced object
     */
    public void setRefId( String refId ) {
        this.refId=refId;
    }

    /** bind/set this value
     */
    public void setValue( String val ) {
        this.value=val;
    }
    
    /** The context name that will be set.
     */
    public void setContext( String ctx ) {
        this.contextName=ctx;
    }

    public void setAttribute( String att ) {
        this.attributeName=att;
    }
    
    
    public void execute() {
        try {
            InitialContext ic=new InitialContext();
            Object o=null;
            
            if( refId != null ) {
                o=project.getReference( refId );
                if( o instanceof TaskAdapter )
                    o=((TaskAdapter)o).getProxy();
            }
            if( o==null )
                o=value;
            // Add other cases.
            log.info( "Binding " + contextName + " " + o );
            ic.bind( contextName, o );

        } catch( Exception ex ) {
            ex.printStackTrace();
        }
    }
    
}
