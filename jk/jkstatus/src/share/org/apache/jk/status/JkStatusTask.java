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

import java.util.Iterator;
import java.util.List;

import org.apache.catalina.ant.BaseRedirectorHelperTask;
import org.apache.tomcat.util.IntrospectionUtils;
import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.Project;

/**
 * Ant task that implements the show <code>/jkstatus</code> command, supported
 * by the mod_jk status (1.2.13) application.
 * 
 * @author Peter Rossbach
 * @version $Revision$
 * @since 5.5.10
 */
public class JkStatusTask extends BaseRedirectorHelperTask {

    /**
     * The descriptive information about this implementation.
     */
    private static final String info = "org.apache.jk.status.JkStatusTask/1.2";

    /**
     * Store status as <code>resultProperty</code> prefix.
     */
    protected String resultproperty;

    /**
     * Echo status at ant console
     */
    protected boolean echo = false;

    /**
     * The login password for the <code>mod_jk status</code> page.
     */
    protected String password = null;

    /**
     * The URL of the <code>mod_jk status</code> page to be used.
     */
    protected String url = "http://localhost:80/jkstatus";

    /**
     * The login username for the <code>mod_jk status</code> page.
     */
    protected String username = null;

    private String errorProperty;

    private String worker;

    private String loadbalancer;

    /**
     * Return descriptive information about this implementation and the
     * corresponding version number, in the format
     * <code>&lt;description&gt;/&lt;version&gt;</code>.
     */
    public String getInfo() {

        return (info);

    }

    public String getPassword() {
        return (this.password);
    }

    public void setPassword(String password) {
        this.password = password;
    }

    public String getUrl() {
        return (this.url);
    }

    public void setUrl(String url) {
        this.url = url;
    }

    public String getUsername() {
        return (this.username);
    }

    public void setUsername(String username) {
        this.username = username;
    }

    /**
     * @return Returns the echo.
     */
    public boolean isEcho() {
        return echo;
    }

    /**
     * @param echo
     *            The echo to set.
     */
    public void setEcho(boolean echo) {
        this.echo = echo;
    }

    /**
     * @return Returns the resultproperty.
     */
    public String getResultproperty() {
        return resultproperty;
    }

    /**
     * @param resultproperty
     *            The resultproperty to set.
     */
    public void setResultproperty(String resultproperty) {
        this.resultproperty = resultproperty;
    }

    /**
     * @return Returns the loadbalancer.
     */
    public String getLoadbalancer() {
        return loadbalancer;
    }
    /**
     * @param loadbalancer The loadbalancer to set.
     */
    public void setLoadbalancer(String loadbalancer) {
        this.loadbalancer = loadbalancer;
    }
    /**
     * @return Returns the worker.
     */
    public String getWorker() {
        return worker;
    }
    /**
     * @param worker The worker to set.
     */
    public void setWorker(String worker) {
        this.worker = worker;
    }
    // --------------------------------------------------------- Public Methods

