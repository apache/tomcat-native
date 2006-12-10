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
 * Ant task that implements the <code>/status</code> update worker command, supported by the
 * mod_jk status (1.2.20) application.
 * 
 * 
 * @author Peter Rossbach
 * @version $Revision:$
 * @since mod_jk 1.2.20
 */
public class JkStatusUpdateWorkerTask extends AbstractJkStatusTask {

    /**
     * The descriptive information about this implementation.
     */
    private static final String info = "org.apache.jk.status.JkStatusUpdateWorkerTask/1.0";

    protected String loadbalancer ;
    
    protected String worker ;

    protected int loadfactor =-1;

    protected String route ;
    
    protected int distance = -1;
    
    protected String redirect;

    protected String domain;
    
    protected int activationCode = -1;

    protected String activation ;

    /**
     * Return descriptive information about this implementation and the
     * corresponding version number, in the format
     * <code>&lt;description&gt;/&lt;version&gt;</code>.
     */
    public String getInfo() {

        return (info);

    }
    
    /**
     *  
     */
    public JkStatusUpdateWorkerTask() {
        super();
        setUrl("http://localhost/jkstatus");
    }

	/**
	 * @return the activation
	 */
	public String getActivation() {
		return activation;
	}

	/**
	 * @param activation the activation to set
	 */
	public void setActivation(String activation) {
		this.activation = activation;
	}

	/**
	 * @return the activationCode
	 */
	public int getActivationCode() {
		return activationCode;
	}

	/**
	 * @param activationCode the activationCode to set
	 */
	public void setActivationCode(int activationCode) {
		this.activationCode = activationCode;
	}

	/**
	 * @return the distance
	 */
	public int getDistance() {
		return distance;
	}

	/**
	 * @param distance the distance to set
	 */
	public void setDistance(int distance) {
		this.distance = distance;
	}

	/**
	 * @return the domain
	 */
	public String getDomain() {
		return domain;
	}

	/**
	 * @param domain the domain to set
	 */
	public void setDomain(String domain) {
		this.domain = domain;
	}

	/**
	 * @return the loadbalancer
	 */
	public String getLoadbalancer() {
		return loadbalancer;
	}

	/**
	 * @param loadbalancer the loadbalaner to set
	 */
	public void setLoadbalancer(String loadbalancer) {
		this.loadbalancer = loadbalancer;
	}

	/**
	 * @return the loadfactor
	 */
	public int getLoadfactor() {
		return loadfactor;
	}

	/**
	 * @param loadfactor the loadfactor to set
	 */
	public void setLoadfactor(int loadfactor) {
		this.loadfactor = loadfactor;
	}

	/**
	 * @return the redirect
	 */
	public String getRedirect() {
		return redirect;
	}

	/**
	 * @param redirect the redirect to set
	 */
	public void setRedirect(String redirect) {
		this.redirect = redirect;
	}

	/**
	 * @return the route
	 */
	public String getRoute() {
		return route;
	}

	/**
	 * @param route the route to set
	 */
	public void setRoute(String route) {
		this.route = route;
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
     * Create JkStatus worker update link
     * <ul>
     * </b>http://localhost/jkstatus?cmd=update&mime=txt&w=loadbalancer&sw=node01&wn=node01&l=lb&wf=1&wa=1&wd=0
     * <br/>
     *
     * 
     * <br/>Tcp worker parameter:
     * <br/>
     * <ul>
     * <li><b>w:<b/> name loadbalancer</li>
     * <li><b>sw:<b/> name tcp worker node</li>
     * <li><b>wf:<b/> load factor</li>
     * <li><b>wn:<b/> route</li>
     * <li><b>wd:<b/> distance</li>
     * <li><b>wa:<b/> activation state</li>
     * <li><b>wr:<b/> redirect route</li>
     * <li><b>wc:<b/> cluster domain</li>
     * </ul>
     * <ul>
     * <li>wa=1 active</li>
     * <li>wa=2 disabled</li>
     * <li>wa=3 stopped</li>
     * </ul>
     * </li>
     * </ul>
     * 
     * @return create jkstatus update worker link
     */
    protected StringBuffer createLink() {
        // Building URL
        StringBuffer sb = new StringBuffer();
        try {
            sb.append("?cmd=update&mime=txt");
            sb.append("&w=");
            sb.append(URLEncoder.encode(loadbalancer, getCharset()));
            sb.append("&sw=");
            sb.append(URLEncoder.encode(worker, getCharset()));
            if (loadfactor >= 0) {
				sb.append("&wf=");
				sb.append(loadfactor);
			}
			if (route != null) {
				sb.append("&wn=");
				sb.append(URLEncoder.encode(route, getCharset()));
			}
			if (activation == null && activationCode > 0 && activationCode < 4) {
				sb.append("&wa=");
				sb.append(activation);
			}
			if (activation != null) {
				sb.append("&wa=");
				sb.append(URLEncoder.encode(activation, getCharset()));
			}
			if (distance >= 0) {
				sb.append("&wd=");
				sb.append(distance);
			}
			if (redirect != null) { // other worker conrecte lb's
				sb.append("&wr=");
				sb.append(URLEncoder.encode(redirect, getCharset()));
			}
			if (domain != null) {
				sb.append("&wc=");
				sb.append(URLEncoder.encode(domain, getCharset()));
			}
            
        } catch (UnsupportedEncodingException e) {
            throw new BuildException("Invalid 'charset' attribute: "
                    + getCharset());
        }
        return sb;
    }

    /**
	 * check correct lb and worker pararmeter
	 */
    protected void checkParameter() {
        if (worker == null) {
            throw new BuildException("Must specify 'worker' attribute");
        }
        if (loadbalancer == null) {
            throw new BuildException("Must specify 'loadbalancer' attribute");
        }
    }
}