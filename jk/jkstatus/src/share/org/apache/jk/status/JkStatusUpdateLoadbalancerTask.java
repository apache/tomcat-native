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
 * Ant task that implements the <code>/status</code> update loadbalancer command, supported by the
 * mod_jk status (1.2.20) application.
 * 
 * 
 * @author Peter Rossbach
 * @version $Revision:$
 * @since mod_jk 1.2.20
 */
public class JkStatusUpdateLoadbalancerTask extends AbstractJkStatusTask {

    /**
     * The descriptive information about this implementation.
     */
    private static final String info = "org.apache.jk.status.JkStatusUpdateLoadbalancerTask/1.0";

    protected String loadbalancer ;
    
    protected int retries = -1;
    protected int recoverWaitTime = -1;
    protected int methodCode = -1;
    protected String method;

    protected Boolean stickySession ;
    
    protected Boolean forceStickySession ;
   
    protected int lockCode = -1;
    protected String lock;

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
    public JkStatusUpdateLoadbalancerTask() {
        super();
        setUrl("http://localhost/jkstatus");
    }
 
    /**
	 * @return the forceStickySession
	 */
	public Boolean getForceStickySession() {
		return forceStickySession;
	}

	/**
	 * @param forceStickySession the forceStickySession to set
	 */
	public void setForceStickySession(Boolean forceStickySession) {
		this.forceStickySession = forceStickySession;
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
	 * @return the locking
	 */
	public String getLock() {
		return lock;
	}

	/**
	 * @param locking the locking to set
	 */
	public void setLock(String locking) {
		this.lock = locking;
	}

	/**
	 * @return the lockingCode
	 */
	public int getLockCode() {
		return lockCode;
	}

	/**
	 * @param lockingCode the lockingCode to set
	 */
	public void setLockCode(int lockingCode) {
		this.lockCode = lockingCode;
	}

	/**
	 * @return the method
	 */
	public String getMethod() {
		return method;
	}

	/**
	 * @param method the method to set
	 */
	public void setMethod(String method) {
		this.method = method;
	}

	/**
	 * @return the methodCode
	 */
	public int getMethodCode() {
		return methodCode;
	}

	/**
	 * @param methodCode the methodCode to set
	 */
	public void setMethodCode(int methodCode) {
		this.methodCode = methodCode;
	}

	/**
	 * @return the recoverWaitTime
	 */
	public int getRecoverWaitTime() {
		return recoverWaitTime;
	}

	/**
	 * @param recoverWaitTime the recoverWaitTime to set
	 */
	public void setRecoverWaitTime(int recoverWaitTime) {
		this.recoverWaitTime = recoverWaitTime;
	}

	/**
	 * @return the retries
	 */
	public int getRetries() {
		return retries;
	}

	/**
	 * @param retries the retries to set
	 */
	public void setRetries(int retries) {
		this.retries = retries;
	}

	/**
	 * @return the stickySession
	 */
	public Boolean getStickySession() {
		return stickySession;
	}

	/**
	 * @param stickySession the stickySession to set
	 */
	public void setStickySession(Boolean stickySession) {
		this.stickySession = stickySession;
	}

	/**
     * Create JkStatus worker update link
     * <ul>
     * </b>http://localhost/jkstatus?cmd=update&mime=txt&w=loadbalancer&lm=1&ll=1&lr=2&lt=60&ls=true&lf=false
     * <br/>
     *
     * 
     * <br/>Tcp worker parameter:
     * <br/>
     * <ul>
     * <li><b>w:<b/> name loadbalancer</li>
     * <li><b>lm:<b/> method (lb strategy)</li>
     * <li><b>ll:<b/> lock</li>
     * <li><b>lr:<b/> retries</li>
     * <li><b>lt:<b/> recover wait timeout</li>
     * <li><b>ls:<b/> sticky session</li>
     * <li><b>lf:<b/> force sticky session</li>
     * </ul>
     * <ul>
     * <li>lm=1 or Requests</li>
     * <li>lm=2 or Traffic</li>
     * <li>lm=3 or Busyness</li>
     * <li>lm=4 or Sessions</li>
     * </ul>
     * <ul>
     * <li>ll=1 or Optimistic</li>
     * <li>ll=2 or Pessimistic</li>
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
            if (stickySession != null) { 
				sb.append("&ls=");
				sb.append(stickySession);
			}
            if (forceStickySession != null) { 
 				sb.append("&lf=");
 				sb.append(forceStickySession);
 			}
			if (retries >= 0) {
				sb.append("&lr=");
				sb.append(retries);
			}
			if (recoverWaitTime >= 0) {
				sb.append("&lt=");
				sb.append(recoverWaitTime);
			}
			if (method == null && methodCode > 0 && methodCode < 5) {
				sb.append("&lm=");
				sb.append(methodCode);
			}
            if (method != null) { 
 				sb.append("&lm=");
 				sb.append(method);
 			}
			if (lock == null && lockCode > 0 && lockCode < 3) {
				sb.append("&ll=");
				sb.append(lockCode);
			}
            if (lock != null) { 
 				sb.append("&ll=");
 				sb.append(lock);
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
        if (loadbalancer == null) {
            throw new BuildException("Must specify 'loadbalancer' attribute");
        }
    }
}