    /**
     * Get jkstatus from server.
     * 
     * @exception BuildException
     *                if a validation error occurs
     */
    public void execute() throws BuildException {

        if (url == null) {
            throw new BuildException("Must specify an 'url'");
        }
        boolean isWorkerOnly = worker != null && !"".equals(worker);
        boolean isLoadbalancerOnly = loadbalancer != null
                && !"".equals(loadbalancer);

        StringBuffer error = new StringBuffer();
        try {
            JkStatusAccessor accessor = new JkStatusAccessor();
            JkStatus status = accessor.status(url, username, password);
            if (status.result != null && !"OK".equals(status.result.type) ) {
                if (getErrorProperty() != null) {
                    getProject().setNewProperty(errorProperty, status.result.message);
                }
                if (isFailOnError()) {
                    throw new BuildException(status.result.message);
                } else {
                    handleErrorOutput(status.result.message);
                    return;
                }

            }
            
            if (!isWorkerOnly && !isLoadbalancerOnly) {
                JkServer server = status.getServer();
                JkSoftware software = status.getSoftware();
                JkResult result = status.getResult();
                if (resultproperty != null) {
                    createProperty(server, "server", "name");
                    createProperty(server, "server", "port");
                    createProperty(software, "web_server");
                    createProperty(software, "jk_version");
                    createProperty(result, "result", "type");
                    createProperty(result, "result", "message");
                }
                if (isEcho()) {
                    handleOutput("server name=" + server.getName() + ":"
                            + server.getPort() + " - " + software.getWeb_server() + " - " + software.getJk_version());
                }
            }
            List balancers = status.getBalancers();
            for (Iterator iter = balancers.iterator(); iter.hasNext();) {
                JkBalancer balancer = (JkBalancer) iter.next();
                String balancerIndex = null;
                if (isLoadbalancerOnly) {
                    if (loadbalancer.equals(balancer.getName())) {
                        if (resultproperty != null) {
                            setPropertyBalancerOnly(balancer);
                        }
                        echoBalancer(balancer);
                        return;
                    }
                } else {
                    if (!isWorkerOnly) {
                        if (resultproperty != null) { 
                        	if ( balancer.getId() >= 0)
                        		balancerIndex = Integer.toString(balancer.getId());
                        	else
                        		balancerIndex = balancer.getName() ;
                        	
                        	setPropertyBalancer(balancer,balancerIndex);
                        }
                        echoBalancer(balancer);
                    }
                    List members = balancer.getBalancerMembers();
                    for (Iterator iterator = members.iterator(); iterator
                            .hasNext();) {
                        JkBalancerMember member = (JkBalancerMember) iterator
                                .next();
                        if (isWorkerOnly) {
                            if (worker.equals(member.getName())) {
                                if (resultproperty != null) {
                                    setPropertyWorkerOnly(balancer, member);
                                }
                                echoWorker(member);
                                return;
                            }
                        } else {
                            if (resultproperty != null) {
                                setPropertyWorker(null, member);
                            }
                            echoWorker(member);
                            if (member.getStatus() != null && !"OK".equals(member.getStatus())) {
                                error.append(" worker name=" + member.getName()
                                        + " status=" + member.getStatus()
                                        + " host=" + member.getAddress());
                            }
                            if (member.getState() != null && !("OK".equals(member.getState()) || "N/A".equals(member.getState())) ){
                                error.append(" worker name=" + member.getName()
                                        + " state=" + member.getState()
                                        + " host=" + member.getAddress());
                            }
                        }
                    }
                    if (!isWorkerOnly) {
                        if (resultproperty != null && members.size() > 0) {
                            getProject().setNewProperty(
                                    resultproperty + "."
                                            + balancer.getName() + ".length",
                                    Integer.toString(members.size()));
                        }
                        List mappings = balancer.getBalancerMappings();
                        int j = 0;
                        String mapIndex ;
                        if( balancerIndex != null )
                        	mapIndex = balancerIndex + ".map" ;
                        else
                        	mapIndex = "map" ;
                        for (Iterator iterator = mappings.iterator(); iterator
                                .hasNext(); j++) {
                            JkBalancerMapping mapping = (JkBalancerMapping) iterator
                                    .next();
                            if (resultproperty != null) {
                            	String stringIndex2 ;
                                if( mapping.getId() >= 0) {
                                    stringIndex2 =  Integer.toString(mapping.getId()) ;
                                } else {
                                    stringIndex2 = Integer.toString(j);
                                }                       	
                                createProperty(mapping, mapIndex,
                                        stringIndex2, "type");
                                createProperty(mapping, mapIndex,
                                        stringIndex2, "uri");
                                createProperty(mapping, mapIndex,
                                        stringIndex2, "context");
                                createProperty(mapping, mapIndex,
                                        stringIndex2, "source");
                            }
                            if (isEcho()) {
                            	String mappingOut ;
                            	
                            	if(mapping.source != null) {
                            		mappingOut =
                            			"balancer name="
                                        + balancer.getName() + " mappingtype="
                                        + mapping.getType() + " uri="
                                        + mapping.getUri() + " source="
                                        + mapping.getSource() ;
                            	} else {
                            		mappingOut = "balancer name="
                                        + balancer.getName() + " mappingtype="
                                        + mapping.getType() + " uri="
                                        + mapping.getUri() + " context="
                                        + mapping.getContext() ;
                            	}
                            	handleOutput(mappingOut);
                            }
                        }
                        if (resultproperty != null && mappings.size() > 0) {
                            getProject().setNewProperty(
                                    resultproperty + "."
                                            + mapIndex + ".length",
                                    Integer.toString(mappings.size()));
                        }
                    }
                }
            }
            if (!isWorkerOnly && !isLoadbalancerOnly) {
                if (resultproperty != null && balancers.size() > 0) {
                    getProject().setNewProperty(
                            resultproperty + ".length",
                            Integer.toString(balancers.size()));
                }
            }
        } catch (Throwable t) {
            error.append(t.getMessage());
            if (getErrorProperty() != null) {
                getProject().setNewProperty(errorProperty, error.toString());
            }
            if (isFailOnError()) {
                throw new BuildException(t);
            } else {
                handleErrorOutput(t.getMessage());
                return;
            }
        }
        if (error.length() != 0) {
            if (getErrorProperty() != null) {
                getProject().setNewProperty(errorProperty, error.toString());
            }
            if (isFailOnError()) {
                // exception should be thrown only if failOnError == true
                // or error line will be logged twice
                throw new BuildException(error.toString());
            }
        }

    }

