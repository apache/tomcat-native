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

/**
 * @author Peter Rossbach
 * @version $Revision$ $Date$
 * @see org.apache.jk.status.JkStatusParser
 */
public class JkServer implements Serializable {
    String name ;
    String port;  
    String software;
    String version ;
    
    /**
     * @return Returns the name.
     */
    public String getName() {
        return name;
    }
    /**
     * @param name The name to set.
     */
    public void setName(String name) {
        this.name = name;
    }
    /**
     * @return Returns the port.
     */
    public String getPort() {
        return port;
    }
    /**
     * @param port The port to set.
     */
    public void setPort(String port) {
        this.port = port;
    }
    /**
     * @return Returns the software.
     */
    public String getSoftware() {
        return software;
    }
    /**
     * @param software The software to set.
     */
    public void setSoftware(String software) {
        this.software = software;
    }
    /**
     * @return Returns the version.
     */
    public String getVersion() {
        return version;
    }
    /**
     * @param version The version to set.
     */
    public void setVersion(String version) {
        this.version = version;
    }
}
