/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
package org.apache.jk.status;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

/**
 * @author Peter Rossbach
 * @version $Revision$ $Date$
 * @see org.apache.jk.status.JkStatusParser
 */
public class JkStatus implements Serializable {

    JkServer server ;
    JkSoftware software ;
    JkResult result ;
    List balancers = new ArrayList() ;
   
    /**
     * @return Returns the balancers.
     */
    public List getBalancers() {
        return balancers;
    }
    /**
     * @param balancers The balancers to set.
     */
    public void setBalancers(List balancers) {
        this.balancers = balancers;
    }
    
    public void addBalancer(JkBalancer balancer) {
      balancers.add(balancer);
    }
    
    public void removeBalancer(JkBalancer balancer) {
      balancers.remove(balancer);
    }

    /**
     * @return Returns the server.
     */
    public JkServer getServer() {
        return server;
    }
    public void setServer(JkServer server) {
       this.server = server ;
    }
	/**
	 * @return the result
	 */
	public JkResult getResult() {
		return result;
	}
	/**
	 * @param result the result to set
	 */
	public void setResult(JkResult result) {
		this.result = result;
	}
    
    /**
     * @return Returns the software.
     */
    public JkSoftware getSoftware() {
        return software;
    }
    /**
     * @param software The software to set.
     */
    public void setSoftware(JkSoftware software) {
        this.software = software;
    }
}