    /**
     * @param member
     */
    private void echoWorker(JkBalancerMember member) {
        if (isEcho()) {
            StringBuffer state = new StringBuffer("worker name=") ;
            state.append( member.getName()) ;
            if(member.getStatus() != null) {
                state.append(" status=");
                state.append(member.getStatus());
            }
            if(member.getState() != null) {
                state.append(" state=");
                state.append(member.getState())  ;
            }
            state.append(" host=");
            state.append(member.getAddress());
            handleOutput(state.toString());
        }
    }

    /**
     * @param balancer
     */
    private void echoBalancer(JkBalancer balancer) {
        if (isEcho()) {
            handleOutput("balancer name=" + balancer.getName() + " type="
                    + balancer.getType());
        }
    }

    /**
     * @param balancer
     */
    private void setPropertyBalancerOnly(JkBalancer balancer) {
        String prefix = resultproperty + "." + balancer.getName();
        if(balancer.getId() >= 0 ) {
        	getProject().setNewProperty(prefix + ".id",
                Integer.toString(balancer.getId()));
        }
        getProject().setNewProperty(prefix + ".type", balancer.getType());
        getProject().setNewProperty(prefix + ".stick_session",
                Boolean.toString(balancer.isSticky_session()));
        getProject().setNewProperty(prefix + ".sticky_session_force",
                Boolean.toString(balancer.isSticky_session_force()));
        getProject().setNewProperty(prefix + ".retries",
                Integer.toString(balancer.getRetries()));
        getProject().setNewProperty(prefix + ".recover_time",
                Integer.toString(balancer.getRecover_time()));
        getProject().setNewProperty(prefix + ".method",
                balancer.getMethod());
        getProject().setNewProperty(prefix + ".good",
                Integer.toString(balancer.getGood()));
        getProject().setNewProperty(prefix + ".degraded",
                Integer.toString(balancer.getDegraded()));
        getProject().setNewProperty(prefix + ".bad",
                Integer.toString(balancer.getBad()));
        getProject().setNewProperty(prefix + ".busy",
                Integer.toString(balancer.getBusy()));
        getProject().setNewProperty(prefix + ".map_count",
                Integer.toString(balancer.getMap_count()));
        getProject().setNewProperty(prefix + ".member_count",
                Integer.toString(balancer.getMember_count()));
        getProject().setNewProperty(prefix + ".max_busy",
                Integer.toString(balancer.getMax_busy()));
        getProject().setNewProperty(prefix + ".lock",
                balancer.getLock());
 
    }

    /**
     * @param balancer
     * @return
     */
    private void setPropertyBalancer(JkBalancer balancer,String balancerIndex) {
     	if(balancer.id >= 0) {
    		createProperty(balancer, balancerIndex, "id");
    	}

        createProperty(balancer, balancerIndex, "name");
        createProperty(balancer, balancerIndex, "type");
        createProperty(balancer, balancerIndex, "sticky_session");
        createProperty(balancer, balancerIndex, "sticky_session_force");
        createProperty(balancer, balancerIndex, "retries");
        createProperty(balancer, balancerIndex, "recover_time");
        if(balancer.getMethod() != null) {
        	createProperty(balancer, balancerIndex, "method");
        }
        if(balancer.getLock() != null) {
        	createProperty(balancer, balancerIndex, "lock");
        }
        if(balancer.getGood() >= 0) {
        	createProperty(balancer, balancerIndex, "good");
        }
        if(balancer.getDegraded() >= 0) {
        	createProperty(balancer, balancerIndex, "degraded");
        }
        if(balancer.getBad() >= 0) {
        	createProperty(balancer, balancerIndex, "bad");
        }
        if(balancer.getBusy() >= 0) {
        	createProperty(balancer, balancerIndex, "busy");
        }
        if(balancer.getMax_busy() >= 0) {
        	createProperty(balancer, balancerIndex, "max_busy");
        }
        if(balancer.getMember_count() >=0) {
        	createProperty(balancer, balancerIndex, "member_count");
        }
        if(balancer.getMember_count() >=0) {
        	createProperty(balancer, balancerIndex, "map_count");
        }
   }

