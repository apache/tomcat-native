/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.jk.status;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;

import org.apache.tools.ant.BuildException;

/**
 * Ant task that implements the <code>/jkstatus?cmd=reset&amp;w=loadbalancer</code> command, supported by the
 * mod_jk status (1.2.20) application.
 * 
 * @author Peter Rossbach
 * @version $Revision$
 * @since mod_jk 1.2.20
 */
public class JkStatusResetTask extends AbstractJkStatusTask {

	/**
     * The descriptive information about this implementation.
     */
    private static final String info = "org.apache.jk.status.JkStatusResetTask/1.1";

    private String worker;
    private String loadbalancer;

    /**
     *  
     */
    public JkStatusResetTask() {
        super();
        setUrl("http://localhost/jkstatus");
    }

    /**
     * Return descriptive information about this implementation and the
     * corresponding version number, in the format
     * <code>&lt;description&gt;/&lt;version&gt;</code>.
     */
    public String getInfo() {

        return (info);

    }
    
    /**
	 * @return the loadbalancer
	 */
	public String getLoadbalancer() {
		return loadbalancer;
	}


	/**
	 * @param loadbalancer the loadbalancer to set
	 */
	public void setLoadbalancer(String loadbalancer) {
		this.loadbalancer = loadbalancer;
	}


	/**
	 * @return the worker
	 */
	public String getWorker() {
		return worker;
	}


	/**
	 * @param worker the worker to set
	 */
	public void setWorker(String worker) {
		this.worker = worker;
	}


	/**
     * Create jkstatus reset link
     * <ul>
     * <li><b>loadbalancer example:
     * </b>http://localhost/jkstatus?cmd=reset&mime=txt&w=loadbalancer</li>
     * <li><b>loadbalancer + sub worker example:
     * </b>http://localhost/jkstatus?cmd=reset&mime=txt&w=loadbalancer&sw=node01</li>
     * </ul>
     * 
     * @return create jkstatus reset link
     */
    protected StringBuffer createLink() {
        // Building URL
        StringBuffer sb = new StringBuffer();
        try {
            sb.append("?cmd=reset");
            sb.append("&mime=txt");
            sb.append("&w=");
            sb.append(URLEncoder.encode(loadbalancer, getCharset()));
            if(worker != null) {
                sb.append("&sw=");
            	sb.append(URLEncoder.encode(worker, getCharset()));        	
        	}

        } catch (UnsupportedEncodingException e) {
            throw new BuildException("Invalid 'charset' attribute: "
                    + getCharset());
        }
        return sb;
    }

    /**
     * check correct pararmeter
     */
    protected void checkParameter() {
        if (loadbalancer == null) {
            throw new BuildException("Must specify 'loadbalanacer' attribute");
        }
    }
}