    /**
     * @param balancerIndex
     * @param member
     */
    private void setPropertyWorker(String balancerIndex, JkBalancerMember member) {
        String workerIndex ;
        if(member.getId() >= 0) {
        	workerIndex = Integer.toString(member.getId());
            createProperty(member, balancerIndex, workerIndex, "id");
            createProperty(member, balancerIndex, workerIndex, "name");
        } else {
        	workerIndex = member.getName();
        }
        createProperty(member, balancerIndex, workerIndex, "type");
        createProperty(member, balancerIndex, workerIndex, "host");
        createProperty(member, balancerIndex, workerIndex, "port");
        createProperty(member, balancerIndex, workerIndex, "address");
        if(member.getJvm_route() != null) {
            createProperty(member, balancerIndex, workerIndex, "jvm_route");
        }
        if(member.getRoute() != null) {
            createProperty(member, balancerIndex, workerIndex, "route");
        }       
        if(member.getStatus() != null) {
            createProperty(member, balancerIndex, workerIndex, "status");
        }
        if(member.getActivation() != null) {
            createProperty(member, balancerIndex, workerIndex, "activation");
        }
        if(member.getState() != null) {
            createProperty(member, balancerIndex, workerIndex, "state");
        }
        createProperty(member, balancerIndex, workerIndex, "lbfactor");
        createProperty(member, balancerIndex, workerIndex, "lbvalue");
        if(member.getLbmult() >= 0) {
            createProperty(member, balancerIndex, workerIndex, "lbmult");
        }
        createProperty(member, balancerIndex, workerIndex, "elected");
        createProperty(member, balancerIndex, workerIndex, "readed");
        createProperty(member, balancerIndex, workerIndex, "busy");
        if(member.getMax_busy() >= 0) {
            createProperty(member, balancerIndex, workerIndex, "max_busy");
        }
        createProperty(member, balancerIndex, workerIndex, "transferred");
        createProperty(member, balancerIndex, workerIndex, "errors");
        if(member.getClient_errors() >= 0) {
            createProperty(member, balancerIndex, workerIndex, "client_errors");
        }
        if(member.getDistance() >= 0) {
            createProperty(member, balancerIndex, workerIndex, "distance");
        }
        if (member.getDomain() != null) {
            createProperty(member, balancerIndex, workerIndex, "domain");
        } else {
            getProject().setNewProperty(resultproperty + "." + balancerIndex + "." + workerIndex +
                    ".domain", "");          
        }
        if (member.getRedirect() != null) {
            createProperty(member, balancerIndex, workerIndex, "redirect");
        } else {
            getProject().setNewProperty(resultproperty + "." + balancerIndex + "." + workerIndex +
                    ".redirect", "");          
        }
    }

    /**
     * @param balancer
     * @param member
     */
    private void setPropertyWorkerOnly(JkBalancer balancer,
            JkBalancerMember member) {
        //String prefix = resultproperty + "." + balancer.getName() + "." + member.getName();
        String prefix = resultproperty + "." + member.getName();
               Project currentProject = getProject();
        if ( balancer.getId() >= 0) { 
        	currentProject.setNewProperty(prefix + ".lb.id",
                Integer.toString(balancer.getId()));
        }
        //currentProject.setNewProperty(prefix + ".lb.name", balancer.getName());
        if( member.getId() >= 0) {
        	currentProject.setNewProperty(prefix + ".id",
                Integer.toString(member.getId()));
        }
        currentProject.setNewProperty(prefix + ".type", member.getType());
        if (member.getJvm_route() != null) {
            currentProject.setNewProperty(prefix + ".jvm_route", member.getJvm_route());
        }
        if (member.getRoute() != null) {
            currentProject.setNewProperty(prefix + ".route", member.getRoute());
        }
        if (member.getStatus() != null) {
            currentProject.setNewProperty(prefix + ".status", member.getStatus());
        }
        if (member.getActivation() != null) {
            currentProject.setNewProperty(prefix + ".activation", member.getActivation());
        }
        if (member.getState() != null) {
            currentProject.setNewProperty(prefix + ".state", member.getState());
        }
        currentProject.setNewProperty(prefix + ".host", member.getHost());
        currentProject.setNewProperty(prefix + ".address", member.getAddress());
        currentProject.setNewProperty(prefix + ".port",
                Integer.toString(member.getPort()));
        currentProject.setNewProperty(prefix + ".lbfactor",
                Integer.toString(member.getLbfactor()));
        currentProject.setNewProperty(prefix + ".lbvalue",
                Long.toString(member.getLbvalue()));
        if(member.getLbmult() >= 0) {
            currentProject.setNewProperty(prefix + ".lbmult",
                    Long.toString(member.getLbmult()));
        }
        currentProject.setNewProperty(prefix + ".elected",
                Long.toString(member.getElected()));
        currentProject.setNewProperty(prefix + ".readed",
                Long.toString(member.getReaded()));
        currentProject.setNewProperty(prefix + ".transferred",
                Long.toString(member.getTransferred()));
        currentProject.setNewProperty(prefix + ".busy",
                Integer.toString(member.getBusy()));
        if(member.getMax_busy() >= 0) {
            currentProject.setNewProperty(prefix + ".max_busy",
                    Long.toString(member.getMax_busy()));
        }
        currentProject.setNewProperty(prefix + ".errors",
                Long.toString(member.getErrors()));
        if(member.getClient_errors() >= 0) {
            currentProject.setNewProperty(prefix + ".client_errors",
                    Long.toString(member.getClient_errors()));
        }
        if(member.getDistance() >= 0) {
            currentProject.setNewProperty(prefix + ".distance",
                    Integer.toString(member.getDistance()));
        }
        if (member.getDomain() != null) {
            currentProject.setNewProperty(prefix + ".domain", member.getDomain());
        } else {
            currentProject.setNewProperty(prefix + ".domain", "");
        }
        if (member.getRedirect() != null) {
            currentProject.setNewProperty(prefix + ".redirect",
                    member.getRedirect());
        } else {
            currentProject.setNewProperty(prefix + ".redirect", "");
        }
        if(member.getTime_to_recover() >= 0) {
            currentProject.setNewProperty(prefix + ".time_to_recover",
                    Integer.toString(member.getTime_to_recover()));
        }
        if(member.getTime_to_recover_min() >= 0) {
            currentProject.setNewProperty(prefix + ".time_to_recover_min",
                    Integer.toString(member.getTime_to_recover_min()));
        }
        if(member.getTime_to_recover_max() >= 0) {
            currentProject.setNewProperty(prefix + ".time_to_recover_max",
                    Integer.toString(member.getTime_to_recover_max()));
            currentProject.setNewProperty(prefix + ".time_to_recover",
                    (Integer.toString(member.getTime_to_recover_min()) +
                    Integer.toString(member.getTime_to_recover_max()) / 2);
        }
            
    }

    /*
     * Set ant property for save error state
     * 
     * @see org.apache.catalina.ant.BaseRedirectorHelperTask#setErrorProperty(java.lang.String)
     */
    public void setErrorProperty(String arg0) {
        errorProperty = arg0;
        super.setErrorProperty(arg0);
    }

    /**
     * @return Returns the errorProperty.
     */
    public String getErrorProperty() {
        return errorProperty;
    }

    protected void createProperty(Object result, String attribute) {
        createProperty(result, null, null, attribute);
    }

    protected void createProperty(Object result, String arraymark,
            String attribute) {
        createProperty(result, arraymark, null, attribute);
    }

    /**
     * create result as property with name from attribute resultproperty
     */
    protected void createProperty(Object result, String arraymark,
            String arraymark2, String attribute) {
        if (resultproperty != null) {
            Object value = IntrospectionUtils.getProperty(result, attribute);
            if (value != null ) {
                StringBuffer propertyname = new StringBuffer(resultproperty);

                if (result instanceof JkBalancer) {
                    if (arraymark != null) {
                        propertyname.append(".");
                        propertyname.append(arraymark);
                    }
                } else if (result instanceof JkServer) {
					if (arraymark != null) {
						propertyname.append(".");
						propertyname.append(arraymark);
					}
                } else if (result instanceof JkSoftware) {
					if (arraymark != null) {
						propertyname.append(".");
						propertyname.append(arraymark);
					}
				} else if (result instanceof JkResult) {
					if (arraymark != null) {
						propertyname.append(".");
						propertyname.append(arraymark);
					}
                } else if (result instanceof JkBalancerMember) {
                    if (arraymark != null) {
                        propertyname.append(".");
                        propertyname.append(arraymark);
                    }
                    if (arraymark2 != null) {
                        propertyname.append(".");
                        propertyname.append(arraymark2);
                    }
                } else if (result instanceof JkBalancerMapping) {
                    if (arraymark != null) {
                        propertyname.append(".");
                        propertyname.append(arraymark);
                    }
                    if (arraymark2 != null) {
                        propertyname.append(".");
                        propertyname.append(arraymark2);
                    }
                }
                propertyname.append(".");
                propertyname.append(attribute);
                getProject().setNewProperty(propertyname.toString(),
                        value.toString());
            }
        }
    }

}
